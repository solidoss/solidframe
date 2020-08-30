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
#include <memory>

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/system/statistic.hpp"
#include "solid/system/cassert.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/functiontraits.hpp"
#include "solid/utility/queue_lockfree.hpp"

namespace solid {

extern const LoggerT workpool_logger;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
namespace lockfree {
//-----------------------------------------------------------------------------
struct WorkerStub {
    enum struct StateE : uint8_t {
        Cancel,
        Wait,
        WaitCancel,
        Notify,
    };
    std::atomic<StateE>     state_;
    std::condition_variable cv_;
    WorkerStub*             pnext_      = nullptr;
    WorkerStub*             plist_prev_ = nullptr;
    WorkerStub*             plist_next_ = nullptr;
    
    WorkerStub():state_(StateE::Cancel){}
    
//     bool notify()
//     {
//         StateE     expected = StateE::Wait;
//         const auto notified = state_.compare_exchange_strong(expected, StateE::Notify,
//             std::memory_order_release,
//             std::memory_order_relaxed);
//         if (notified) {
//             cv_.notify_one();
//         }
//         return notified;
//     }

    void wake()
    {
        cv_.notify_one();
    }

//     bool cancel()
//     {
//         StateE expected = StateE::Wait;
//         return state_.compare_exchange_strong(expected, StateE::Cancel,
//             std::memory_order_release,
//             std::memory_order_relaxed);
//     }

    bool wait(std::mutex& _rmutex, std::atomic<bool>& _rrunning)
    {
        size_t count = 64;
        StateE state;
        while ((state = state_.load(/*std::memory_order_acquire*/)) != StateE::Wait && count--) {
            std::this_thread::yield();
        }

        if (state != StateE::Wait) {
            return true;
        }

        {
            std::unique_lock<std::mutex> lock{_rmutex};
            cv_.wait(lock, [&_rrunning, this]() { return state_.load() != StateE::Wait || !_rrunning; });
        }

        return _rrunning;
    }
};
//-----------------------------------------------------------------------------

struct WorkPoolConfiguration {
    size_t max_worker_count_;
    size_t max_job_queue_size_;

    explicit WorkPoolConfiguration(
        const size_t _max_worker_count   = std::thread::hardware_concurrency(),
        const size_t _max_job_queue_size = std::numeric_limits<size_t>::max())
        : max_worker_count_(_max_worker_count == 0 ? std::thread::hardware_concurrency() : _max_worker_count)
        , max_job_queue_size_(_max_job_queue_size == 0 ? std::numeric_limits<size_t>::max() : _max_job_queue_size)
    {
    }
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

template <typename Job, size_t QNBits = 10>
class WorkPool : NonCopyable {
    using ThisT          = WorkPool<Job, QNBits>;
    using WorkerFactoryT = std::function<std::thread()>;
    using ThreadVectorT  = std::vector<std::thread>;
    using JobQueueT      = Queue<Job, QNBits>;
    using AtomicBoolT    = std::atomic<bool>;
    using JobQueuePtrT   = std::unique_ptr<JobQueueT>;

    WorkPoolConfiguration    config_;
    AtomicBoolT              running_;
    std::atomic<size_t>      thr_cnt_;
    WorkerFactoryT           worker_factory_fnc_;
    JobQueuePtrT             job_q_ptr_;
    ThreadVectorT            thr_vec_;
    std::mutex               thr_mtx_;
    std::mutex               job_mtx_;
    std::condition_variable  thr_cnd_;
    std::atomic<WorkerStub*> worker_head_;
    WorkerStub*              pregistered_worker_front_ = nullptr;
    WorkerStub*              pregistered_worker_back_  = nullptr;
#ifdef SOLID_HAS_STATISTICS
    struct Statistic : solid::Statistic {
        std::atomic<size_t>   max_worker_count_;
        std::atomic<size_t>   max_jobs_in_queue_;
        std::atomic<uint64_t> max_jobs_on_thread_;
        std::atomic<uint64_t> min_jobs_on_thread_;
        std::atomic<size_t>   wait_count_;

        Statistic()
            : max_worker_count_(0)
            , max_jobs_in_queue_(0)
            , max_jobs_on_thread_(0)
            , min_jobs_on_thread_(-1)
            , wait_count_(0)
        {
        }

        std::ostream& print(std::ostream& _ros) const override
        {
            _ros << " max_worker_count = " << max_worker_count_;
            _ros << " max_jobs_in_queue = " << max_jobs_in_queue_;
            _ros << " max_jobs_on_thread = " << max_jobs_on_thread_;
            _ros << " min_jobs_on_thread = " << min_jobs_on_thread_;
            _ros << " wait_count = " << wait_count_;
            return _ros;
        }
    } statistic_;
#endif

public:
    static constexpr size_t node_capacity = JobQueueT::node_capacity;

    WorkPool()
        : config_()
        , running_(false)
        , thr_cnt_(0)
        , worker_head_{nullptr}
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
        , worker_head_{nullptr}
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

    void dumpStatistics(const bool _dump_queue_too = true) const;

    void stop()
    {
        doStop();
    }

private:
    template <class JT>
    size_t doJobPush(const JT& _rj, const bool _wait = true);
    template <class JT>
    size_t doJobPush(JT&& _uj, const bool _wait = true);

    bool doJobPop(WorkerStub& _rws, Job& _rjob);

    void doStop();

    template <class JobHandlerFnc, typename... Args>
    void doStart(
        const WorkPoolConfiguration& _cfg,
        size_t                       _start_wkr_cnt,
        JobHandlerFnc                _job_handler_fnc,
        Args&&... _args);

    void doWorkerPush(WorkerStub& _rws);
    bool doWorkerPop(WorkerStub*& _rpws);

    void doRegisterWorker(WorkerStub& _rws);
    void doUnregisterWorker(WorkerStub& _rws);
}; //WorkPool

//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
template <class JT>
void WorkPool<Job, QNBits>::push(const JT& _jb)
{
    const size_t qsz     = doJobPush(_jb);
    const size_t thr_cnt = thr_cnt_.load();

    if (thr_cnt < config_.max_worker_count_ && qsz > thr_cnt) {
        std::lock_guard<std::mutex> lock(thr_mtx_);
        if (qsz > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
            thr_vec_.emplace_back(worker_factory_fnc_());
            ++thr_cnt_;
            solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
        }
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
template <class JT>
void WorkPool<Job, QNBits>::push(JT&& _jb)
{
    const size_t qsz     = doJobPush(std::move(_jb));
    const size_t thr_cnt = thr_cnt_.load();

    if (thr_cnt < config_.max_worker_count_ && qsz > thr_cnt) {
        std::lock_guard<std::mutex> lock(thr_mtx_);
        if (qsz > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
            thr_vec_.emplace_back(worker_factory_fnc_());
            ++thr_cnt_;
            solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
        }
    }
    solid_statistic_max(statistic_.max_jobs_in_queue_, qsz);
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
template <class JT>
bool WorkPool<Job, QNBits>::tryPush(const JT& _jb)
{
    const size_t qsz     = doJobPush(_jb, false /*wait*/);
    const size_t thr_cnt = thr_cnt_.load();

    if (qsz != InvalidSize()) {
        if (thr_cnt < config_.max_worker_count_ && qsz > thr_cnt) {
            std::lock_guard<std::mutex> lock(thr_mtx_);
            if (qsz > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
                thr_vec_.emplace_back(worker_factory_fnc_());
                ++thr_cnt_;
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
template <typename Job, size_t QNBits>
template <class JT>
bool WorkPool<Job, QNBits>::tryPush(JT&& _jb)
{
    const size_t qsz     = doJobPush(std::move(_jb), false /*wait*/);
    const size_t thr_cnt = thr_cnt_.load();

    if (qsz != InvalidSize()) {
        if (thr_cnt < config_.max_worker_count_ && qsz > thr_cnt) {
            std::lock_guard<std::mutex> lock(thr_mtx_);
            if (qsz > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
                thr_vec_.emplace_back(worker_factory_fnc_());
                ++thr_cnt_;
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
template <typename Job, size_t QNBits>
template <class JT>
size_t WorkPool<Job, QNBits>::doJobPush(const JT& _rj, const bool _wait)
{
    const size_t sz = job_q_ptr_->push(_rj, _wait);

    if (sz != InvalidSize()) {
        WorkerStub* pworker_stub;
        while (doWorkerPop(pworker_stub)) {
            solid_dbg(workpool_logger, Verbose, "pop_worker: "<<pworker_stub);
            //pworker_stub valid states: Wait, WaitCancel
            bool do_break = true;
            
            while(true){
                auto expect_state = WorkerStub::StateE::Wait;
                if(pworker_stub->state_.compare_exchange_strong(expect_state, WorkerStub::StateE::Notify)){
                    pworker_stub->wake();
                    break;
                }else{
                    solid_check(expect_state == WorkerStub::StateE::WaitCancel, "expect state not Cancel but: "<<static_cast<int>(expect_state));
                    if(pworker_stub->state_.compare_exchange_strong(expect_state, WorkerStub::StateE::Cancel)){
                        do_break = false;
                        break;
                    }else{
                        solid_check(expect_state == WorkerStub::StateE::Wait, "expect state not Wait but: "<<static_cast<int>(expect_state));
                        solid_dbg(workpool_logger, Verbose, "force notify: "<<pworker_stub);
                        continue;//try wake another thread
                    }
                }
            }
            if(do_break) break;
        }
    }
    return sz;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
template <class JT>
size_t WorkPool<Job, QNBits>::doJobPush(JT&& _rj, const bool _wait)
{
    const size_t sz = job_q_ptr_->push(std::move(_rj), _wait);

    if (sz != InvalidSize()) {
        WorkerStub* pworker_stub;
        while (doWorkerPop(pworker_stub)) {
            solid_dbg(workpool_logger, Verbose, "pop_worker: "<<pworker_stub);
            //pworker_stub valid states: Wait, WaitCancel
            bool do_break = true;
            
            while(true){
                auto expect_state = WorkerStub::StateE::Wait;
                if(pworker_stub->state_.compare_exchange_strong(expect_state, WorkerStub::StateE::Notify)){
                    pworker_stub->wake();
                    break;
                }else{
                    solid_check(expect_state == WorkerStub::StateE::WaitCancel, "expect state not Cancel but: "<<static_cast<int>(expect_state));
                    if(pworker_stub->state_.compare_exchange_strong(expect_state, WorkerStub::StateE::Cancel)){
                        do_break = false;
                        break;
                    }else{
                        solid_check(expect_state == WorkerStub::StateE::Wait, "expect state not Wait but: "<<static_cast<int>(expect_state));
                        solid_dbg(workpool_logger, Verbose, "force notify: "<<pworker_stub);
                        continue;//try wake another thread
                    }
                }
            }
            if(do_break) break;
        }
    }
    return sz;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
bool WorkPool<Job, QNBits>::doJobPop(WorkerStub& _rws, Job& _rjob)
{
    //_rws valid states: Cancel, WaitCancel
    auto expect_state = WorkerStub::StateE::WaitCancel;
    if(_rws.state_.compare_exchange_strong(expect_state, WorkerStub::StateE::Wait)){
    }else{
        solid_check(expect_state == WorkerStub::StateE::Cancel, "expect state not Cancel but: "<<static_cast<int>(expect_state));
        _rws.state_.store(WorkerStub::StateE::Wait);
        solid_dbg(workpool_logger, Verbose, "push_worker: "<<&_rws);
        doWorkerPush(_rws);
    }

    while (!job_q_ptr_->pop(_rjob)) {
        if (_rws.wait(job_mtx_, running_)) {
            solid_dbg(workpool_logger, Verbose, "push_worker: "<<&_rws <<" state = "<<static_cast<int>(_rws.state_.load()));
            _rws.state_.store(WorkerStub::StateE::Wait);
            doWorkerPush(_rws);
            continue;
        } else if(job_q_ptr_->pop(_rjob)){
            break;
        }else{
            solid_dbg(workpool_logger, Verbose, this << " no more waiting");
            return false;
        }
    }
    
    //_rws valid states: Wait, Notify
    
    expect_state = WorkerStub::StateE::Wait;
    if(_rws.state_.compare_exchange_strong(expect_state, WorkerStub::StateE::WaitCancel)){
        
    }else{
        solid_check(expect_state == WorkerStub::StateE::Notify, "expect state not Notify but: "<<static_cast<int>(expect_state));
        _rws.state_.store(WorkerStub::StateE::Cancel);
        WorkerStub* pworker_stub;
        while (doWorkerPop(pworker_stub)) {
            solid_dbg(workpool_logger, Verbose, "pop_worker: "<<pworker_stub);
            if(pworker_stub == &_rws){
                continue;
            }
            //pworker_stub valid states: Wait, WaitCancel
            bool do_break = true;
            
            while(true){
                auto expect_state = WorkerStub::StateE::Wait;
                if(pworker_stub->state_.compare_exchange_strong(expect_state, WorkerStub::StateE::Notify)){
                    pworker_stub->wake();
                    break;
                }else{
                    solid_check(expect_state == WorkerStub::StateE::WaitCancel, "expect state not Cancel but: "<<static_cast<int>(expect_state));
                    if(pworker_stub->state_.compare_exchange_strong(expect_state, WorkerStub::StateE::Cancel)){
                        do_break = false;
                        break;
                    }else{
                        solid_check(expect_state == WorkerStub::StateE::Wait, "expect state not Wait but: "<<static_cast<int>(expect_state));
                        solid_dbg(workpool_logger, Verbose, "force notify: "<<pworker_stub);
                        continue;//try wake another thread
                    }
                }
            }
            if(do_break) break;
        }
    }

    return true;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
void WorkPool<Job, QNBits>::doWorkerPush(WorkerStub& _rws)
{
    _rws.pnext_ = worker_head_.load(std::memory_order_relaxed);

    while (!worker_head_.compare_exchange_weak(_rws.pnext_, &_rws/*,
        std::memory_order_release,
        std::memory_order_relaxed*/))
        ;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
bool WorkPool<Job, QNBits>::doWorkerPop(WorkerStub*& _rpws)
{
    WorkerStub* pold_head = worker_head_.load();
    while (pold_head && !worker_head_.compare_exchange_strong(pold_head, pold_head->pnext_/*, std::memory_order_acquire, std::memory_order_relaxed*/))
        ;
    _rpws = pold_head;
    return pold_head != nullptr;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
void WorkPool<Job, QNBits>::doStop()
{
    bool expect = true;

    if (running_.compare_exchange_strong(expect, false)) {
    } else {
        return;
    }
    {
        std::unique_lock<std::mutex> lock(thr_mtx_);

        for (auto pcrt_ws = pregistered_worker_front_; pcrt_ws; pcrt_ws = pcrt_ws->plist_next_) {
            pcrt_ws->wake();
        }
        
        thr_cnd_.wait(lock, [this](){return thr_cnt_ == 0;});
        
        for (auto& t : thr_vec_) {
            t.join();
        }
        thr_vec_.clear();
    }
    
    dumpStatistics(false); //the queue statistic will be dumped on its destructor
    {
#ifdef SOLID_HAS_STATISTICS
        const size_t max_jobs_in_queue = config_.max_job_queue_size_ == static_cast<size_t>(-1) ? config_.max_job_queue_size_ : config_.max_job_queue_size_ + JobQueueT::node_capacity;
        solid_check_log(statistic_.max_jobs_in_queue_ <= max_jobs_in_queue, workpool_logger, "statistic_.max_jobs_in_queue_ = " << statistic_.max_jobs_in_queue_ << " <= config_.max_job_queue_size_ = " << max_jobs_in_queue);
        solid_check_log(statistic_.max_worker_count_ <= config_.max_worker_count_, workpool_logger, "statistic_.max_worker_count_ = " << statistic_.max_worker_count_ << " <= config_.max_worker_count_ = " << config_.max_worker_count_);
#endif
    }
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
template <class JobHandlerFnc, typename... Args>
void WorkPool<Job, QNBits>::doStart(
    const WorkPoolConfiguration& _cfg,
    size_t                       _start_wkr_cnt,
    JobHandlerFnc                _job_handler_fnc,
    Args&&... _args)
{

    auto lambda = [_job_handler_fnc, this, _args...]() {
        return std::thread(
            [this](JobHandlerFnc _job_handler_fnc, Args&&... _args) {
                uint64_t   job_count = 0;
                Job        job;
                WorkerStub stub;

                doRegisterWorker(stub);

                while (doJobPop(stub, job)) {
                    _job_handler_fnc(job, std::forward<Args>(_args)...);
                    solid_statistic_inc(job_count);
                }

                doUnregisterWorker(stub);

                solid_dbg(workpool_logger, Warning, this << " worker exited after handling " << job_count << " jobs");
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
        job_q_ptr_.reset(new JobQueueT(config_.max_job_queue_size_));
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
template <typename Job, size_t QNBits>
void WorkPool<Job, QNBits>::dumpStatistics(const bool _dump_queue_too) const
{
#ifdef SOLID_HAS_STATISTICS
    if (_dump_queue_too) {
        job_q_ptr_->dumpStatistics();
    }
    solid_log(workpool_logger, Statistic, "Workpool " << this << " statistic:" << this->statistic_);
#endif
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
void WorkPool<Job, QNBits>::doRegisterWorker(WorkerStub& _rws)
{
    std::unique_lock<std::mutex> lock(thr_mtx_);
    _rws.plist_prev_ = pregistered_worker_back_;
    
    if (pregistered_worker_back_ != nullptr) {
        pregistered_worker_back_->plist_next_ = &_rws;
        pregistered_worker_back_              = &_rws;
    } else {
        pregistered_worker_front_ = pregistered_worker_back_ = &_rws;
    }
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
void WorkPool<Job, QNBits>::doUnregisterWorker(WorkerStub& _rws)
{
    std::unique_lock<std::mutex> lock(thr_mtx_);
    if (_rws.plist_prev_ != nullptr) {
        _rws.plist_prev_->plist_next_ = _rws.plist_next_;
    } else {
        pregistered_worker_front_ = _rws.plist_next_;
    }

    if (_rws.plist_next_ != nullptr) {
        _rws.plist_next_->plist_prev_ = _rws.plist_prev_;
    } else {
        pregistered_worker_back_ = _rws.plist_prev_;
    }
    if(thr_cnt_.fetch_sub(1) == 1){
        thr_cnd_.notify_one();
    }
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
} //namespace lockfree
} //namespace solid
