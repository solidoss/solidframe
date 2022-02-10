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
template <typename Job, typename MCastJob = void, size_t QNBits = workpool_default_node_capacity_bit_count, typename Base = impl::WorkPoolBase>
class WorkPool;

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
        std::atomic<uint16_t>                                       exec_cnt_;
        uint16_t                                                    release_cnt_ = 0;

        template <class J>
        MCastNode(J&& _rj)
            : exec_cnt_(0)
        {
            ::new (&data_) MCastJob(std::forward<J>(_rj));
        }

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
        MCastNode* pmcast_ = nullptr;
        Job*       pjob_   = nullptr;

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
            _mcast_job_handler_fnc(pop_context.pmcast_->job(), std::forward<Args>(_args)...);
            solid_statistic_inc(mcast_job_count);
        }
        if (pop_context.pjob_) {
            _job_handler_fnc(*pop_context.pjob_, std::forward<Args>(_args)...);
            std::destroy_at(std::launder(pop_context.pjob_));
            solid_statistic_inc(job_count);
        } else if (!pop_context.pmcast_) {
            std::this_thread::yield();
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

    bool   should_notify       = false;
    size_t wait_loop_count     = 0;
    size_t continue_loop_count = 0;

    //Try destroy the MCast object, before aquiring lock
    if (_rcontext.pmcast_) {
        auto& rmcast_node     = *_rcontext.pmcast_;
        _rcontext.pmcast_     = nullptr;
        const size_t exec_cnt = rmcast_node.exec_cnt_.fetch_add(1) + 1;
        solid_check(exec_cnt <= config_.max_worker_count_);
        if (exec_cnt == config_.max_worker_count_) {
            rmcast_node.clear();
            should_notify = true;
        }
    }

    std::unique_lock<std::mutex> lock(mtx_);

    if (_rcontext.pcontext_) {
        if (_rcontext.pjob_) {
            _rcontext.pjob_      = nullptr;
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
        }
    } else if (_rcontext.pjob_) {
        _rcontext.pjob_ = nullptr;
        job_dq_[_rcontext.job_index_].clear();
        job_list_free_.pushBack(_rcontext.job_index_);
        _rcontext.job_index_ = InvalidIndex();
    }

    enum struct RunOptionE {
        Return,
        Wait,
        Continue,
        Exit,
    };

    while (true) {
        RunOptionE option             = RunOptionE::Wait;
        bool       should_fetch_mcast = _rcontext.mcast_exec_id_ != mcast_push_id_;

        if (_rcontext.pcontext_) {
            const auto front_mcast_id = _rcontext.pcontext_->job_list_.front().mcast_id_;
            if (front_mcast_id == _rcontext.mcast_exec_id_ || front_mcast_id == (_rcontext.mcast_exec_id_ + 1)) {
                option             = RunOptionE::Return;
                _rcontext.pjob_    = &_rcontext.pcontext_->job_list_.front().job();
                should_fetch_mcast = front_mcast_id != _rcontext.mcast_exec_id_;
            } else {
                option = RunOptionE::Continue;
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
                    option               = RunOptionE::Return;
                } else if (rnode.pcontext_->job_list_.empty()) {
                    --rnode.pcontext_->use_count_;
                    _rcontext.pcontext_ = rnode.pcontext_;
                    _rcontext.pjob_     = &job_list_.front().job();
                    rnode.pcontext_->job_list_.pushBack(job_list_.popFront());
                    option = RunOptionE::Return;
                } else {
                    //we have a job on a currently running synchronization context
                    rnode.pcontext_->job_list_.pushBack(job_list_.popFront());
                    --rnode.pcontext_->use_count_;
                    should_fetch_mcast = false;
                    option             = RunOptionE::Continue;
                    //solid_log(workpool_logger, Error, job_list_.size());
                }
            } else {
                option = RunOptionE::Continue;
            }
        } else if (!running_.load(std::memory_order_relaxed) && !should_fetch_mcast && context_dq_.size() == free_context_stack_.size()) {
            option = RunOptionE::Exit;
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
                }
                _rcontext.mcast_index_ = next_index;
            }

            should_notify = true;
            ++_rcontext.mcast_exec_id_;
            //solid_log(workpool_logger, Error, _rcontext.mcast_index_<<" "<<_rcontext.mcast_exec_id_);
            _rcontext.pmcast_ = &mcast_dq_[_rcontext.mcast_index_];
            option            = RunOptionE::Return;
        }

        if (should_notify) {
            should_notify = false;
            sig_cnd_.notify_all();
        }

        if (option == RunOptionE::Return) {
            lock.unlock();
            solid_statistic_max(statistic_.max_pop_wait_loop_count_, wait_loop_count);
            return true;
        } else if (option == RunOptionE::Wait) {
            ++wait_loop_count;
            Base::wait(sig_cnd_, lock);
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

            if (qsz <= config_.max_worker_count_) {
                sig_cnd_.notify_one(); //using all because sig_cnd_ is used for job_q_ size limitation
            }
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

            if (qsz <= config_.max_worker_count_) {
                sig_cnd_.notify_one(); //using all because sig_cnd_ is used for job_q_ size limitation
            }
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
