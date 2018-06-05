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

template <typename Job>
class WorkPool {
    enum States {
        StoppedE,
        StoppingE,
        RunningE
    };
    using ThisT          = WorkPool<Job>;
    using WorkerFactoryT = std::function<std::thread()>;
    using ThreadVectorT  = std::vector<std::thread>;
    using JobQueueT      = Queue<Job>;

    WorkerFactoryT          worker_factory_fnc_;
    JobQueueT               job_q_;
    ThreadVectorT           thr_vec_;
    std::mutex              mtx_;
    States                  state_;
    std::condition_variable sig_cnd_;
    WorkPoolConfiguration   config_;
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
    WorkPool()
    {
    }

    ~WorkPool()
    {
        stop(true);
    }

    template <class JT>
    void push(const JT& _jb);

    template <class JT>
    void push(JT&& _jb);

    template <class I>
    void push(I _i, const I& _end)
    {
    }

    template <class JobHandleFnc, typename... Args>
    void start(
        const size_t                 _start_wkr_cnt,
        const WorkPoolConfiguration& _cfg,
        JobHandleFnc                 _job_handler_fnc,
        Args... _args)
    {
        doStart(
            std::integral_constant<size_t, function_traits<JobHandleFnc>::arity>(),
            _cfg,
            _start_wkr_cnt,
            _job_handler_fnc,
            _args...);
    }

    template <class JobHandleFnc, typename... Args>
    void start(
        const WorkPoolConfiguration& _cfg,
        JobHandleFnc                 _job_handler_fnc,
        Args... _args)
    {
        doStart(
            std::integral_constant<size_t, function_traits<JobHandleFnc>::arity>(),
            _cfg,
            0,
            _job_handler_fnc,
            _args...);
    }

    void stop(bool _wait = true);

private:
    size_t doWaitJob(std::unique_lock<std::mutex>& _lock);

    bool pop(Job& _rjob);

    void doStart(const WorkPoolConfiguration& _cfg, size_t _start_wkr_cnt, WorkerFactoryT&& _uworker_factory_fnc);

    bool doStop(std::unique_lock<std::mutex>& _lock, const bool _wait, ThreadVectorT& _rthr_vec);

    template <class JobHandlerFnc>
    void doStart(
        std::integral_constant<size_t, 1>,
        const WorkPoolConfiguration& _cfg,
        const size_t                 _start_wkr_cnt,
        JobHandlerFnc                _job_handler_fnc);

    template <class JobHandlerFnc, typename... Args>
    void doStart(
        std::integral_constant<size_t, 2>,
        const WorkPoolConfiguration& _cfg,
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
        std::unique_lock<std::mutex> lock(mtx_);

        if (job_q_.size() < config_.max_job_queue_size_) {
        } else {
            do {
                sig_cnd_.wait(lock);
            } while (job_q_.size() >= config_.max_job_queue_size_);
        }

        job_q_.push(_jb);
        sig_cnd_.notify_one();

        if (job_q_.size() > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
            thr_vec_.emplace_back(worker_factory_fnc_());
            solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
        }

        qsz = job_q_.size();
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
        std::unique_lock<std::mutex> lock(mtx_);

        if (job_q_.size() < config_.max_job_queue_size_) {
        } else {
            do {
                sig_cnd_.wait(lock);
            } while (job_q_.size() >= config_.max_job_queue_size_);
        }

        job_q_.push(std::move(_jb));
        sig_cnd_.notify_one();

        if (job_q_.size() > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
            thr_vec_.emplace_back(worker_factory_fnc_());
            solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
        }

        qsz = job_q_.size();
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
}

//-----------------------------------------------------------------------------
template <typename Job>
void WorkPool<Job>::stop(bool _wait)
{
    solid_dbg(workpool_logger, Verbose, this << " stop " << _wait);
    ThreadVectorT thr_vec;
    {
        std::unique_lock<std::mutex> lock(mtx_);
        doStop(lock, _wait, thr_vec);
    }
    for (auto& t : thr_vec) {
        t.join();
    }
    if (thr_vec.size()) {
#ifdef SOLID_HAS_STATISTICS
        solid_dbg(workpool_logger, Verbose, "Workpool " << this << " statistic:" << this->statistic_);
#endif
    }
}

//-----------------------------------------------------------------------------
template <typename Job>
size_t WorkPool<Job>::doWaitJob(std::unique_lock<std::mutex>& _lock)
{
    while (job_q_.empty() && state_ == RunningE) {
        sig_cnd_.wait(_lock);
    }
    return job_q_.size();
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
void WorkPool<Job>::doStart(const WorkPoolConfiguration& _cfg, size_t _start_wkr_cnt, WorkerFactoryT&& _uworker_factory_fnc)
{
    solid_dbg(workpool_logger, Verbose, this << " start " << _start_wkr_cnt << " " << _cfg.max_worker_count_ << ' ' << _cfg.max_job_queue_size_);
    if (_start_wkr_cnt > _cfg.max_worker_count_) {
        _start_wkr_cnt = _cfg.max_worker_count_;
    }
    ThreadVectorT thr_vec;
    {
        std::unique_lock<std::mutex> lock(mtx_);

        worker_factory_fnc_ = std::move(_uworker_factory_fnc);

        if (state_ != StoppedE) {
            doStop(lock, true, thr_vec);
        }

        state_ = RunningE;

        for (size_t i = 0; i < _start_wkr_cnt; ++i) {
            thr_vec_.emplace_back(worker_factory_fnc_());
            solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
        }
    }
    for (auto& t : thr_vec) {
        t.join();
    }
}
//-----------------------------------------------------------------------------
template <typename Job>
bool WorkPool<Job>::doStop(std::unique_lock<std::mutex>& _lock, const bool _wait, ThreadVectorT& _rthr_vec)
{
    if (state_ == StoppedE)
        return false;
    state_ = StoppingE;

    sig_cnd_.notify_all();

    if (!_wait)
        return false;

    _rthr_vec = std::move(thr_vec_);

    thr_vec_.clear();
    state_ = StoppedE;
    return true;
}
//-----------------------------------------------------------------------------
template <typename Job>
template <class JobHandlerFnc>
void WorkPool<Job>::doStart(
    std::integral_constant<size_t, 1>,
    const WorkPoolConfiguration& _cfg,
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

    doStart(_cfg, _start_wkr_cnt, std::move(worker_factory_fnc));
}
//-----------------------------------------------------------------------------
template <typename Job>
template <class JobHandlerFnc, typename... Args>
void WorkPool<Job>::doStart(
    std::integral_constant<size_t, 2>,
    const WorkPoolConfiguration& _cfg,
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

    doStart(_cfg, _start_wkr_cnt, std::move(worker_factory_fnc));
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

struct FunctionWorkPool : WorkPool<std::function<void()>> {
    using WorkPoolT = WorkPool<std::function<void()>>;
    void start(
        const size_t                 _start_wkr_cnt,
        const WorkPoolConfiguration& _cfg)
    {
        WorkPoolT::start(
            _start_wkr_cnt,
            _cfg,
            [](std::function<void()>& _rfnc) {
                _rfnc();
            });
    }

    void start(
        const WorkPoolConfiguration& _cfg)
    {
        WorkPoolT::start(
            0,
            _cfg,
            [](std::function<void()>& _rfnc) {
                _rfnc();
            });
    }
};

} //namespace solid
