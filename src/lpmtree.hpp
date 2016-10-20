/**
 * create: 2016-10-09
 * author: mr.wclong@yahoo.com
 */
#ifndef COH_LPMTREE_HPP
#define COH_LPMTREE_HPP
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include "iobufs.hpp"

namespace coh {

    template<typename T >
    class lpmtree {
    public:
        lpmtree();
        ~lpmtree();

        void                insert(const std::string& index, std::shared_ptr<T > value);
        std::shared_ptr<T > search(const std::string& index, std::string* prefix);
        std::shared_ptr<T > search(const slice& index, std::string* prefix);
        std::vector<std::shared_ptr<T > >   values() const;
    private:
        struct node_t;
        typedef std::shared_ptr<T >     value_t;
        typedef std::vector<node_t* >   child_t;
        struct node_t {
            char    index;
            value_t value;
            child_t child;
            int     num;

            node_t(int num_of_nodes = 256)
                : num(num_of_nodes) {
                child.resize(num_of_nodes, nullptr);
            }
            ~node_t() {
                for(int i = 0; i < num; ++i) {
                    delete child[i];
                }
            }
        };

        bool    exist(node_t* current) {
            return current != nullptr;
        }

        node_t* alloc(node_t* current, char index) {
            current = new node_t();
            current->index = index;
            return current;
        }

        node_t*                             root_;
        std::vector<std::shared_ptr<T > >   values_;
    };

    template<typename T >
    inline lpmtree<T >::lpmtree()
        : root_(nullptr)
    {
        root_ = new node_t();
    }

    template<typename T >
    inline lpmtree<T >::~lpmtree()
    {
        delete root_;
        root_ = nullptr;
    }

    template<typename T >
    inline void lpmtree<T >::insert(const std::string& index, std::shared_ptr<T > value)
    {
        node_t* current = root_;
        for(auto& chr : index) {
            unsigned char idx = (unsigned char)chr;
            if(!exist(current->child[idx])) {
                current->child[idx] = alloc(current->child[idx], chr);
            }
            current = current->child[idx];
        }
        current->value = value;

        if(std::find(values_.begin(), values_.end(), value) == values_.end()) {
            values_.push_back(value);
        }
    }

    template<typename T >
    inline std::shared_ptr<T > lpmtree<T >::search(const slice& index, std::string* prefix)
    {
        if(prefix) {
            prefix->clear();
        }

        std::string         match;
        std::shared_ptr<T > found;
        node_t* current = root_;
        const char* data = index.data();
        for(int i = 0; i < index.size(); ++i) {
            unsigned char   idx = (unsigned char)data[i];
            char            chr = data[i];
            if(!exist(current->child[idx])) {
                break;
            }
            else {
                current = current->child[idx];
                match.push_back(chr);
                if(current->value) {
                    found = current->value;
                    if(prefix) {
                        *prefix = match;
                    }
                }
            }
        }
        return found;
    }

    template<typename T >
    inline std::shared_ptr<T > lpmtree<T >::search(const std::string& index, std::string* prefix)
    {
        return search(slice(index.data(), index.size()), prefix);
    }

    template<typename T >
    inline std::vector<std::shared_ptr<T> > lpmtree<T>::values() const
    {
        return values_;
    }


}

#ifdef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "3rd/doctest.h"
#include "lpmtree.hpp"

#include <stdio.h>
#include <string>
#include <vector>


TEST_CASE("prefix tree search")
{
    coh::lpmtree<std::string > tree;

    tree.insert("0", std::make_shared<std::string >("USA"));
    tree.insert("0086", std::make_shared<std::string >("CHN"));
    tree.insert("008610", std::make_shared<std::string >("CHN.BEIJING"));
    tree.insert("008621", std::make_shared<std::string >("CHN.WUHAN"));
    tree.insert("0086755", std::make_shared<std::string >("CHN.SHENZHEN"));
    tree.insert("00867551", std::make_shared<std::string >("CHN.SHENZHEN.FUTIAN"));

    std::string matched;
    CHECK(*tree.search("00", &matched) == "USA");
    CHECK(matched == "0");
    CHECK(*tree.search("00862", &matched) == "CHN");
    CHECK(matched == "0086");
    CHECK(*tree.search("008610", &matched) == "CHN.BEIJING");
    CHECK(matched == "008610");
    CHECK(*tree.search("00867552", &matched) == "CHN.SHENZHEN");
    CHECK(matched == "0086755");
    CHECK(*tree.search("0086755123456", &matched) == "CHN.SHENZHEN.FUTIAN");
    CHECK(matched == "00867551");

    printf("prefix tree search done\r\n");

}

#endif
#endif
