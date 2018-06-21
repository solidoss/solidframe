// solid/utility/workpool_atomic.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
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

extern const LoggerT workpool_logger;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
namespace thread_safe {

template <class T, unsigned NBits = 5>
class Queue {
    static constexpr const size_t node_mask = bitsToMask(NBits);
    static constexpr const size_t node_size = bitsToCount(NBits);

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
            if (_pn)
                _pn->use_cnt_.fetch_add(1);
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
        {
        }

        std::ostream& print(std::ostream& _ros) const override
        {
            _ros << " push_count_ = " << push_count_;
            _ros << " pop_count_ = " << pop_count_;
            _ros << " pop_node_count_ = " << pop_node_count_;
            _ros << " push_node_count_ = " << push_node_count_;
            _ros << " new_node_count_ = " << new_node_count_;
            _ros << " del_node_count_ = " << del_node_count_;
            _ros << " pop_notif_ = " << pop_notif_;
            _ros << " push_notif_ = " << push_notif_;
            _ros << " wait_pop_on_pos_ = " << wait_pop_on_pos_;
            _ros << " wait_pop_on_next_ = " << wait_pop_on_next_;
            return _ros;
        }
    } statistic_;
#endif
public:
    static constexpr size_t nodeSize()
    {
        return node_size;
    }

    Queue()
        : size_(0)
        , pempty_(nullptr)
    {
        Node* pn = newNode();
        pop_end_.nodeExchange(pn);
        push_end_.nodeExchange(pn);
    }

    ~Queue();

    size_t push(const T& _rt, const size_t _max_queue_size)
    {
        T* pt = nullptr;
        return doPush(_rt, std::move(*pt), _max_queue_size, std::integral_constant<bool, true>());
    }

    size_t push(T&& _rt, const size_t _max_queue_size)
    {
        T* pt = nullptr;
        return doPush(*pt, std::move(_rt), _max_queue_size, std::integral_constant<bool, false>());
    }

    bool pop(T& _rt, std::atomic<bool>& _running, const size_t _max_queue_size);

    void wake()
    {
        pop_end_.condition_.notify_all();
    }

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
        SOLID_ASSERT(cnt != 0);
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

    T* doCopyOrMove(Node& _rn, const size_t _pos, const T& _rt, T&& _ut, std::integral_constant<bool, true>)
    {
        return new (_rn.data_ + (_pos * sizeof(T))) T{_rt};
    }

    T* doCopyOrMove(Node& _rn, const size_t _pos, const T& _rt, T&& _ut, std::integral_constant<bool, false>)
    {
        return new (_rn.data_ + (_pos * sizeof(T))) T{std::move(_ut)};
    }

    template <bool IsCopy>
    size_t doPush(const T& _rt, T&& _ut, const size_t _max_queue_size, std::integral_constant<bool, IsCopy>);
};

//-----------------------------------------------------------------------------

template <class T, unsigned NBits>
Queue<T, NBits>::~Queue()
{
    solid_dbg(workpool_logger, Verbose, this);
    nodeRelease(pop_end_.nodeExchange(nullptr), __LINE__);
    nodeRelease(push_end_.nodeExchange(nullptr), __LINE__);

    Node* pn;
    while ((pn = popEmptyNode())) {
        delete pn;
        solid_statistic_inc(statistic_.del_node_count_);
    }

    solid_dbg(workpool_logger, Verbose, this);
#ifdef SOLID_HAS_STATISTICS
    solid_dbg(workpool_logger, Statistic, "Queue: " << this << " statistic:" << this->statistic_);
#endif
}
//-----------------------------------------------------------------------------
#ifdef SOLID_WP_PRINT
inline void printt(const size_t& _rt, const int _l, void* _pn, const size_t _pos)
{
    solid_dbg(workpool_logger, Info, '[' << _l << "] " << _rt << ' ' << _pn << ' ' << _pos);
}
template <class T>
inline void printt(const T&, const int, void*, const size_t) {}
#endif
//NOTE(*):
//      we cannot use notify_one because we have no control on which thread is waken up.
//      Suppose we have two threads waiting, one for position 4 and one for 5
//      push() thread, fills-up position 4, increments push_commit_pos_ to 5 and notifies waiting threads.
//      If we would use notify_one, it may happen that the the waken up thread be the one waiting on
//      position 5 - its waiting condition is not satisfied and it keeps on waiting and the
//      thread waiting for position 4 is never waken.
//NOTE(**):
//      Problematic situation:
//      push threads reserve positions: 5,6
//      pop threads reserve positions: 5,6
//
//      position 6 is commited but pop thread handling position 5 must make no progress until position 5
//      is also commited
template <class T, unsigned NBits>
template <bool IsCopy>
size_t Queue<T, NBits>::doPush(const T& _rt, T&& _ut, const size_t _max_queue_size, std::integral_constant<bool, IsCopy> _is_copy)
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
                while (!pn->push_commit_pos_.compare_exchange_weak(crtpos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
                    crtpos = pos;
                    std::this_thread::yield();
                }
            }
            nodeRelease(pn, __LINE__);
#ifdef SOLID_WP_PRINT
            if (pos == 0) {
                printt(_rt, __LINE__, pn, 0);
            }
#endif

            const size_t pop_wait_cnt = pop_end_.wait_count_.load();

            if (pop_wait_cnt != 0) {
                {
                    std::unique_lock<std::mutex> lock(pop_end_.mutex_);
                }
                solid_dbg(workpool_logger, Verbose, this << " pop_wait_cnt = " << pop_wait_cnt);
                pop_end_.condition_.notify_all(); //see NOTE(*) below
            }

            solid_statistic_inc(statistic_.push_count_);
            solid_dbg(workpool_logger, Verbose, this << " done push " << sz);
            return sz;
        } else {
            //overflow
            bool do_notify_pop_end = false;
            {
                std::unique_lock<std::mutex> lock(push_end_.mutex_);

                if (size_.load() >= _max_queue_size) {
                    push_end_.wait_count_.fetch_add(1);
                    push_end_.condition_.wait(lock, [this, _max_queue_size]() { return size_.load() < _max_queue_size; });
                    push_end_.wait_count_.fetch_sub(1);
                }

                //pn is locked!
                //the following check is safe because push_end_.pnode_ is
                //modified only under push_end_.mutex_ lock
                if (push_end_.pnode_ == pn) {
                    solid_dbg(workpool_logger, Verbose, this << " newNode");
                    //ABA cannot happen because pn is locked and cannot be in the empty stack
                    Node* pnewn = newNode();
                    pnewn->use_cnt_.fetch_add(1); //one for ptmpn->next_
                    Node* ptmpn = push_end_.nodeExchange(pnewn);

                    ptmpn->next_.store(pnewn);

                    nodeRelease(ptmpn, __LINE__);

                    do_notify_pop_end = pop_end_.wait_count_.load() != 0;
                }
                nodeRelease(pn, __LINE__);
            }
            if (do_notify_pop_end) {
                {
                    std::unique_lock<std::mutex> lock(pop_end_.mutex_);
                }
                pop_end_.condition_.notify_all(); //see NOTE(*) below
                solid_statistic_inc(statistic_.pop_notif_);
            }
        }
    } while (true);
}
//-----------------------------------------------------------------------------
template <class T, unsigned NBits>
bool Queue<T, NBits>::pop(T& _rt, std::atomic<bool>& _running, const size_t _max_queue_size)
{
    do {
        Node*        pn  = popNodeAquire();
        const size_t pos = pn->pop_pos_.fetch_add(1); //reserve waiting spot

        if (pos < node_size) {
            {
                size_t push_commit_pos;
                size_t count = 64;

                while (pos >= (push_commit_pos = pn->push_commit_pos_.load(std::memory_order_relaxed)) && count--) {
                    std::this_thread::yield();
                }

                if (pos >= push_commit_pos) {
                    solid_dbg(workpool_logger, Verbose, this << " need to wait - pos = " << pos << " >= commit_pos = " << push_commit_pos);
                    //need to wait
                    std::unique_lock<std::mutex> lock(pop_end_.mutex_);

                    pop_end_.wait_count_.fetch_add(1);
                    pop_end_.condition_.wait(
                        lock,
                        [pn, pos, &_running]() {
                            return (pos < pn->push_commit_pos_.load(std::memory_order_relaxed)) || !_running.load();
                        });
                    pop_end_.wait_count_.fetch_sub(1);
                    solid_statistic_inc(statistic_.wait_pop_on_pos_);
                }
            }
            const size_t push_commit_pos = pn->push_commit_pos_.load(std::memory_order_relaxed);

            std::atomic_thread_fence(std::memory_order_acquire);

            if (pos >= push_commit_pos) {
                solid_dbg(workpool_logger, Info, this << " stop worker");
                nodeRelease(pn, __LINE__);
                return false;
            }

            _rt             = std::move(pn->item(pos));
            const size_t sz = size_.fetch_sub(1) - 1;
#ifdef SOLID_WP_PRINT
            if (pos == 0) {
                printt(_rt, __LINE__, pn, push_commit_pos);
            }
#endif
            SOLID_CHECK(sz < 10000000ULL);
            nodeRelease(pn, __LINE__);
            if (sz < _max_queue_size && (_max_queue_size - sz) <= push_end_.wait_count_.load()) {
                solid_dbg(workpool_logger, Verbose, this << " notify push - size = " << sz << " wait_count = " << push_end_.wait_count_.load());
                //we need the lock here in order to be certain that push threads
                // are either waiting on push_end_.condition_ or have not yet read size_ value
                {
                    std::lock_guard<std::mutex> lock(push_end_.mutex_);
                }
                push_end_.condition_.notify_one();
                solid_statistic_inc(statistic_.push_notif_);
            }
            solid_statistic_inc(statistic_.pop_count_);
            solid_dbg(workpool_logger, Verbose, this << " done pop - pos = " << pos);
            return true;
        } else {
            std::unique_lock<std::mutex> lock(pop_end_.mutex_);

            if (pn->next_.load() != nullptr) {
            } else {
                pop_end_.wait_count_.fetch_add(1);
                pop_end_.condition_.wait(
                    lock,
                    [pn, &_running]() {
                        return pn->next_.load() != nullptr || !_running.load();
                    });
                pop_end_.wait_count_.fetch_sub(1);
                solid_statistic_inc(statistic_.wait_pop_on_next_);
            }

            if (pn->next_.load() == nullptr) {
                solid_dbg(workpool_logger, Warning, this << " stop worker");
                nodeRelease(pn, __LINE__);
                return false;
            }

            if (pop_end_.pnode_ == pn) {
                //ABA cannot happen because pn is locked and cannot be in the empty stack
                Node* ptmpn = pop_end_.nodeNext();
                solid_dbg(workpool_logger, Verbose, this << " move to new node " << pn << " -> " << pn->next_.load());
                SOLID_CHECK(ptmpn == pn, ptmpn << " != " << pn);
                nodeRelease(ptmpn, __LINE__);
            }
            nodeRelease(pn, __LINE__);
        }

    } while (true);
}

} //namespace thread_safe

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

struct WorkPoolConfiguration {
    size_t max_worker_count_;
    size_t max_job_queue_size_;

    explicit WorkPoolConfiguration(
        const size_t _max_worker_count   = std::thread::hardware_concurrency(),
        const size_t _max_job_queue_size = std::numeric_limits<size_t>::max())
        : max_worker_count_(_max_worker_count == 0 ? std::thread::hardware_concurrency() : _max_worker_count)
        , max_job_queue_size_(_max_job_queue_size == 0 ? std::numeric_limits<size_t>::max() : _max_job_queue_size)
    {
    }
};
//-----------------------------------------------------------------------------
//! Pool of threads handling Jobs
/*!
 * 
 * Requirements
 *  * Given a valid reference to a WorkPool, one MUST be able to always push
 *      new jobs
 *  * All pushed jobs MUST be handled.
 *
 * Design decisions
 *  * Start/stop interface not appropriate because:
 *      - One will be able to push new jobs after the WorkPool is created but
 *          "start" might never be called so jobs cannot be handled.
 *      - Jobs pushed after stop cannot be handled.
 *  * Non-copyable, non-movable, constructor start interface
 *      + Works with the given requirements
 *      - Cannot do prepare for stopping (stop(wait = false)) then wait for stopping (stop(wait = true))
 *          this way stopping multiple workpools may take longer
 *      = One can use WorkPool as a shared_ptr to ensure it is available for as long as it is needed.
 */

//-----------------------------------------------------------------------------

template <typename Job, size_t QNBits = 10>
class WorkPool {
    using ThisT          = WorkPool<Job, QNBits>;
    using WorkerFactoryT = std::function<std::thread()>;
    using ThreadVectorT  = std::vector<std::thread>;
    using JobQueueT      = thread_safe::Queue<Job, QNBits>;
    using AtomicBoolT    = std::atomic<bool>;

    const WorkPoolConfiguration config_;
    AtomicBoolT                 running_;
    std::atomic<size_t>         thr_cnt_;
    WorkerFactoryT              worker_factory_fnc_;
    JobQueueT                   job_q_;
    ThreadVectorT               thr_vec_;
    std::mutex                  thr_mtx_;
#ifdef SOLID_HAS_STATISTICS
    struct Statistic : solid::Statistic {
        std::atomic<size_t>   max_worker_count_;
        std::atomic<size_t>   max_jobs_in_queue_;
        std::atomic<uint64_t> max_jobs_on_thread_;
        std::atomic<uint64_t> min_jobs_on_thread_;
        std::atomic<size_t>   wait_count_;

        Statistic()
            : max_worker_count_(0)
            , max_jobs_in_queue_(0)
            , max_jobs_on_thread_(0)
            , min_jobs_on_thread_(-1)
            , wait_count_(0)
        {
        }

        std::ostream& print(std::ostream& _ros) const override
        {
            _ros << " max_worker_count_ = " << max_worker_count_;
            _ros << " max_jobs_in_queue_ = " << max_jobs_in_queue_;
            _ros << " max_jobs_on_thread_ = " << max_jobs_on_thread_;
            _ros << " min_jobs_on_thread_ = " << min_jobs_on_thread_;
            _ros << " wait_count_ = " << wait_count_;
            return _ros;
        }
    } statistic_;
#endif
public:
    template <class JobHandleFnc, typename... Args>
    WorkPool(
        const size_t                 _start_wkr_cnt,
        const WorkPoolConfiguration& _cfg,
        JobHandleFnc                 _job_handler_fnc,
        Args... _args)
        : config_(_cfg)
        , running_(true)
        , thr_cnt_(0)
    {
        doStart(
            std::integral_constant<size_t, function_traits<JobHandleFnc>::arity>(),
            _start_wkr_cnt,
            _job_handler_fnc,
            _args...);
    }

    template <class JobHandleFnc, typename... Args>
    WorkPool(
        const WorkPoolConfiguration& _cfg,
        JobHandleFnc                 _job_handler_fnc,
        Args... _args)
        : config_(_cfg)
        , running_(true)
        , thr_cnt_(0)
    {
        doStart(
            std::integral_constant<size_t, function_traits<JobHandleFnc>::arity>(),
            0,
            _job_handler_fnc,
            _args...);
    }

    ~WorkPool()
    {
        doStop();
        solid_dbg(workpool_logger, Verbose, this);
    }

    template <class JT>
    void push(const JT& _jb);

    template <class JT>
    void push(JT&& _jb);

private:
    bool pop(Job& _rjob);

    void doStart(size_t _start_wkr_cnt, WorkerFactoryT&& _uworker_factory_fnc);

    void doStop();

    template <class JobHandlerFnc>
    void doStart(
        std::integral_constant<size_t, 1>,
        const size_t  _start_wkr_cnt,
        JobHandlerFnc _job_handler_fnc);

    template <class JobHandlerFnc, typename... Args>
    void doStart(
        std::integral_constant<size_t, 2>,
        const size_t  _start_wkr_cnt,
        JobHandlerFnc _job_handler_fnc,
        Args... _args);
}; //WorkPool

//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
template <class JT>
void WorkPool<Job, QNBits>::push(const JT& _jb)
{
    const size_t qsz     = job_q_.push(_jb, config_.max_job_queue_size_);
    const size_t thr_cnt = thr_cnt_.load();

    if (thr_cnt < config_.max_worker_count_ && qsz > thr_cnt) {
        std::lock_guard<std::mutex> lock(thr_mtx_);
        if (qsz > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
            thr_vec_.emplace_back(worker_factory_fnc_());
            ++thr_cnt_;
            solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
        }
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
template <class JT>
void WorkPool<Job, QNBits>::push(JT&& _jb)
{
    const size_t qsz     = job_q_.push(std::move(_jb), config_.max_job_queue_size_);
    const size_t thr_cnt = thr_cnt_.load();

    if (thr_cnt < config_.max_worker_count_ && qsz > thr_cnt) {
        std::lock_guard<std::mutex> lock(thr_mtx_);
        if (qsz > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
            thr_vec_.emplace_back(worker_factory_fnc_());
            ++thr_cnt_;
            solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
        }
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
bool WorkPool<Job, QNBits>::pop(Job& _rjob)
{
    return job_q_.pop(_rjob, running_, config_.max_job_queue_size_);
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
void WorkPool<Job, QNBits>::doStart(size_t _start_wkr_cnt, WorkerFactoryT&& _uworker_factory_fnc)
{
    solid_dbg(workpool_logger, Verbose, this << " start " << _start_wkr_cnt << " " << config_.max_worker_count_ << ' ' << config_.max_job_queue_size_);
    if (_start_wkr_cnt > config_.max_worker_count_) {
        _start_wkr_cnt = config_.max_worker_count_;
    }

    worker_factory_fnc_ = std::move(_uworker_factory_fnc);

    {
        std::unique_lock<std::mutex> lock(thr_mtx_);

        for (size_t i = 0; i < _start_wkr_cnt; ++i) {
            thr_vec_.emplace_back(worker_factory_fnc_());
            solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
        }
        thr_cnt_ += _start_wkr_cnt;
    }
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
void WorkPool<Job, QNBits>::doStop()
{
    bool expect = true;

    if (running_.compare_exchange_strong(expect, false)) {
    } else {
        SOLID_ASSERT(false); //doStop called multiple times
        return;
    }
    {
        std::unique_lock<std::mutex> lock(thr_mtx_);
        job_q_.wake();

        for (auto& t : thr_vec_) {
            t.join();
        }
        thr_vec_.clear();
    }
    {
#ifdef SOLID_HAS_STATISTICS
        solid_dbg(workpool_logger, Statistic, "Workpool " << this << " statistic:" << this->statistic_);
        const size_t max_jobs_in_queue = config_.max_job_queue_size_ == static_cast<size_t>(-1) ? config_.max_job_queue_size_ : config_.max_job_queue_size_ + JobQueueT::nodeSize();
        SOLID_CHECK(statistic_.max_jobs_in_queue_ <= max_jobs_in_queue, "statistic_.max_jobs_in_queue_ = " << statistic_.max_jobs_in_queue_ << " <= config_.max_job_queue_size_ = " << max_jobs_in_queue);
        SOLID_CHECK(statistic_.max_worker_count_ <= config_.max_worker_count_, "statistic_.max_worker_count_ = " << statistic_.max_worker_count_ << " <= config_.max_worker_count_ = " << config_.max_worker_count_);
#endif
    }
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
template <class JobHandlerFnc>
void WorkPool<Job, QNBits>::doStart(
    std::integral_constant<size_t, 1>,
    const size_t  _start_wkr_cnt,
    JobHandlerFnc _job_handler_fnc)
{
    WorkerFactoryT worker_factory_fnc = [_job_handler_fnc, this]() {
        return std::thread(
            [this](JobHandlerFnc _job_handler_fnc) {
                Job      job;
                uint64_t job_count = 0;

                while (pop(job)) {
                    _job_handler_fnc(job);
                    solid_statistic_inc(job_count);
                }

                solid_dbg(workpool_logger, Verbose, this << " worker exited after handling " << job_count << " jobs");
                solid_statistic_max(statistic_.max_jobs_on_thread_, job_count);
                solid_statistic_min(statistic_.min_jobs_on_thread_, job_count);
            },
            _job_handler_fnc);
    };

    doStart(_start_wkr_cnt, std::move(worker_factory_fnc));
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
template <class JobHandlerFnc, typename... Args>
void WorkPool<Job, QNBits>::doStart(
    std::integral_constant<size_t, 2>,
    const size_t  _start_wkr_cnt,
    JobHandlerFnc _job_handler_fnc,
    Args... _args)
{
    WorkerFactoryT worker_factory_fnc = [_job_handler_fnc, this, _args...]() {
        return std::thread(
            [this](JobHandlerFnc _job_handler_fnc, Args&&... _args) {
                using SecondArgumentT = typename function_traits<JobHandlerFnc>::template argument<1>;
                using ContextT        = typename std::remove_cv<typename std::remove_reference<SecondArgumentT>::type>::type;

                ContextT ctx{std::forward<Args>(_args)...};
                Job      job;
                uint64_t job_count = 0;

                while (pop(job)) {
                    _job_handler_fnc(job, std::ref(ctx));
                    solid_statistic_inc(job_count);
                }

                solid_dbg(workpool_logger, Verbose, this << " worker exited after handling " << job_count << " jobs");
                solid_statistic_max(statistic_.max_jobs_on_thread_, job_count);
                solid_statistic_min(statistic_.min_jobs_on_thread_, job_count);
            },
            _job_handler_fnc,
            _args...);
    };

    doStart(_start_wkr_cnt, std::move(worker_factory_fnc));
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

struct FunctionWorkPool : WorkPool<std::function<void()>> {
    using WorkPoolT = WorkPool<std::function<void()>>;
    FunctionWorkPool(
        const size_t                 _start_wkr_cnt,
        const WorkPoolConfiguration& _cfg)
        : WorkPoolT(
              _start_wkr_cnt,
              _cfg,
              [](std::function<void()>& _rfnc) {
                  _rfnc();
              })
    {
    }

    FunctionWorkPool(
        const WorkPoolConfiguration& _cfg)
        : WorkPoolT(
              0,
              _cfg,
              [](std::function<void()>& _rfnc) {
                  _rfnc();
              })
    {
    }
};

} //namespace solid
