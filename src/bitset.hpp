/**
 * create: 2014-12-30
 * author: longwc@uway.cn
 */
#ifndef COH_BITSET_HPP
#define COH_BITSET_HPP
#include <limits>
#include <vector>
#include <string>

namespace coh {

    class bitset {
    public:
        typedef size_t  block_type;

        static const size_t npos = static_cast<size_t >(-1);
        static const block_type bits_per_block = std::numeric_limits<block_type >::digits;

        class reference {
            friend class bitset;
            reference(block_type& block, block_type i);
        public:
            reference& flip();
            operator bool() const;
            bool operator~() const;
            reference& operator= (bool x);
            reference& operator= (const reference& rhs);
            reference& operator|=(bool x);
            reference& operator&=(bool x);
            reference& operator^=(bool x);
            reference& operator-=(bool x);
        private:
            block_type& block_;
            block_type  masks_;
        };

        typedef bool const_reference;

        // construct an empty bitset
        bitset();
        // construct a bitset with given size
        bitset(size_t size, bool value = false);
        bitset(const bitset& rhs);
        bitset& operator= (const bitset& rhs);

        // construct a bitset from a sequence of blocks
        template<typename InputIterator>
        bitset(InputIterator first, InputIterator last,
               typename std::iterator_traits<InputIterator>::pointer p = 0) {
            bits_.insert(bits_.end(), first, last);
            nums_ = bits_.size() * bits_per_block;
        }

        bitset operator~ () const;
        bitset operator<<(size_t n) const;
        bitset operator>>(size_t n) const;

        bitset& operator<<= (size_t n);
        bitset& operator>>= (size_t n);
        bitset& operator&=  (const bitset& rhs);
        bitset& operator|=  (const bitset& rhs);
        bitset& operator^=  (const bitset& rhs);
        bitset& operator-=  (const bitset& rhs);

        friend bitset operator& (const bitset& x, const bitset& y);
        friend bitset operator| (const bitset& x, const bitset& y);
        friend bitset operator^ (const bitset& x, const bitset& y);
        friend bitset operator- (const bitset& x, const bitset& y);

        friend bool operator== (const bitset& x, const bitset& y);
        friend bool operator!= (const bitset& x, const bitset& y);
        friend bool operator < (const bitset& x, const bitset& y);

        friend void swap(bitset& x, bitset& y);

        void clear();
        void resize(size_t n, bool value = false);

        // set bit i to 'bit'
        bitset& set(size_t i, bool bit = true);
        // set all bits to 1
        bitset& set();
        // reset bit i to 0
        bitset& reset(size_t i);
        // reset all bits to 0
        bitset& reset();

        bitset& flip(size_t i);
        bitset& flip();

        void push_back(bool bit);
        void append(block_type block);

        reference operator[](size_t i);
        const_reference operator[](size_t i) const;

        // number of 1-bits. also known 'population count' or 'hamming weight'
        size_t count() const;
        // number of blocks used
        size_t block() const;
        // number of bits
        size_t size() const;
        // check whether bitset is empty (zero length)
        bool   empty() const;

        // find first position of which bit is 1
        size_t find_first() const;
        // find next position of which bit is 1
        size_t find_next(size_t i) const;

        std::string to_string(bool msb_to_lsb = true,
                              bool all_bits = false,
                              size_t cutoff = 0) const;
    private:
        static size_t block_index(size_t i) {
            return i / bits_per_block;
        }
        static block_type bit_index(size_t i) {
            return i % bits_per_block;
        }
        static block_type bit_mask(size_t i) {
            return block_type(1) << bit_index(i);
        }
        static size_t bits_to_block(size_t bits) {
            return bits / bits_per_block + static_cast<size_t >(bits % bits_per_block != 0);
        }

        static size_t lowest_bit(block_type block);
        block_type extra_bits() const;
        void zero_unused_bits();

        size_t find_from(size_t i) const;

        std::vector<block_type >    bits_;
        size_t                      nums_;
    };

}

#endif
