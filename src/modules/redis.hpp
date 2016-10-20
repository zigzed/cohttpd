/**
 * create: 2016-10-18
 * author: mr.wclong@yahoo.com
 */
#ifndef MODULE_REDIS_HPP
#define MODULE_REDIS_HPP
#include "../handler.hpp"
#include "hiredis/hiredis.h"
#include <unordered_map>

using namespace coh;

class redis_module : public module_handler {
public:
    redis_module(http_service::options option);
    int  handle_events();
    const char* path() const;
    const char* name() const;
    void on_process_init();
    void on_process_exit();
    void on_connect_init(conid_t cid);
    void on_connect_exit(conid_t cid);
    void on_request_header(conid_t              cid,
                           txnid_t              tid,
                           const http_request&  req,
                           http_response*       rsp);
private:
    http_service::options   option_;
    bool                    running_;

    struct command {
        conid_t                   cid;
        std::vector<std::string > cmd;
    };

    co_chan<command >                                                   redis_order_;
    std::unordered_map<conid_t, co_chan<std::shared_ptr<redisReply> > > redis_reply_;
};

extern "C" module_handler* create_module(http_service::options option)
{
    return new redis_module(option);
}

#endif
