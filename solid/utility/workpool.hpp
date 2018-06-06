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
namespace thread_safe{

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
public:
    void push(const T &_rt, const size_t _max_queue_size){
        
    }
    
    void push(T &&_rt, const size_t _max_queue_size){
        
    }
    
};
}//namespace thread_safe

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
    using JobQueueT      = /*thread_safe::*/Queue<Job>;
    using AtomicBoolT   = std::atomic<bool>;
    
    const WorkPoolConfiguration   config_;
    AtomicBoolT             running_;
    std::atomic<size_t>     thr_cnt_;
    WorkerFactoryT          worker_factory_fnc_;
    JobQueueT               job_q_;
    ThreadVectorT           thr_vec_;
    std::mutex              mtx_;
    std::mutex              thr_mtx_;
    std::condition_variable sig_cnd_;
#ifdef SOLID_HAS_STATISTICS
    struct Statistic : solid::Statistic {
        std::atomic<size_t>   max_worker_count_;
        std::atomic<size_t>   max_jobs_in_queue_;
        std::atomic<uint64_t> max_jobs_on_thread_;
        std::atomic<uint64_t> min_jobs_on_thread_;

        Statistic()
            : max_worker_count_(0)
            , max_jobs_in_queue_(0)
            , max_jobs_on_thread_(0)
            , min_jobs_on_thread_(-1)
        {
        }

        std::ostream& print(std::ostream& _ros) const override
        {
            _ros << " max_worker_count_ = " << max_worker_count_;
            _ros << " max_jobs_in_queue_ = " << max_jobs_in_queue_;
            _ros << " max_jobs_on_thread_ = " << max_jobs_on_thread_;
            _ros << " min_jobs_on_thread_ = " << min_jobs_on_thread_;
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
        Args... _args):config_(_cfg), running_(true), thr_cnt_(0)
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
        Args... _args):config_(_cfg), running_(true), thr_cnt_(0)
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
    bool doWaitJob(std::unique_lock<std::mutex>& _lock);

    bool pop(Job& _rjob);

    void doStart(size_t _start_wkr_cnt, WorkerFactoryT&& _uworker_factory_fnc);

    void doStop();

    template <class JobHandlerFnc>
    void doStart(
        std::integral_constant<size_t, 1>,
        const size_t                 _start_wkr_cnt,
        JobHandlerFnc                _job_handler_fnc);

    template <class JobHandlerFnc, typename... Args>
    void doStart(
        std::integral_constant<size_t, 2>,
        const size_t                 _start_wkr_cnt,
        JobHandlerFnc                _job_handler_fnc,
        Args... _args);
}; //WorkPool

//-----------------------------------------------------------------------------
template <typename Job>
template <class JT>
void WorkPool<Job>::push(const JT& _jb)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            if (job_q_.size() < config_.max_job_queue_size_) {
            } else {
                do {
                    sig_cnd_.wait(lock);
                } while (job_q_.size() >= config_.max_job_queue_size_);
            }

            job_q_.push(_jb);
            qsz = job_q_.size();
            sig_cnd_.notify_one();
        }
        
        const size_t thr_cnt = thr_cnt_.load();
        
        if(thr_cnt < config_.max_worker_count_ && qsz > thr_cnt){
            std::lock_guard<std::mutex> lock(thr_mtx_);
            if (qsz > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
                thr_vec_.emplace_back(worker_factory_fnc_());
                ++thr_cnt_;
                solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
            }
        }
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job>
template <class JT>
void WorkPool<Job>::push(JT&& _jb)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            if (job_q_.size() < config_.max_job_queue_size_) {
            } else {
                do {
                    sig_cnd_.wait(lock);
                } while (job_q_.size() >= config_.max_job_queue_size_);
            }

            job_q_.push(std::move(_jb));
            qsz = job_q_.size();
            sig_cnd_.notify_one();
        }
        
        const size_t thr_cnt = thr_cnt_.load();
        
        if(thr_cnt < config_.max_worker_count_ && qsz > thr_cnt){
            std::lock_guard<std::mutex> lock(thr_mtx_);
            if (qsz > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
                thr_vec_.emplace_back(worker_factory_fnc_());
                ++thr_cnt_;
                solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
            }
        }
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job>
bool WorkPool<Job>::doWaitJob(std::unique_lock<std::mutex>& _lock)
{
    while (job_q_.empty() && running_.load(std::memory_order_relaxed)) {
        sig_cnd_.wait(_lock);
    }
    return !job_q_.empty();
}
//-----------------------------------------------------------------------------
template <typename Job>
bool WorkPool<Job>::pop(Job& _rjob)
{

    std::unique_lock<std::mutex> lock(mtx_);

    if (doWaitJob(lock)) {
        const bool was_full = job_q_.size() == config_.max_job_queue_size_;
        _rjob               = std::move(job_q_.front());
        job_q_.pop();

        if (was_full)
            sig_cnd_.notify_all();
        return true;
    }
    return false;
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
    
    if(running_.compare_exchange_strong(expect, false)){
    }else{
        SOLID_ASSERT(false);//doStop called multiple times
        return;
    }
    {
        std::unique_lock<std::mutex> lock(thr_mtx_);
        sig_cnd_.notify_all();

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
    const size_t                 _start_wkr_cnt,
    JobHandlerFnc                _job_handler_fnc)
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
    const size_t                 _start_wkr_cnt,
    JobHandlerFnc                _job_handler_fnc,
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
        const WorkPoolConfiguration& _cfg):WorkPoolT(
            _start_wkr_cnt,
            _cfg,
            [](std::function<void()>& _rfnc) {
                _rfnc();
            })
    {
    }

    FunctionWorkPool(
        const WorkPoolConfiguration& _cfg): WorkPoolT(
            0,
            _cfg,
            [](std::function<void()>& _rfnc) {
                _rfnc();
            })
    {
    }
};

} //namespace solid
