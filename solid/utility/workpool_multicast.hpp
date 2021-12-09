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
#include "solid/utility/innerlist.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"
#include "solid/utility/workpool_base.hpp"

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
    std::atomic<size_t>   job_count_;

    WorkPoolMulticastStatistic();

    std::ostream& print(std::ostream& _ros) const override;
};

template <class WP, class ContextStub>
class SynchronizationContext {
    WP*          pwp_  = nullptr;
    ContextStub* pctx_ = nullptr;
    template <typename Job, typename MCastJob, size_t QNBits, typename Base>
    friend class WorkPoolMulticast;

    SynchronizationContext(WP* _pwp, ContextStub* _pctx)
        : pwp_(_pwp)
        , pctx_(_pctx)
    {
    }

public:
    SynchronizationContext() {}

    SynchronizationContext(const SynchronizationContext& _other) = delete;

    SynchronizationContext(SynchronizationContext&& _other)
        : pwp_(_other.pwp_)
        , pctx_(_other.pctx_)
    {
        _other.pwp_  = nullptr;
        _other.pctx_ = nullptr;
    }

    ~SynchronizationContext()
    {
        clear();
    }

    SynchronizationContext& operator=(const SynchronizationContext& _other) = delete;

    SynchronizationContext& operator=(SynchronizationContext&& _other)
    {
        if (this != &_other) {
            clear();
            pwp_         = _other.pwp_;
            pctx_        = _other.pctx_;
            _other.pwp_  = nullptr;
            _other.pctx_ = nullptr;
        }
        return *this;
    }

    void clear()
    {
        if (!empty()) {
            pwp_->release(pctx_);
            pctx_ = nullptr;
            pwp_  = nullptr;
        }
    }

    bool empty() const
    {
        return pwp_ == nullptr;
    }

    template <class JT>
    void push(JT&& _jb)
    {
        solid_check(!empty());
        pwp_->doPush(std::forward<JT>(_jb), pctx_);
    }

    template <class JT>
    bool tryPush(JT&& _jb)
    {
        solid_check(!empty());
        return pwp_->doTryPush(std::forward<JT>(_jb), pctx_);
    }
};

/*
NOTE:
    - a node (index) from the JobDqT can be in either:
        job_list_free_
        job_list_
        ContextStub->job_list_
*/

template <typename Job, typename MCastJob = Job, size_t QNBits = 10, typename Base = impl::WorkPoolBase>
class WorkPoolMulticast : protected Base {
    struct ContextStub;

    struct JobNode : inner::Node<1> {
        Job          job_;
        ContextStub* pcontext_ = nullptr;

        template <class J>
        JobNode(J&& _rj, ContextStub* _pcontext = nullptr)
            : job_(std::forward<J>(_rj))
            , pcontext_(_pcontext)
        {
        }
    };

    using ThisT          = WorkPoolMulticast<Job, MCastJob, QNBits, Base>;
    using ThreadVectorT  = std::vector<std::thread>;
    using JobDqT         = std::deque<JobNode>;
    using JobInnerListT  = inner::List<JobDqT, 0>;
    using MCastJobQueueT = Queue<MCastJob, QNBits>;
    using AtomicBoolT    = std::atomic<bool>;

    struct ContextStub {
        size_t        use_count_ = 0;
        JobInnerListT job_list_;

        ContextStub(JobDqT& _rjob_dq)
            : job_list_(_rjob_dq)
        {
        }
    };

    using ContextDqT    = std::deque<ContextStub>;
    using ContextStackT = Stack<ContextStub*, QNBits>;
    using ContextQueueT = Queue<ContextStub*, QNBits>;

    WorkPoolConfiguration   config_;
    AtomicBoolT             running_;
    JobDqT                  job_dq_;
    JobInnerListT           job_list_;
    JobInnerListT           job_list_free_;
    MCastJobQueueT          mcast_job_q_;
    ContextDqT              context_dq_;
    ContextQueueT           pending_context_q_;
    ContextStackT           free_context_stack_;
    ThreadVectorT           thr_vec_;
    std::mutex              mtx_;
    std::condition_variable sig_cnd_;
    size_t                  mcast_current_exec_id_    = 0;
    size_t                  mcast_current_exec_count_ = 0;
#ifdef SOLID_HAS_STATISTICS
    WorkPoolMulticastStatistic statistic_;
#endif
    template <class WP, class ContextStub>
    friend class SynchronizationContext;

public:
    static constexpr size_t node_capacity = bits_to_count(QNBits);
    using SynchronizationContextT         = SynchronizationContext<ThisT, ContextStub>;

    WorkPoolMulticast()
        : config_()
        , running_(false)
        , job_list_(job_dq_)
        , job_list_free_(job_dq_)
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
        , job_list_(job_dq_)
        , job_list_free_(job_dq_)
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

    SynchronizationContextT createSynchronizationContext()
    {

        return SynchronizationContextT{this, doCreateContext()};
    }

    template <class JT>
    void push(JT&& _jb)
    {
        doPush(std::forward<JT>(_jb), nullptr);
    }

    template <class JT>
    bool tryPush(JT&& _jb)
    {
        return doTryPush(std::forward<JT>(_jb), nullptr);
    }

    template <class JT>
    void pushAll(JT&& _jb);

    template <class JT>
    bool tryPushAll(JT&& _jb);

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
        MCastJob mcast_job_;
        Job*     pjob_             = nullptr;
        bool     has_mcast_        = false;
        bool     has_mcast_update_ = false;

        //used only on pop
        ContextStub* pcontext_      = nullptr;
        size_t       mcast_exec_id_ = 0;
        size_t       job_index_     = InvalidIndex();
        size_t       mcast_index_   = InvalidIndex();
    };

    void release(ContextStub* _pctx)
    {
        if (_pctx) {
            std::lock_guard<std::mutex> lock(mtx_);
            --_pctx->use_count_;
            if (_pctx->job_list_.empty()) {
                free_context_stack_.push(_pctx);
                sig_cnd_.notify_all();
            }
        }
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

    template <class JT>
    void doPush(JT&& _jb, ContextStub* _pctx);

    template <class JT>
    bool doTryPush(JT&& _jb, ContextStub* _pctx);

    ContextStub* doCreateContext()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (free_context_stack_.empty()) {
            context_dq_.emplace_back(job_dq_);
            context_dq_.back().use_count_ = 1;
            return &context_dq_.back();
        } else {
            auto pctx = free_context_stack_.top();
            free_context_stack_.pop();
            pctx->use_count_ = 1;
            return pctx;
        }
    }
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
        if (pop_context.has_mcast_) {
            ++mcast_job_count;
            _mcast_job_handler_fnc(pop_context.mcast_job_, std::forward<Args>(_args)...);
        }
        if (pop_context.pjob_) {
            _job_handler_fnc(*pop_context.pjob_, std::forward<Args>(_args)...);
            solid_statistic_inc(job_count);
        }
    }

    solid_dbg(workpool_logger, Verbose, this << " worker exited after handling " << job_count << " jobs");
    solid_statistic_max(statistic_.max_jobs_on_thread_, job_count);
    solid_statistic_min(statistic_.min_jobs_on_thread_, job_count);
    solid_statistic_add(statistic_.job_count_, job_count);
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
    bool                         should_notify = false;
    std::unique_lock<std::mutex> lock(mtx_);

    _rcontext.has_mcast_        = false;
    _rcontext.has_mcast_update_ = false;

    if (_rcontext.pcontext_) {
        job_list_free_.pushBack(_rcontext.pcontext_->job_list_.popFront());

        if (!_rcontext.pcontext_->job_list_.empty()) {
            pending_context_q_.push(_rcontext.pcontext_);
        } else if (_rcontext.pcontext_->use_count_ == 0) {
            free_context_stack_.push(_rcontext.pcontext_);
            should_notify = true;
        }
        _rcontext.pcontext_ = nullptr;
        _rcontext.pjob_     = nullptr;
    } else if (_rcontext.pjob_) {
        job_list_free_.pushBack(_rcontext.job_index_);
        _rcontext.pjob_      = nullptr;
        _rcontext.job_index_ = InvalidIndex();
    }

    while (true) {
        bool should_wait = true;

        //tryPopMCastJob
        if (!mcast_job_q_.empty()) {

            if (_rcontext.mcast_exec_id_ != mcast_current_exec_id_) {
                ++_rcontext.mcast_exec_id_;
                _rcontext.mcast_job_        = mcast_job_q_.front(); //copy
                _rcontext.has_mcast_        = true;
                _rcontext.has_mcast_update_ = (mcast_current_exec_count_ == 0);
                should_wait                 = false;

                ++mcast_current_exec_count_;
                if (mcast_current_exec_count_ == config_.max_worker_count_) {
                    //done with current mcast item
                    mcast_job_q_.pop();
                    mcast_current_exec_count_ = 0;
                    should_notify             = true;
                    if (!mcast_job_q_.empty()) {
                        ++mcast_current_exec_id_;
                    }
                }
            }
        }

        //tryPopJobFromContext
        if (!pending_context_q_.empty()) {
            _rcontext.pcontext_ = pending_context_q_.front();
            pending_context_q_.pop();
            should_wait     = false;
            _rcontext.pjob_ = &_rcontext.pcontext_->job_list_.front().job_;
        } else if (!job_list_.empty()) {
            //tryPopJob
            auto& rnode = job_list_.front();

            if (rnode.pcontext_ == nullptr) {
                _rcontext.pjob_      = &job_list_.front().job_;
                _rcontext.job_index_ = job_list_.popFront();
                should_wait          = false;
            } else if (rnode.pcontext_->job_list_.empty()) {
                --rnode.pcontext_->use_count_;
                _rcontext.pcontext_ = rnode.pcontext_;
                _rcontext.pjob_     = &job_list_.front().job_;
                rnode.pcontext_->job_list_.pushBack(job_list_.popFront());
                should_wait = false;
            } else {
                //we have a job on a currently running synchronization context
                rnode.pcontext_->job_list_.pushBack(job_list_.popFront());
                --rnode.pcontext_->use_count_;
                should_wait = false; //don't want to keep the lock for too long
            }
        }

        if (should_notify) {
            sig_cnd_.notify_all();
        }

        if (should_wait) {
            if (
                !running_.load(std::memory_order_relaxed) && mcast_job_q_.empty() && job_list_.empty() && pending_context_q_.empty() && context_dq_.size() == free_context_stack_.size() //all contexts have been released
            ) {
                break;
            }
            Base::wait(sig_cnd_, lock);
        } else {
            return true;
        }
    }
    return false;
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
bool WorkPoolMulticast<Job, MCastJob, QNBits, Base>::doTryPush(JT&& _jb, ContextStub* _pctx)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            if (job_list_.size() < config_.max_job_queue_size_) {
            } else {
                return false;
            }

            if (job_list_free_.empty()) {
                job_dq_.emplace_back(std::forward<JT>(_jb), _pctx);
                job_list_.pushBack(job_dq_.size() - 1);
            } else {
                const auto idx     = job_list_free_.popFront();
                job_dq_[idx].job_  = std::forward<JT>(_jb);
                job_dq_[idx].pctx_ = _pctx;
                job_list_.pushBack(idx);
            }
            qsz = job_list_.size();
        }

        sig_cnd_.notify_all(); //using all because sig_cnd_ is used for job_q_ size limitation
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
    return true;
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
void WorkPoolMulticast<Job, MCastJob, QNBits, Base>::doPush(JT&& _jb, ContextStub* _pctx)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            if (job_list_.size() < config_.max_job_queue_size_) {
            } else {
                do {
                    Base::wait(sig_cnd_, lock);
                } while (job_list_.size() >= config_.max_job_queue_size_);
            }

            if (job_list_free_.empty()) {
                job_dq_.emplace_back(std::forward<JT>(_jb), _pctx);
                job_list_.pushBack(job_dq_.size() - 1);
            } else {
                const auto idx         = job_list_free_.popFront();
                job_dq_[idx].job_      = std::forward<JT>(_jb);
                job_dq_[idx].pcontext_ = _pctx;
                job_list_.pushBack(idx);
            }
            if (_pctx) {
                ++_pctx->use_count_;
            }
            qsz = job_list_.size();
        }

        sig_cnd_.notify_all(); //using all because sig_cnd_ is used for job_q_ size limitation
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
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
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
bool WorkPoolMulticast<Job, MCastJob, QNBits, Base>::tryPushAll(JT&& _jb)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            if (mcast_job_q_.size() < config_.max_job_queue_size_) {
            } else {
                return false;
            }

            mcast_job_q_.push(std::move(_jb));
            qsz = mcast_job_q_.size();
            if (qsz == 1) {
                ++mcast_current_exec_id_;
            }
        }

        sig_cnd_.notify_all(); //using all because sig_cnd_ is used for job_q_ size limitation
    }
    solid_statistic_max(statistic_.max_mcast_jobs_in_queue_, qsz);
    return true;
}
//-----------------------------------------------------------------------------
} //namespace solid
