#include "hello.hpp"


hello_module::hello_module(http_service::options option)
    : module_handler(option)
    , option_(option)
{
}

int hello_module::handle_events()
{
    return module_handler::HE_REQUEST_HDR;
}

const char* hello_module::path() const
{
    return "/hello";
}

const char* hello_module::name() const
{
    return "hello";
}

void hello_module::on_request_header(conid_t              cid,
                                     txnid_t              tid,
                                     const http_request&  req,
                                     http_response*       rsp)
{
    rsp->status(200, "OK");
    rsp->keep_alive(req.keep_alive());
    rsp->content_type("text/html", NULL);

    int n = rsp->printf("%s\r\n", "<html>world!</html>");

    rsp->header("Content-Length", "%d", n);
    rsp->eom();
}
