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
//-----------------------------------------------------------------------------
struct WorkPoolStatistic : solid::Statistic {
    std::atomic<size_t>   max_worker_count_;
    std::atomic<size_t>   max_jobs_in_queue_;
    std::atomic<uint64_t> max_jobs_on_thread_;
    std::atomic<uint64_t> min_jobs_on_thread_;

    WorkPoolStatistic();

    std::ostream& print(std::ostream& _ros) const override;
};

template <typename Job, typename MCastJob = void, size_t QNBits = workpool_default_node_capacity_bit_count, typename Base = impl::WorkPoolBase>
class WorkPool;

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

template <typename Job, size_t QNBits, typename Base>
class WorkPool<Job, void, QNBits, Base> : protected Base {
    using ThisT          = WorkPool<Job, void, QNBits, Base>;
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
    WorkPoolStatistic statistic_;
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
        size_t                       _start_wkr_cnt,
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
    void push(JT&& _jb);

    template <class JT>
    bool tryPush(JT&& _jb);

#ifdef SOLID_HAS_STATISTICS
    const WorkPoolStatistic& statistic() const
    {
        return statistic_;
    }
#endif
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
        size_t                       _start_wkr_cnt,
        JobHandlerFnc                _job_handler_fnc,
        Args&&... _args);
}; //WorkPool

//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
template <class JT>
void WorkPool<Job, void, QNBits, Base>::push(JT&& _jb)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            if (job_q_.size() < config_.max_job_queue_size_) {
            } else {
                Base::wait(sig_cnd_, lock, [this]() { return job_q_.size() < config_.max_job_queue_size_; });
            }

            job_q_.push(std::forward<JT>(_jb));
            qsz = job_q_.size();
        }
        if (qsz == 1) {
            sig_cnd_.notify_all(); //using all because sig_cnd_ is job_q_ size limitation
        }
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
bool WorkPool<Job, void, QNBits, Base>::tryPush(JT&& _jb)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            if (job_q_.size() < Base::config_.max_job_queue_size_) {
            } else {
                return false;
            }

            job_q_.push(std::forward<JT>(_jb));
            qsz = job_q_.size();
        }

        if (qsz == 1) {
            sig_cnd_.notify_all(); //using all because sig_cnd_ is job_q_ size limitation
        }

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
bool WorkPool<Job, void, QNBits, Base>::doWaitJob(std::unique_lock<std::mutex>& _lock)
{
    while (job_q_.empty() && running_.load(std::memory_order_relaxed)) {
        Base::wait(sig_cnd_, _lock);
    }
    return !job_q_.empty();
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
bool WorkPool<Job, void, QNBits, Base>::pop(Job& _rjob)
{

    std::unique_lock<std::mutex> lock(mtx_);

    if (doWaitJob(lock)) {
        const bool was_full = job_q_.size() == config_.max_job_queue_size_;
        _rjob               = std::move(job_q_.front());
        job_q_.pop();

        if (was_full) {
            sig_cnd_.notify_all();
        }
        return true;
    }
    return false;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
void WorkPool<Job, void, QNBits, Base>::doStop()
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
#ifdef SOLID_HAS_STATISTICS
    solid_log(workpool_logger, Statistic, "Workpool " << this << " statistic:" << this->statistic_);
#endif
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
template <class JobHandlerFnc, typename... Args>
void WorkPool<Job, void, QNBits, Base>::doStart(
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
// WorkPool with multicast and Synchronization support
//-----------------------------------------------------------------------------
struct WorkPoolMulticastStatistic : solid::Statistic {
    std::atomic<size_t>   max_jobs_in_queue_;
    std::atomic<size_t>   max_mcast_jobs_in_queue_;
    std::atomic<uint64_t> max_jobs_on_thread_;
    std::atomic<uint64_t> min_jobs_on_thread_;
    std::atomic<uint64_t> max_mcast_jobs_on_thread_;
    std::atomic<uint64_t> min_mcast_jobs_on_thread_;
    std::atomic<size_t>   job_count_;
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

    struct JobNode : inner::Node<1> {
        std::aligned_storage_t<sizeof(Job), alignof(Job)> data_;
        ContextStub*                                      pcontext_ = nullptr;
        size_t                                            mcast_id_ = 0;

        template <class J>
        JobNode(J&& _rj, const size_t _mcast_id = 0, ContextStub* _pcontext = nullptr)
            : pcontext_(_pcontext)
            , mcast_id_(_mcast_id)
        {
            ::new (&data_) Job(std::forward<J>(_rj));
        }
        void clear()
        {
            pcontext_ = nullptr;
            mcast_id_ = 0;
            std::destroy_at(std::launder(reinterpret_cast<Job*>(&data_)));
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
    };

    struct MCastNode : inner::Node<1> {
        std::aligned_storage_t<sizeof(MCastJob), alignof(MCastJob)> data_;
        uint16_t                                                    exec_cnt_    = 0;
        uint16_t                                                    release_cnt_ = 0;

        template <class J>
        MCastNode(J&& _rj)
        {
            ::new (&data_) MCastJob(std::forward<J>(_rj));
        }

        MCastNode() {} //this is needed for sentinel

        MCastJob& job()
        {
            return *std::launder(reinterpret_cast<MCastJob*>(&data_));
        }

        template <class J>
        void job(J&& _rj)
        {
            ::new (&data_) MCastJob(std::forward<J>(_rj));
        }

        void clear()
        {
            std::destroy_at(std::launder(reinterpret_cast<MCastJob*>(&data_)));
        }

        void release()
        {
            exec_cnt_    = 0;
            release_cnt_ = 0;
        }
    };

    using ThisT           = WorkPool<Job, MCastJob, QNBits, Base>;
    using ThreadVectorT   = std::vector<std::thread>;
    using JobDqT          = std::deque<JobNode>;
    using JobInnerListT   = inner::List<JobDqT, 0>;
    using MCastDqT        = std::deque<MCastNode>;
    using MCastInnerListT = inner::List<MCastDqT, 0>;
    using AtomicBoolT     = std::atomic<bool>;

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
    MCastDqT                mcast_dq_;
    MCastInnerListT         mcast_list_;
    MCastInnerListT         mcast_list_free_;
    ContextDqT              context_dq_;
    ContextStackT           free_context_stack_;
    ThreadVectorT           thr_vec_;
    std::mutex              mtx_;
    std::condition_variable sig_cnd_;
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
        : config_()
        , running_(false)
        , job_list_(job_dq_)
        , job_list_free_(job_dq_)
        , mcast_list_(mcast_dq_)
        , mcast_list_free_(mcast_dq_)
    {
    }

    template <class JobHandleFnc, class MCastJobHandleFnc, typename... Args>
    WorkPool(
        const WorkPoolConfiguration& _cfg,
        JobHandleFnc                 _job_handler_fnc,
        MCastJobHandleFnc            _mcast_job_handler_fnc,
        Args&&... _args)
        : config_()
        , running_(false)
        , job_list_(job_dq_)
        , job_list_free_(job_dq_)
        , mcast_list_(mcast_dq_)
        , mcast_list_free_(mcast_dq_)
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
        MCastJob* pmcast_ = nullptr;
        Job*      pjob_   = nullptr;

        //used only on pop
        ContextStub* pcontext_      = nullptr;
        size_t       mcast_exec_id_ = 0;
        size_t       job_index_     = InvalidIndex();
        size_t       mcast_index_   = 0; //initially it points to the sentinel
    };

    void release(ContextStub* _pctx)
    {
        if (_pctx) {
            std::lock_guard<std::mutex> lock(mtx_);
            --_pctx->use_count_;
            if (_pctx->job_list_.empty() && _pctx->use_count_ == 0) {
                free_context_stack_.push(_pctx);
                sig_cnd_.notify_all();
            }
        }
    }

    bool pop(PopContext& _rcontext);

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

    config_ = _cfg;

    {
        std::lock_guard<std::mutex> lock(mtx_);

        job_dq_.clear();
        job_list_.clear();
        job_list_free_.clear();
        mcast_dq_.clear();
        mcast_list_.clear();
        mcast_list_free_.clear();

        //mcast sentinel.
        mcast_dq_.emplace_back();
        mcast_dq_.back().exec_cnt_ = config_.max_worker_count_;
        mcast_list_.pushBack(mcast_dq_.size() - 1);

        for (size_t i = 0; i < config_.max_worker_count_; ++i) {
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
    uint64_t job_count       = 0;
    uint64_t mcast_job_count = 0;

    PopContext pop_context;

    while (pop(pop_context)) {
        if (pop_context.pmcast_) {
            _mcast_job_handler_fnc(*pop_context.pmcast_, std::forward<Args>(_args)...);
            solid_statistic_inc(mcast_job_count);
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
        sig_cnd_.notify_all();

        for (auto& t : thr_vec_) {
            t.join();
        }
        thr_vec_.clear();
    }
#ifdef SOLID_HAS_STATISTICS
    solid_log(workpool_logger, Statistic, "WorkPool " << this << " statistic:" << this->statistic_);
#endif
}

//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
bool WorkPool<Job, MCastJob, QNBits, Base>::pop(PopContext& _rcontext)
{

    bool                         should_notify   = false;
    size_t                       wait_loop_count = 0;
    std::unique_lock<std::mutex> lock(mtx_);

    if (_rcontext.pcontext_) {
        if (_rcontext.pjob_) {
            const auto job_index = _rcontext.pcontext_->job_list_.popFront();
            job_dq_[job_index].clear();
            job_list_free_.pushBack(job_index);

            if (!_rcontext.pcontext_->job_list_.empty()) {
            } else {
                if (_rcontext.pcontext_->use_count_ == 0) {
                    free_context_stack_.push(_rcontext.pcontext_);
                    should_notify = true;
                }
                _rcontext.pcontext_ = nullptr;
            }
            _rcontext.pjob_ = nullptr;
        }
    } else if (_rcontext.pjob_) {
        job_dq_[_rcontext.job_index_].clear();
        job_list_free_.pushBack(_rcontext.job_index_);
        _rcontext.pjob_      = nullptr;
        _rcontext.job_index_ = InvalidIndex();
    }

    if (_rcontext.pmcast_) {
        _rcontext.pmcast_ = nullptr;

        auto& rmcast_node = mcast_dq_[_rcontext.mcast_index_];
        ++rmcast_node.exec_cnt_;
        solid_check(rmcast_node.exec_cnt_ <= config_.max_worker_count_);
        if (rmcast_node.exec_cnt_ == config_.max_worker_count_) {
            solid_check(_rcontext.mcast_index_ == mcast_list_.frontIndex());
            rmcast_node.clear();
            should_notify = true;
        }
    }

    while (true) {
        bool should_wait        = true;
        bool should_fetch_mcast = _rcontext.mcast_exec_id_ != mcast_push_id_;

        if (_rcontext.pcontext_) {
            const auto front_mcast_id = _rcontext.pcontext_->job_list_.front().mcast_id_;
            if (front_mcast_id == _rcontext.mcast_exec_id_ || front_mcast_id == (_rcontext.mcast_exec_id_ + 1)) {
                should_wait        = false;
                _rcontext.pjob_    = &_rcontext.pcontext_->job_list_.front().job();
                should_fetch_mcast = front_mcast_id != _rcontext.mcast_exec_id_;
            }
        } else if (!job_list_.empty()) {
            //tryPopJob
            auto& rnode = job_list_.front();

            if (rnode.mcast_id_ == _rcontext.mcast_exec_id_ || rnode.mcast_id_ == (_rcontext.mcast_exec_id_ + 1)) {

                should_notify      = job_list_.size() == config_.max_job_queue_size_;
                should_fetch_mcast = rnode.mcast_id_ != _rcontext.mcast_exec_id_;

                if (rnode.pcontext_ == nullptr) {
                    _rcontext.pjob_      = &rnode.job();
                    _rcontext.job_index_ = job_list_.popFront();
                    should_wait          = false;

                } else if (rnode.pcontext_->job_list_.empty()) {
                    --rnode.pcontext_->use_count_;
                    _rcontext.pcontext_ = rnode.pcontext_;
                    _rcontext.pjob_     = &job_list_.front().job();
                    rnode.pcontext_->job_list_.pushBack(job_list_.popFront());
                    should_wait = false;
                } else {
                    //we have a job on a currently running synchronization context
                    rnode.pcontext_->job_list_.pushBack(job_list_.popFront());
                    --rnode.pcontext_->use_count_;
                    should_wait        = false; //don't want to keep the lock for too long
                    should_fetch_mcast = false;
                }
            }
        }

        if (should_fetch_mcast) {
            //solid_log(workpool_logger, Error, _rcontext.mcast_index_<<" "<<_rcontext.mcast_exec_id_<<' '<<mcast_push_id_);
            {
                const auto next_index  = mcast_list_.nextIndex(_rcontext.mcast_index_);
                auto&      rmcast_node = mcast_dq_[_rcontext.mcast_index_];
                ++rmcast_node.release_cnt_;
                if (rmcast_node.release_cnt_ == config_.max_worker_count_) {
                    solid_check(_rcontext.mcast_index_ == mcast_list_.frontIndex());
                    rmcast_node.release();
                    mcast_list_free_.pushBack(mcast_list_.popFront());
                    should_notify = true;
                }
                _rcontext.mcast_index_ = next_index;
            }

            ++_rcontext.mcast_exec_id_;
            //solid_log(workpool_logger, Error, _rcontext.mcast_index_<<" "<<_rcontext.mcast_exec_id_);
            _rcontext.pmcast_ = &mcast_dq_[_rcontext.mcast_index_].job();
            should_wait       = false;
        }

        if (should_notify) {
            sig_cnd_.notify_all();
        }

        if (should_wait) {
            solid_check(!mcast_list_.empty());
            if (
                !running_.load(std::memory_order_relaxed) && mcast_list_.size() == 1 && mcast_list_.front().exec_cnt_ == config_.max_worker_count_ && job_list_.empty() && context_dq_.size() == free_context_stack_.size() //all contexts have been released
            ) {
                break;
            }
            Base::wait(sig_cnd_, lock);
            ++wait_loop_count;
        } else {
            solid_statistic_max(statistic_.max_pop_wait_loop_count_, wait_loop_count);
            return true;
        }
    }
    solid_statistic_max(statistic_.max_pop_wait_loop_count_, wait_loop_count);
    return false;
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
bool WorkPool<Job, MCastJob, QNBits, Base>::doTryPush(JT&& _jb, ContextStub* _pctx)
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
                job_dq_.emplace_back(std::forward<JT>(_jb), mcast_push_id_, _pctx);
                job_list_.pushBack(job_dq_.size() - 1);
            } else {
                const auto idx = job_list_free_.popFront();
                job_dq_[idx].job(std::forward<JT>(_jb));
                job_dq_[idx].pcontext_ = _pctx;
                job_dq_[idx].mcast_id_ = mcast_push_id_;
                job_list_.pushBack(idx);
            }
            qsz = job_list_.size();
        }
        if (qsz == 1) {
            sig_cnd_.notify_all(); //using all because sig_cnd_ is used for job_q_ size limitation
        }
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
    return true;
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
void WorkPool<Job, MCastJob, QNBits, Base>::doPush(JT&& _jb, ContextStub* _pctx)
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
                job_dq_.emplace_back(std::forward<JT>(_jb), mcast_push_id_, _pctx);
                job_list_.pushBack(job_dq_.size() - 1);
            } else {
                const auto idx = job_list_free_.popFront();
                job_dq_[idx].job(std::forward<JT>(_jb));
                job_dq_[idx].pcontext_ = _pctx;
                job_dq_[idx].mcast_id_ = mcast_push_id_;
                job_list_.pushBack(idx);
            }
            if (_pctx) {
                ++_pctx->use_count_;
            }
            qsz = job_list_.size();
        }
        if (qsz == 1) {
            sig_cnd_.notify_all(); //using all because sig_cnd_ is used for job_q_ size limitation
        }
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
void WorkPool<Job, MCastJob, QNBits, Base>::pushAll(JT&& _jb)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            if (mcast_list_.size() < config_.max_job_queue_size_) {
            } else {
                do {
                    Base::wait(sig_cnd_, lock);
                } while (mcast_list_.size() >= config_.max_job_queue_size_);
            }
            if (mcast_list_free_.empty()) {
                mcast_dq_.emplace_back(std::forward<JT>(_jb));
                mcast_list_.pushBack(mcast_dq_.size() - 1);
            } else {
                const auto idx = mcast_list_free_.popFront();
                mcast_dq_[idx].job(std::forward<JT>(_jb));
                mcast_list_.pushBack(idx);
            }
            qsz = mcast_list_.size();
            ++mcast_push_id_;
        }
        if (qsz == 1) {
            sig_cnd_.notify_all(); //using all because sig_cnd_ is used for job_q_ size limitation
        }
    }
    solid_statistic_max(statistic_.max_mcast_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob, size_t QNBits, typename Base>
template <class JT>
bool WorkPool<Job, MCastJob, QNBits, Base>::tryPushAll(JT&& _jb)
{
    size_t qsz;
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            if (mcast_list_.size() < config_.max_job_queue_size_) {
            } else {
                return false;
            }

            if (mcast_list_free_.empty()) {
                mcast_dq_.emplace_back(std::forward<JT>(_jb));
                mcast_list_.pushBack(mcast_dq_.size() - 1);
            } else {
                const auto idx      = mcast_list_free_.popFront();
                mcast_dq_[idx].job_ = std::forward<JT>(_jb);
                mcast_list_.pushBack(idx);
            }
            qsz = mcast_list_.size();
            ++mcast_push_id_;
        }
        if (qsz == 1) {
            sig_cnd_.notify_all(); //using all because sig_cnd_ is used for job_q_ size limitation
        }
    }
    solid_statistic_max(statistic_.max_mcast_jobs_in_queue_, qsz);
    return true;
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <typename Job, typename MCastJob = void>
using WorkPoolT = WorkPool<Job, MCastJob>;

template <class Job, class MCast = Job, size_t FunctionDataSize = function_default_data_size, template <typename, typename> class WP = WorkPoolT>
using CallPoolT = CallPool<Job, MCast, FunctionDataSize, WP>;
} //namespace locking
} //namespace solid
