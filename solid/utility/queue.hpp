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
    static constexpr const size_t node_mask = bitsToMask(NBits);
    static constexpr const size_t node_size = bitsToCount(NBits);
    
    struct Node {
        Node(Node* _pnext = nullptr)
            : next(_pnext)
        {
        }
        Node*    next;
        uint8_t  data[node_size * sizeof(T)];
    };
    
    
    size_t sz;
    size_t popsz;
    T*     pb; //back
    T*     pf; //front
    Node*  ptn; //empty nodes
    
public:
    typedef T&       reference;
    typedef T const& const_reference;

public:
    Queue()
        : sz(0)
        , popsz(0)
        , pb(nullptr)
        , pf(nullptr)
        , ptn(nullptr)
    {
    }
    Queue(Queue&& _rq)
        : sz(_rq.sz)
        , popsz(_rq.popsz)
        , pb(_rq.pb)
        , pf(_rq.pf)
        , ptn(_rq.ptn)
    {
        _rq.sz    = 0;
        _rq.popsz = 0;
        _rq.pb    = nullptr;
        _rq.pf    = nullptr;
        _rq.ptn   = nullptr;
    }
    
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;
    
    ~Queue()
    {
        clear();
    }

    Queue& operator=(Queue&& _rq)
    {
        clear();
        
        sz    = _rq.sz;
        popsz = _rq.popsz;
        pb    = _rq.pb;
        pf    = _rq.pf;
        ptn   = _rq.ptn;

        _rq.sz    = 0;
        _rq.popsz = 0;
        _rq.pb    = nullptr;
        _rq.pf    = nullptr;
        _rq.ptn   = nullptr;
        return *this;
    }

    bool   empty() const { return !sz; }
    size_t size() const { return sz; }

    void push(const T& _t)
    {
        if ((sz + popsz) & node_mask)
            ++pb;
        else
            pb = pushNode(pb);

        ++sz;
        new (pb) T(_t);
    }

    void push(T&& _t)
    {
        if ((sz + popsz) & node_mask)
            ++pb;
        else
            pb = pushNode(pb);

        ++sz;
        new (pb) T(std::move(_t));
    }

    reference back()
    {
        return *pb;
    }
    const_reference back() const
    {
        return *pb;
    }

    void pop()
    {
        pf->~T();
        --sz;
        if ((++popsz) & node_mask)
            ++pf;
        else {
            pf    = popNode(pf);
            popsz = 0;
        }
    }
    
    reference front()
    {
        return *pf;
    }
    
    const_reference front() const
    {
        return *pf;
    }
    
    void clear(){
        while (sz){
            pop();
        }
        Node* pn = pf ? (Node*)(((char*)pf) - popsz * sizeof(T) - sizeof(Node*)) : nullptr;
        while (ptn) {
            Node* tn = ptn->next;
            SOLID_ASSERT(ptn != pn);
            delete ptn;
            ptn = tn;
        }
        delete pn;
    }
private:
    
    constexpr Node* node(void *_p)const{
        return reinterpret_cast<Node*>(static_cast<uint8_t*>(_p) - node_size * sizeof(T) + sizeof(T) - sizeof(Node*));
    }
    
    T* pushNode(void* _p)
    {
        Node* pn = _p ?  node(_p) : nullptr;
        if (ptn) {
            Node* tn = ptn;
            ptn      = ptn->next;
            tn->next = nullptr;
            if (pn) {
                pn->next = tn;
                return (T*)tn->data;
            } else {
                return (pf = (T*)tn->data);
            }
        } else {
            if (pn) {
                pn->next = new Node;
                pn       = pn->next;
                return (T*)pn->data;
            } else {
                pn = new Node;
                return (pf = (T*)pn->data);
            }
        }
    }
    T* popNode(void* _p)
    {
        SOLID_ASSERT(_p);
        Node* pn  = node(_p);
        Node* ppn = pn->next;
        pn->next  = ptn;
        ptn       = pn; //cache the node
        if (ppn) {
            return (T*)(ppn->data);
        } else {
            SOLID_ASSERT(!sz);
            pb = nullptr;
            return nullptr;
        }
    }
};



} //namespace solid
