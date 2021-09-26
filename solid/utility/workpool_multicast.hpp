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
    using ThreadVectorT  = std::vector<std::thread>;
    using JobQueueT      = Queue<Job, QNBits>;
    using MCastJobQueueT = Queue<MCastJob, QNBits>;
    using AtomicBoolT    = std::atomic<bool>;

    WorkPoolConfiguration   config_;
    AtomicBoolT             running_;
    JobQueueT               job_q_;
    MCastJobQueueT          mcast_job_q_;
    ThreadVectorT           thr_vec_;
    std::mutex              mtx_;
    std::condition_variable sig_cnd_;
    size_t                  mcast_current_fetch_id_ = 0;
    size_t                  mcast_current_fetch_count_ = 0;
    size_t                  mcast_current_exec_id_ = 0;
    size_t                  mcast_current_exec_count_ = 0;
#ifdef SOLID_HAS_STATISTICS
    struct Statistic : solid::Statistic {
        std::atomic<size_t>   max_jobs_in_queue_;
        std::atomic<size_t>   max_mcast_jobs_in_queue_;
        std::atomic<uint64_t> max_jobs_on_thread_;
        std::atomic<uint64_t> min_jobs_on_thread_;
        std::atomic<size_t>   max_mcast_jobs_on_thread_;
        std::atomic<size_t>   min_mcast_jobs_on_thread_;
        std::atomic<size_t>   mcast_updates_;
        
        

        Statistic()
            : max_jobs_in_queue_(0)
            , max_mcast_jobs_in_queue_(0)
            , max_jobs_on_thread_(0)
            , min_jobs_on_thread_(-1)
            , max_mcast_jobs_on_thread_(0)
            , min_mcast_jobs_on_thread_(-1)
            , mcast_updates_(0)
        {
        }

        std::ostream& print(std::ostream& _ros) const override
        {
            _ros << " max_jobs_in_queue_ = " << max_jobs_in_queue_;
            _ros << " max_mcast_jobs_in_queue_ = " << max_mcast_jobs_in_queue_;
            _ros << " max_jobs_on_thread_ = " << max_jobs_on_thread_;
            _ros << " min_jobs_on_thread_ = " << min_jobs_on_thread_;
            _ros << " max_mcast_jobs_on_thread_ = " << max_mcast_jobs_on_thread_;
            _ros << " min_mcast_jobs_on_thread_ = " << min_mcast_jobs_on_thread_;
            _ros << " mcast_updates_ = " << mcast_updates_;
            
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
    struct PopContext{
        Job      job_;
        MCastJob mcast_job_;
        bool     has_job_ = false;
        bool     has_mcast_execute_ = false;
        bool     has_mcast_update_ = false;
        
        //used only on pop
        size_t   mcast_fetch_id_ = 0;
        size_t   mcast_exec_id_ = 0;
    };

    bool pop(PopContext &_rcontext);

    void doStop();

    template <class JobHandlerFnc, class MCastJobHandleFnc, class MCastSyncUpdateFnc, typename... Args>
    void doStart(
        const WorkPoolConfiguration& _cfg,
        JobHandlerFnc                _job_handler_fnc,
        MCastJobHandleFnc            _mcast_job_handler_fnc,
        MCastSyncUpdateFnc           _mcast_sync_update_fnc,
        Args&&... _args);
    
    template <class JobHandlerFnc, class MCastJobHandleFnc, class MCastSyncUpdateFnc, typename... Args>
    void doRun(
        JobHandlerFnc                _job_handler_fnc,
        MCastJobHandleFnc            _mcast_job_handler_fnc,
        MCastSyncUpdateFnc           _mcast_sync_update_fnc,
        Args... _args);
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
    bool expect = false;

    if (!running_.compare_exchange_strong(expect, true)) {
        return;
    }
    
    config_             = _cfg;

    {
        std::lock_guard<std::mutex> lock(mtx_);

        for (size_t i = 0; i < config_.max_worker_count_; ++i) {
            thr_vec_.emplace_back(
                std::thread{
                    [this](JobHandlerFnc _job_handler_fnc, MCastJobHandleFnc _mcast_job_handler_fnc, MCastSyncUpdateFnc _mcast_sync_update_fnc, Args &&... _args) {
                        doRun(_job_handler_fnc, _mcast_job_handler_fnc, _mcast_sync_update_fnc, std::forward<Args>(_args)...);
                    },
                    _job_handler_fnc, _mcast_job_handler_fnc, _mcast_sync_update_fnc, _args...
                }
            );
        }
    }
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JobHandlerFnc, class MCastJobHandleFnc, class MCastSyncUpdateFnc, typename... Args>
void WorkPoolMulticast<Job, MCastJob, QNBits, Base>::doRun(
    JobHandlerFnc                _job_handler_fnc,
    MCastJobHandleFnc            _mcast_job_handler_fnc,
    MCastSyncUpdateFnc           _mcast_sync_update_fnc,
    Args... _args)
{
    uint64_t job_count = 0;
    uint64_t mcast_job_count = 0;
    uint64_t mcast_update_count = 0;
    
    PopContext pop_context;

    while (pop(pop_context)) {
        //call sync_update first because _mcast_job_handler_fnc might ::move
        //the mcast_job
        if(pop_context.has_mcast_update_){
            _mcast_sync_update_fnc(std::cref(pop_context.mcast_job_), std::forward<Args>(_args)...);
            ++mcast_update_count;
        }
        if(pop_context.has_mcast_execute_){
            ++mcast_job_count;
            _mcast_job_handler_fnc(pop_context.mcast_job_, std::forward<Args>(_args)...);
        }
        if(pop_context.has_job_){
            _job_handler_fnc(pop_context.job_, std::forward<Args>(_args)...);
            solid_statistic_inc(job_count);
        }
    }

    solid_dbg(workpool_logger, Verbose, this << " worker exited after handling " << job_count << " jobs");
    solid_statistic_max(statistic_.max_jobs_on_thread_, job_count);
    solid_statistic_min(statistic_.min_jobs_on_thread_, job_count);
    solid_statistic_max(statistic_.max_mcast_jobs_on_thread_, mcast_job_count);
    solid_statistic_min(statistic_.min_mcast_jobs_on_thread_, mcast_job_count);
    solid_statistic_add(statistic_.mcast_updates_, mcast_update_count);
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
bool WorkPoolMulticast<Job, MCastJob, QNBits, Base>::pop(PopContext &_rcontext){
    std::unique_lock<std::mutex> lock(mtx_);
    
    _rcontext.has_job_ = false;
    _rcontext.has_mcast_execute_ = false;
    _rcontext.has_mcast_update_ = false;
    
    while(true) {
        bool should_notify = false;
        bool should_wait = true;
        if(!job_q_.empty()){
            should_notify  = job_q_.size() == config_.max_job_queue_size_;
            _rcontext.job_ = std::move(job_q_.front());
            _rcontext.has_job_ = true;
            should_wait = false;
            should_notify = true;
            job_q_.pop();
        }
        if(!mcast_job_q_.empty()){
            if(_rcontext.mcast_fetch_id_ != mcast_current_fetch_id_){
                ++_rcontext.mcast_fetch_id_;
                ++mcast_current_fetch_count_;
                if(mcast_current_fetch_count_ == config_.max_worker_count_){
                    _rcontext.has_mcast_update_ = true;
                    should_wait = false;
                    should_notify = true;
                    mcast_current_exec_id_ = mcast_current_fetch_id_;
                }
            }
            
            if(mcast_current_fetch_count_ == config_.max_worker_count_ && _rcontext.mcast_exec_id_ != mcast_current_exec_id_){
                ++_rcontext.mcast_exec_id_;
                _rcontext.has_mcast_execute_ = true;
                should_wait = false;
                _rcontext.mcast_job_ = mcast_job_q_.front();//copy
                ++mcast_current_exec_count_;
                if(mcast_current_exec_count_ == config_.max_worker_count_){
                    //done with current mcast item
                    mcast_job_q_.pop();
                    mcast_current_exec_count_ = 0;
                    mcast_current_fetch_count_ = 0;
                    should_notify = true;
                    if(!mcast_job_q_.empty()){
                        ++mcast_current_fetch_id_;
                        should_notify = true;
                    }
                }
            }
        }

        if (should_notify){
            sig_cnd_.notify_all();
        }
        if (should_wait){
            if(!running_.load(std::memory_order_relaxed) && mcast_job_q_.empty()){
                break;
            }
            sig_cnd_.wait(lock);
        }
        return true;
    }
    return false;
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
            if(qsz == 1){
                ++mcast_current_fetch_id_;
            }
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
