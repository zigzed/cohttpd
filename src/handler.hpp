/**
 * create: 2016-10-04
 * author: mr.wclong@yahoo.com
 */
#ifndef COH_HANDLER_HPP
#define COH_HANDLER_HPP
#include <stdint.h>
#include "server.hpp"
#include "request.hpp"
#include "response.hpp"
#include "yaml-cpp/yaml.h"

namespace coh {

    typedef int64_t     conid_t;
    typedef int32_t     txnid_t;

    class module_handler {
    public:
        enum handle_event {
            HE_NONE         = 0x00,
            HE_PROCESS_INIT = 0x01,
            HE_PROCESS_EXIT = 0x02,
            HE_CONNECT_INIT = 0x04,
            HE_CONNECT_EXIT = 0x08,
            HE_REQUEST_HDR  = 0x10,
            HE_REQUEST_BODY = 0x20
        };

        virtual ~module_handler() {}

        explicit module_handler(http_service::options option,
                                YAML::Node            config) {}
        virtual int  handle_events    () = 0;
        virtual const char* path      () const = 0;
        virtual const char* name      () const = 0;

        /** 进程启动时调用。注意，因为 cohttpd 是多进程模型的，所以会被调用多次。*/
        virtual void on_process_init  () {}
        /** 进程退出时调用。注意，因为 cohttpd 是多进程模型的，所以会被调用多次。*/
        virtual void on_process_exit  () {}
        /** 每个连接建立时调用。每个连接会有进程内唯一的连接号 */
        virtual void on_connect_init  (conid_t id) {}
        /** 每个连接释放时调用 */
        virtual void on_connect_exit  (conid_t id) {}
        /** 每次请求头解析完毕后调用
         * @param   cid 连接号，每个进程内唯一的连接编号
         * @param   tid 事务号，同一个连接内多次请求的编号，标识一次请求响应事务。
         * @param   req 请求头
         * @param   rsp 响应内容
         */
        virtual void on_request_header(conid_t              cid,
                                       txnid_t              tid,
                                       const http_request&  req,
                                       http_response*       rsp) {}
        /** 每次请求全部内容解析完毕后调用
         * @param   cid 连接号，每个进程内唯一的连接编号
         * @param   tid 事务号，同一个连接内多次请求的编号，标识一次请求响应事务。
         * @param   req 请求头
         * @param   rsp 响应内容
         */
        virtual void on_request_body  (conid_t              cid,
                                       txnid_t              tid,
                                       const http_request&  req,
                                       http_response*       rsp) {}
    };

//    class filter_handler {
//    public:
//        virtual ~filter_handler();

//    };

}


#endif
