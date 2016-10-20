#include "default.hpp"


default_module::default_module(http_service::options option)
    : module_handler(option)
    , option_(option)
{
}

int default_module::handle_events()
{
    return module_handler::HE_REQUEST_HDR;
}

const char* default_module::path() const
{
    return NULL;
}

const char* default_module::name() const
{
    return "default";
}

void default_module::on_request_header(conid_t              cid,
                                       txnid_t              tid,
                                       const http_request&  req,
                                       http_response*       rsp)
{
    rsp->status(404, "Not Found");
    rsp->keep_alive(req.keep_alive());
    rsp->content_type("text/html", NULL);

    int n = rsp->printf("%s\r\n", "<html><body><h1>Not Found</h1></body></html>");

    rsp->header("Content-Length", "%d", n);
    rsp->eom();
}
