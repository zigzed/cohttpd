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


#ifndef SO_REUSEPORT
#define SO_REUSEPORT    15
#endif
#ifndef TCP_FASTOPEN
#define TCP_FASTOPEN    23
#endif

io_service  ios;

static void do_fork (coh::http_service::options& options,
                     tcp::acceptor&              acceptor,
                     coh::semops&                mutex);

struct check_alive {
    check_alive(coh::http_service::options& options,
                tcp::acceptor&              acceptor,
                coh::semops&                mutex);
    void check();
private:
    coh::http_service::options& options_;
    tcp::acceptor&              acceptor_;
    coh::semops&                mutex_;
};

static void is_alive();

int main(int argc, char* argv[])
{
    coh::http_service::options options;
    options.timer_idle  = seconds(5);
    options.timer_life  = minutes(1);
    options.reuse_port  = 0;
    options.serve_port  = 8080;
    options.process     = 2;
    options.mod_path    = "modules";
    options.coh_log     = initlog(false);
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
            do_fork(options, tcp_acceptor, acceptor_mutex);
        }

        check_alive monitor(options, tcp_acceptor, acceptor_mutex);
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
             coh::semops&                mutex)
{
    int rc = fork();
    if(rc == 0) {
        coh::modules    module_manager;
        module_manager.setup(options, options.mod_path.c_str());
        options.handler = &module_manager;

        coh::http_service& service = coh::http_service::instance();
        service.setup(&ios,
                      &acceptor,
                      &mutex,
                      options);

        go std::bind(&coh::http_service::start, service);

        co_sched.RunLoop();
    }
}

check_alive::check_alive(coh::http_service::options& options,
                         tcp::acceptor&              acceptor,
                         coh::semops&                mutex)
    : options_(options)
    , acceptor_(acceptor)
    , mutex_(mutex)
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
        do_fork(options_, acceptor_, mutex_);
    }

    co_timer_add(seconds(5), std::bind(&check_alive::check, this));
}
