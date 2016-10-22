/**
 * create: 2016-10-10
 * author: mr.wclong@yahoo.com
 */
#ifndef MODULE_DEFAULT_HPP
#define MODULE_DEFAULT_HPP
#include "../handler.hpp"

using namespace coh;

class default_module : public module_handler {
public:
    default_module(http_service::options option,
                   YAML::Node            config);
    int  handle_events();
    const char* path() const;
    const char* name() const;
    void on_request_header(conid_t              cid,
                           txnid_t              tid,
                           const http_request&  req,
                           http_response*       rsp);
private:
    http_service::options   option_;
};

extern "C" module_handler* create_module(http_service::options  option,
                                         YAML::Node             config)
{
    return new default_module(option, config);
}

#endif
