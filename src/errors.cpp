/**
 * create: 2016-02-06
 * author: mr.wclong@yahoo.com
 */
#include "errors.hpp"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

namespace coh {

    static const char* get_basename(const char* file)
    {
        const char* basefile = strrchr(file, '/');
        if(basefile)
            return basefile + 1;

        return file;
    }

    static char* rhandle_error(char* buf, size_t size, int code)
    {
        snprintf(buf, size, " [%d]:%s", code, strerror(code));
        return buf;
    }

    static std::string vreport_stack(const char* file, int line, const char* func, int code, const char* format, va_list ap)
    {
        std::string msg;
        char*   buf = new char[2048];
        size_t  len = 2048;

        char prefix[2048];
        snprintf(prefix, 2048, "[%s:%d:%s] ", get_basename(file), line, func);
        msg += prefix;

        for(int tries = 10; tries; --tries) {
            va_list args;
    #if     defined(va_copy)
            va_copy(args, ap);
    #elif   defined(__va_copy)
            __va_copy(args, ap);
    #else
            memcpy(&args, &ap, sizeof(va_list));
    #endif
            int ncopy = vsnprintf(buf, len, format, args);
            va_end(args);

            if(ncopy > -1 && ncopy < len) {
                msg += buf;
                delete[] buf;
                break;
            }
            else {
                if(ncopy > 0) {
                    len = ncopy + 1;
                }
                else {
                    len *= 2;
                }
                delete[] buf;
                buf = new char[len];
            }
        }

        char suffix[2048];
        msg += rhandle_error(suffix, sizeof(suffix), code);

        return msg;
    }

    std::string report_stack(const char *file, int line, const char *func, int code, const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        std::string report = vreport_stack(file, line, func, code, format, args);
        va_end(args);

        return report;
    }

}
