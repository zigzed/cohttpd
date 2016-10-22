/**
 * create: 2016-10-18
 * author: mr.wclong@yahoo.com
 */
#ifndef COH_MODULES_HPP
#define COH_MODULES_HPP
#include "handler.hpp"
#include "server.hpp"
#include <string.h>
#include <unordered_map>
#include <functional>
#include <vector>

namespace coh {

    class modules {
    public:
        void                                setup(http_service::options option, const char* path, const char* config);
        bool                                setup_handler(const char* path, module_handler* mh);
        std::shared_ptr<module_handler >    match_handler(const char* path) const;
        std::vector<std::shared_ptr<module_handler > >  list_handler() const;
    private:
        struct hash {
            inline size_t operator()(const char* k) const {
                size_t r = static_cast<size_t >(14695981039346656037ULL);
                while(*k) {
                    r ^= static_cast<size_t >(toupper(*k));
                    r *= static_cast<size_t >(1099511628211ULL);
                    k++;
                }
                return r;
            }
        };
        struct comp {
            inline bool operator()(const char* a, const char* b) const {
                return strcasecmp(a, b) == 0;
            }
        };

        typedef std::shared_ptr<module_handler >                        module_t;
        typedef std::unordered_map<const char*, module_t, hash, comp >  modules_t;

        module_t    default_;
        modules_t   modules_;
    };

}

#endif
