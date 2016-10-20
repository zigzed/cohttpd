/**
 * create: 2016-09-27
 * author: mr.wclong@yahoo.com
 */
#ifndef COH_REQUEST_HPP
#define COH_REQUEST_HPP
#include <map>
#include <vector>

#include "iobufs.hpp"
#include "http_parser.h"

namespace coh {

    class http_codec;

    struct http_request {
    public:
        struct parsed_url {
            unsigned short nport;
            slice   schema;
            slice   host;
            slice   port;
            slice   path;
            slice   query;
            slice   frag;
            slice   user;
        };

        typedef std::vector<slice >   chunk_t;

        http_request();

        const char*       surl() const {
            return strurl_.c_str();
        }
        const parsed_url& url() const {
            return decurl_;
        }
        // HTTP 主版本
        unsigned short  major()  const {
            return parsed_.http_major;
        }
        // HTTP 次版本
        unsigned short  minor()  const {
            return parsed_.http_minor;
        }
        // HTTP 请求方法
        http_method     method() const {
            return (http_method)parsed_.method;
        }
        // HTTP keep-alive or not
        bool            keep_alive()  const {
            return http_should_keep_alive(&parsed_) != 0;
        }
        // HTTP 解析错误信息
        enum http_errno error() const {
            return (enum http_errno)parsed_.http_errno;
        }

        // 按照HTTP头字段名称获取字段值
        const std::string*  operator[](const char* field) const;
        // 按照HTTP头的索引顺序获取字段值
        const std::pair<std::string, std::string >* operator[](size_t index) const;

        // HTTP头中的字段数量
        size_t          fields()    const {
            return fields_.size();
        }
        // 当前收到的 HTTP 数据包 Body 的大小
        int64_t         bodysz()    const {
            return bodysz_;
        }
        const chunk_t&  bodybf()    const;

        bool            headers_completed() const {
            return headers_completed_;
        }
        bool            message_completed() const {
            return message_completed_;
        }

        void            reset();
    private:
        http_codec* codec_;
    private:
        void sorthdr();

        friend class http_codec;
        typedef std::vector<std::pair<std::string, std::string > >  fields_t;

        http_parser             parsed_;
        std::string             strurl_;
        parsed_url              decurl_;
        fields_t                fields_;
        chunk_t                 bodybf_;
        int64_t                 bodysz_;

        bool                    headers_completed_;
        bool                    message_completed_;
        bool                    state_field_value_;
    };

}

#endif
