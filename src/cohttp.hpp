/**
 * create: 2016-02-06
 * author: mr.wclong@yahoo.com
 */
#ifndef COH_COHTTP_HPP
#define COH_COHTTP_HPP

#include <chrono>

#include "libgo/coroutine.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/dist_sink.h"
#include "boost/asio.hpp"
#include "boost/filesystem.hpp"

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std::chrono;

inline std::shared_ptr<spdlog::logger > initlog(const char* file, bool enable_console)
{
    boost::filesystem::path p(file);
    boost::filesystem::create_directory(p);

    char name[64];
    snprintf(name, 64, "%s/cohttpd-%d", file, getpid());

    auto dist_sink = std::make_shared<spdlog::sinks::dist_sink_st >();
    auto logfile = std::make_shared<
                    spdlog::sinks::daily_file_sink<
                        spdlog::details::null_mutex,
                        spdlog::sinks::dateonly_daily_file_name_calculator > >(name, "log", 23, 59);

    dist_sink->add_sink(logfile);
    if(enable_console) {
        auto console = std::make_shared<spdlog::sinks::ansicolor_sink >(spdlog::sinks::stdout_sink_st::instance());
        dist_sink->add_sink(console);
    }

    auto clog = std::make_shared<spdlog::logger >("coh", dist_sink);
    clog->set_pattern("%L%H%M%S.%f %t %v");
    clog->flush_on(spdlog::level::err);
    return clog;
}

#endif
