// solid/utility/workpool_mutex.hpp
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
#include "solid/system/statistic.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/functiontraits.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/workpool_base.hpp"
namespace solid {

extern const LoggerT workpool_logger;
namespace locking {
//-----------------------------------------------------------------------------
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

template <typename Job, size_t QNBits = 10, typename Base = impl::WorkPoolBase>
class WorkPool : protected Base {
    using ThisT          = WorkPool<Job, QNBits, Base>;
    using WorkerFactoryT = std::function<std::thread()>;
    using ThreadVectorT  = std::vector<std::thread>;
    using JobQueueT      = Queue<Job, QNBits>;
    using AtomicBoolT    = std::atomic<bool>;

    WorkPoolConfiguration   config_;
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
    static constexpr size_t node_capacity = bits_to_count(QNBits);

    WorkPool()
        : config_()
        , running_(false)
        , thr_cnt_(0)
    {
    }

    template <class JobHandleFnc, typename... Args>
    WorkPool(
        const WorkPoolConfiguration& _cfg,
        const size_t                 _start_wkr_cnt,
        JobHandleFnc                 _job_handler_fnc,
        Args&&... _args)
        : config_()
        , running_(false)
        , thr_cnt_(0)
    {
        doStart(
            _cfg,
            _start_wkr_cnt,
            _job_handler_fnc,
            std::forward<Args>(_args)...);
    }

    template <class JobHandleFnc, typename... Args>
    void start(
        const WorkPoolConfiguration& _cfg,
        const size_t                 _start_wkr_cnt,
        JobHandleFnc                 _job_handler_fnc,
        Args&&... _args)
    {
        doStart(
            _cfg,
            _start_wkr_cnt,
            _job_handler_fnc,
            std::forward<Args>(_args)...);
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

    template <class JT>
    bool tryPush(const JT& _jb);

    template <class JT>
    bool tryPush(JT&& _jb);

    void dumpStatistics() const;

    void stop()
    {
        doStop();
    }

private:
    bool doWaitJob(std::unique_lock<std::mutex>& _lock);

    bool pop(Job& _rjob);

    void doStop();

    template <class JobHandlerFnc, typename... Args>
    void doStart(
        const WorkPoolConfiguration& _cfg,
        const size_t                 _start_wkr_cnt,
        JobHandlerFnc                _job_handler_fnc,
        Args&&... _args);
}; //WorkPool

//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
template <class JT>
void WorkPool<Job, QNBits, Base>::push(const JT& _jb)
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
        }

        sig_cnd_.notify_one();

        const size_t thr_cnt = thr_cnt_.load();

        if (thr_cnt < config_.max_worker_count_ && qsz > thr_cnt) {
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
template <typename Job, size_t QNBits, typename Base>
template <class JT>
void WorkPool<Job, QNBits, Base>::push(JT&& _jb)
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
        }

        sig_cnd_.notify_one();

        const size_t thr_cnt = thr_cnt_.load();

        if (thr_cnt < config_.max_worker_count_ && qsz > thr_cnt) {
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
template <typename Job, size_t QNBits, typename Base>
template <class JT>
bool WorkPool<Job, QNBits, Base>::tryPush(const JT& _jb)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            if (job_q_.size() < config_.max_job_queue_size_) {
            } else {
                return false;
            }

            job_q_.push(_jb);
            qsz = job_q_.size();
        }

        sig_cnd_.notify_one();

        const size_t thr_cnt = thr_cnt_.load();

        if (thr_cnt < config_.max_worker_count_ && qsz > thr_cnt) {
            std::lock_guard<std::mutex> lock(thr_mtx_);
            if (qsz > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
                thr_vec_.emplace_back(worker_factory_fnc_());
                ++thr_cnt_;
                solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
            }
        }
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
    return true;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
template <class JT>
bool WorkPool<Job, QNBits, Base>::tryPush(JT&& _jb)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            if (job_q_.size() < Base::config_.max_job_queue_size_) {
            } else {
                return false;
            }

            job_q_.push(std::move(_jb));
            qsz = job_q_.size();
        }

        sig_cnd_.notify_one();

        const size_t thr_cnt = thr_cnt_.load();

        if (thr_cnt < config_.max_worker_count_ && qsz > thr_cnt) {
            std::lock_guard<std::mutex> lock(thr_mtx_);
            if (qsz > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
                thr_vec_.emplace_back(worker_factory_fnc_());
                ++thr_cnt_;
                solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
            }
        }
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
    return true;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
bool WorkPool<Job, QNBits, Base>::doWaitJob(std::unique_lock<std::mutex>& _lock)
{
    while (job_q_.empty() && running_.load(std::memory_order_relaxed)) {
        sig_cnd_.wait(_lock);
    }
    return !job_q_.empty();
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
bool WorkPool<Job, QNBits, Base>::pop(Job& _rjob)
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
template <typename Job, size_t QNBits, typename Base>
void WorkPool<Job, QNBits, Base>::doStop()
{
    bool expect = true;

    if (running_.compare_exchange_strong(expect, false)) {
    } else {
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
    dumpStatistics();
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
template <class JobHandlerFnc, typename... Args>
void WorkPool<Job, QNBits, Base>::doStart(
    const WorkPoolConfiguration& _cfg,
    size_t                       _start_wkr_cnt,
    JobHandlerFnc                _job_handler_fnc,
    Args&&... _args)
{

    auto lambda = [_job_handler_fnc, this, _args...]() {
        return std::thread(
            [this](JobHandlerFnc _job_handler_fnc, Args&&... _args) {
                uint64_t job_count = 0;
                Job      job;

                while (pop(job)) {
                    _job_handler_fnc(job, std::forward<Args>(_args)...);
                    solid_statistic_inc(job_count);
                }

                solid_dbg(workpool_logger, Verbose, this << " worker exited after handling " << job_count << " jobs");
                solid_statistic_max(statistic_.max_jobs_on_thread_, job_count);
                solid_statistic_min(statistic_.min_jobs_on_thread_, job_count);
            },
            _job_handler_fnc, _args...);
    };

    solid_dbg(workpool_logger, Verbose, this << " start " << _start_wkr_cnt << " " << config_.max_worker_count_ << ' ' << config_.max_job_queue_size_);
    if (_start_wkr_cnt > config_.max_worker_count_) {
        _start_wkr_cnt = config_.max_worker_count_;
    }

    bool expect = false;

    if (running_.compare_exchange_strong(expect, true)) {
        config_             = _cfg;
        worker_factory_fnc_ = lambda;

        {
            std::unique_lock<std::mutex> lock(thr_mtx_);

            for (size_t i = 0; i < _start_wkr_cnt; ++i) {
                thr_vec_.emplace_back(worker_factory_fnc_());
                solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
            }
            thr_cnt_ += _start_wkr_cnt;
        }
    }
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
void WorkPool<Job, QNBits, Base>::dumpStatistics() const
{
#ifdef SOLID_HAS_STATISTICS
    solid_log(workpool_logger, Statistic, "Workpool " << this << " statistic:" << this->statistic_);
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
} //namespace locking
} //namespace solid
