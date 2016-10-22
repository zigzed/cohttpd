/**
 * create: 2016-10-18
 * author: mr.wclong@yahoo.com
 */
#include "modules.hpp"
#include <dlfcn.h>
#include "boost/filesystem.hpp"

namespace coh {

    void modules::setup(http_service::options option, const char* path, const char* config)
    {
        module_handler* (*mod_handler)(http_service::options, YAML::Node );

        boost::filesystem::path p(path);
        try {
            YAML::Node modcfg = YAML::LoadFile(config);
            modcfg = modcfg["Modules"];

            if(!boost::filesystem::exists(p)) {
                return;
            }

            for(boost::filesystem::directory_entry& x : boost::filesystem::directory_iterator(p)) {
                if(!boost::filesystem::is_regular_file(x)) {
                    continue;
                }
                dlerror();
                void* lib = dlopen(x.path().native().c_str(), RTLD_NOW | RTLD_LOCAL);
                if(lib == NULL) {
                    option.coh_log->error("loading module {} failed: {}",
                                          x.path().native().c_str(),
                                          dlerror());
                    continue;
                }
                // 清除错误信息
                dlerror();
                mod_handler = (module_handler* (*)(http_service::options, YAML::Node ))dlsym(lib, "create_module");
                const char* rc = dlerror();
                if(rc != NULL) {
                    option.coh_log->warn("loading module {} failed: {}",
                                         x.path().native().c_str(),
                                         rc);
                    dlclose(lib);
                    continue;
                }

                module_handler* mh = nullptr;
                if(modcfg[boost::filesystem::basename(x.path()).c_str()]) {
                    mh = mod_handler(option, modcfg[boost::filesystem::basename(x.path()).c_str()]);
                }
                else {
                    mh = mod_handler(option, YAML::Node());
                }
                if(mh) {
                    setup_handler(mh->path(), mh);
                    option.coh_log->info("loading module {} done",
                                         x.path().native().c_str());
                }
            }
        }
        catch(const boost::filesystem::filesystem_error& e) {
            option.coh_log->error("setup modules on path {} failed: {}",
                                  path, e.what());
        }
        catch(const std::runtime_error& e) {
            option.coh_log->error("setup modules on path {} failed: {}",
                                  path, e.what());
        }
    }

    bool modules::setup_handler(const char* path, module_handler* mh)
    {
        if(path == NULL) {
            default_ = std::shared_ptr<module_handler >(mh);
            return true;
        }

        auto it = modules_.find(path);
        if(it != modules_.end()) {
            return false;
        }
        modules_.insert(std::make_pair(strdup(path), std::shared_ptr<module_handler >(mh)));
        return true;
    }

    std::shared_ptr<module_handler > modules::match_handler(const char* path) const
    {
        if(path == NULL)
            return default_;

        auto it = modules_.find(path);
        if(it != modules_.end())
            return it->second;

        return default_;
    }

    std::vector<std::shared_ptr<module_handler > > modules::list_handler() const
    {
        std::vector<std::shared_ptr<module_handler > > r;
        for(auto it : modules_) {
            r.push_back(it.second);
        }
        return r;
    }

}
