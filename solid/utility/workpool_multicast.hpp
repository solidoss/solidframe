// solid/utility/workpool_multicast.hpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
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

template <typename Job, typename MCastJob = Job, size_t QNBits = 10, typename Base = impl::WorkPoolBase>
class WorkPoolMulticast {

    using ThisT          = WorkPoolMulticast<Job, MCastJob, QNBits, Base>;
    using WorkerFactoryT = std::function<std::thread()>;
    using ThreadVectorT  = std::vector<std::thread>;
    using JobQueueT      = Queue<Job, QNBits>;
    using MCastJobQueueT = Queue<MCastJob, QNBits>;
    using AtomicBoolT    = std::atomic<bool>;

    WorkPoolConfiguration   config_;
    AtomicBoolT             running_;
    WorkerFactoryT          worker_factory_fnc_;
    JobQueueT               job_q_;
    MCastJobQueueT          mcast_job_q_;
    ThreadVectorT           thr_vec_;
    std::mutex              mtx_;
    std::condition_variable sig_cnd_;
#ifdef SOLID_HAS_STATISTICS
    struct Statistic : solid::Statistic {
        std::atomic<size_t>   max_jobs_in_queue_;
        std::atomic<size_t>   max_mcast_jobs_in_queue_;
        std::atomic<uint64_t> max_jobs_on_thread_;
        std::atomic<uint64_t> min_jobs_on_thread_;

        Statistic()
            : max_jobs_in_queue_(0)
            , max_mcast_jobs_in_queue_(0)
            , max_jobs_on_thread_(0)
            , min_jobs_on_thread_(-1)
        {
        }

        std::ostream& print(std::ostream& _ros) const override
        {
            _ros << " max_jobs_in_queue_ = " << max_jobs_in_queue_;
            _ros << " max_mcast_jobs_in_queue_ = " << max_mcast_jobs_in_queue_;
            _ros << " max_jobs_on_thread_ = " << max_jobs_on_thread_;
            _ros << " min_jobs_on_thread_ = " << min_jobs_on_thread_;
            return _ros;
        }
    } statistic_;
#endif
public:
    static constexpr size_t node_capacity = bits_to_count(QNBits);

    WorkPoolMulticast()
        : config_()
        , running_(false)
    {
    }

    template <class JobHandleFnc, class MCastJobHandleFnc, class MCastSyncUpdateFnc, typename... Args>
    WorkPoolMulticast(
        const WorkPoolConfiguration& _cfg,
        JobHandleFnc                 _job_handler_fnc,
        MCastJobHandleFnc            _mcast_job_handler_fnc,
        MCastSyncUpdateFnc           _mcast_sync_update_fnc,
        Args&&... _args)
        : config_()
        , running_(false)
    {
        doStart(
            _cfg,
            _job_handler_fnc,
            _mcast_job_handler_fnc,
            _mcast_sync_update_fnc,
            std::forward<Args>(_args)...);
    }

    template <class JobHandleFnc, class MCastJobHandleFnc, class MCastSyncUpdateFnc, typename... Args>
    void start(
        const WorkPoolConfiguration& _cfg,
        JobHandleFnc                 _job_handler_fnc,
        MCastJobHandleFnc            _mcast_job_handler_fnc,
        MCastSyncUpdateFnc           _mcast_sync_update_fnc,
        Args&&... _args)
    {
        doStart(
            _cfg,
            _job_handler_fnc,
            _mcast_job_handler_fnc,
            _mcast_sync_update_fnc,
            std::forward<Args>(_args)...);
    }

    ~WorkPoolMulticast()
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
    
    template <class JT>
    void pushAllSync(const JT& _jb);

    template <class JT>
    void pushAllSync(JT&& _jb);
    
    
    void dumpStatistics() const;

    void stop()
    {
        doStop();
    }

private:
    bool doWaitJob(std::unique_lock<std::mutex>& _lock);

    bool pop(Job& _rjob);

    void doStop();

    template <class JobHandlerFnc, class MCastJobHandleFnc, class MCastSyncUpdateFnc, typename... Args>
    void doStart(
        const WorkPoolConfiguration& _cfg,
        JobHandlerFnc                _job_handler_fnc,
        MCastJobHandleFnc            _mcast_job_handler_fnc,
        MCastSyncUpdateFnc           _mcast_sync_update_fnc,
        Args&&... _args);
};

//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JobHandlerFnc, class MCastJobHandleFnc, class MCastSyncUpdateFnc, typename... Args>
void WorkPoolMulticast<Job, MCastJob, QNBits, Base>::doStart(
    const WorkPoolConfiguration& _cfg,
    JobHandlerFnc                _job_handler_fnc,
    MCastJobHandleFnc            _mcast_job_handler_fnc,
    MCastSyncUpdateFnc           _mcast_sync_update_fnc,
    Args&&... _args)
{
    
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
void WorkPoolMulticast<Job, MCastJob, QNBits, Base>::doStop()
{
    bool expect = true;

    if (running_.compare_exchange_strong(expect, false)) {
    } else {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mtx_);
        sig_cnd_.notify_all();

        for (auto& t : thr_vec_) {
            t.join();
        }
        thr_vec_.clear();
    }
    dumpStatistics();
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
void WorkPoolMulticast<Job, MCastJob, QNBits, Base>::push(const JT& _jb){
    
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
void WorkPoolMulticast<Job, MCastJob, QNBits, Base>::push(JT&& _jb){
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
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
void WorkPoolMulticast<Job, MCastJob, QNBits, Base>::pushAllSync(const JT& _jb){
    
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
void WorkPoolMulticast<Job, MCastJob, QNBits, Base>::pushAllSync(JT&& _jb){
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            if (mcast_job_q_.size() < config_.max_job_queue_size_) {
            } else {
                do {
                    sig_cnd_.wait(lock);
                } while (mcast_job_q_.size() >= config_.max_job_queue_size_);
            }

            mcast_job_q_.push(std::move(_jb));
            qsz = mcast_job_q_.size();
        }

        sig_cnd_.notify_one();
    }
    solid_statistic_max(statistic_.max_mcast_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
void WorkPoolMulticast<Job, MCastJob, QNBits, Base>::dumpStatistics() const
{
#ifdef SOLID_HAS_STATISTICS
    solid_log(workpool_logger, Statistic, "WorkPoolMulticast " << this << " statistic:" << this->statistic_);
#endif
}
} //namespace solid
