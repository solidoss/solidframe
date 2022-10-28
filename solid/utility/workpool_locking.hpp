// solid/utility/workpool_locking.hpp
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
#include "solid/utility/innerlist.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"
#include "solid/utility/workpool_base.hpp"
namespace solid {

extern const LoggerT workpool_logger;
namespace locking {

namespace impl {

template <class T>
class Stack {
    T* phead_ = nullptr;

public:
    void push(T* _pt)
    {
        _pt->pnext_ = phead_;
        phead_      = _pt;
    }

    T* pop()
    {
        auto* pold = phead_;
        if (pold) {
            phead_ = pold->pnext_;
        }
        return pold;
    }

    void clear()
    {
        phead_ = nullptr;
    }

    bool empty() const
    {
        return phead_ == nullptr;
    }
};

template <class T>
class Queue {
    T* pfront_ = nullptr;
    T* pback_  = nullptr;

public:
    void push(T* _pt)
    {
        _pt->pnext_ = nullptr;
        if (pback_) {
            pback_->pnext_ = _pt;
            pback_         = _pt;
        } else {
            pfront_ = pback_ = _pt;
        }
    }

    inline T* front() const
    {
        return pfront_;
    }

    T* pop()
    {
        if (pfront_) {
            T* prv  = pfront_;
            pfront_ = pfront_->pnext_;
            if (pfront_ == nullptr) {
                pback_ = nullptr;
            }
            return prv;
        } else {
            return nullptr;
        }
    }
    inline bool empty() const
    {
        return pfront_ == nullptr;
    }
    void clear()
    {
        pfront_ = pback_ = nullptr;
    }
};

} // namespace impl

//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob = void, size_t QNBits = workpool_default_node_capacity_bit_count, typename Base = solid::impl::WorkPoolBase>
class WorkPool;

//-----------------------------------------------------------------------------
// WorkPool with multicast and Synchronization support
//-----------------------------------------------------------------------------
struct WorkPoolMulticastStatistic : solid::Statistic {
    std::atomic<size_t>   max_jobs_in_queue_;
    std::atomic<size_t>   max_mcast_in_queue_;
    std::atomic<uint64_t> max_jobs_on_thread_;
    std::atomic<uint64_t> min_jobs_on_thread_;
    std::atomic<uint64_t> max_mcast_on_thread_;
    std::atomic<uint64_t> min_mcast_on_thread_;
    std::atomic<size_t>   push_job_count_;
    std::atomic<size_t>   push_mcast_count_;
    std::atomic<size_t>   pop_job_count_;
    std::atomic<size_t>   pop_mcast_count_;
    std::atomic<size_t>   max_pop_wait_loop_count_;

    WorkPoolMulticastStatistic();

    std::ostream& print(std::ostream& _ros) const override;
};

template <class WP, class ContextStub>
class SynchronizationContext {
    WP*          pwp_  = nullptr;
    ContextStub* pctx_ = nullptr;
    template <typename Job, typename MCastJob, size_t QNBits, typename Base>
    friend class WorkPool;

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

template <typename Job, typename MCastJob, size_t QNBits, typename Base>
class WorkPool : protected Base {
    struct ContextStub;

    struct JobNode {
        std::aligned_storage_t<sizeof(Job), alignof(Job)> data_;
        ContextStub*                                      pcontext_ = nullptr;
        size_t                                            mcast_id_ = 0;
        JobNode*                                          pnext_    = nullptr;

        void clear()
        {
            pcontext_ = nullptr;
            mcast_id_ = 0;
        }

        Job& job()
        {
            return *std::launder(reinterpret_cast<Job*>(&data_));
        }
        template <class J>
        void job(J&& _rj)
        {
            ::new (&data_) Job(std::forward<J>(_rj));
        }

        void destroy()
        {
            std::destroy_at(std::launder(reinterpret_cast<Job*>(&data_)));
        }
    };

    struct MCastNode {
        std::aligned_storage_t<sizeof(MCastJob), alignof(MCastJob)> data_;
        std::atomic<uint16_t>                                       exec_cnt_;
        uint16_t                                                    release_cnt_ = 0;
        MCastNode*                                                  pnext_       = nullptr;

        MCastNode()
            : exec_cnt_(0)
        {
        }

        MCastJob& job()
        {
            return *std::launder(reinterpret_cast<MCastJob*>(&data_));
        }

        template <class J>
        void job(J&& _rj)
        {
            ::new (&data_) MCastJob(std::forward<J>(_rj));
        }

        void destroy()
        {
            std::destroy_at(std::launder(reinterpret_cast<MCastJob*>(&data_)));
        }

        void release()
        {
            exec_cnt_    = 0;
            release_cnt_ = 0;
        }
    };

    using ThisT         = WorkPool<Job, MCastJob, QNBits, Base>;
    using ThreadVectorT = std::vector<std::thread>;
    using JobDqT        = std::deque<JobNode>;
    using MCastDqT      = std::deque<MCastNode>;
    using JobStackT     = impl::Stack<JobNode>;
    using MCastStackT   = impl::Stack<MCastNode>;
    using JobQueueT     = impl::Queue<JobNode>;
    using MCastQueueT   = impl::Queue<MCastNode>;
    using AtomicBoolT   = std::atomic<bool>;

    struct ContextStub {
        size_t       use_count_ = 0;
        JobQueueT    job_queue_;
        ContextStub* pnext_ = nullptr;
    };

    using ContextDqT    = std::deque<ContextStub>;
    using ContextStackT = impl::Stack<ContextStub>;

    AtomicBoolT             running_;
    JobDqT                  job_dq_;
    JobQueueT               job_queue_;
    size_t                  job_queue_size_ = 0;
    JobStackT               free_job_stack_;
    MCastDqT                mcast_dq_;
    MCastQueueT             mcast_queue_;
    size_t                  mcast_queue_size_ = 0;
    MCastStackT             free_mcast_stack_;
    ContextDqT              context_dq_;
    ContextStackT           free_context_stack_;
    size_t                  free_context_stack_size_ = 0;
    ThreadVectorT           thr_vec_;
    std::mutex              push_mtx_;
    std::condition_variable push_sig_cnd_;
    std::mutex              pop_mtx_;
    std::condition_variable pop_sig_cnd_;
    size_t                  mcast_push_id_ = 0;
#ifdef SOLID_HAS_STATISTICS
    WorkPoolMulticastStatistic statistic_;
#endif
    template <class WP, class ContextStub>
    friend class SynchronizationContext;

public:
    static constexpr size_t node_capacity = bits_to_count(QNBits);
    using SynchronizationContextT         = SynchronizationContext<ThisT, ContextStub>;

    WorkPool()
        : running_(false)
    {
    }

    template <class JobHandleFnc, class MCastJobHandleFnc, typename... Args>
    WorkPool(
        const WorkPoolConfiguration& _cfg,
        JobHandleFnc                 _job_handler_fnc,
        MCastJobHandleFnc            _mcast_job_handler_fnc,
        Args&&... _args)
        : running_(false)
    {
        doStart(
            _cfg,
            _job_handler_fnc,
            _mcast_job_handler_fnc,
            std::forward<Args>(_args)...);
    }

    template <class JobHandleFnc, class MCastJobHandleFnc, typename... Args>
    void start(
        const WorkPoolConfiguration& _cfg,
        JobHandleFnc                 _job_handler_fnc,
        MCastJobHandleFnc            _mcast_job_handler_fnc,
        Args&&... _args)
    {
        doStart(
            _cfg,
            _job_handler_fnc,
            _mcast_job_handler_fnc,
            std::forward<Args>(_args)...);
    }

    ~WorkPool()
    {
        doStop();
        solid_log(workpool_logger, Verbose, this);
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
        MCastNode* pmcast_ = nullptr;
        JobNode*   pjob_   = nullptr;

        // used only on pop
        ContextStub* pcontext_      = nullptr;
        size_t       mcast_exec_id_ = 0;
        MCastNode*   plast_mcast_   = nullptr;
    };

    void release(ContextStub* _pctx)
    {
        if (_pctx) {
            std::lock_guard<std::mutex> lock(pop_mtx_);
            --_pctx->use_count_;
            if (_pctx->job_queue_.empty() && _pctx->use_count_ == 0) {
                free_context_stack_.push(_pctx);
                ++free_context_stack_size_;
                pop_sig_cnd_.notify_all();
            }
        }
    }

    bool pop(PopContext& _rcontext);

    bool doTryDestroyMcastOnPop(PopContext& _rcontext);
    bool doTryDestroyJobOnPop(PopContext& _rcontext, std::unique_lock<std::mutex>& _rlock);

    void doStop();

    template <class JobHandlerFnc, class MCastJobHandleFnc, typename... Args>
    void doStart(
        const WorkPoolConfiguration& _cfg,
        JobHandlerFnc                _job_handler_fnc,
        MCastJobHandleFnc            _mcast_job_handler_fnc,
        Args&&... _args);

    template <class JobHandlerFnc, class MCastJobHandleFnc, typename... Args>
    void doRun(
        JobHandlerFnc     _job_handler_fnc,
        MCastJobHandleFnc _mcast_job_handler_fnc,
        Args&&... _args);

    template <class JT>
    void doPush(JT&& _jb, ContextStub* _pctx);

    template <class JT>
    bool doTryPush(JT&& _jb, ContextStub* _pctx);

    ContextStub* doCreateContext();

    JobNode* doTryAllocateJobNode();
    JobNode* doAllocateJobNode();

    MCastNode* doTryAllocateMCastNode();
    MCastNode* doAllocateMCastNode();
};

//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JobHandlerFnc, class MCastJobHandleFnc, typename... Args>
void WorkPool<Job, MCastJob, QNBits, Base>::doStart(
    const WorkPoolConfiguration& _cfg,
    JobHandlerFnc                _job_handler_fnc,
    MCastJobHandleFnc            _mcast_job_handler_fnc,
    Args&&... _args)
{
    bool expect = false;

    if (!running_.compare_exchange_strong(expect, true)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock1{pop_mtx_};
        std::lock_guard<std::mutex> lock2{push_mtx_}; // always lock pop_mtx before push_mtx

        Base::config_ = _cfg;

        thr_vec_.clear();
        job_dq_.clear();
        job_queue_.clear();
        free_job_stack_.clear();
        mcast_dq_.clear();
        mcast_queue_.clear();
        free_mcast_stack_.clear();

        // mcast sentinel.
        mcast_dq_.emplace_back();
        mcast_dq_.back().exec_cnt_ = Base::config_.max_worker_count_;
        mcast_queue_.push(&mcast_dq_.back());

        for (size_t i = 0; i < Base::config_.max_worker_count_; ++i) {
            thr_vec_.emplace_back(
                std::thread{
                    [this](JobHandlerFnc _job_handler_fnc, MCastJobHandleFnc _mcast_job_handler_fnc, Args&&... _args) {
                        doRun(_job_handler_fnc, _mcast_job_handler_fnc, std::forward<Args>(_args)...);
                    },
                    _job_handler_fnc, _mcast_job_handler_fnc, _args...});
        }
    }
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JobHandlerFnc, class MCastJobHandleFnc, typename... Args>
void WorkPool<Job, MCastJob, QNBits, Base>::doRun(
    JobHandlerFnc     _job_handler_fnc,
    MCastJobHandleFnc _mcast_job_handler_fnc,
    Args&&... _args)
{
    uint64_t job_count   = 0;
    uint64_t mcast_count = 0;

    Base::config_.on_thread_start_fnc_();

    PopContext pop_context;
    pop_context.plast_mcast_ = mcast_queue_.front(); // the sentinel

    while (pop(pop_context)) {
        if (pop_context.pmcast_) {
            _mcast_job_handler_fnc(pop_context.pmcast_->job(), std::forward<Args>(_args)...);
            solid_statistic_inc(mcast_count);
            solid_statistic_inc(statistic_.pop_mcast_count_);
        }
        if (pop_context.pjob_) {
            _job_handler_fnc(pop_context.pjob_->job(), std::forward<Args>(_args)...);
            solid_statistic_inc(job_count);
            solid_statistic_inc(statistic_.pop_job_count_);
        } else if (!pop_context.pmcast_) {
            std::this_thread::yield();
        }
    }

    solid_log(workpool_logger, Verbose, this << " worker exited after handling " << job_count << " jobs and " << mcast_count << " mcasts");

    solid_statistic_max(statistic_.max_jobs_on_thread_, job_count);
    solid_statistic_min(statistic_.min_jobs_on_thread_, job_count);
    solid_statistic_max(statistic_.max_mcast_on_thread_, mcast_count);
    solid_statistic_min(statistic_.min_mcast_on_thread_, mcast_count);

    Base::config_.on_thread_stop_fnc_();
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
void WorkPool<Job, MCastJob, QNBits, Base>::doStop()
{
    bool expect = true;

    if (running_.compare_exchange_strong(expect, false)) {
    } else {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(pop_mtx_);
        pop_sig_cnd_.notify_all();
    }

    for (auto& t : thr_vec_) {
        t.join();
    }
    thr_vec_.clear();
#ifdef SOLID_HAS_STATISTICS
    solid_log(workpool_logger, Statistic, "WorkPool " << this << " statistic:" << this->statistic_);
#endif
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
bool WorkPool<Job, MCastJob, QNBits, Base>::doTryDestroyMcastOnPop(PopContext& _rcontext)
{
    if (_rcontext.pmcast_) {
        auto& rmcast_node     = *_rcontext.pmcast_;
        _rcontext.pmcast_     = nullptr;
        const size_t exec_cnt = rmcast_node.exec_cnt_.fetch_add(1) + 1;
        solid_check(exec_cnt <= Base::config_.max_worker_count_);
        if (exec_cnt == Base::config_.max_worker_count_) {
            rmcast_node.destroy();
            return true;
        }
    }
    return false;
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
bool WorkPool<Job, MCastJob, QNBits, Base>::doTryDestroyJobOnPop(PopContext& _rcontext, std::unique_lock<std::mutex>& _rlock)
{
    bool should_notify = false;
    if (_rcontext.pcontext_) {
        if (_rcontext.pjob_) {
            _rcontext.pjob_->destroy();
            _rcontext.pjob_->clear();

            _rlock = std::unique_lock<std::mutex>{pop_mtx_};

            solid_check(_rcontext.pcontext_->job_queue_.pop() == _rcontext.pjob_);

            {
                std::unique_lock<std::mutex> tmp_lock{push_mtx_};
                free_job_stack_.push(_rcontext.pjob_);
            }

            push_sig_cnd_.notify_one();

            if (!_rcontext.pcontext_->job_queue_.empty()) {
            } else {
                if (_rcontext.pcontext_->use_count_ == 0) {
                    free_context_stack_.push(_rcontext.pcontext_);
                    ++free_context_stack_size_;
                    should_notify = true;
                }
                _rcontext.pcontext_ = nullptr;
            }
        } else {
            _rlock = std::unique_lock<std::mutex>{pop_mtx_};
        }
    } else if (_rcontext.pjob_) {
        _rcontext.pjob_->destroy();
        _rcontext.pjob_->clear();
        {
            std::unique_lock<std::mutex> tmp_lock{push_mtx_};
            free_job_stack_.push(_rcontext.pjob_);
        }
        push_sig_cnd_.notify_one();
        _rlock = std::unique_lock<std::mutex>{pop_mtx_};
    } else {
        _rlock = std::unique_lock<std::mutex>{pop_mtx_};
    }
    return should_notify;
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
bool WorkPool<Job, MCastJob, QNBits, Base>::pop(PopContext& _rcontext)
{
    enum struct RunOptionE {
        Return,
        Wait,
        Continue,
        Exit,
    };

    bool   should_notify       = false;
    size_t wait_loop_count     = 0;
    size_t continue_loop_count = 0;

    // Try destroy the MCast object, before acquiring lock
    should_notify = doTryDestroyMcastOnPop(_rcontext) || should_notify;

    std::unique_lock<std::mutex> lock;

    should_notify = doTryDestroyJobOnPop(_rcontext, lock) || should_notify;

    _rcontext.pjob_ = nullptr;

    while (true) {
        RunOptionE option             = RunOptionE::Wait;
        bool       should_fetch_mcast = _rcontext.mcast_exec_id_ != mcast_push_id_;

        if (_rcontext.pcontext_) {
            const auto front_mcast_id = _rcontext.pcontext_->job_queue_.front()->mcast_id_;
            if (front_mcast_id == _rcontext.mcast_exec_id_ || front_mcast_id == (_rcontext.mcast_exec_id_ + 1)) {
                option             = RunOptionE::Return;
                _rcontext.pjob_    = _rcontext.pcontext_->job_queue_.front();
                should_fetch_mcast = front_mcast_id != _rcontext.mcast_exec_id_;
            } else {
                option = RunOptionE::Continue;
            }
        } else if (!job_queue_.empty()) {
            // tryPopJob
            auto& rnode = *job_queue_.front();

            if (rnode.mcast_id_ == _rcontext.mcast_exec_id_ || rnode.mcast_id_ == (_rcontext.mcast_exec_id_ + 1)) {

                should_notify      = job_queue_size_ == Base::config_.max_job_queue_size_;
                should_fetch_mcast = rnode.mcast_id_ != _rcontext.mcast_exec_id_;

                if (rnode.pcontext_ == nullptr) {
                    _rcontext.pjob_ = job_queue_.pop();
                    --job_queue_size_;
                    option = RunOptionE::Return;
                    // solid_log(workpool_logger, Error, _rcontext.pjob_);
                } else if (rnode.pcontext_->job_queue_.empty()) {
                    --rnode.pcontext_->use_count_;
                    _rcontext.pcontext_ = rnode.pcontext_;
                    _rcontext.pjob_     = job_queue_.pop();
                    _rcontext.pcontext_->job_queue_.push(_rcontext.pjob_);
                    --job_queue_size_;
                    option = RunOptionE::Return;
                    // solid_log(workpool_logger, Error, _rcontext.pjob_);
                } else {
                    // we have a job on a currently running synchronization context
                    rnode.pcontext_->job_queue_.push(job_queue_.pop());
                    --job_queue_size_;
                    --rnode.pcontext_->use_count_;
                    should_fetch_mcast = false;
                    option             = RunOptionE::Continue;
                    // solid_log(workpool_logger, Error, job_list_.size());
                }
            } else {
                option = RunOptionE::Continue;
            }
        } else if (!running_.load(std::memory_order_relaxed) && !should_fetch_mcast && context_dq_.size() == free_context_stack_size_) {
            option = RunOptionE::Exit;
        }

        if (should_fetch_mcast) {
            auto* const pnext_mcast = _rcontext.plast_mcast_->pnext_;
            ++_rcontext.plast_mcast_->release_cnt_;
            if (_rcontext.plast_mcast_->release_cnt_ == Base::config_.max_worker_count_) {
                solid_check(_rcontext.plast_mcast_ == mcast_queue_.front());
                _rcontext.plast_mcast_->release();
                {
                    std::unique_lock<std::mutex> tmp_lock{push_mtx_};
                    free_mcast_stack_.push(mcast_queue_.pop());
                }
                push_sig_cnd_.notify_all();
                --mcast_queue_size_;
            }
            _rcontext.plast_mcast_ = pnext_mcast;

            should_notify = true;
            ++_rcontext.mcast_exec_id_;
            _rcontext.pmcast_ = _rcontext.plast_mcast_;
            option            = RunOptionE::Return;
        }

        if (should_notify) {
            should_notify = false;
            pop_sig_cnd_.notify_all();
        }

        if (option == RunOptionE::Return) {
            lock.unlock();
            solid_statistic_max(statistic_.max_pop_wait_loop_count_, wait_loop_count);
            return true;
        } else if (option == RunOptionE::Wait) {
            ++wait_loop_count;
            Base::wait(pop_sig_cnd_, lock);
        } else if (option == RunOptionE::Continue) {
            ++continue_loop_count;
        } else {
            lock.unlock();
            solid_statistic_max(statistic_.max_pop_wait_loop_count_, wait_loop_count);
            return false;
        }
    }
    return false;
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
bool WorkPool<Job, MCastJob, QNBits, Base>::doTryPush(JT&& _jb, ContextStub* _pctx)
{
    auto* const pnode = doTryAllocateJobNode();

    if (pnode) {
        pnode->job(std::forward<JT>(_jb));
        pnode->pcontext_ = _pctx;
        size_t qsz;
        {
            std::lock_guard<std::mutex> lock(pop_mtx_);
            job_queue_.push(pnode);
            pnode->mcast_id_ = mcast_push_id_;
            ++job_queue_size_;
            qsz = job_queue_size_;
            if (_pctx) {
                ++_pctx->use_count_;
            }
        }
        if (qsz <= Base::config_.max_worker_count_) {
            pop_sig_cnd_.notify_one();
        }
        solid_statistic_inc(statistic_.push_job_count_);
        solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
        return true;
    } else {
        return false;
    }
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
void WorkPool<Job, MCastJob, QNBits, Base>::doPush(JT&& _jb, ContextStub* _pctx)
{
    auto* const pnode = doAllocateJobNode();

    pnode->job(std::forward<JT>(_jb));
    pnode->pcontext_ = _pctx;
    size_t qsz;
    {
        std::lock_guard<std::mutex> lock(pop_mtx_);
        job_queue_.push(pnode);
        pnode->mcast_id_ = mcast_push_id_;
        ++job_queue_size_;
        qsz = job_queue_size_;
        if (_pctx) {
            ++_pctx->use_count_;
        }
        if (qsz <= Base::config_.max_worker_count_) {
            pop_sig_cnd_.notify_one();
        }
    }
    solid_statistic_inc(statistic_.push_job_count_);
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
void WorkPool<Job, MCastJob, QNBits, Base>::pushAll(JT&& _jb)
{
    auto* const pnode = doAllocateMCastNode();

    pnode->job(std::forward<JT>(_jb));
    size_t qsz;
    {
        std::lock_guard<std::mutex> lock(pop_mtx_);
        mcast_queue_.push(pnode);
        ++mcast_queue_size_;
        qsz = mcast_queue_size_;
        ++mcast_push_id_;
    }
    pop_sig_cnd_.notify_all();
    solid_statistic_inc(statistic_.push_mcast_count_);
    solid_statistic_max(statistic_.max_mcast_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
bool WorkPool<Job, MCastJob, QNBits, Base>::tryPushAll(JT&& _jb)
{
    auto* const pnode = doTryAllocateMCastNode();

    if (pnode) {
        pnode->job(std::forward<JT>(_jb));
        size_t qsz;
        {
            std::lock_guard<std::mutex> lock(pop_mtx_);
            mcast_queue_.push(pnode);
            ++mcast_queue_size_;
            qsz = mcast_queue_size_;
            ++mcast_push_id_;
        }
        pop_sig_cnd_.notify_all();
        solid_statistic_inc(statistic_.push_mcast_count_);
        solid_statistic_max(statistic_.max_mcast_in_queue_, qsz);
        return true;
    } else {
        return false;
    }
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
typename WorkPool<Job, MCastJob, QNBits, Base>::ContextStub* WorkPool<Job, MCastJob, QNBits, Base>::doCreateContext()
{
    std::lock_guard<std::mutex> lock(pop_mtx_);
    if (free_context_stack_.empty()) {
        context_dq_.emplace_back();
        context_dq_.back().use_count_ = 1;
        return &context_dq_.back();
    } else {
        auto pctx = free_context_stack_.pop();
        --free_context_stack_size_;
        pctx->use_count_ = 1;
        return pctx;
    }
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
typename WorkPool<Job, MCastJob, QNBits, Base>::JobNode* WorkPool<Job, MCastJob, QNBits, Base>::doTryAllocateJobNode()
{
    std::unique_lock<std::mutex> lock(push_mtx_);
    auto*                        pnode = free_job_stack_.pop();
    if (pnode) {
    } else {
        if (job_dq_.size() < Base::config_.max_job_queue_size_) {
            job_dq_.emplace_back();
            pnode = &job_dq_.back();
        }
    }
    return pnode;
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
typename WorkPool<Job, MCastJob, QNBits, Base>::JobNode* WorkPool<Job, MCastJob, QNBits, Base>::doAllocateJobNode()
{
    std::unique_lock<std::mutex> lock(push_mtx_);
    auto*                        pnode = free_job_stack_.pop();
    if (pnode) {
    } else {
        if (job_dq_.size() < Base::config_.max_job_queue_size_) {
            job_dq_.emplace_back();
            pnode = &job_dq_.back();
        } else {
            while ((pnode = free_job_stack_.pop()) == nullptr) {
                Base::wait(push_sig_cnd_, lock);
            }
        }
    }
    return pnode;
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
typename WorkPool<Job, MCastJob, QNBits, Base>::MCastNode* WorkPool<Job, MCastJob, QNBits, Base>::doTryAllocateMCastNode()
{
    std::unique_lock<std::mutex> lock(push_mtx_);
    auto*                        pnode = free_mcast_stack_.pop();
    if (pnode) {
    } else {
        if (mcast_dq_.size() < Base::config_.max_job_queue_size_) {
            mcast_dq_.emplace_back();
            pnode = &mcast_dq_.back();
        }
    }
    return pnode;
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
typename WorkPool<Job, MCastJob, QNBits, Base>::MCastNode* WorkPool<Job, MCastJob, QNBits, Base>::doAllocateMCastNode()
{
    std::unique_lock<std::mutex> lock(push_mtx_);
    auto*                        pnode = free_mcast_stack_.pop();
    if (pnode) {
    } else {
        if (mcast_dq_.size() < Base::config_.max_job_queue_size_) {
            mcast_dq_.emplace_back();
            pnode = &mcast_dq_.back();
        } else {
            while ((pnode = free_mcast_stack_.pop()) == nullptr) {
                Base::wait(push_sig_cnd_, lock);
            }
        }
    }
    return pnode;
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob = void>
using WorkPoolT = WorkPool<Job, MCastJob>;

template <class Job, class MCast = Job, size_t FunctionDataSize = function_default_data_size, template <typename, typename> class WP = WorkPoolT>
using CallPoolT = CallPool<Job, MCast, FunctionDataSize, WP>;
} // namespace locking
} // namespace solid
