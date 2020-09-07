// solid/utility/queue_lockfree.hpp
//
// Copyright (c) 2020 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once
#define NOMINMAX
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/system/statistic.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/functiontraits.hpp"
#include "solid/utility/queue.hpp"

namespace solid {
namespace lockfree {

extern const LoggerT queue_logger;

template <class T, unsigned NBits = 5>
class Queue : NonCopyable {
    static constexpr const size_t node_mask = bits_to_mask(NBits);
    static constexpr const size_t node_size = bits_to_count(NBits);

    struct Node {
        std::atomic<size_t> push_pos_;
        std::atomic<size_t> push_commit_pos_;
        std::atomic<size_t> pop_pos_;
        std::atomic<size_t> use_cnt_;
        std::atomic<Node*>  next_;
        uint8_t             data_[node_size * sizeof(T)];

        Node()
            : push_pos_(0)
            , push_commit_pos_(0)
            , pop_pos_(0)
            , use_cnt_(0)
            , next_(nullptr)
        {
        }

        void clear()
        {
            push_commit_pos_ = 0;
            push_pos_        = 0;
            pop_pos_         = 0;
            next_            = nullptr;
            use_cnt_         = 0;
        }

        T& item(const size_t _i)
        {
            return reinterpret_cast<T*>(data_)[_i];
        }
    };

    struct End {
        Node*                   pnode_;
        std::atomic<size_t>     wait_count_;
        std::atomic_flag        spin_lock_;
        std::mutex              mutex_;
        std::condition_variable condition_;

        End()
            : pnode_(nullptr)
            , wait_count_(0)
        {
            spin_lock_.clear();
        }

        void spinLockAcquire()
        {
            while (spin_lock_.test_and_set(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        }

        void spinLockRelease()
        {
            spin_lock_.clear(std::memory_order_release);
        }

        Node* nodeAcquire()
        {
            spinLockAcquire();
            Node* const pn = pnode_;
            pn->use_cnt_.fetch_add(1);
            spinLockRelease();
            return pn;
        }

        Node* nodeExchange(Node* _pn)
        {
            if (_pn) {
                _pn->use_cnt_.fetch_add(1);
            }
            spinLockAcquire();
            Node* const pn = pnode_;
            pnode_         = _pn;
            spinLockRelease();
            return pn;
        }

        Node* nodeNext()
        {
            spinLockAcquire();
            Node* const pn = pnode_;
            pnode_         = pn->next_.load();
            spinLockRelease();
            return pn;
        }
    };
    const size_t        max_size_;
    std::atomic<size_t> size_;
    End                 pop_end_;
    End                 push_end_;
    std::atomic<Node*>  pempty_;
#ifdef SOLID_HAS_STATISTICS
    struct Statistic : solid::Statistic {
        std::atomic<size_t> push_count_;
        std::atomic<size_t> push_node_count_;
        std::atomic<size_t> pop_count_;
        std::atomic<size_t> pop_node_count_;
        std::atomic<size_t> new_node_count_;
        std::atomic<size_t> del_node_count_;
        std::atomic<size_t> pop_notif_;
        std::atomic<size_t> push_notif_;
        std::atomic<size_t> wait_pop_on_pos_;
        std::atomic<size_t> wait_pop_on_next_;
        std::atomic<size_t> notify_all_count_;
        Statistic()
            : push_count_(0)
            , push_node_count_(0)
            , pop_count_(0)
            , pop_node_count_(0)
            , new_node_count_(0)
            , del_node_count_(0)
            , pop_notif_(0)
            , push_notif_(0)
            , wait_pop_on_pos_(0)
            , wait_pop_on_next_(0)
            , notify_all_count_(0)
        {
        }

        std::ostream& print(std::ostream& _ros) const override
        {
            _ros << " push_count = " << push_count_;
            _ros << " pop_count = " << pop_count_;
            _ros << " pop_node_count = " << pop_node_count_;
            _ros << " push_node_count = " << push_node_count_;
            _ros << " new_node_count = " << new_node_count_;
            _ros << " del_node_count = " << del_node_count_;
            _ros << " pop_notif = " << pop_notif_;
            _ros << " push_notif = " << push_notif_;
            _ros << " wait_pop_on_pos = " << wait_pop_on_pos_;
            _ros << " wait_pop_on_next = " << wait_pop_on_next_;
            _ros << " notify_all_count = " << notify_all_count_;
            return _ros;
        }
    } statistic_;
#endif
public:
    static constexpr size_t node_capacity = node_size;

    Queue(const size_t _max_size = InvalidSize())
        : max_size_(_max_size)
        , size_(0)
        , pempty_(nullptr)
    {
        Node* pn = newNode();
        pop_end_.nodeExchange(pn);
        push_end_.nodeExchange(pn);
    }

    ~Queue();

    size_t push(const T& _rt, const bool _wait = true)
    {
        T* pt = nullptr;
        return doPush(_rt, std::move(*pt), std::integral_constant<bool, true>(), _wait);
    }

    size_t push(T&& _rt, const bool _wait = true)
    {
        T* pt = nullptr;
        return doPush(*pt, std::move(_rt), std::integral_constant<bool, false>(), _wait);
    }

    bool pop(T& _rt);

    void wake()
    {
        std::lock_guard<std::mutex> lock(pop_end_.mutex_);
        pop_end_.condition_.notify_all();
    }
    void dumpStatistics() const;

private:
    Node* newNode()
    {
        Node* pold = popEmptyNode();
        if (pold == nullptr) {
            pold = new Node;
            solid_statistic_inc(statistic_.new_node_count_);
        } else {
            pold->next_.store(nullptr);
        }
        solid_statistic_inc(statistic_.pop_node_count_);
        return pold;
    }

    Node* popEmptyNode()
    {
        Node* pold = pempty_.load();
        while (pold && !pempty_.compare_exchange_weak(pold, pold->next_.load())) {
        }
        return pold;
    }

    void pushEmptyNode(Node* _pn)
    {
        _pn->clear();
        Node* pcrt = pempty_.load();
        _pn->next_ = pcrt;
        while (!pempty_.compare_exchange_weak(pcrt, _pn)) {
            _pn->next_ = pcrt;
        }
        solid_statistic_inc(statistic_.push_node_count_);
    }

    void nodeRelease(Node* _pn, const int _line)
    {
        const size_t cnt = _pn->use_cnt_.fetch_sub(1);
        solid_assert_log(cnt != 0, queue_logger);
        if (cnt == 1) {
            //the last one
            pushEmptyNode(_pn);
        }
    }

    Node* popNodeAquire()
    {
        return pop_end_.nodeAcquire();
    }

    Node* pushNodeAcquire()
    {
        return push_end_.nodeAcquire();
    }

    T* doCopyOrMove(Node& _rn, const size_t _pos, const T& _rt, T&& /*_ut*/, std::integral_constant<bool, true>)
    {
        return new (_rn.data_ + (_pos * sizeof(T))) T{_rt};
    }

    T* doCopyOrMove(Node& _rn, const size_t _pos, const T& /*_rt*/, T&& _ut, std::integral_constant<bool, false>)
    {
        return new (_rn.data_ + (_pos * sizeof(T))) T{std::move(_ut)};
    }

    template <bool IsCopy>
    size_t doPush(const T& _rt, T&& _ut, std::integral_constant<bool, IsCopy>, const bool _wait);
};

//-----------------------------------------------------------------------------
template <class T, unsigned NBits>
void Queue<T, NBits>::dumpStatistics() const
{
#ifdef SOLID_HAS_STATISTICS
    solid_dbg(queue_logger, Statistic, "Queue: " << this << " statistic:" << this->statistic_);
#endif
}

template <class T, unsigned NBits>
Queue<T, NBits>::~Queue()
{
    solid_dbg(queue_logger, Verbose, this);
    nodeRelease(pop_end_.nodeExchange(nullptr), __LINE__);
    nodeRelease(push_end_.nodeExchange(nullptr), __LINE__);

    Node* pn;
    while ((pn = popEmptyNode())) {
        delete pn;
        solid_statistic_inc(statistic_.del_node_count_);
    }

    solid_dbg(queue_logger, Verbose, this);
    dumpStatistics();
}
//-----------------------------------------------------------------------------
template <class T, unsigned NBits>
template <bool IsCopy>
size_t Queue<T, NBits>::doPush(const T& _rt, T&& _ut, std::integral_constant<bool, IsCopy> _is_copy, const bool _wait)
{

    do {
        Node*        pn  = pushNodeAcquire();
        const size_t pos = pn->push_pos_.fetch_add(1);

        if (pos < node_size) {
            doCopyOrMove(*pn, pos, _rt, std::move(_ut), _is_copy);

            const size_t sz = size_.fetch_add(1) + 1;

            std::atomic_thread_fence(std::memory_order_release);

            {
                size_t crtpos = pos;
                while (!pn->push_commit_pos_.compare_exchange_weak(crtpos, pos + 1, std::memory_order_release, std::memory_order_relaxed)) {
                    crtpos = pos;
                    std::this_thread::yield();
                }
            }
            nodeRelease(pn, __LINE__);
            return sz;
        } else {
            //overflow
            std::unique_lock<std::mutex> lock(push_end_.mutex_);

            if (size_.load() >= max_size_) {
                if (_wait) {
                    solid_dbg(queue_logger, Warning, this << "wait qsz = " << size_.load());
                    push_end_.wait_count_.fetch_add(1);
                    push_end_.condition_.wait(lock, [this]() { return size_.load() < max_size_; });
                    push_end_.wait_count_.fetch_sub(1);
                } else {
                    nodeRelease(pn, __LINE__);
                    return InvalidSize();
                }
            }

            //pn is locked!
            //the following check is safe because push_end_.pnode_ is
            //modified only under push_end_.mutex_ lock
            if (push_end_.pnode_ == pn) {
                solid_dbg(queue_logger, Verbose, this << " newNode");
                //ABA cannot happen because pn is locked and cannot be in the empty stack
                Node* pnewn = newNode();
                pnewn->use_cnt_.fetch_add(1); //one for ptmpn->next_
                Node* ptmpn = push_end_.nodeExchange(pnewn);

                ptmpn->next_.store(pnewn);

                nodeRelease(ptmpn, __LINE__);
            }
            nodeRelease(pn, __LINE__);
        }
    } while (true);
}
//-----------------------------------------------------------------------------
template <class T, unsigned NBits>
bool Queue<T, NBits>::pop(T& _rt)
{
    size_t loop_count = 0;
    do {
        ++loop_count;

        Node*  pn  = popNodeAquire();
        size_t pos = pn->pop_pos_.load();
        bool   valid;
        while ((valid = (pos < pn->push_commit_pos_.load(/*std::memory_order_acquire*/)))) {
            if (pn->pop_pos_.compare_exchange_weak(pos, pos + 1)) {
                break;
            }
        }

        if (valid) {
            std::atomic_thread_fence(std::memory_order_acquire);

            _rt = std::move(pn->item(pos));

            const size_t qsz = size_.fetch_sub(1);
            if (qsz == max_size_) {
                solid_dbg(queue_logger, Warning, this << " qsz = " << qsz);
                std::unique_lock<std::mutex> lock(push_end_.mutex_);
                push_end_.condition_.notify_all();
            }
        } else if (pos == node_size) {
            std::unique_lock<std::mutex> lock(pop_end_.mutex_);
            if (pn->next_.load() != nullptr) {
                if (pop_end_.pnode_ == pn) {
                    //ABA cannot happen because pn is locked and cannot be in the empty stack
                    Node* ptmpn = pop_end_.nodeNext();
                    //solid_dbg(queue_logger, Verbose, this << " move to new node " << pn << " -> " << pn->next_.load()<<" "<<pos<<" "<<pn->push_commit_pos_<< " "<<loop_count);
                    solid_check_log(ptmpn == pn, queue_logger, ptmpn << " != " << pn);
                    nodeRelease(ptmpn, __LINE__);
                }
                nodeRelease(pn, __LINE__);
                continue;
            } /*else{
                solid_dbg(queue_logger, Verbose, this << " nothing to pop "<<pn<<" "<<size_<<" "<<pos<<" "<<pn->push_commit_pos_<<" "<<loop_count);
            }*/
        } /*else{
            solid_dbg(queue_logger, Verbose, this << " nothing to pop "<<pn<<" "<<size_<<" "<<pos<<" "<<pn->push_commit_pos_<<" "<<loop_count);
        }*/

        nodeRelease(pn, __LINE__);
        return valid;
    } while (true);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
} //namespace lockfree
} //namespace solid
