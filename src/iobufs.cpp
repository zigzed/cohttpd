/**
 * create: 2016-09-28
 * author: mr.wclong@yahoo.com
 */
#include "iobufs.hpp"
#include <functional>
#include <cassert>

namespace coh {

    iobufs::iobufs(int32_t chunk_size, int32_t chunk_count)
        : bitmap_(chunk_count, true)
        , size_(chunk_size), count_(chunk_count)
        , chunk_(nullptr)
    {
        chunk_ = (char* )malloc((chunk_size + sizeof(iobuf)) * chunk_count);
    }

    iobufs::~iobufs()
    {
        free(chunk_);
        chunk_ = nullptr;
    }

    iobuf_t iobufs::acquire()
    {
        iobuf_t r;
        int i = bitmap_.find_first();
        if(i != bitmap_.npos) {
            bitmap_.set(i, false);
            iobuf* p = (iobuf* )(chunk_ + (i * (size_ + sizeof(iobuf))));
            r = std::shared_ptr<iobuf >(p, std::bind(&iobufs::release, this, std::placeholders::_1));
        }
        else {
            iobuf* p = (iobuf* )malloc(size_ + sizeof(iobuf));
            r = std::shared_ptr<iobuf >(p, free);
        }
        iobuf_init(r, size_);
        return r;
    }

    void iobufs::release(iobuf* io)
    {
        assert(inrange(io));

        int i = ((char* )io - chunk_)/(size_ + sizeof(iobuf));
        bitmap_.set(i, true);
    }

}
