// solid/utility/workpool_lockfree.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
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
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/statistic.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/functiontraits.hpp"
#include "solid/utility/queue_lockfree.hpp"
#include "solid/utility/workpool_base.hpp"

#define SOLID_USE_ATOMIC_WORKERSTUB_NEXT

namespace solid {
namespace lockfree {

template <typename Job, typename MCastJob = void, size_t QNBits = workpool_default_node_capacity_bit_count, typename Base = solid::impl::WorkPoolBase>
class WorkPool;
//-----------------------------------------------------------------------------
struct WorkerStub {
    enum struct StateE : uint8_t {
        Cancel,
        Wait,
        WaitCancel,
        Notify,
    };
    std::atomic<StateE>     state_;
    std::atomic<bool>       stacked_;
    std::condition_variable cnd_;
    std::mutex              mtx_;
#ifdef SOLID_USE_ATOMIC_WORKERSTUB_NEXT
    std::atomic<size_t> next_ = 0;
#else
    size_t next_ = 0;
#endif
    std::thread::id     pop_thr_id_;
    std::atomic<size_t> pop_counter_;
    uint16_t            aba_counter_ = 0;

    WorkerStub()
        : state_(StateE::Cancel)
        , stacked_(false)
        , pop_counter_(0)
    {
    }

    inline static constexpr size_t thrId(const size_t _aba_id)
    {
        return _aba_id & bits_to_mask((sizeof(size_t) - sizeof(aba_counter_)) * 8);
    }

    size_t abaId(const size_t _thr_id)
    {
        return _thr_id | (static_cast<size_t>(++aba_counter_) << ((sizeof(size_t) - sizeof(aba_counter_)) * 8));
    }
};
//-----------------------------------------------------------------------------
struct ThreadStub {
    std::thread thread_;
    WorkerStub* pworker_ = nullptr;

    ThreadStub(std::thread&& _rthr)
        : thread_(std::move(_rthr))
    {
    }
};
struct WorkPoolStatistic : solid::Statistic {
    std::atomic<size_t>   max_worker_count_;
    std::atomic<size_t>   max_jobs_in_queue_;
    std::atomic<uint64_t> max_jobs_on_thread_;
    std::atomic<uint64_t> min_jobs_on_thread_;
    std::atomic<size_t>   wait_count_;
    std::atomic<size_t>   max_worker_wake_loop_;
    std::atomic<size_t>   max_job_pop_loop_;

    QueueStatistic queue_statistic_;

    WorkPoolStatistic();

    std::ostream& print(std::ostream& _ros) const override;
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

template <typename Job, size_t QNBits, typename Base>
class WorkPool<Job, void, QNBits, Base> : protected Base {
    using ThisT          = WorkPool<Job, void, QNBits, Base>;
    using WorkerFactoryT = std::function<std::thread(const size_t)>;
    using ThreadVectorT  = std::vector<ThreadStub>;
    using JobQueueT      = Queue<Job, QNBits, typename Base::QueueBase>;
    using AtomicBoolT    = std::atomic<bool>;
    using JobQueuePtrT   = std::unique_ptr<JobQueueT>;

    AtomicBoolT             running_;
    std::atomic<size_t>     thr_cnt_;
    size_t                  stopping_thr_cnt_ = 0;
    size_t                  starting_thr_cnt_ = 0;
    WorkerFactoryT          worker_factory_fnc_;
    JobQueuePtrT            job_q_ptr_;
    ThreadVectorT           thr_vec_;
    std::mutex              thr_mtx_;
    std::condition_variable thr_cnd_;
    std::atomic<size_t>     worker_head_;
    WorkerStub*             pregistered_worker_front_ = nullptr;
    WorkerStub*             pregistered_worker_back_  = nullptr;
#ifdef SOLID_HAS_STATISTICS
    WorkPoolStatistic statistic_;
#endif

public:
    static constexpr size_t node_capacity = JobQueueT::node_capacity;

    WorkPool()
        : running_(false)
        , thr_cnt_(0)
        , worker_head_{static_cast<size_t>(InvalidIndex())}
    {
    }

    template <class JobHandleFnc, typename... Args>
    WorkPool(
        const WorkPoolConfiguration& _cfg,
        JobHandleFnc                 _job_handler_fnc,
        Args&&... _args)
        : running_(false)
        , thr_cnt_(0)
        , worker_head_{static_cast<size_t>(InvalidIndex())}
    {
        doStart(
            _cfg,
            _cfg.max_worker_count_,
            _job_handler_fnc,
            std::forward<Args>(_args)...);
    }

    template <class JobHandleFnc, typename... Args>
    void start(
        const WorkPoolConfiguration& _cfg,
        JobHandleFnc                 _job_handler_fnc,
        Args&&... _args)
    {
        doStart(
            _cfg,
            _cfg.max_worker_count_,
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
    const QueueStatistic& queueStatistic() const
    {
        return job_q_ptr_->statistic();
    }

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
    template <class JT, bool Wait>
    size_t doJobPush(JT&& _uj, std::bool_constant<Wait>);

    bool doJobPop(WorkerStub& _rws, const size_t _thr_id, Job& _rjob);

    void doStop();

    template <class JobHandlerFnc, typename... Args>
    void doStart(
        const WorkPoolConfiguration& _cfg,
        size_t                       _start_wkr_cnt,
        JobHandlerFnc                _job_handler_fnc,
        Args&&... _args);

    void doWorkerPush(WorkerStub& _rws, const size_t _thr_id);
    bool doWorkerPop(WorkerStub*& _rpws);

    bool doWorkerWake(WorkerStub* _pws = nullptr);

    bool doRegisterWorker(WorkerStub& _rws, const size_t _thr_id);
    void doUnregisterWorker(WorkerStub& _rws, const size_t _thr_id);

    WorkerStub& worker(const size_t _thr_id) const
    {
        return *thr_vec_[_thr_id].pworker_;
    }

    bool doWait(WorkerStub& _rws)
    {
        size_t             count = 128;
        WorkerStub::StateE state;
        while ((state = _rws.state_.load(/*std::memory_order_acquire*/)) == WorkerStub::StateE::Wait && count--) {
            std::this_thread::yield();
        }

        {
            std::unique_lock<std::mutex> lock{_rws.mtx_};
            Base::wait(_rws.cnd_, lock, [this, &_rws]() { return _rws.state_.load() != WorkerStub::StateE::Wait || !running_.load(std::memory_order_relaxed); });
        }

        //NOTE: no push should happen after running_ is set to false
        return running_.load(std::memory_order_relaxed);
    }

}; //WorkPool

//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
template <class JT>
void WorkPool<Job, void, QNBits, Base>::push(JT&& _jb)
{
    solid_check(running_.load(std::memory_order_relaxed));
    const size_t qsz     = doJobPush(std::forward<JT>(_jb), std::true_type());
    const size_t thr_cnt = thr_cnt_.load();

    if (thr_cnt < Base::config_.max_worker_count_ && qsz > thr_cnt) {
        std::lock_guard<std::mutex> lock(thr_mtx_);
        solid_check(running_.load(std::memory_order_relaxed));
        if (qsz > thr_vec_.size() && thr_vec_.size() < Base::config_.max_worker_count_) {
            ++thr_cnt_;
            thr_vec_.emplace_back(worker_factory_fnc_(thr_vec_.size()));
            solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
        }
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
template <class JT>
bool WorkPool<Job, void, QNBits, Base>::tryPush(JT&& _jb)
{
    solid_check(running_.load(std::memory_order_relaxed));
    const size_t qsz = doJobPush(std::forward<JT>(_jb), std::false_type() /*wait*/);

    if (qsz != InvalidSize()) {
        const size_t thr_cnt = thr_cnt_.load();
        if (thr_cnt < Base::config_.max_worker_count_ && qsz > thr_cnt) {
            std::lock_guard<std::mutex> lock(thr_mtx_);
            solid_check(running_.load(std::memory_order_relaxed));
            if (qsz > thr_vec_.size() && thr_vec_.size() < Base::config_.max_worker_count_) {
                ++thr_cnt_;
                thr_vec_.emplace_back(worker_factory_fnc_(thr_vec_.size()));
                solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
            }
        }
        solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
        return true;
    } else {
        return false;
    }
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
bool WorkPool<Job, void, QNBits, Base>::doWorkerWake(WorkerStub* _pws)
{
    WorkerStub* pworker_stub;
    while (doWorkerPop(pworker_stub)) {
        //solid_dbg(workpool_logger, Verbose, "pop_worker: "<<pworker_stub);
        //pworker_stub valid states: Wait, WaitCancel
        bool   do_break   = true;
        size_t loop_count = 0;
        while (true) {
            ++loop_count;
            solid_statistic_max(statistic_.max_worker_wake_loop_, loop_count);
            if (pworker_stub != _pws) {
            } else {
                continue;
            }
            auto expect_state = WorkerStub::StateE::Wait;
            if (pworker_stub->state_.compare_exchange_strong(expect_state, WorkerStub::StateE::Notify)) {
                //solid_dbg(workpool_logger, Warning, "wake worker");
                {
                    std::lock_guard<std::mutex> lock(pworker_stub->mtx_);
                }
                pworker_stub->cnd_.notify_one();
                return true;
            } else {
                solid_assert_log(expect_state == WorkerStub::StateE::WaitCancel, workpool_logger, "expect state not WaitCancel but: " << static_cast<int>(expect_state) << " count " << loop_count);
                if (pworker_stub->state_.compare_exchange_strong(expect_state, WorkerStub::StateE::Cancel)) {
                    //solid_dbg(workpool_logger, Warning, "worker canceled");
                    do_break = false;
                    break;
                } else {
                    solid_assert_log(expect_state == WorkerStub::StateE::Wait, workpool_logger, "expect state not Wait but: " << static_cast<int>(expect_state) << " count " << loop_count);
                    //solid_dbg(workpool_logger, Warning, "force notify: "<<pworker_stub<< " count "<<count);
                    continue; //try wake another thread
                }
            }
        }
        if (do_break)
            break;
    }
    return false;
}

//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
template <class JT, bool Wait>
size_t WorkPool<Job, void, QNBits, Base>::doJobPush(JT&& _rj, std::bool_constant<Wait>)
{
    const size_t sz = job_q_ptr_->template push<Wait>(std::forward<JT>(_rj));

    if (sz != InvalidSize()) {
        if (doWorkerWake()) {

        } else {
            //solid_dbg(workpool_logger, Verbose, "no worker notified - "<<sz);
        }
    }
    return sz;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
bool WorkPool<Job, void, QNBits, Base>::doJobPop(WorkerStub& _rws, const size_t thr_id_, Job& _rjob)
{
    //_rws valid states: Cancel, WaitCancel
    auto expect_state = WorkerStub::StateE::WaitCancel;
    //bool did_push = true;
    if (_rws.state_.compare_exchange_strong(expect_state, WorkerStub::StateE::Wait)) {
        //did_push = false;
    } else {
        solid_assert_log(expect_state == WorkerStub::StateE::Cancel, workpool_logger, "expect state not Cancel but: " << static_cast<int>(expect_state));
        _rws.state_.store(WorkerStub::StateE::Wait);
        //solid_dbg(workpool_logger, Verbose, "push_worker: "<<&_rws);
        doWorkerPush(_rws, thr_id_);
    }
    size_t loop_count = 0;
    while (!job_q_ptr_->pop(_rjob)) {
        //solid_dbg(workpool_logger, Warning, "worker wait "<<did_push<<' '<<loop_count);
        ++loop_count;
        solid_statistic_inc(statistic_.wait_count_);
        solid_statistic_max(statistic_.max_job_pop_loop_, loop_count);
        if (doWait(_rws)) {
            //solid_dbg(workpool_logger, Verbose, "push_worker: "<<&_rws <<" state = "<<static_cast<int>(_rws.state_.load()));
            auto       expect_state = WorkerStub::StateE::Notify;
            const bool ok           = _rws.state_.compare_exchange_strong(expect_state, WorkerStub::StateE::Wait);
            solid_assert_log(ok, workpool_logger, "expect state not Notify, but: " << static_cast<int>(expect_state));
            doWorkerPush(_rws, thr_id_);
            continue;
        } else if (job_q_ptr_->pop(_rjob)) {
            break;
        } else {
            //solid_dbg(workpool_logger, Warning, this << " no more waiting");
            return false;
        }
    }
    //_rws valid states: Wait, Notify

    expect_state = WorkerStub::StateE::Wait;
    if (_rws.state_.compare_exchange_strong(expect_state, WorkerStub::StateE::WaitCancel)) {

    } else {
        solid_assert_log(expect_state == WorkerStub::StateE::Notify, workpool_logger, "expect state not Notify but: " << static_cast<int>(expect_state));
        _rws.state_.store(WorkerStub::StateE::Cancel);
        //solid_dbg(workpool_logger, Warning, "already notified");
        if (doWorkerWake(&_rws)) {

        } else {
            //solid_dbg(workpool_logger, Warning, "no worker notified!");
        }
    }

    return true;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
void WorkPool<Job, void, QNBits, Base>::doWorkerPush(WorkerStub& _rws, const size_t _thr_id)
{

    //bool expect = false;
    //const bool stacked_ok = _rws.stacked_.compare_exchange_strong(expect, true);
    //solid_assert_log(stacked_ok, workpool_logger);
    //((void)stacked_ok);

    const size_t aba_id = _rws.abaId(_thr_id);
#ifdef SOLID_USE_ATOMIC_WORKERSTUB_NEXT
    auto next = worker_head_.load(std::memory_order_relaxed);
    _rws.next_.store(next);
    while (!worker_head_.compare_exchange_weak(next, aba_id /*,
        std::memory_order_release,
        std::memory_order_relaxed*/
        )) {
        _rws.next_.store(next);
    }
#else
    _rws.next_   = worker_head_.load(std::memory_order_relaxed);
    while (!worker_head_.compare_exchange_weak(_rws.next_, aba_id /*,
        std::memory_order_release,
        std::memory_order_relaxed*/
        ))
        ;
#endif
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
bool WorkPool<Job, void, QNBits, Base>::doWorkerPop(WorkerStub*& _rpws)
{
    size_t old_head = worker_head_.load();
#ifdef SOLID_USE_ATOMIC_WORKERSTUB_NEXT
    while (old_head != InvalidIndex() && !worker_head_.compare_exchange_weak(old_head, worker(WorkerStub::thrId(old_head)).next_.load() /*, std::memory_order_acquire, std::memory_order_relaxed*/))
        ;
#else
    while (old_head != InvalidIndex() && !worker_head_.compare_exchange_weak(old_head, worker(WorkerStub::thrId(old_head)).next_ /*, std::memory_order_acquire, std::memory_order_relaxed*/))
        ;
#endif

    if (old_head != InvalidIndex()) {
        _rpws = &worker(WorkerStub::thrId(old_head));
        //bool expect = true;
        //const bool stacked_ok = _rpws->stacked_.compare_exchange_strong(expect, false);
        //if(stacked_ok){
        //    _rpws->pop_thr_id_ = std::this_thread::get_id();
        //}
        //solid_assert_log(stacked_ok, workpool_logger, "last pop thr "<<_rpws->pop_thr_id_<<" vs "<<std::this_thread::get_id());
        //((void)stacked_ok);
        return true;
    } else {
        _rpws = nullptr;
        return false;
    }
}
//-----------------------------------------------------------------------------
static std::atomic<int> loop_cnt{0};

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

        for (auto& t : thr_vec_) {
            if (t.pworker_) {
                solid_dbg(workpool_logger, Warning, this << " worker wake " << t.thread_.get_id());
                {
                    std::lock_guard<std::mutex> lock(t.pworker_->mtx_);
                }
                t.pworker_->cnd_.notify_one();
            }
        }

        thr_cnd_.wait(lock, [this]() { return thr_cnt_ == 0; });

        for (auto& t : thr_vec_) {
            t.thread_.join();
        }
        thr_vec_.clear();
    }
#ifdef SOLID_HAS_STATISTICS
    solid_log(workpool_logger, Statistic, "Workpool " << this << " statistic:" << this->statistic_);
    {

        const size_t max_jobs_in_queue = Base::config_.max_job_queue_size_ == static_cast<size_t>(-1) ? Base::config_.max_job_queue_size_ : Base::config_.max_job_queue_size_ + JobQueueT::node_capacity;
        solid_check_log(statistic_.max_jobs_in_queue_ <= max_jobs_in_queue, workpool_logger, "statistic_.max_jobs_in_queue_ = " << statistic_.max_jobs_in_queue_ << " <= config_.max_job_queue_size_ = " << max_jobs_in_queue);
        solid_check_log(statistic_.max_worker_count_ <= Base::config_.max_worker_count_, workpool_logger, "statistic_.max_worker_count_ = " << statistic_.max_worker_count_ << " <= config_.max_worker_count_ = " << Base::config_.max_worker_count_);
    }
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

    auto lambda = [_job_handler_fnc, this, _args...](const size_t _id) {
        return std::thread(
            [this](const size_t _id, JobHandlerFnc _job_handler_fnc, Args&&... _args) {
                uint64_t   job_count = 0;
                Job        job;
                WorkerStub stub;
                if (!doRegisterWorker(stub, _id)) {
                    solid_dbg(workpool_logger, Warning, this << " worker rejected");
                    return;
                }
                Base::config_.on_thread_start_fnc_();

                solid_dbg(workpool_logger, Warning, this << " worker started");

                while (doJobPop(stub, _id, job)) {
                    _job_handler_fnc(job, std::forward<Args>(_args)...);
                    solid_statistic_inc(job_count);
                }

                doUnregisterWorker(stub, _id);

                solid_dbg(workpool_logger, Warning, this << " worker stopping after handling " << job_count << " jobs");
                solid_statistic_max(statistic_.max_jobs_on_thread_, job_count);
                solid_statistic_min(statistic_.min_jobs_on_thread_, job_count);

                Base::config_.on_thread_stop_fnc_();
            },
            _id,
            _job_handler_fnc, _args...);
    };

    if (_start_wkr_cnt > Base::config_.max_worker_count_) {
        _start_wkr_cnt = Base::config_.max_worker_count_;
    }

    bool expect = false;

    if (running_.compare_exchange_strong(expect, true)) {
        Base::config_ = _cfg;

        solid_dbg(workpool_logger, Verbose, this << " start " << _start_wkr_cnt << " " << Base::config_.max_worker_count_ << ' ' << Base::config_.max_job_queue_size_);

        worker_factory_fnc_ = lambda;
#ifdef SOLID_HAS_STATISTICS
        job_q_ptr_.reset(new JobQueueT(statistic_.queue_statistic_, Base::config_.max_job_queue_size_));
#else
        job_q_ptr_.reset(new JobQueueT(Base::config_.max_job_queue_size_));
#endif

        {
            std::unique_lock<std::mutex> lock(thr_mtx_);

            thr_vec_.reserve(Base::config_.max_worker_count_);

            thr_cnt_ += _start_wkr_cnt;
            for (size_t i = 0; i < _start_wkr_cnt; ++i) {
                thr_vec_.emplace_back(worker_factory_fnc_(thr_vec_.size()));
                solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
            }
            thr_cnd_.wait(lock, [this]() { return thr_cnt_ == starting_thr_cnt_; });
        }
    }
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
bool WorkPool<Job, void, QNBits, Base>::doRegisterWorker(WorkerStub& _rws, const size_t _thr_id)
{
    std::unique_lock<std::mutex> lock(thr_mtx_);
    if (running_) {
        thr_vec_[_thr_id].pworker_ = &_rws;
        ++starting_thr_cnt_;
        if (starting_thr_cnt_ == thr_cnt_) {
            thr_cnd_.notify_all();
        }
        return true;
    }
    --thr_cnt_;
    thr_cnd_.notify_all();
    return false;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits, typename Base>
void WorkPool<Job, void, QNBits, Base>::doUnregisterWorker(WorkerStub& _rws, const size_t _thr_id)
{
    std::unique_lock<std::mutex> lock(thr_mtx_);
    ++stopping_thr_cnt_;

    if (stopping_thr_cnt_ >= thr_cnt_) {
        thr_cnd_.notify_all();
    } else {
        thr_cnd_.wait(lock, [this]() { return thr_cnt_ <= stopping_thr_cnt_; });
    }

    if (thr_cnt_.fetch_sub(1) == 1) {
        thr_cnd_.notify_all();
    }
}
//-----------------------------------------------------------------------------
template <typename Job, typename MCast = void>
using WorkPoolT = WorkPool<Job, MCast>;
//-----------------------------------------------------------------------------

template <class Job, class MCast = Job, size_t FunctionDataSize = function_default_data_size, template <typename, typename> class WP = WorkPoolT>
using CallPoolT = CallPool<Job, MCast, FunctionDataSize, WP>;
} //namespace lockfree
} //namespace solid
