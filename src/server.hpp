/**
 * create: 2016-09-27
 * author: mr.wclong@yahoo.com
 */
#ifndef COH_SERVER_HPP
#define COH_SERVER_HPP
#include "cohttp.hpp"
#include "iobufs.hpp"

namespace coh {

    class semops;
    class modules;
    class module_handler;
    class http_service {
    public:
        typedef std::shared_ptr<spdlog::logger >    spdlogs_t;
        struct options {
            size_t                  max_header_size = 8192;
            size_t                  max_body_size   = 10 * 1024 * 1024;
            size_t                  iobuf_size      = max_header_size;
            size_t                  iobuf_count     = 16384;
            int                     reuse_port      = 0;
            int                     serve_port      = 80;
            int                     process         = 2;
            steady_clock::duration  timer_idle      = seconds(5);
            steady_clock::duration  timer_life      = minutes(1);
            std::string             mod_path;
            modules*                handler;
            spdlogs_t               coh_log;

            options();
        };

        static http_service& instance();
        http_service();
        ~http_service();

        void setup(io_service* ios, tcp::acceptor* acc, semops* sem, options opt);
        void start();
        void stop ();
    private:
        void time_tick();

        io_service*                 ios_;
        tcp::acceptor*              acc_;
        semops*                     sem_;
        options                     opt_;
        bool                        run_;
        // 对每个 http 服务有自己的内存管理单元，这样可以不用考虑多线程或者多进程的问题
        std::shared_ptr<iobufs >    mem_;
        co_timer_id                 tik_;
    };

    /** 对应每个 http 连接的处理
     * 为了避免资源的占用，每个 http 连接会设置空闲超时时间和总的会话时间，前者避免空闲的连接
     * 占用过多的资源，后者避免慢的连接（比如一次发送一个字符，每次间隔一段时间）导致的资源占用
     *
     * 注意：空闲定时器只在传输过程中激活，和后续模块的处理时间无关，只确认请求或者响应能够在
     * 空闲时间内传输完毕
     * 生存定时器是从连接建立开始一直激活，限定了每个连接能够占用的最大的时间
     */
    class http_connection {
    public:
        typedef uint64_t    conid_t;
        typedef uint32_t    txnid_t;

        http_connection(std::shared_ptr<iobufs >        memory,
                        std::shared_ptr<tcp::socket>    trans,
                        http_service::options           options,
                        conid_t                         conid);
        ~http_connection();

        void parse();
    private:
        void on_idle_expired();
        void on_life_expired();

        void start_timer();
        void pause_timer();
        void clear_timer();
        bool process_request(std::list<iobuf_t>& buf, txnid_t txn);

        tcp::endpoint                   addrs_;
        std::shared_ptr<tcp::socket >   trans_;
        http_service::options           option_;
        conid_t                         connid_;
        co_timer_id                     timer_idle_;
        co_timer_id                     timer_life_;
        std::shared_ptr<iobufs >        memory_;
    };


}

#endif
