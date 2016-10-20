/**
 * create: 2016-09-27
 * author: mr.wclong@yahoo.com
 */
#ifndef COH_IOBUFS_HPP
#define COH_IOBUFS_HPP
#include <stdint.h>
#include <string.h>

#include <memory>
#include <vector>

#include "bitset.hpp"

namespace coh {

    struct iobuf;
    typedef std::shared_ptr<iobuf > iobuf_t;

    struct iobuf {
        int     size;
        int     wpos;
        int     rpos;
        char    buff[0];
    };

    class slice {
    public:
        slice() : buf_(nullptr), len_(0) {}
        slice(const char* buf, int len)
            : buf_(buf), len_(len) {}
        void reset() {
            buf_ = nullptr;
            len_ = 0;
        }
        const char* data() {
            return buf_ ? buf_ : nullptr;
        }
        const char* data() const {
            return buf_ ? buf_ : nullptr;
        }
        size_t      size() const {
            return len_;
        }
        size_t      hash() const {
            static const size_t seed = 14695981039346656037ULL;
            static const size_t prime= 1099511628211ULL;
            size_t r = seed;
            unsigned char* pv = (unsigned char* )buf_;
            while(pv < (unsigned char* )buf_ + len_) {
                r ^= *pv++;
                r *= prime;
            }
            return r;
        }
        bool operator< (const slice& rhs) const {
            return compare(*this, rhs) < 0;
        }
        bool operator==(const slice& rhs) const {
            return compare(*this, rhs) == 0;
        }
        bool operator!=(const slice& rhs) const {
            return compare(*this, rhs) != 0;
        }
    private:
        static inline int compare(const slice& a, const slice& b) {
            int al = a.size();
            int bl = b.size();

            return al == bl ? memcmp(a.data(), b.data(), al)
                            : al - bl;
        }

        const char* buf_;
        int         len_;
    };

    inline void iobuf_init(iobuf* io, int size)
    {
        io->rpos = io->wpos = 0;
        io->size = size;
    }

    inline void iobuf_init(iobuf_t& io, int size)
    {
        io->rpos = io->wpos = 0;
        io->size = size;
    }

    inline char* iobuf_get_w_buf(iobuf_t& io)
    {
        return io->buff + io->wpos;
    }

    inline int iobuf_get_w_len(const iobuf_t& io)
    {
        return io->wpos;
    }

    inline int iobuf_get_w_available(const iobuf_t& io)
    {
        return io->size - io->wpos;
    }

    inline char* iobuf_get_r_buf(iobuf_t& io)
    {
        return io->buff + io->rpos;
    }

    inline int iobuf_get_r_len(const iobuf_t& io)
    {
        return io->wpos - io->rpos;
    }

    inline bool iobuf_set_w_len(iobuf_t& io, int size)
    {
        if(io->wpos + size > io->size)
            return false;

        io->wpos += size;
        return true;
    }

    inline bool iobuf_set_r_len(iobuf_t& io, int size)
    {
        if(io->rpos + size > io->wpos)
            return false;

        io->rpos += size;
        return true;
    }


    class iobufs {
    public:
        iobufs(int32_t chunk_size, int32_t chunk_count);
        ~iobufs();

        iobuf_t acquire();
        int     capacity() const;
    private:
        bool    inrange(iobuf* p);
        void    release(iobuf* p);

        bitset  bitmap_;
        int32_t size_;
        int32_t count_;
        char*   chunk_;
    };

    inline bool iobufs::inrange(iobuf* p)
    {
        return (char* )p >= chunk_ &&
                (char* )p < (chunk_ + (size_ + sizeof(iobuf)) * count_);
    }

}

#endif
