/**
 * create: 2016-02-06
 * author: mr.wclong@yahoo.com
 */
#ifndef COH_ERRORS_HPP
#define COH_ERRORS_HPP

#include <string>

namespace coh {

    #define PRINTF(FMT, X)      __attribute__ (( __format__ ( __printf__, FMT, X )))

    std::string report_stack(const char*    filename,
                             int            line,
                             const char*    function,
                             int            code,
                             const char*    format,
                             ...) PRINTF(5, 6);

}

#define THROW_EXCEPT(category, format, ...) \
    do { \
        int _lib_error_code = errno; \
        std::string _lib_error_message = coh::report_stack(__FILE__, \
                                                           __LINE__, \
                                                           __FUNCTION__, \
                                                           _lib_error_code, \
                                                           format, \
                                                           ##__VA_ARGS__); \
        throw category(_lib_error_message.c_str()); \
    } while(0)

#define BUG_ON(expr)    \
    do { \
        if((expr)) { \
            THROW_EXCEPT(std::runtime_error, "BUG ON(%s)", #expr); \
        } \
    } while(0)


#endif
