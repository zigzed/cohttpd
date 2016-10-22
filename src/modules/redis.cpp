#include "redis.hpp"
#include "boost/tokenizer.hpp"
#include "boost/lexical_cast.hpp"

redis_module::redis_module(http_service::options option, YAML::Node config)
    : module_handler(option, config)
    , option_(option)
    , running_(true)
    , redis_order_(nullptr)
{
    if(config["servers"]) {
        const YAML::Node& servers = config["servers"];
        boost::char_separator<char > sep(":");
        for(size_t i = 0; i < servers.size(); ++i) {
            std::string ep = servers[i].as<std::string >();
            boost::tokenizer<boost::char_separator<char > > tokens(ep, sep);
            std::vector<std::string > ve(tokens.begin(), tokens.end());
            if(ve.size() != 2) {
                option_.coh_log->error("redis module config servers.{}: {} not a valid endpoint",
                                       i, ep);
                continue;
            }
            try {
                endpoint e;
                e.host = ve[0];
                e.port = boost::lexical_cast<unsigned short >(ve[1]);
                config_.push_back(e);
            }
            catch(const boost::bad_lexical_cast& e) {
                option_.coh_log->error("redis module config servers.{} : {} not a valid port: {}",
                                       i, ep,
                                       e.what());
            }
        }
        option_.coh_log->info("redis module connecting to {} servers",
                              config_.size());
    }
    if(!config_.empty()) {
        redis_order_ = new co_chan<command >* [config_.size()];
        for(size_t i = 0; i < config_.size(); ++i) {
            redis_order_[i] = new co_chan<command >(1024);
        }
    }
}

int redis_module::handle_events()
{
    return module_handler::HE_REQUEST_HDR |
            module_handler::HE_PROCESS_INIT |
            module_handler::HE_PROCESS_EXIT |
            module_handler::HE_CONNECT_INIT |
            module_handler::HE_CONNECT_EXIT;
}

const char* redis_module::path() const
{
    return "/redis";
}

const char* redis_module::name() const
{
    return "redis";
}

void redis_module::on_process_init()
{
    for(size_t i = 0; i < config_.size(); ++i) {
        const endpoint& ep = config_[i];
        // 对于每个进程启动一个 redis 连接，这个连接接收命令，并将对应的响应数据送到相应的连接
        go [i, ep, this] {
            bool connected = false;
            std::shared_ptr<redisContext > context;
            while(running_) {
                if(context.get() == NULL || (context.get() && context->err)) {
                    context = std::shared_ptr<redisContext >(redisConnect(ep.host.c_str(), ep.port),
                                                             [] (redisContext* c) { redisFree(c); });
                }
                if(context.get() == NULL || (context.get() && context->err)) {
                    option_.coh_log->error("connecting redis {}:{} failed: {}",
                                           ep.host, ep.port,
                                           context.get() ? context->errstr : "allocate redis context");
                    // 如果连接失败则等待1秒后重试
                    co_sleep(1000);
                    continue;
                }
                if(!connected) {

                }
                command cmd;
                *redis_order_[i] >> cmd;
                conid_t id = cmd.cid;
                auto it = redis_reply_.find(id);
                if(it == redis_reply_.end())
                    continue;

                for(size_t i = 0; i < cmd.cmd.size(); ++i) {
                    redisAppendCommand(context.get(), cmd.cmd[i].c_str());
                }

                for(size_t i = 0; i < cmd.cmd.size(); ++i) {
                    void* rr = NULL;
                    redisGetReply(context.get(), &rr);

                    it->second << std::shared_ptr<redisReply >((redisReply* )rr,
                                                               [](redisReply* reply) { freeReplyObject(reply); });
                }
            }
        };
    };
}

void redis_module::on_process_exit()
{
}

void redis_module::on_connect_init(conid_t cid)
{
    redis_reply_[cid] = co_chan<std::shared_ptr<redisReply > >(256);
}

void redis_module::on_connect_exit(conid_t cid)
{
    redis_reply_.erase(cid);
}

void redis_module::on_request_header(conid_t              cid,
                                     txnid_t              tid,
                                     const http_request&  req,
                                     http_response*       rsp)
{
    rsp->status(200, "OK");
    rsp->keep_alive(req.keep_alive());
    rsp->content_type("text/html", NULL);

    char t1[128];
    snprintf(t1, 128, "SET key_%lu_%u %lu_%u", cid, tid, cid, tid);
    char t2[128];
    snprintf(t2, 128, "GET key_%lu_%u", cid, tid);
    command cmd;
    cmd.cid = cid;
    cmd.cmd.push_back(t1);
    cmd.cmd.push_back(t2);

    std::shared_ptr<redisReply > r1, r2;

    if(config_.size() > 0) {
        int id = cid % config_.size();
        if(redis_order_[id]->TimedPush(std::move(cmd),
                                    milliseconds(100))) {
            redis_reply_[cid].TimedPop(r1, milliseconds(100));
            redis_reply_[cid].TimedPop(r2, milliseconds(100));
        }
    }

    int n = 0;

    if(r1.get() == nullptr) {
        n = rsp->printf("<html>%s</html>\r\n", "null reply");
    }
    else {
        n = rsp->printf("<html>%d, %ld, %s</html>\r\n",
                        r1->type, r1->integer, r1->str);
    }

    if(r2.get() == NULL) {
        n += rsp->printf("<html>%s</html>\r\n", "null reply");
    }
    else {
        n += rsp->printf("<html>%d, %ld, %s</html>\r\n",
                         r2->type, r2->integer, r2->str);
        char t3[128];
        snprintf(t3, 128, "%lu_%u", cid, tid);
        if(strcmp(t3, r2->str) != 0) {
            n += rsp->printf("<html>not same</html>\r\n");
        }
        else {
            n += rsp->printf("<html>same</html>\r\n");
        }
    }

    rsp->header("Content-Length", "%d", n);
    rsp->eom();
}
