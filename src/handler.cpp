/**
 * create: 2016-10-06
 * author: mr.wclong@yahoo.com
 */
#include "handler.hpp"

namespace coh {

    class default_handler : public module_handler {
    public:
        void on_request_header(conid_t              cid,
                               txnid_t              tid,
                               const http_request&  req,
                               http_response*       rsp);
    };

    void default_handler::on_request_header(conid_t              cid,
                                            txnid_t              tid,
                                            const http_request&  req,
                                            http_response*       rsp)
    {
        // TODO: 返回 404 错误信息
    }

}
