/**
 * create: 2016-09-29
 * author: mr.wclong@yahoo.com
 */
#include <functional>

#include "codecs.hpp"
#include "request.hpp"
#include "server.hpp"
#include "http_parser.h"

namespace coh {

    // FIXME: 当一个 iobuf 中包含一个半数据请求包时，会导致解析出来的数据并不连续的情况

    http_codec::http_codec()
    {
        std::vector<int > a;

        memset(&config_, 0, sizeof(config_));
        config_.on_message_begin    = http_codec::on_message_begin;
        config_.on_url              = http_codec::on_url;
        config_.on_header_field     = http_codec::on_header_field;
        config_.on_header_value     = http_codec::on_header_value;
        config_.on_headers_complete = http_codec::on_headers_complete;
        config_.on_body             = http_codec::on_body;
        config_.on_message_complete = http_codec::on_message_complete;
    }

    // 注意，需要将 parse 分为 parse_header 和 parse_body，如果分开的话可以提前
    // 调用外部模块。这样如果一个请求的 body 非常大，而模块并不关心 body 的话就可以提前
    // 开始处理了。
    http_request* http_codec::parse(iobuf_t iov, http_request* req)
    {
        req->codec_ = this;

        while(iobuf_get_r_len(iov) > 0 && !req->message_completed() && req->error() == HPE_OK) {
            size_t nparsed = http_parser_execute(&req->parsed_,
                                                 &config_,
                                                 iobuf_get_r_buf(iov),
                                                 iobuf_get_r_len(iov));
            iobuf_set_r_len(iov, nparsed);
        }
        if(req->error() == HPE_CB_message_complete || req->error() == HPE_CB_headers_complete) {
            req->parsed_.http_errno = HPE_OK;
        }
        return req;
    }

    int http_codec::of_message_begin(http_parser* hp)
    {
        return 0;
    }

    int http_codec::of_headers_complete(http_parser* hp)
    {
        http_request* r = (http_request* )hp->data;
        r->headers_completed_ = true;
        r->sorthdr();

        std::string&    url = r->strurl_;
        http_parser_url hpu;
        http_parser_url_init(&hpu);
        http_parser_parse_url(url.data(),
                              url.size(),
                              r->method() == HTTP_CONNECT,
                              &hpu);
        if(hpu.field_set & (1 << UF_SCHEMA)) {
            r->decurl_.schema   = slice(url.data() + hpu.field_data[UF_SCHEMA].off,
                                        hpu.field_data[UF_SCHEMA].len);
//            url[hpu.field_data[UF_SCHEMA].off + hpu.field_data[UF_SCHEMA].len] = 0;
        }
        if(hpu.field_set & (1 << UF_HOST)) {
            r->decurl_.host     = slice(url.data() + hpu.field_data[UF_HOST].off,
                                        hpu.field_data[UF_HOST].len);
//            url[hpu.field_data[UF_HOST].off + hpu.field_data[UF_HOST].len] = 0;
        }
        if(hpu.field_set & (1 << UF_PORT)) {
            r->decurl_.port     = slice(url.data() + hpu.field_data[UF_PORT].off,
                                        hpu.field_data[UF_PORT].len);
//            url[hpu.field_data[UF_PORT].off + hpu.field_data[UF_PORT].len] = 0;
        }
        if(hpu.field_set & (1 << UF_PATH)) {
            r->decurl_.path     = slice(url.data() + hpu.field_data[UF_PATH].off,
                                        hpu.field_data[UF_PATH].len);
//            url[hpu.field_data[UF_PATH].off + hpu.field_data[UF_PATH].len] = 0;
        }
        if(hpu.field_set & (1 << UF_QUERY)) {
            r->decurl_.query    = slice(url.data() + hpu.field_data[UF_QUERY].off,
                                        hpu.field_data[UF_QUERY].len);
//            url[hpu.field_data[UF_QUERY].off + hpu.field_data[UF_QUERY].len] = 0;
        }
        if(hpu.field_set & (1 << UF_FRAGMENT)) {
            r->decurl_.frag     = slice(url.data() + hpu.field_data[UF_FRAGMENT].off,
                                        hpu.field_data[UF_FRAGMENT].len);
//            url[hpu.field_data[UF_FRAGMENT].off + hpu.field_data[UF_FRAGMENT].len] = 0;
        }
        if(hpu.field_set & (1 << UF_USERINFO)) {
            r->decurl_.user = slice(url.data() + hpu.field_data[UF_USERINFO].off,
                                    hpu.field_data[UF_USERINFO].len);
//            url[hpu.field_data[UF_USERINFO].off + hpu.field_data[UF_USERINFO].len] = 0;
        }
        r->decurl_.nport = hpu.port;

        // 返回非0，强制要求 http-parser 解析完头部信息后返回给外部进行处理
        return 100;
    }

    int http_codec::of_message_complete(http_parser* hp)
    {
        http_request* r = (http_request* )hp->data;
        r->message_completed_ = true;

        // 返回非0，强制要求 http-parser 一次只处理一个请求
        return 100;
    }

    int http_codec::of_url(http_parser* hp, const char* at, size_t length)
    {
        // 注意：因为本函数会多次被调用（例如第一次接收到的数据包不完整，后续会将剩余的
        // 部分再次通过本函数调用来通知，所以不能将最后的内容置为0的方式，而是需要保存
        // 开始位置和长度信息
        http_request* r = (http_request* )hp->data;
        std::string& u  = r->strurl_;
        u.insert(u.end(), at, at + length);

        return 0;
    }

    int http_codec::of_header_field(http_parser* hp, const char* at, size_t length)
    {
        // 注意：因为本函数会多次被调用（例如第一次接收到的数据包不完整，后续会将剩余的
        // 部分再次通过本函数调用来通知，所以不能将最后的内容置为0的方式，而是需要保存
        // 开始位置和长度信息
        http_request* r = (http_request* )hp->data;
        // 之前一直在处理 field 的值，所以这是增加一个新的包头域
        if(r->state_field_value_) {
            r->state_field_value_ = false;
            r->fields_.push_back(std::make_pair(std::string(at, at + length), std::string()));
        }
        // 这是对之前的包头域的key部分的补充
        else {
            if(r->fields_.empty()) {
                return 1;
            }
            std::string& f = r->fields_.back().first;
            f.insert(f.end(), at, at + length);
        }
        return 0;
    }

    int http_codec::of_header_value(http_parser* hp, const char* at, size_t length)
    {
        // 因为本函数会多次被调用（例如第一次接收到的数据包不完整，后续会将剩余的
        // 部分再次通过本函数调用来通知，所以不能将最后的内容置为0的方式，而是需要保存
        // 开始位置和长度信息
        http_request* r = (http_request* )hp->data;
        // 之前一直在处理 field 的值，所以这是增加一个新的包头域
        r->state_field_value_ = true;
        if(r->fields_.empty()) {
            return 1;
        }
        std::string& f = r->fields_.back().second;
        f.insert(f.end(), at, at + length);
        return 0;
    }

    int http_codec::of_body(http_parser* hp, const char* at, size_t length)
    {
        http_request* r = (http_request* )hp->data;
        r->bodysz_ += length;

        if(r->bodybf_.empty()) {
            r->bodybf_.push_back(slice(at, length));
            return 0;
        }

        slice& c = r->bodybf_.back();
        if(c.data() + c.size() == at) {
            c = slice(c.data(), c.size() + length);
        }
        else {
            r->bodybf_.push_back(slice(at, length));
        }

        return 0;
    }

}
