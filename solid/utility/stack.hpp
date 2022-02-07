// solid/utility/stack.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once
#include "solid/system/cassert.hpp"
#include "solid/system/convertors.hpp"
#include <algorithm>
#include <cstdlib>

namespace solid {

//! A simple and fast stack with interface similar to std one
/*!
    The advantages are:
    - twice faster then the std one
    - while pushing new objects, the allready pushed are not relocated
    in memory (no reallocation is performed)
*/
template <class T, size_t NBits = 5>
class Stack {
    static constexpr const size_t node_mask = bits_to_mask(NBits);
    static constexpr const size_t node_size = bits_to_count(NBits);

    struct Node {
        using Storage = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

        Node(Node* _pprev = nullptr)
            : pprev_(_pprev)
        {
        }

        Storage data_[node_size];
        Node*   pprev_;
    };

    size_t size_              = 0;
    T*     ptop_              = nullptr;
    Node*  ptop_cached_nodes_ = nullptr;

public:
    using reference       = T&;
    using const_reference = T const&;

public:
    Stack()
    {
    }

    Stack(Stack&& _rthat)
        : size_(_rthat.size_)
        , ptop_(_rthat.ptop_)
        , ptop_cached_nodes_(_rthat.ptop_cached_nodes_)
    {
        _rthat.size_              = 0;
        _rthat.ptop_              = nullptr;
        _rthat.ptop_cached_nodes_ = nullptr;
    }
    ~Stack()
    {
        while (size_ != 0) {
            pop();
        }

        while (ptop_cached_nodes_) {
            Node* pnode = ptop_cached_nodes_->pprev_;
            delete ptop_cached_nodes_;
            ptop_cached_nodes_ = pnode;
        }
    }

    Stack& operator=(Stack&& _rthat)
    {
        size_              = _rthat.size_;
        ptop_              = _rthat.ptop_;
        ptop_cached_nodes_ = _rthat.ptop_cached_nodes_;

        _rthat.size_              = 0;
        _rthat.ptop_              = nullptr;
        _rthat.ptop_cached_nodes_ = nullptr;
        return *this;
    }

    bool   empty() const { return !size_; }
    size_t size() const { return size_; }

    void push(const T& _value)
    {
        if ((size_)&node_mask) {
            ++ptop_;
        } else {
            ptop_ = pushNode(ptop_);
        }

        ++size_;
        new (ptop_) T{_value};
    }

    void push(T&& _value)
    {
        if ((size_)&node_mask) {
            ++ptop_;
        } else {
            ptop_ = pushNode(ptop_);
        }

        ++size_;
        new (ptop_) T{std::move(_value)};
    }

    reference       top() { return *ptop_; }
    const_reference top() const { return *ptop_; }

    void pop()
    {
        std::destroy_at(std::launder(ptop_));

        if ((--size_) & node_mask) {
            --ptop_;
        } else {
            ptop_ = popNode(ptop_);
        }
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
            Node* pnode           = pcurrent_node;
            pcurrent_node         = ptop_cached_nodes_;
            ptop_cached_nodes_    = ptop_cached_nodes_->pprev_;
            pcurrent_node->pprev_ = pnode;
        } else {
            pcurrent_node = new Node{pcurrent_node};
        }
        return std::launder(reinterpret_cast<T*>(&pcurrent_node->data_[0]));
    }

    T* popNode(T* _pvalue)
    {
        Node* pcurrent_node = node(_pvalue, 0);
        Node* pprev_node    = pcurrent_node->pprev_;
        solid_assert_log(pcurrent_node != ptop_cached_nodes_, generic_logger);
        pcurrent_node->pprev_ = ptop_cached_nodes_;
        ptop_cached_nodes_    = pcurrent_node; //cache the node
        if (pprev_node) {
            return std::launder(reinterpret_cast<T*>(&pprev_node->data_[node_size - 1]));
        } else {
            solid_assert_log(!size_, generic_logger);
            return nullptr;
        }
    }

private:
    Stack(const Stack&);
    Stack& operator=(const Stack&);
};

} //namespace solid
