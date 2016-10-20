/**
 * create: 2016-09-29
 * author: mr.wclong@yahoo.com
 */
#ifndef COH_CODECS_HPP
#define COH_CODECS_HPP
#include "cohttp.hpp"
#include "iobufs.hpp"
#include "server.hpp"
#include "request.hpp"
#include "http_parser.h"

namespace coh {

    class http_codec {
    public:
        http_codec();
        /** 解析 HTTP 请求，对于每个完整的请求会需要调用两次，第一次返回包头，第二次返回
         * body。这样做的目的是为了在 body 比较大的场景下，如果登记的模块不需要 body
         * 的话就可以先处理了。
         */
        http_request* parse(iobuf_t iov, http_request* req);
    private:
        static int on_message_begin   (http_parser* hp) {
            return ((http_request* )hp->data)->codec_->of_message_begin(hp);
        }
        static int on_headers_complete(http_parser* hp) {
            return ((http_request* )hp->data)->codec_->of_headers_complete(hp);
        }
        static int on_message_complete(http_parser* hp) {
            return ((http_request* )hp->data)->codec_->of_message_complete(hp);
        }
        static int on_url             (http_parser* hp, const char* at, size_t length) {
            return ((http_request* )hp->data)->codec_->of_url(hp, at, length);
        }
        static int on_header_field    (http_parser* hp, const char* at, size_t length) {
            return ((http_request* )hp->data)->codec_->of_header_field(hp, at, length);
        }
        static int on_header_value    (http_parser* hp, const char* at, size_t length) {
            return ((http_request* )hp->data)->codec_->of_header_value(hp, at, length);
        }
        static int on_body            (http_parser* hp, const char* at, size_t length) {
            return ((http_request* )hp->data)->codec_->of_body(hp, at, length);
        }

        int of_message_begin   (http_parser* hp);
        int of_headers_complete(http_parser* hp);
        int of_message_complete(http_parser* hp);
        int of_url             (http_parser* hp, const char* at, size_t length);
        int of_header_field    (http_parser* hp, const char* at, size_t length);
        int of_header_value    (http_parser* hp, const char* at, size_t length);
        int of_body            (http_parser* hp, const char* at, size_t length);


        http_parser_settings            config_;
    };

}

#ifdef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "3rd/doctest.h"
#include "codecs.hpp"
#include "iobufs.hpp"
#include "request.hpp"

#include <stdio.h>
#include <string>
#include <vector>


TEST_CASE("basic http request parser")
{
    coh::http_codec codec;
    coh::iobufs     mpool(1024, 4);
    coh::iobuf_t    iov = mpool.acquire();

    char* p = coh::iobuf_get_w_buf(iov);
    int n = snprintf(p, coh::iobuf_get_w_available(iov),
                     "%s",
                     "POST / HTTP/1.1\r\n"
                     "Host: www.example.com\r\n"
                     "Content-Type: application/x-www-form-urlencoded\r\n"
                     "Content-Length: 4\r\n"
                     "Connection: close\r\n"
                     "\r\n"
                     "q=42\r\n");
    coh::iobuf_set_w_len(iov, n);

    coh::http_request req;
    codec.parse(iov, &req);

    CHECK(req.headers_completed());
    CHECK(req.major() == 1);
    CHECK(req.minor() == 1);
    CHECK(req.method() == HTTP_POST);
    CHECK(req.error() == HPE_OK);

    codec.parse(iov, &req);

    CHECK(req.message_completed());
    CHECK(req.bodysz() == 4);
    CHECK(req.keep_alive() == false);
    CHECK(req.error() == HPE_OK);
}

TEST_CASE("partial http request header parser")
{
    coh::http_codec codec;
    coh::iobufs     mpool(1024, 4);
    coh::iobuf_t    iov = mpool.acquire();

    char* p = coh::iobuf_get_w_buf(iov);
    int n = snprintf(p, coh::iobuf_get_w_available(iov),
                     "%s",
                     "POST / HTTP/1.1\r\n"
                     "Host: www.example.com\r\n"
                     "Content-Type: application/x-www-form-urlencoded\r\n"
                     "Content-Length: 4\r\n"
                     "Connec");
    coh::iobuf_set_w_len(iov, n);

    coh::http_request req;
    codec.parse(iov, &req);

    CHECK(!req.headers_completed());
    CHECK(req.major() == 1);
    CHECK(req.minor() == 1);
    CHECK(req.method() == HTTP_POST);
    CHECK(req.error() == HPE_OK);
}

TEST_CASE("partial http request body parser")
{
    coh::http_codec codec;
    coh::iobufs     mpool(1024, 4);
    coh::iobuf_t    iov = mpool.acquire();

    char* p = coh::iobuf_get_w_buf(iov);
    int n = snprintf(p, coh::iobuf_get_w_available(iov),
                     "%s",
                     "POST / HTTP/1.1\r\n"
                     "Host: www.example.com\r\n"
                     "Content-Type: application/x-www-form-urlencoded\r\n"
                     "Content-Length: 4\r\n"
                     "Connection: close\r\n"
                     "\r\n"
                     "q=4");
    coh::iobuf_set_w_len(iov, n);

    coh::http_request req;
    codec.parse(iov, &req);

    CHECK(req.headers_completed());
    CHECK(req.major() == 1);
    CHECK(req.minor() == 1);
    CHECK(req.method() == HTTP_POST);
    CHECK(req.error() == HPE_OK);

    codec.parse(iov, &req);

    CHECK(!req.message_completed());
    CHECK(req.bodysz() == 3);
    CHECK(req.keep_alive() == false);
    CHECK(req.error() == HPE_OK);
}

TEST_CASE("fragments http request parser")
{
    coh::http_codec codec;
    coh::iobufs     mpool(1024, 4);
    coh::iobuf_t    iov = mpool.acquire();

    char* p = coh::iobuf_get_w_buf(iov);
    int n = snprintf(p, coh::iobuf_get_w_available(iov),
                     "%s",
                     "POST / HTTP/1.1\r\n"
                     "Host: www.example.com\r\n"
                     "Content-Type: application/x-www-form-urlencoded\r\n"
                     "Content-Length: 4\r\n"
                     "Connection: close\r\n"
                     "\r\n"
                     "q=42\r\n");
    coh::iobuf_set_w_len(iov, n);

    int size = 1;
    coh::http_request req;
    while(req.error() == HPE_OK && !req.message_completed()) {
        size++;
        if(size < n) {
            iov->wpos = size;
        }
        codec.parse(iov, &req);
    }

    CHECK(req.headers_completed());
    CHECK(req.major() == 1);
    CHECK(req.minor() == 1);
    CHECK(req.method() == HTTP_POST);
    CHECK(req.error() == HPE_OK);

    codec.parse(iov, &req);

    CHECK(req.message_completed());
    CHECK(req.bodysz() == 4);
    CHECK(req.keep_alive() == false);
    CHECK(req.error() == HPE_OK);
}

TEST_CASE("http pipeline request parser")
{
    coh::http_codec codec;
    coh::iobufs     mpool(1024, 4);
    coh::iobuf_t    iov = mpool.acquire();

    char* p = coh::iobuf_get_w_buf(iov);
    int n = snprintf(p, coh::iobuf_get_w_available(iov),
                     "%s",
                     "POST /a HTTP/1.1\r\n"
                     "Host: www.example.com\r\n"
                     "Content-Type: application/x-www-form-urlencoded\r\n"
                     "Content-Length: 4\r\n"
                     "Connection: keep-alive\r\n"
                     "\r\n"
                     "q=42\r\n"
                     "GET /b HTTP/1.1\r\n"
                     "Host: www.example.com\r\n"
                     "Content-Type: application/x-www-form-urlencoded\r\n"
                     "Content-Length: 8\r\n"
                     "Connection: close\r\n"
                     "\r\n"
                     "q=421234\r\n"
                     );
    coh::iobuf_set_w_len(iov, n);

    coh::http_request req;
    codec.parse(iov, &req);

    CHECK(req.headers_completed());
    CHECK(req.major() == 1);
    CHECK(req.minor() == 1);
    CHECK(req.method() == HTTP_POST);
    CHECK(req.error() == HPE_OK);

    codec.parse(iov, &req);

    CHECK(req.message_completed());
    CHECK(req.bodysz() == 4);
    CHECK(req.keep_alive() == true);
    CHECK(req.error() == HPE_OK);


    req.reset();
    codec.parse(iov, &req);

    CHECK(req.headers_completed());
    CHECK(req.major() == 1);
    CHECK(req.minor() == 1);
    CHECK(req.method() == HTTP_GET);
    CHECK(req.error() == HPE_OK);

    codec.parse(iov, &req);

    CHECK(req.message_completed());
    CHECK(req.bodysz() == 8);
    CHECK(req.keep_alive() == false);
    CHECK(req.error() == HPE_OK);
}


#endif

#endif
