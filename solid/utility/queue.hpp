// solid/utility/queue.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <cstdlib>
#include <utility>

#include "solid/system/cassert.hpp"
#include "solid/system/convertors.hpp"

namespace solid {

//! A simple and fast queue with interface similar to std::queue
/*!
    The advantages are:
    - twice faster then the std one
    - while pushing new objects, the allready pushed are not relocated
    in memory (no reallocation is performed)
*/
template <class T, unsigned NBits = 5>
class Queue {
    static constexpr const size_t node_mask = bits_to_mask(NBits);
    static constexpr const size_t node_size = bits_to_count(NBits);

    struct Node {
        using Storage = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

        Node(Node* _pnext = nullptr)
            : pnext_(_pnext)
        {
        }
        Storage       data_[node_size];
        Node*         pnext_;
    };

    size_t size_ = 0;
    size_t current_node_pop_count_ = 0;
    T*     pback_ = nullptr;
    T*     pfront_ = nullptr;
    Node* ptop_cached_nodes_ = nullptr;

public:
    using reference = T&;
    using const_reference = T const&;

public:
    Queue() {}

    Queue(Queue&& _rthat)
        : size_(_rthat.size_)
        , current_node_pop_count_(_rthat.current_node_pop_count_)
        , pback_(_rthat.pback_)
        , pfront_(_rthat.pfront_)
        , ptop_cached_nodes_(_rthat.ptop_cached_nodes_)
    {
        _rthat.size_ = 0;
        _rthat.current_node_pop_count_ = 0;
        _rthat.pback_ = nullptr;
        _rthat.pfront_ = nullptr;
        _rthat.ptop_cached_nodes_ = nullptr;
    }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    ~Queue()
    {
        clear();
    }

    Queue& operator=(Queue&& _rthat)
    {
        clear();

        size_    = _rthat.size_;
        current_node_pop_count_ = _rthat.current_node_pop_count_;
        pback_    = _rthat.pback_;
        pfront_    = _rthat.pfront_;
        ptop_cached_nodes_ = _rthat.ptop_cached_nodes_;

        _rthat.size_ = 0;
        _rthat.current_node_pop_count_ = 0;
        _rthat.pback_ = nullptr;
        _rthat.pfront_ = nullptr;
        _rthat.ptop_cached_nodes_ = nullptr;
        return *this;
    }

    bool   empty() const { return !size_; }
    size_t size() const { return size_; }

    void push(const T& _value)
    {
        if ((size_ + current_node_pop_count_) & node_mask) {
            ++pback_;
        }
        else {
            pback_ = pushNode(pback_);
        }
        ++size_;
        new (pback_) T{ _value };
    }

    void push(T&& _value)
    {
        if ((size_ + current_node_pop_count_) & node_mask) {
            ++pback_;
        }
        else {
            pback_ = pushNode(pback_);
        }
        ++size_;
        
        new (pback_) T{ std::move(_value) };
    }

    reference back()
    {
        return *pback_;
    }
    const_reference back() const
    {
        return *pback_;
    }

    reference front()
    {
        return *pfront_;
    }

    const_reference front() const
    {
        return *pfront_;
    }

    void pop()
    {
        pfront_->~T();
        --size_;
        if ((++current_node_pop_count_) & node_mask)
            ++pfront_;
        else {
            pfront_    = popNode(pfront_);
            current_node_pop_count_ = 0;
        }
    }

    void clear()
    {
        while (size_ != 0) {
            pop();
        }
        
        Node* pcurrent_node = pfront_ ? node(pfront_, current_node_pop_count_) : nullptr;
        while (ptop_cached_nodes_ != nullptr) {
            Node* pnext_chached_node = ptop_cached_nodes_->pnext_;
            solid_assert_log(ptop_cached_nodes_ != pnext_chached_node, generic_logger);
            delete ptop_cached_nodes_;
            ptop_cached_nodes_ = pnext_chached_node;
        }
        delete pcurrent_node;
    }

private:
    constexpr Node* node(T* _plast_in_node, const size_t _index = (node_size - 1)) const
    {
        return std::launder(reinterpret_cast<Node*>(_plast_in_node - _index));
    }

    T* pushNode(T* _pvalue)
    {
        Node* pcurrent_node = _pvalue ? node(_pvalue) : nullptr;
        if (ptop_cached_nodes_) {
            Node* pnode = ptop_cached_nodes_;
            ptop_cached_nodes_ = ptop_cached_nodes_->pnext_;
            pnode->pnext_ = nullptr;
            if (pcurrent_node) {
                pcurrent_node->pnext_ = pnode;
                return std::launder(reinterpret_cast<T*>(&pnode->data_[0]));
            } else {
                return (pfront_ = std::launder(reinterpret_cast<T*>(&pnode->data_[0])));
            }
        } else {
            if (pcurrent_node) {
                pcurrent_node->pnext_ = new Node;
                pcurrent_node = pcurrent_node->pnext_;
                return std::launder(reinterpret_cast<T*>(&pcurrent_node->data_[0]));
            } else {
                pcurrent_node = new Node;
                return (pfront_ = std::launder(reinterpret_cast<T*>(&pcurrent_node->data_[0])));
            }
        }
    }
    T* popNode(T* _pvalue)
    {
        solid_assert_log(_pvalue, generic_logger);
        Node* pcurrent_node  = node(_pvalue);
        Node* pnext_node = pcurrent_node->pnext_;
        pcurrent_node->pnext_  = ptop_cached_nodes_;
        ptop_cached_nodes_ = pcurrent_node; //cache the node
        if (pnext_node) {
            return std::launder(reinterpret_cast<T*>(&pnext_node->data_[0]));
        } else {
            solid_assert_log(!size_, generic_logger);
            pback_ = nullptr;
            return nullptr;
        }
    }
};

} //namespace solid
