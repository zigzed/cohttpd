/**
 * create: 2016-09-30
 * author: mr.wclong@yahoo.com
 */
#include <stdarg.h>
#include <time.h>

#include "response.hpp"
#include "errors.hpp"

namespace coh {

    http_response::http_response(std::shared_ptr<iobufs> memory)
        : memory_(memory)
        , status_(-1)
        , alive_(1)
        , iseom_(0)
    {
        // 分别存储响应包头和响应内容
        header_.push_back(memory_->acquire());
        body_.push_back(memory_->acquire());
    }

    void http_response::reset()
    {
        status_ = -1;
        alive_  = 1;
        iseom_  = 0;
        header_.clear();
        body_.clear();
    }

    static void http_response_current_date(iobuf_t& iov)
    {
        static const char* WEEK_DAY[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        static const char* MONTH[] = {"Jan", "Feb", "Mar", "Apr",
                                      "May", "Jun", "Jul", "Aug",
                                      "Sep", "Oct", "Nov", "Dec"};
        struct tm tm;
        time_t t = time(NULL);
        gmtime_r(&t, &tm);

        char* p = iobuf_get_w_buf(iov);
        int   s = iobuf_get_w_available(iov);
        int   n = snprintf(p, s, "Date: %s, %d %s %d %02d:%02d:%02d GMT\r\n",
                           WEEK_DAY[tm.tm_wday],
                           tm.tm_mday,
                           MONTH[tm.tm_mon],
                           tm.tm_year + 1900,
                           tm.tm_hour, tm.tm_min, tm.tm_sec);
        iobuf_set_w_len(iov, n);
    }

    http_response& http_response::status(int code, const char* message)
    {
        status_ = code;

        iobuf_t& iov = header_.back();
        char* p = iobuf_get_w_buf(iov);
        int   s = iobuf_get_w_available(iov);
        int   n = snprintf(p, s, "HTTP/1.1 %d %s\r\n"
                           "Server: cohttp/0.9.0\r\n",
                           code, message);
        iobuf_set_w_len(iov, n);

        http_response_current_date(iov);

        return *this;
    }

    http_response& http_response::content_type(const char* type, const char* charset)
    {
        if(status_ == -1) {
            THROW_EXCEPT(std::runtime_error, "response should set status first");
        }

        iobuf_t& iov = header_.back();
        char* p = iobuf_get_w_buf(iov);
        int   s = iobuf_get_w_available(iov);
        int   n = snprintf(p, s, "Content-Type: %s",
                           type ? type : "text/html");
        if(charset) {
            int n2 = snprintf(p + n, s - n, "; charset=%s\r\n",
                              charset);
            n += n2;
        }
        else {
            int n2 = snprintf(p + n, s - n, "%s", "\r\n");
            n += n2;
        }

        iobuf_set_w_len(iov, n);
        return *this;
    }

    http_response& http_response::keep_alive(bool alive)
    {
        if(status_ == -1) {
            THROW_EXCEPT(std::runtime_error, "response should set status first");
        }
        alive_ = (alive ? 1 : 0);

        iobuf_t& iov = header_.back();
        char* p = iobuf_get_w_buf(iov);
        int   s = iobuf_get_w_available(iov);
        int   n = snprintf(p, s, "Connection: %s\r\n",
                           alive ? "keep-alive" : "close");

        iobuf_set_w_len(iov, n);
        return *this;
    }

    http_response& http_response::header(const char* field, const char* value)
    {
        if(status_ == -1) {
            THROW_EXCEPT(std::runtime_error, "response should set status first");
        }

        iobuf_t& iov = header_.back();
        char* p = iobuf_get_w_buf(iov);
        int   s = iobuf_get_w_available(iov);
        int   n = snprintf(p, s, "%s: %s\r\n",
                           field, value);
        if(n < 0 || n > s) {
            THROW_EXCEPT(std::runtime_error, "too larget http response header: "
                                             "available %d, expected %d",
                         s, n);
        }
        iobuf_set_w_len(iov, n);
        return *this;
    }

    http_response& http_response::header(const char* field, const char* fmt, ...)
    {
        if(status_ == -1) {
            THROW_EXCEPT(std::runtime_error, "response should set status first");
        }

        int   s = 0;
        char* p = NULL;
        iobuf_t& iov = header_.back();

        // 判断需要的内存长度
        va_list ap;
        va_start(ap, fmt);
        s = vsnprintf(p, s, fmt, ap);
        va_end(ap);
        s += strlen(field) + 4;

        if(s < 0 || s > iobuf_get_w_available(iov)) {
            THROW_EXCEPT(std::runtime_error, "too larget http response header: %s "
                                             "available %d, expected %d",
                         field, iobuf_get_w_available(iov), s);
        }

        p = iobuf_get_w_buf(iov);
        s = iobuf_get_w_available(iov);

        int n1 = snprintf(p, s, "%s: ", field);

        va_start(ap, fmt);
        int l = vsnprintf(p + n1, s - n1, fmt, ap);
        va_end(ap);

        int n2 = snprintf(p + n1 + l, s - n1 - l, "%s", "\r\n");

        iobuf_set_w_len(iov, l + n1 + n2);

        return *this;
    }

    int http_response::printf(const char* fmt, ...)
    {
        iobuf_t iov = body_.back();
        int retry = 0;
        int r = 0;
        do {
            char* p = iobuf_get_w_buf(iov);
            int   n = iobuf_get_w_available(iov);

            va_list ap;
            va_start(ap, fmt);
            r = vsnprintf(p, n, fmt, ap);
            va_end(ap);

            if(r > -1 && r <= n) {
                iobuf_set_w_len(iov, r);
                break;
            }
            else {
                iov = memory_->acquire();
                body_.push_back(iov);
            }

            retry++;
        } while(retry < 2);

        return r;
    }

    http_response& http_response::append(const char* buf, int len)
    {
        iobuf_t& iov= body_.back();
        int used    = 0;
        int avail   = iobuf_get_w_available(iov);
        while(used < len) {
            char* p     = iobuf_get_w_buf(iov);
            int   n     = (len - used > avail) ? avail : len - used;
            memcpy(p, buf + used, n);
            iobuf_set_w_len(iov, n);
            used += n;
            avail-= n;
            if(avail < len - used) {
                iov = memory_->acquire();
                body_.push_back(iov);
                avail = iobuf_get_w_available(iov);
            }
        }

        return *this;
    }

    http_response& http_response::eom()
    {
        iseom_ = 1;

        iobuf_t& iov = header_.back();
        // 需要在最前面加入响应包头的 \r\n
        char* p = iobuf_get_w_buf(iov);
        int n = snprintf(p, iobuf_get_w_available(iov), "%s", "\r\n");
        iobuf_set_w_len(iov, n);

        return *this;
    }

    std::vector<iobuf_t > http_response::buffers() const
    {
        std::vector<iobuf_t > r(header_);
        r.insert(r.end(), body_.begin(), body_.end());
        return r;
    }


}
