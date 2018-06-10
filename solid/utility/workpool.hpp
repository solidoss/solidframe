// solid/utility/workpool.hpp
//
// Copyright (c) 2007, 2008, 2018 Valentin Palade (vipalade @ gmail . com)
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
        std::atomic<size_t> pop_pos_;
        std::atomic<size_t> use_cnt_;
        std::atomic<Node*>  next_;
        uint32_t            unique_;
        uint8_t             data_[node_size * sizeof(T)];

        Node()
            : push_pos_(0)
            , pop_pos_(0)
            , use_cnt_(0)
            , next_(nullptr)
        {
        }

        void clear()
        {
            push_pos_ = 0;
            pop_pos_  = 0;
            next_     = nullptr;
        }

        T& item(const size_t _i)
        {
            return reinterpret_cast<T*>(data_)[_i];
        }
    };

    std::atomic<size_t>     size_;
    std::atomic<Node*>      ppush_; //push end
    std::atomic<Node*>      ppop_; //pop end
    Node*                   pempty_; //empty nodes
    std::mutex              push_mtx_;
    std::mutex              pop_mtx_;
    std::condition_variable push_cnd_;
    std::condition_variable pop_cnd_;
    std::atomic<size_t>     push_wait_cnt_;
    std::atomic<size_t>     pop_wait_cnt_;

public:
    Queue()
        : size_(0)
        , ppush_(nullptr)
        , ppop_(nullptr)
        , pempty_(nullptr)
        , push_wait_cnt_(0)
        , pop_wait_cnt_(0)
    {
        ppush_ = ppop_ = popEmptyNode();
    }

    ~Queue()
    {
        Node* pn = ppop_.load();
        while (pn) {
            Node* p = pn;
            pn      = pn->next_.load();
            delete p;
        }

        pn = pempty_;
        while (pn) {
            Node* p = pn;
            pn      = pn->next_.load();
            delete p;
        }
        pempty_ = nullptr;
    }

    size_t push(const T& _rt, const size_t _max_queue_size)
    {
    }

    size_t push(T&& _rt, const size_t _max_queue_size)
    {
        do {
            Node* const  pn  = ppush_.load();
            const size_t pos = pn->push_pos_.fetch_add(1);

            if (pos < node_size) {
                new (pn->data_ + (pos * sizeof(T))) T(std::move(_rt));

                const size_t sz = size_.fetch_add(1) + 1;

                if (sz < pop_wait_cnt_.load()) {
                    std::unique_lock<std::mutex> lock(pop_mtx_);
                    pop_cnd_.notify_one();
                }

                return sz;
            } else if (pos >= node_size) {
                //overflow
                std::unique_lock<std::mutex> lock(push_mtx_);

                const auto unique = pn->unique_; //try to prevent ABA problem

                while (size_.load() >= _max_queue_size) {
                    push_wait_cnt_.fetch_add(1);
                    push_cnd_.wait(lock);
                    push_wait_cnt_.fetch_sub(1);
                }

                if (ppush_.load() == pn && pn->unique_ == unique) {
                    ++pn->unique_;
                    Node* pempty = popEmptyNode();
                    pn->next_    = pempty;
                    ppush_       = pempty;
                }
            }
        } while (true);
    }

    bool pop(T& _rt, std::atomic<bool>& _running, const size_t _max_queue_size)
    {
        Node* pn = ppop_.load();

        do {
            pn->use_cnt_.fetch_add(1);

            const size_t pos = pn->pop_pos_.fetch_add(1); //reserve waiting spot
            size_t       crt_size;
            if (pos < pn->push_pos_.load() && pos < node_size && (crt_size = size_.load()) != 0) {
                //size_ != 0 ensures that in case push_pos and pop_pos point to the same
                //position, we read the item after it was written
                _rt             = std::move(pn->item(pos));
                const size_t sz = size_.fetch_sub(1) - 1;
                if (sz < _max_queue_size && (_max_queue_size - sz) < push_wait_cnt_.load()) {
                    std::lock_guard<std::mutex> lock(push_mtx_);
                    //we need the lock here so we are certain that push threads
                    // are either waiting on push_cnd_ or have not yet read size_ value
                    push_cnd_.notify_one();
                }
                pn->use_cnt_.fetch_sub(1);
                return true;
            } else if (pos < node_size) {
                // wait on reserved pop position
                {
                    std::unique_lock<std::mutex> lock(pop_mtx_);

                    while (pos >= pn->push_pos_.load() && size_.load() && _running.load()) {
                        pop_wait_cnt_.fetch_add(1);
                        pop_cnd_.wait(lock);
                        pop_wait_cnt_.fetch_sub(1);
                    }
                }

                if (pos < pn->push_pos_.load()) {
                    _rt = std::move(pn->item(pos));

                    const size_t sz = size_.fetch_sub(1) - 1;
                    if (sz < _max_queue_size && (_max_queue_size - sz) < push_wait_cnt_.load()) {
                        std::lock_guard<std::mutex> lock(push_mtx_);
                        //we need the lock here so we are certain that push threads
                        // are either waiting on push_cnd_ or have not yet read size_ value
                        push_cnd_.notify_one();
                    }
                    pn->use_cnt_.fetch_sub(1);
                    return true;
                } else {
                    //no pending items and !running
                    pn->use_cnt_.fetch_sub(1);
                    return false;
                }
            } else {
                // try to move to the next node
                Node *pnn = pn->next_.load();
                if(pnn && ppop_.compare_exchange_strong(pn, pnn)){
                    //TODO: save pn
                    if(pn->use_cnt_.fetch_sub(1) == 0){
                        std::lock_guard<std::mutex> lock(push_mtx_);
                        pushEmptyNode(pn);
                        pn = nullptr;
                    }else{
                        //node still in use
                    }
                    pn = pnn;
                }else if(pnn == nullptr){
                    std::unique_lock<std::mutex> lock(pop_mtx_);

                    while (size_.load() && _running.load()) {
                        pop_wait_cnt_.fetch_add(1);
                        pop_cnd_.wait(lock);
                        pop_wait_cnt_.fetch_sub(1);
                    }
                    if(size_.load() || !_running.load()){
                        pn->use_cnt_.fetch_sub(1);
                        pn = ppop_.load();
                    }else{
                        pn->use_cnt_.fetch_sub(1);
                        return false;
                    }
                }
            }
        } while (true);
    }
#if 0
    bool pop(T& _rt, std::atomic<bool>& _running, const size_t _max_queue_size)
    {
        Node* pn = ppop_.load();
        
        do {
            pn->use_cnt_.fetch_add(1);
            
            size_t pos = pn->pop_pos_.fetch_add(1); //reserve waiting spot
            
            if (pos < pn->push_pos_.load() && pos < node_size) {
                _rt             = std::move(pn->item(pos));
                const size_t sz = size_.fetch_sub(1) - 1;
                if (sz < _max_queue_size && (_max_queue_size - sz) < push_wait_cnt_.load()) {
                    std::lock_guard<std::mutex> lock(push_mtx_);
                    push_wait_size_ = sz;
                    push_cnd_.notify_one();
                }
                pn->use_cnt_.fetch_sub(1);
                return true;
            } else if (pos < node_size) {
                {
                    std::unique_lock<std::mutex> lock(pop_mtx_);
                    while (pos >= pn->push_pos_.load() && _running.load()) {
                        pop_cnd_.wait(lock);
                    }
                }
                if (pos < pn->push_pos_.load()) {
                    _rt = std::move(pn->item(pos));

                    const size_t sz = size_.fetch_sub(1) - 1;
                    if (sz < _max_queue_size && (_max_queue_size - sz) < push_wait_cnt_.load()) {
                        std::lock_guard<std::mutex> lock(push_mtx_);
                        push_wait_size_ = sz;
                        push_cnd_.notify_one();
                    }
                    pn->use_cnt_.fetch_sub(1);
                    return true;
                } else {
                    pn->use_cnt_.fetch_sub(1);
                    return false;
                }
            } else {
                Node* pnn = pn->next_.load();
                if (pn->use_cnt_.fetch_sub(1) == 1) {
                    std::lock_guard<std::mutex> lock(push_mtx_);
                    pushEmptyNode(pn);
                    pn = nullptr;
                }
                if (pnn == nullptr) {
                    std::unique_lock<std::mutex> lock(pop_mtx_);
                    while (size_.load() == 0 && _running.load()) {
                        pop_cnd_.wait(lock);
                    }
                    pn = ppop_.load();
                }else{
                    pn = pnn;
                    ppop_.store(pnn);//TODO???
                }
            }
        } while (true);
    }
#endif
    void wake()
    {
        pop_cnd_.notify_all();
    }

private:
    Node* popEmptyNode()
    {
        Node* pn;
        if (pempty_) {
            pn      = pempty_;
            pempty_ = pempty_->next_.load();
        } else {
            pn = new Node;
        }
        return pn;
    }

    void pushEmptyNode(Node* _pn)
    {
        _pn->clear();
        _pn->next_ = pempty_;
        pempty_    = _pn;
    }
};
} //namespace thread_safe

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

struct WorkPoolConfiguration {
    size_t max_worker_count_;
    size_t max_job_queue_size_;

    explicit WorkPoolConfiguration(
        const size_t _max_worker_count   = std::thread::hardware_concurrency(),
        const size_t _max_job_queue_size = std::numeric_limits<size_t>::max())
        : max_worker_count_(_max_worker_count)
        , max_job_queue_size_(_max_job_queue_size)
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

template <typename Job>
class WorkPool {
    using ThisT          = WorkPool<Job>;
    using WorkerFactoryT = std::function<std::thread()>;
    using ThreadVectorT  = std::vector<std::thread>;
    using JobQueueT      = thread_safe::Queue<Job>;
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
template <typename Job>
template <class JT>
void WorkPool<Job>::push(const JT& _jb)
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
template <typename Job>
template <class JT>
void WorkPool<Job>::push(JT&& _jb)
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
template <typename Job>
bool WorkPool<Job>::pop(Job& _rjob)
{
    return job_q_.pop(_rjob, running_, config_.max_job_queue_size_);
}
//-----------------------------------------------------------------------------
template <typename Job>
void WorkPool<Job>::doStart(size_t _start_wkr_cnt, WorkerFactoryT&& _uworker_factory_fnc)
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
template <typename Job>
void WorkPool<Job>::doStop()
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
        solid_dbg(workpool_logger, Verbose, "Workpool " << this << " statistic:" << this->statistic_);
#endif
    }
}
//-----------------------------------------------------------------------------
template <typename Job>
template <class JobHandlerFnc>
void WorkPool<Job>::doStart(
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
template <typename Job>
template <class JobHandlerFnc, typename... Args>
void WorkPool<Job>::doStart(
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
