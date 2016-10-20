/**
 * create: 2016-09-27
 * author: mr.wclong@yahoo.com
 */
#include "cohttp.hpp"
#include "server.hpp"
#include "semops.hpp"
#include "codecs.hpp"
#include "response.hpp"
#include "handler.hpp"
#include "modules.hpp"

namespace coh {

//    http_service::options::options()
//    {
//    }

    http_service& http_service::instance()
    {
        static http_service gHttpService;
        return gHttpService;
    }

    http_service::http_service()
        : ios_(nullptr)
        , acc_(nullptr)
        , sem_(nullptr)
        , run_(true)
    {
    }

    void http_service::setup(io_service* ios, tcp::acceptor* acc, semops* sem, options opt)
    {
        ios_ = ios;
        acc_ = acc;
        sem_ = sem;
        opt_ = opt;
        mem_ = std::shared_ptr<iobufs >(new iobufs(opt.memory.iobuf_size, opt.memory.iobuf_count));

        opt_.coh_log = initlog(opt.logger.path.c_str(), opt.logger.cout);
    }

    http_service::~http_service()
    {
    }

    void http_service::start()
    {
        auto clog = opt_.coh_log;

        tik_ = co_timer_add(seconds(1), std::bind(&http_service::time_tick, this));

        auto mhs = opt_.handler->list_handler();
        for(auto mh : mhs) {
            if(mh->handle_events() & module_handler::HE_PROCESS_INIT) {
                mh->on_process_init();
            }
        }

        http_connection::conid_t connection_id = ((http_connection::conid_t)getpid()) << 32;
        uint32_t    conid = 0;
        while(run_) {
            std::shared_ptr<tcp::socket > s(new tcp::socket(*ios_));

            boost::system::error_code ec;
            if(opt_.reuse_port == 0)
                sem_->wait();
            acc_->accept(*s, ec);
            if(opt_.reuse_port == 0)
                sem_->wake();

            if(ec.value() != 0) {
                clog->warn("accepting new connection failed {}",
                           ec.message());
                continue;
            }

            conid++;

            std::shared_ptr<http_connection > connection(new http_connection(mem_,
                                                                             s,
                                                                             opt_,
                                                                             connection_id | conid));
            go std::bind(&http_connection::parse, connection);
        }

        for(auto mh : mhs) {
            if(mh->handle_events() & module_handler::HE_PROCESS_INIT) {
                mh->on_process_exit();
            }
        }
    }

    void http_service::stop()
    {
        run_ = false;
    }

    void http_service::time_tick()
    {
        opt_.coh_log->flush();
        tik_ = co_timer_add(seconds(1), std::bind(&http_service::time_tick, this));
    }

    http_connection::http_connection(std::shared_ptr<iobufs>        memory,
                                     std::shared_ptr<tcp::socket >  trans,
                                     http_service::options          option,
                                     conid_t                        connid)
        : trans_(trans)
        , option_(option)
        , connid_(connid)
        , memory_(memory)
    {
        auto mhs = option_.handler->list_handler();
        for(auto mh : mhs) {
            if(mh->handle_events() & module_handler::HE_CONNECT_INIT) {
                mh->on_connect_init(connid_);
            }
        }

        addrs_ = trans->remote_endpoint();

        if(option_.timer_life != seconds(-1)) {
            timer_life_ = co_timer_add(steady_clock::now() + option_.timer_life,
                                       std::bind(&http_connection::on_life_expired, this));
        }
    }

    http_connection::~http_connection()
    {
        clear_timer();

        trans_->close();

        auto mhs = option_.handler->list_handler();
        for(auto mh : mhs) {
            if(mh->handle_events() & module_handler::HE_CONNECT_EXIT) {
                mh->on_connect_exit(connid_);
            }
        }
    }

    bool http_connection::process_request(std::list<iobuf_t >& buf, txnid_t txn)
    {        
        auto clog = option_.coh_log;

        http_codec              decoder;
        http_request            request;
        http_response           response(memory_);
        int     header_size = 0;
        int     body_size   = 0;

        std::shared_ptr<module_handler > mh;
        iobuf_t iov = buf.back();

        start_timer();
        do {
            // 解析
            decoder.parse(iov, &request);
            if(request.error() != HPE_OK) {
                clog->error("{}:{}\t{}\t{}\tparsing header failed: {}, {}, {}",
                            addrs_.address().to_string(), addrs_.port(),
                            http_method_str(request.method()),
                            request.surl(),
                            request.error(),
                            http_errno_name(request.error()),
                            http_errno_description(request.error()));
                return false;
            }
            if(request.headers_completed())
                break;

            // 如果接收缓存可用的存储空间不足，就直接分配一个。
            if(iobuf_get_w_available(iov) <= 0) {
                iov = memory_->acquire();
                buf.push_back(iov);
            }

            boost::system::error_code ec;
            size_t n = trans_->read_some(buffer(iobuf_get_w_buf(iov),
                                                iobuf_get_w_available(iov)),
                                         ec);
            if(ec.value() != 0 && ec.value() != error::eof) {
                clog->error("{}:{}\treading some header failed: {}, {}",
                            addrs_.address().to_string(), addrs_.port(),
                            ec.value(), ec.message());
            }
            if(ec.value() != 0)
                return false;
            iobuf_set_w_len(iov, n);
            header_size += n;
            if(header_size > option_.memory.max_header_size) {
                clog->error("{}:{}\theader size {} run out of range {}",
                            header_size, option_.memory.max_header_size);
                return false;
            }
        } while(!request.headers_completed());
        pause_timer();

        // 请求包头接收完毕，处理包头
        mh = option_.handler->match_handler(request.url().path.data());

        if(mh.get() && (mh->handle_events() & module_handler::HE_REQUEST_HDR)) {
            mh->on_request_header(connid_, txn, request, &response);
        }

        start_timer();
        do {
            // 解析
            decoder.parse(iov, &request);
            if(request.error() != HPE_OK) {
                clog->error("{}:{}\t{}\t{}\tparsing message failed: {}, {}, {}",
                            addrs_.address().to_string(), addrs_.port(),
                            http_method_str(request.method()),
                            request.surl(),
                            request.error(),
                            http_errno_name(request.error()),
                            http_errno_description(request.error()));
                return false;
            }
            if(request.message_completed())
                break;

            // 如果接收缓存可用的存储空间不足，就直接分配一个。
            if(iobuf_get_w_available(iov) <= 0) {
                iov = memory_->acquire();
                buf.push_back(iov);
            }

            boost::system::error_code ec;
            size_t n = trans_->read_some(buffer(iobuf_get_w_buf(iov),
                                                iobuf_get_w_available(iov)),
                                         ec);
            if(ec.value() != 0 && ec.value() != error::eof) {
                clog->error("{}:{}\treading some message failed: {}, {}",
                            addrs_.address().to_string(), addrs_.port(),
                            ec.value(), ec.message());
            }
            if(ec.value() != 0)
                return false;
            iobuf_set_w_len(iov, n);
            body_size += n;
            if(body_size > option_.memory.max_body_size) {
                clog->error("{}:{}\body size {} run out of range {}",
                            body_size, option_.memory.max_body_size);
                return false;
            }
        } while(!request.message_completed());
        pause_timer();

        // 请求内容接收完毕，处理内容
        if(mh.get() && (mh->handle_events() & module_handler::HE_REQUEST_BODY)) {
            mh->on_request_body(connid_, txn, request, &response);
        }

        // 如果模块的请求响应有响应信息，那么需要发送
        start_timer();

        std::vector<const_buffer >  obs;
        std::vector<iobuf_t >       ob = response.buffers();
        for(size_t i = 0; i < ob.size(); ++i) {
            iobuf_t& b = ob[i];
            if(iobuf_get_r_len(b) > 0) {
                obs.push_back(buffer(iobuf_get_r_buf(b),
                                     iobuf_get_r_len(b)));
            }
        }

        boost::system::error_code ec;
        size_t bytes = boost::asio::write(*trans_, obs, ec);

        // 发送完毕后再次停止空闲超时定时器
        pause_timer();

        if(ec.value() == 0) {
            clog->info("{}:{}\t{}\t{}\t{}\t{}\t{}",
                       addrs_.address().to_string(), addrs_.port(),
                       http_method_str(request.method()),
                       request.surl(),
                       response.status(),
                       bytes,
                       mh ? mh->name() : "");
        }
        else {
            clog->error("{}:{}\t{}\t{}\t{}\t{}\t{}\t{}",
                        addrs_.address().to_string(), addrs_.port(),
                        http_method_str(request.method()),
                        request.surl(),
                        response.status(),
                        bytes,
                        mh ? mh->name() : "",
                        ec.message());
        }

        // 如果不需要 keep-alive 那么关闭连接
        return ec.value() == 0 && response.keep_alive();
    }

    void http_connection::parse()
    {
        auto clog = option_.coh_log;

        std::list<iobuf_t > reqbuf;

        try {
            bool    keep_alive = true;
            txnid_t txn = 0;
            while(trans_ && keep_alive) {
                if(reqbuf.empty()) {
                    reqbuf.push_back(memory_->acquire());
                }
                keep_alive = process_request(reqbuf, txn);
                txn++;

                // 将已经读取完成的内存释放，避免一个长交互的连接占用太多内存
                iobuf_t iov;
                do {
                    iov = reqbuf.front();
                    if(iobuf_get_r_len(iov) == 0)
                        reqbuf.pop_front();
                    else
                        break;
                } while(!reqbuf.empty());
            }
        }
        catch(const boost::system::system_error& e) {
            clog->error("{}> parse error from {}: {}, {}",
                        getpid(),
                        addrs_.address().to_string(),
                        e.code().value(), e.what());
        }
    }

    void http_connection::on_idle_expired()
    {
        if(trans_) {

            auto clog = option_.coh_log;
            clog->error("{}> remove idle connection {}",
                        getpid(),
                        addrs_.address().to_string());

            trans_->shutdown(tcp::socket::shutdown_both);
        }
        timer_idle_.reset();
    }

    void http_connection::on_life_expired()
    {
        if(trans_) {

            auto clog = option_.coh_log;
            clog->error("{}> remove long connection {}",
                        getpid(),
                        addrs_.address().to_string());

            trans_->shutdown(tcp::socket::shutdown_both);
        }
        timer_life_.reset();
    }

    void http_connection::start_timer()
    {
        pause_timer();
        timer_idle_ = co_timer_add(steady_clock::now() + option_.timer_idle,
                                   std::bind(&http_connection::on_idle_expired, this));
    }

    void http_connection::pause_timer()
    {
        if(timer_idle_) {
            co_timer_cancel(timer_idle_);
        }
    }

    void http_connection::clear_timer()
    {
        if(timer_idle_) {
            co_timer_cancel(timer_idle_);
            timer_idle_.reset();
        }
        if(timer_life_) {
            co_timer_cancel(timer_life_);
            timer_life_.reset();
        }
    }


}
