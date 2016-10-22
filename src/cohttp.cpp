/**
 * create: 2016-09-27
 * author: mr.wclong@yahoo.com
 */
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <stdexcept>

#include "cohttp.hpp"
#include "errors.hpp"
#include "semops.hpp"
#include "server.hpp"
#include "modules.hpp"

#include "yaml-cpp/yaml.h"


#ifndef SO_REUSEPORT
#define SO_REUSEPORT    15
#endif
#ifndef TCP_FASTOPEN
#define TCP_FASTOPEN    23
#endif

io_service  ios;

static void do_fork (coh::http_service::options& options,
                     tcp::acceptor&              acceptor,
                     coh::semops&                mutex,
                     const char*                 config);

struct check_alive {
    check_alive(coh::http_service::options& options,
                tcp::acceptor&              acceptor,
                coh::semops&                mutex,
                const std::string&          config);
    void check();
private:
    coh::http_service::options& options_;
    tcp::acceptor&              acceptor_;
    coh::semops&                mutex_;
    std::string                 config_;
};

static void                         is_alive    ();
static coh::http_service::options   load_config (const char* file);

int main(int argc, char* argv[])
{
    const char* config = argv[1];

    coh::http_service::options options = load_config(config);
    options.coh_log     = initlog(options.logger.path.c_str(), options.logger.cout);
    auto clog = options.coh_log;

    rlimit rl;
    rl.rlim_cur = 1024 * 10;
    rl.rlim_max = 1024 * 100;
    if(setrlimit(RLIMIT_NOFILE, &rl) != 0) {
        int code = errno;
        clog->error("setrlimit for file handles failed: {} {}",
                    code, strerror(code));
    }

    coh::semops acceptor_mutex(1);

    tcp::acceptor tcp_acceptor(ios);
    tcp_acceptor.open(tcp::v4());

    tcp_acceptor.set_option(socket_base::reuse_address(true));
    tcp::acceptor::native_type s = tcp_acceptor.native_handle();
    socklen_t   olen = sizeof(int);
    if(options.reuse_port != 0) {
        if(getsockopt(s, SOL_SOCKET, SO_REUSEPORT, (void* )&options.reuse_port, &olen) == -1) {
            options.reuse_port = 0;
        }
        else {
            options.reuse_port = 1;
            olen = sizeof(int);
            if(setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (void* )&options.reuse_port, olen) == 0) {
                clog->info("set SO_REUSEPORT for socket handle done");
            }
            else {
                options.reuse_port = 0;
            }
        }
    }
    int fastopen = 1;
    if(setsockopt(s, IPPROTO_TCP, TCP_FASTOPEN, (void* )&fastopen, sizeof(int)) == 0) {
        clog->info("set TCP_FASTOPEN for socket handle done");
    }

    try {
        tcp_acceptor.bind(tcp::endpoint(tcp::v4(), options.serve_port));
        tcp_acceptor.listen();

        for(int i = 0; i < options.process; ++i) {
            do_fork(options, tcp_acceptor, acceptor_mutex, config);
        }

        check_alive monitor(options, tcp_acceptor, acceptor_mutex, config);
        // 监控各个子进程的状态，每隔5秒检查一次子进程的运行状态
        co_timer_add(seconds(5), std::bind(&check_alive::check, &monitor));

        co_sched.RunLoop();

    }
    catch(const boost::system::system_error& e) {
        clog->error("start cohttpd system error: {}, {}",
                    e.code().value(), e.what());
    }
    catch(const std::runtime_error& e) {
        clog->error("start cohttpd runtime error: {}",
                    e.what());
    }
    
    return 0;    
}

void do_fork(coh::http_service::options& options,
             tcp::acceptor&              acceptor,
             coh::semops&                mutex,
             const char*                 config)
{
    int rc = fork();
    if(rc == 0) {
        try {
            coh::modules    module_manager;
            module_manager.setup(options, options.mod_path.c_str(), config);
            options.handler = &module_manager;

            coh::http_service& service = coh::http_service::instance();
            service.setup(&ios,
                          &acceptor,
                          &mutex,
                          options);

            go std::bind(&coh::http_service::start, service);

            co_sched.RunLoop();
        }
        catch(const std::runtime_error& e) {
            options.coh_log->error("forking service failed: {}",
                                   e.what());
        }
    }
}

check_alive::check_alive(coh::http_service::options& options,
                         tcp::acceptor&              acceptor,
                         coh::semops&                mutex,
                         const std::string&          config)
    : options_(options)
    , acceptor_(acceptor)
    , mutex_(mutex)
    , config_(config)
{
}

void check_alive::check()
{
    int count = 0;
    for(pid_t pid; (pid = waitpid(-1, NULL, WNOHANG)) > 0; ) {
        options_.coh_log->error("cohttp service {} exited, restart",
                                pid);
        count++;
    }
    for(int i = 0; i < count; ++i) {
        do_fork(options_, acceptor_, mutex_, config_.c_str());
    }

    co_timer_add(seconds(5), std::bind(&check_alive::check, this));
}

coh::http_service::options load_config(const char* file)
{
    coh::http_service::options options;
    try {
        YAML::Node config = YAML::LoadFile(file);

        if(config["ListenPort"]) {
            options.serve_port  = config["ListenPort"].as<unsigned short >();
        }
        if(config["ModulePath"]) {
            options.mod_path    = config["ModulePath"].as<std::string >();
        }
        if(config["CpuUsed"]) {
            options.process     = config["CpuUsed"].as<int >();
        }
        const YAML::Node& connections   = config["Connection"];
        if(connections["IdleTimer"]) {
            options.timer_idle  = seconds(connections["IdleTimer"].as<int >());
        }
        if(connections["LifeTimer"]) {
            options.timer_life  = seconds(connections["LifeTimer"].as<int >());
        }
        if(connections["MaxHeaderSize"]) {
            options.memory.max_header_size= connections["MaxHeaderSize"].as<int >();
        }
        if(connections["MaxBodySize"]) {
            options.memory.max_body_size= connections["MaxBodySize"].as<int >() * 1024 * 1024;
        }

        const YAML::Node& memory        = config["Memory"];
        if(memory["ChunkSize"]) {
            options.memory.iobuf_size   = memory["ChunkSize"].as<int >();
        }
        if(memory["ChunkCount"]) {
            options.memory.iobuf_count  = memory["ChunkCount"].as<int >();
        }

        const YAML::Node& logger        = config["Logger"];
        if(logger["Path"]) {
            options.logger.path         = logger["Path"].as<std::string >();
        }
        if(logger["console"]) {
            options.logger.cout         = true;
        }
    }
    catch(const YAML::ParserException& e) {
        fprintf(stderr, "parsing config file %s failed: %s\r\n",
                file, e.what());
    }
    catch(const YAML::BadFile& e) {
        fprintf(stderr, "loading config file %s failed: %s\r\n",
                file, e.what());
    }
    catch(const YAML::InvalidNode& e) {
        fprintf(stderr, "process config file %s failed: %s\r\n",
                file, e.what());
    }
    catch(const YAML::BadConversion& e) {
        fprintf(stderr, "process config file %s failed: %s\r\n",
                file, e.what());
    }

    return options;
}
