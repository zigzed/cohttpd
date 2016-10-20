/**
 * create: 2016-09-30
 * author: mr.wclong@yahoo.com
 */
#ifndef COH_RESPONSE_HPP
#define COH_RESPONSE_HPP
#include "iobufs.hpp"

namespace coh {

    class http_response {
    public:
        http_response(std::shared_ptr<iobufs > memory);

        void reset();

        // 响应包头的函数
        http_response&  status(int code, const char* message);
        http_response&  content_type(const char* type, const char* charset = nullptr);
        http_response&  keep_alive(bool alive);
        http_response&  header(const char* field, const char* value);
        http_response&  header(const char* field, const char* fmt, ...);

        // 响应内容部分的函数
        int             printf(const char* fmt, ...);
        http_response&  append(const char* buf, int len);

        // end of message
        http_response&  eom();
        bool            iseom() const {
            return iseom_ == 1;
        }

        bool    keep_alive() const {
            return alive_ == 1;
        }
        int     status() const {
            return status_;
        }

        std::vector<iobuf_t > buffers() const;
    private:
        std::shared_ptr<iobufs >    memory_;
        std::vector<iobuf_t >       header_;
        std::vector<iobuf_t >       body_;

        int                         status_;
        int                         alive_;
        int                         iseom_;
    };

}

#endif
