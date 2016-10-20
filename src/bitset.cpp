/**
 * create: 2014-12-30
 * author: longwc@uway.cn
 */
#include "bitset.hpp"
#include <cassert>

namespace coh {

    namespace {
        unsigned char count_table[] = {
            0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4, 2,
            3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3,
            3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3,
            4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4,
            3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5,
            6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4,
            4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5,
            6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 2, 3, 3, 4, 3, 4, 4, 5,
            3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 3,
            4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6,
            6, 7, 6, 7, 7, 8
        };
    }

    bitset::reference::reference(block_type& block, block_type i)
        : block_(block), masks_(block_type(1) << i)
    {
        assert(i < bits_per_block);
    }

    bitset::reference& bitset::reference::flip()
    {
        block_ ^= masks_;
        return *this;
    }

    bitset::reference::operator bool() const
    {
        return (block_ & masks_) != 0;
    }

    bool bitset::reference::operator~() const
    {
        return (block_ & masks_) == 0;
    }

    bitset::reference& bitset::reference::operator=(bool x)
    {
        x ? block_ |= masks_ : block_ &= ~masks_;
        return *this;
    }

    bitset::reference& bitset::reference::operator=(const bitset::reference& rhs)
    {
        rhs ? block_ |= masks_ : block_ &= ~masks_;
        return *this;
    }

    bitset::reference& bitset::reference::operator|=(bool x)
    {
        if(x) {
            block_ |= masks_;
        }
        return *this;
    }

    bitset::reference& bitset::reference::operator&=(bool x)
    {
        if(!x) {
            block_ &= ~masks_;
        }
        return *this;
    }

    bitset::reference& bitset::reference::operator^=(bool x)
    {
        if(x) {
            block_ ^= masks_;
        }
        return *this;
    }

    bitset::reference& bitset::reference::operator-=(bool x)
    {
        if(x) {
            block_ &= ~masks_;
        }
        return *this;
    }

    ////////////////////////////////////////////////////////////////////////////
    bitset::bitset()
        : nums_(0)
    {
    }

    bitset::bitset(size_t size, bool value)
        : bits_(bits_to_block(size),  value ? ~block_type(0) : 0)
        , nums_(size)
    {
    }

    bitset::bitset(const bitset& rhs)
        : bits_(rhs.bits_)
        , nums_(rhs.nums_)
    {
    }

    bitset& bitset::operator=(const bitset& rhs)
    {
        if(this == &rhs)
            return *this;
        bits_ = rhs.bits_;
        nums_ = rhs.nums_;
        return *this;
    }

    bitset bitset::operator~() const
    {
        bitset b(*this);
        b.flip();
        return b;
    }

    void swap(bitset& x, bitset& y)
    {
        std::swap(x.bits_, y.bits_);
        std::swap(x.nums_, y.nums_);
    }

    bitset bitset::operator<<(size_t n) const
    {
        bitset b(*this);
        return b <<= n;
    }

    bitset bitset::operator>>(size_t n) const
    {
        bitset b(*this);
        return b >>= n;
    }

    bitset& bitset::operator<<=(size_t n)
    {
        if(n >= nums_)
            return reset();

        if(n > 0) {
            size_t last = block() - 1;
            size_t div  = n / bits_per_block;
            size_t r    = bit_index(n);
            block_type* b= &bits_[0];

            assert(block() >= 1);
            assert(div <= last);

            if(r != 0) {
                for(size_t i = last - div; i > 0; --i)
                    b[i + div] = (b[i] << r) | (b[i - 1] >> (bits_per_block - r));
                b[div] = b[0] << r;
            }
            else {
                for(size_t i = last - div; i > 0; --i)
                    b[i + div] = b[i];
                b[div] = b[0];
            }

            std::fill_n(b, div, block_type(0));
            zero_unused_bits();
        }
        return *this;
    }

    bitset& bitset::operator>>=(size_t n)
    {
        if(n >= nums_)
            return reset();

        if(n > 0) {
            size_t last = block() - 1;
            size_t div  = n / bits_per_block;
            size_t r    = bit_index(n);
            block_type* b = &bits_[0];

            assert(block() >= 1);
            assert(div <= last);

            if(r != 0) {
                for(size_t i = last - div; i > 0; --i)
                    b[i - div] = (b[i] >> r) | (b[i + 1] << (bits_per_block - r));
                b[last - div] = b[last] >> r;
            }
            else {
                for(size_t i = div; i <= last; ++i)
                    b[i - div] = b[i];
            }

            std::fill_n(b + (block() - div), div, block_type(0));
        }
        return *this;
    }

    bitset& bitset::operator&=(const bitset& rhs)
    {
        size_t b = (block() < rhs.block() ? block() : rhs.block());
        for(size_t i = 0; i < b; ++i)
            bits_[i] &= rhs.bits_[i];
        return *this;
    }

    bitset& bitset::operator|=(const bitset& rhs)
    {
        size_t b = (block() < rhs.block() ? block() : rhs.block());
        for(size_t i = 0; i < b; ++i)
            bits_[i] |= rhs.bits_[i];
        return *this;
    }

    bitset& bitset::operator^=(const bitset& rhs)
    {
        size_t b = (block() < rhs.block() ? block() : rhs.block());
        for(size_t i = 0; i < b; ++i)
            bits_[i] ^= rhs.bits_[i];
        return *this;
    }

    bitset& bitset::operator-=(const bitset& rhs)
    {
        size_t b = (block() < rhs.block() ? block() : rhs.block());
        for(size_t i = 0; i < b; ++i)
            bits_[i] &= ~rhs.bits_[i];
        return *this;
    }

    bitset operator& (const bitset& x, const bitset& y)
    {
        bitset b(x);
        return b &= y;
    }

    bitset operator| (const bitset& x, const bitset& y)
    {
        bitset b(x);
        return b |= y;
    }

    bitset operator^ (const bitset& x, const bitset& y)
    {
        bitset b(x);
        return b ^= y;
    }

    bitset operator- (const bitset& x, const bitset& y)
    {
        bitset b(x);
        return b -= y;
    }

    bool operator==(const bitset& x, const bitset& y)
    {
        return x.nums_ == y.nums_ && x.bits_ == y.bits_;
    }

    bool operator!=(const bitset& x, const bitset& y)
    {
        return !(x == y);
    }

    bool operator< (const bitset& x, const bitset& y)
    {
        size_t b = (x.block() < y.block() ? x.block() : y.block());
        for(size_t r = 0; r < b; ++r) {
            if(x.bits_[r] < y.bits_[r])
                return true;
            if(x.bits_[r] > y.bits_[r])
                return false;
        }
        if(y.size() > x.size())
            return true;

        return false;
    }

    void bitset::resize(size_t n, bool value)
    {
        size_t old = block();
        size_t required = bits_to_block(n);
        block_type block_value = value ? ~block_type(0) : block_type(0);

        if(required != old)
            bits_.resize(required, block_value);

        if(value && (n > nums_) && extra_bits())
            bits_[old - 1] |= (block_value << extra_bits());

        nums_ = n;
        zero_unused_bits();
    }

    void bitset::clear()
    {
        bits_.clear();
        nums_ = 0;
    }

    void bitset::push_back(bool bit)
    {
        size_t s = size();
        resize(s + 1);
        set(s, bit);
    }

    void bitset::append(block_type b)
    {
        size_t excess = extra_bits();
        if(excess) {
            assert(!bits_.empty());
            bits_.push_back(b >> (bits_per_block - excess));
            bits_[bits_.size() - 2] |= (b << excess);
        }
        else {
            bits_.push_back(b);
        }
        nums_ += bits_per_block;
    }

    bitset& bitset::set(size_t i, bool bit)
    {
        assert(i < nums_);
        if(bit)
            bits_[block_index(i)] |= bit_mask(i);
        else
            reset(i);

        return *this;
    }

    bitset& bitset::set()
    {
        std::fill(bits_.begin(), bits_.end(), ~block_type(0));
        zero_unused_bits();
        return *this;
    }

    bitset& bitset::reset(size_t i)
    {
        assert(i < nums_);
        bits_[block_index(i)] &= ~bit_mask(i);
        return *this;
    }

    bitset& bitset::reset()
    {
        std::fill(bits_.begin(), bits_.end(), block_type(0));
        return *this;
    }

    bitset& bitset::flip(size_t i)
    {
        assert(i < nums_);
        bits_[block_index(i)] ^= bit_mask(i);
        return *this;
    }

    bitset& bitset::flip()
    {
        for(size_t i = 0; i < block(); ++i)
            bits_[i] = ~bits_[i];
        zero_unused_bits();
        return *this;
    }

    bool bitset::operator[](size_t i) const
    {
        assert(i < nums_);
        return (bits_[block_index(i)] & bit_mask(i)) != 0;
    }

    bitset::reference bitset::operator[](size_t i)
    {
        assert(i < nums_);
        return reference(bits_[block_index(i)], bit_index(i));
    }

    size_t bitset::count() const
    {
        std::vector<block_type >::const_iterator first = bits_.begin();
        size_t n = 0;
        size_t length = block();
        while(length) {
            block_type b = *first;
            while(b) {
                n += count_table[b & ((1u << 8) - 1)];
                b >>= 8;
            }
            ++first;
            --length;
        }
        return n;
    }

    size_t bitset::block() const
    {
        return bits_.size();
    }

    size_t bitset::size() const
    {
        return nums_;
    }

    bool bitset::empty() const
    {
        return bits_.empty();
    }

    size_t bitset::find_first() const
    {
        return find_from(0);
    }

    size_t bitset::find_next(size_t i) const
    {
        if(i >= (size() - 1) || size() == 0)
            return npos;
        ++i;
        size_t bi = block_index(i);
        block_type b = bits_[bi] & (~block_type(0) << bit_index(i));
        return b ? bi * bits_per_block + lowest_bit(b) : find_from(bi + 1);
    }

    size_t bitset::lowest_bit(block_type b)
    {
        block_type x = b - (b & (b - 1));
        size_t log = 0;
        while(x >>= 1)
            ++log;
        return log;
    }

    bitset::block_type bitset::extra_bits() const
    {
        return bit_index(size());
    }

    void bitset::zero_unused_bits()
    {
        if(extra_bits())
            bits_.back() &= ~(~block_type(0) << extra_bits());
    }

    size_t bitset::find_from(size_t i) const
    {
        while(i < block() && bits_[i] == 0)
            ++i;
        if(i >= block())
            return npos;
        return i * bits_per_block + lowest_bit(bits_[i]);
    }

    std::string bitset::to_string(bool msb_to_lsb,
                                  bool all,
                                  size_t cutoff) const
    {
        std::string str;
        size_t str_size = all ? bits_per_block * block() : size();
        if(cutoff == 0 || str_size <= cutoff)
            str.assign(str_size, '0');
        else {
            str.assign(cutoff + 2, '0');
            str[cutoff + 0] = '.';
            str[cutoff + 1] = '.';
            str_size = cutoff;
        }

        for(size_t i = 0; i < std::min(str_size, size()); ++i) {
            if((*this)[i])
                str[msb_to_lsb ? str_size - i - 1 : i] = '1';
        }
        return str;
    }

}
