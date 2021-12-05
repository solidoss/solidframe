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
#include "solid/utility/innerlist.hpp"

namespace solid {
extern const LoggerT workpool_logger;

struct WorkPoolMulticastStatistic : solid::Statistic {
    std::atomic<size_t>   max_jobs_in_queue_;
    std::atomic<size_t>   max_mcast_jobs_in_queue_;
    std::atomic<uint64_t> max_jobs_on_thread_;
    std::atomic<uint64_t> min_jobs_on_thread_;
    std::atomic<uint64_t> max_mcast_jobs_on_thread_;
    std::atomic<uint64_t> min_mcast_jobs_on_thread_;
    std::atomic<size_t>   mcast_updates_;

    WorkPoolMulticastStatistic();

    std::ostream& print(std::ostream& _ros) const override;
};

template <class WP, class ContextStub>
class SynchronizationContext{
    WP              *pwp_ = nullptr;
    ContextStub     *pctx_ = nullptr;
    template <typename Job, typename MCastJob, size_t QNBits, typename Base>
    friend class WorkPoolMulticast;
    
    SynchronizationContext(WP *_pwp, ContextStub *_pctx):pwp_(_pwp), pctx_(_pctx){}
    
public:
    SynchronizationContext(){}
    
    SynchronizationContext(SynchronizationContext &&_other): pwp_(_other.pwp_), pctx_(_other.pctx_){
        _other.pwp_ = nullptr;
        _other.pctx_ = nullptr;
    }
    
    ~SynchronizationContext(){
        clear();
    }
    
    SynchronizationContext& operator=(SynchronizationContext &&_other){
        if(this != &_other){
            clear();
            pwp_ = _other.pwp_;
            pctx_ = _other.pctx_;
        }
        return *this;
    }
    
    void clear(){
        if(!empty()){
            pwp_->release(pctx_);
        }
    }
    
    bool empty()const{
        return pwp_ == nullptr;
    }
};

/*
NOTE:
    - a node (index) from the JobDqT can be in either:
        empty_node_list_
        node_list_
        ContextStub->node_list_
*/

template <typename Job, typename MCastJob = Job, size_t QNBits = 10, typename Base = impl::WorkPoolBase>
class WorkPoolMulticast : protected Base {
    struct ContextStub;
    
    struct JobNode: inner::Node<1>{
        Job job_;
        ContextStub *pctx_ = nullptr;
    };
    
    using ThisT          = WorkPoolMulticast<Job, MCastJob, QNBits, Base>;
    using ThreadVectorT  = std::vector<std::thread>;
    using JobDqT         = std::deque<JobNode>;
    using InnerListT     = inner::List<JobDqT, 0>;
    using MCastJobQueueT = Queue<MCastJob, QNBits>;
    using AtomicBoolT    = std::atomic<bool>;
    
    struct ContextStub{
        size_t      use_count_ = 0;
        InnerListT  node_list_;
    };
    
    WorkPoolConfiguration   config_;
    AtomicBoolT             running_;
    JobDqT                  job_dq_;
    InnerListT              empty_node_list_;
    InnerListT              node_list_;
    MCastJobQueueT          mcast_job_q_;
    ThreadVectorT           thr_vec_;
    std::mutex              mtx_;
    std::condition_variable sig_cnd_;
    size_t                  mcast_current_exec_id_    = 0;
    size_t                  mcast_current_exec_count_ = 0;
    bool                    synch_lock_               = false;
#ifdef SOLID_HAS_STATISTICS
    WorkPoolMulticastStatistic statistic_;
#endif
    
public:
    static constexpr size_t node_capacity = bits_to_count(QNBits);
    using SynchronizationContextT = SynchronizationContext<ThisT, ContextStub>;
    
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
    
    SynchronizationContextT createSynchronizationContext(){
        return SynchronizationContextT{this, nullptr};
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
    void pushAll(const JT& _jb);

    template <class JT>
    void pushAll(JT&& _jb);

#ifdef SOLID_HAS_STATISTICS
    const WorkPoolMulticastStatistic& statistic() const
    {
        return statistic_;
    }
#endif
    void stop()
    {
        doStop();
    }

private:
    struct PopContext {
        Job      job_;
        MCastJob mcast_job_;
        bool     has_job_           = false;
        bool     has_synch_job_     = false;
        bool     has_mcast_execute_ = false;
        bool     has_mcast_update_  = false;

        //used only on pop
        size_t mcast_exec_id_ = 0;
    };
    
    void release(ContextStub *_pctx){
        //TODO:
    }
    
    bool pop(PopContext& _rcontext);

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
        JobHandlerFnc      _job_handler_fnc,
        MCastJobHandleFnc  _mcast_job_handler_fnc,
        MCastSyncUpdateFnc _mcast_sync_update_fnc,
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

    config_ = _cfg;

    {
        std::lock_guard<std::mutex> lock(mtx_);

        for (size_t i = 0; i < config_.max_worker_count_; ++i) {
            thr_vec_.emplace_back(
                std::thread{
                    [this](JobHandlerFnc _job_handler_fnc, MCastJobHandleFnc _mcast_job_handler_fnc, MCastSyncUpdateFnc _mcast_sync_update_fnc, Args&&... _args) {
                        doRun(_job_handler_fnc, _mcast_job_handler_fnc, _mcast_sync_update_fnc, std::forward<Args>(_args)...);
                    },
                    _job_handler_fnc, _mcast_job_handler_fnc, _mcast_sync_update_fnc, _args...});
        }
    }
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JobHandlerFnc, class MCastJobHandleFnc, class MCastSyncUpdateFnc, typename... Args>
void WorkPoolMulticast<Job, MCastJob, QNBits, Base>::doRun(
    JobHandlerFnc      _job_handler_fnc,
    MCastJobHandleFnc  _mcast_job_handler_fnc,
    MCastSyncUpdateFnc _mcast_sync_update_fnc,
    Args... _args)
{
    uint64_t job_count          = 0;
    uint64_t mcast_job_count    = 0;
    uint64_t mcast_update_count = 0;

    PopContext pop_context;

    while (pop(pop_context)) {
        //call sync_update first because _mcast_job_handler_fnc might ::move
        //the mcast_job
        if (pop_context.has_mcast_update_) {
            _mcast_sync_update_fnc(std::cref(pop_context.mcast_job_), std::forward<Args>(_args)...);
            ++mcast_update_count;
        }
        if (pop_context.has_mcast_execute_) {
            ++mcast_job_count;
            _mcast_job_handler_fnc(pop_context.mcast_job_, std::forward<Args>(_args)...);
        }
        if (pop_context.has_job_) {
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
#ifdef SOLID_HAS_STATISTICS
    solid_log(workpool_logger, Statistic, "WorkPoolMulticast " << this << " statistic:" << this->statistic_);
#endif
}

//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
bool WorkPoolMulticast<Job, MCastJob, QNBits, Base>::pop(PopContext& _rcontext)
{
    _rcontext.has_job_           = false;
    _rcontext.has_mcast_execute_ = false;
    _rcontext.has_mcast_update_  = false;

    std::unique_lock<std::mutex> lock(mtx_);

    
    return false;
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
bool WorkPoolMulticast<Job, MCastJob, QNBits, Base>::tryPush(const JT& _jb)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            
        }

        sig_cnd_.notify_all(); //using all because sig_cnd_ is job_q_ size limitation
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
    return true;
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
bool WorkPoolMulticast<Job, MCastJob, QNBits, Base>::tryPush(JT&& _jb)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            
        }

        sig_cnd_.notify_all(); //using all because sig_cnd_ is job_q_ size limitation
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
    return true;
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
void WorkPoolMulticast<Job, MCastJob, QNBits, Base>::push(const JT& _jb)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            
        }

        sig_cnd_.notify_all(); //using all because sig_cnd_ is job_q_ size limitation
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
void WorkPoolMulticast<Job, MCastJob, QNBits, Base>::push(JT&& _jb)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            
        }

        sig_cnd_.notify_all(); //using all because sig_cnd_ is job_q_ size limitation
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
void WorkPoolMulticast<Job, MCastJob, QNBits, Base>::pushAll(const JT& _jb)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            if (mcast_job_q_.size() < config_.max_job_queue_size_) {
            } else {
                do {
                    Base::wait(sig_cnd_, lock);
                } while (mcast_job_q_.size() >= config_.max_job_queue_size_);
            }

            mcast_job_q_.push(_jb);
            qsz = mcast_job_q_.size();
            if (qsz == 1) {
                ++mcast_current_exec_id_;
            }
        }

        sig_cnd_.notify_all(); //using all because sig_cnd_ is job_q_ size limitation
    }
    solid_statistic_max(statistic_.max_mcast_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
void WorkPoolMulticast<Job, MCastJob, QNBits, Base>::pushAll(JT&& _jb)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            if (mcast_job_q_.size() < config_.max_job_queue_size_) {
            } else {
                do {
                    Base::wait(sig_cnd_, lock);
                } while (mcast_job_q_.size() >= config_.max_job_queue_size_);
            }

            mcast_job_q_.push(std::move(_jb));
            qsz = mcast_job_q_.size();
            if (qsz == 1) {
                ++mcast_current_exec_id_;
            }
        }

        sig_cnd_.notify_all(); //using all because sig_cnd_ is job_q_ size limitation
    }
    solid_statistic_max(statistic_.max_mcast_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
} //namespace solid
