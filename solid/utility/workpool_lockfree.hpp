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
    std::atomic<bool>       stacked_;
    std::condition_variable cv_;
    size_t                  next_      = 0;
    std::thread::id         pop_thr_id_;
    size_t                  pop_count_ = 0;
    std::atomic<size_t>     pop_counter_;
    uint16_t                aba_counter_ = 0;
    
    WorkerStub():state_(StateE::Cancel), stacked_(false), pop_counter_(0){}
    
    void wake()
    {
        cv_.notify_one();
    }

    bool wait(std::mutex& _rmutex, std::atomic<bool>& _rrunning)
    {
        size_t count = 64;
        StateE state;
        while ((state = state_.load(/*std::memory_order_acquire*/)) == StateE::Wait && count--) {
            std::this_thread::yield();
        }

        {
            std::unique_lock<std::mutex> lock{_rmutex};
            cv_.wait(lock, [&_rrunning, this]() { return state_.load() != StateE::Wait || !_rrunning; });
            //solid_check(state_.load() == WorkerStub::StateE::Notify, "expect state not Notify but: "<<static_cast<int>(state_.load()));
        }
        
        //NOTE: no push should happen after _rrunning is set to false
        return _rrunning;
    }
    
    inline static constexpr size_t thrId(const size_t _aba_id){
        return _aba_id & bits_to_mask((sizeof(size_t) - sizeof(aba_counter_)) * 8);
    }
    
    size_t abaId(const size_t _thr_id){
        return _thr_id | (static_cast<size_t>(++aba_counter_) << ((sizeof(size_t) - sizeof(aba_counter_)) * 8));
    }
};
//-----------------------------------------------------------------------------
struct ThreadStub{
    std::thread thread_;
    WorkerStub  *pworker_ = nullptr;
    
    ThreadStub(std::thread &&_rthr): thread_(std::move(_rthr)){}
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
    using WorkerFactoryT = std::function<std::thread(const size_t)>;
    using ThreadVectorT  = std::vector<ThreadStub>;
    using JobQueueT      = Queue<Job, QNBits>;
    using AtomicBoolT    = std::atomic<bool>;
    using JobQueuePtrT   = std::unique_ptr<JobQueueT>;

    WorkPoolConfiguration    config_;
    AtomicBoolT              running_;
    std::atomic<size_t>      thr_cnt_;
    size_t                   stopping_thr_cnt_ = 0;
    size_t                   starting_thr_cnt_ = 0;
    WorkerFactoryT           worker_factory_fnc_;
    JobQueuePtrT             job_q_ptr_;
    ThreadVectorT            thr_vec_;
    std::mutex               thr_mtx_;
    std::mutex               job_mtx_;
    std::condition_variable  thr_cnd_;
    std::atomic<size_t>      worker_head_;
    WorkerStub*              pregistered_worker_front_ = nullptr;
    WorkerStub*              pregistered_worker_back_  = nullptr;
    std::atomic<size_t>      push_count_;
    std::atomic<size_t>      pop_count_;
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
        , worker_head_{static_cast<size_t>(InvalidIndex())}
        , push_count_{0}
        , pop_count_{0}
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
        , worker_head_{static_cast<size_t>(InvalidIndex())}
        , push_count_{0}
        , pop_count_{0}
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
    
    bool doWorkerWake(WorkerStub *_pws = nullptr);

    bool doRegisterWorker(WorkerStub& _rws, const size_t _thr_id);
    void doUnregisterWorker(WorkerStub& _rws, const size_t _thr_id);
    
    WorkerStub& worker(const size_t _thr_id)const{
        return *thr_vec_[_thr_id].pworker_;
    }
}; //WorkPool

//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
template <class JT>
void WorkPool<Job, QNBits>::push(const JT& _jb)
{
    solid_check(running_.load(std::memory_order_relaxed));
    const size_t qsz     = doJobPush(_jb);
    const size_t thr_cnt = thr_cnt_.load();

    if (thr_cnt < config_.max_worker_count_ && qsz > thr_cnt) {
        std::lock_guard<std::mutex> lock(thr_mtx_);
        solid_check(running_.load(std::memory_order_relaxed));
        if (qsz > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
            ++thr_cnt_;
            thr_vec_.emplace_back(worker_factory_fnc_(thr_vec_.size()));
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
    solid_check(running_.load(std::memory_order_relaxed));
    const size_t qsz     = doJobPush(std::move(_jb));
    const size_t thr_cnt = thr_cnt_.load();

    if (thr_cnt < config_.max_worker_count_ && qsz > thr_cnt) {
        std::lock_guard<std::mutex> lock(thr_mtx_);
        solid_check(running_.load(std::memory_order_relaxed));
        if (qsz > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
            ++thr_cnt_;
            thr_vec_.emplace_back(worker_factory_fnc_(thr_vec_.size()));
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
    solid_check(running_.load(std::memory_order_relaxed));
    const size_t qsz     = doJobPush(_jb, false /*wait*/);
    const size_t thr_cnt = thr_cnt_.load();

    if (qsz != InvalidSize()) {
        if (thr_cnt < config_.max_worker_count_ && qsz > thr_cnt) {
            std::lock_guard<std::mutex> lock(thr_mtx_);
            solid_check(running_.load(std::memory_order_relaxed));
            if (qsz > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
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
template <typename Job, size_t QNBits>
template <class JT>
bool WorkPool<Job, QNBits>::tryPush(JT&& _jb)
{
    solid_check(running_.load(std::memory_order_relaxed));
    const size_t qsz     = doJobPush(std::move(_jb), false /*wait*/);
    const size_t thr_cnt = thr_cnt_.load();

    if (qsz != InvalidSize()) {
        if (thr_cnt < config_.max_worker_count_ && qsz > thr_cnt) {
            std::lock_guard<std::mutex> lock(thr_mtx_);
            solid_check(running_.load(std::memory_order_relaxed));
            if (qsz > thr_vec_.size() && thr_vec_.size() < config_.max_worker_count_) {
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
template <typename Job, size_t QNBits>
bool WorkPool<Job, QNBits>::doWorkerWake(WorkerStub *_pws){
    WorkerStub* pworker_stub;
    while (doWorkerPop(pworker_stub)) {
        //solid_dbg(workpool_logger, Verbose, "pop_worker: "<<pworker_stub);
        //pworker_stub valid states: Wait, WaitCancel
        bool do_break = true;
        size_t count = 0;
        while(true){
            ++count;
            if(pworker_stub != _pws){
            }else{
                continue;
            }
            auto expect_state = WorkerStub::StateE::Wait;
            if(pworker_stub->state_.compare_exchange_strong(expect_state, WorkerStub::StateE::Notify)){
                //solid_dbg(workpool_logger, Warning, "wake worker");
                std::lock_guard<std::mutex> lock(job_mtx_);
                pworker_stub->wake();
                return true;
            }else{
                solid_assert_logx(expect_state == WorkerStub::StateE::WaitCancel, workpool_logger, "expect state not WaitCancel but: "<<static_cast<int>(expect_state)<< " count "<<count);
                if(pworker_stub->state_.compare_exchange_strong(expect_state, WorkerStub::StateE::Cancel)){
                    //solid_dbg(workpool_logger, Warning, "worker canceled");
                    do_break = false;
                    break;
                }else{
                    solid_assert_logx(expect_state == WorkerStub::StateE::Wait, workpool_logger, "expect state not Wait but: "<<static_cast<int>(expect_state)<< " count "<<count);
                    //solid_dbg(workpool_logger, Warning, "force notify: "<<pworker_stub<< " count "<<count);
                    continue;//try wake another thread
                }
            }
        }
        if(do_break) break;
    }
    return false;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
template <class JT>
size_t WorkPool<Job, QNBits>::doJobPush(const JT& _rj, const bool _wait)
{
    const size_t sz = job_q_ptr_->push(_rj, _wait);

    if (sz != InvalidSize()) {
        ++push_count_;
        if(doWorkerWake()){
            
        }else{
            //solid_dbg(workpool_logger, Verbose, "no worker notified - "<<sz);
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
        ++push_count_;
        if(doWorkerWake()){
            
        }else{
            //solid_dbg(workpool_logger, Verbose, "no worker notified - "<<sz);
        }
    }
    return sz;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
bool WorkPool<Job, QNBits>::doJobPop(WorkerStub& _rws, const size_t thr_id_, Job& _rjob)
{
    //_rws valid states: Cancel, WaitCancel
    auto expect_state = WorkerStub::StateE::WaitCancel;
    //bool did_push = true;
    if(_rws.state_.compare_exchange_strong(expect_state, WorkerStub::StateE::Wait)){
        //did_push = false;
    }else{
        solid_assert_logx(expect_state == WorkerStub::StateE::Cancel, workpool_logger, "expect state not Cancel but: "<<static_cast<int>(expect_state));
        _rws.state_.store(WorkerStub::StateE::Wait);
        //solid_dbg(workpool_logger, Verbose, "push_worker: "<<&_rws);
        doWorkerPush(_rws, thr_id_);
    }
    size_t loop_count = 0;
    while (!job_q_ptr_->pop(_rjob)) {
        //solid_dbg(workpool_logger, Warning, "worker wait "<<pop_count_<<" "<<push_count_<<' '<<did_push<<' '<<loop_count);
        ++loop_count;
        if (_rws.wait(job_mtx_, running_)) {
            //solid_dbg(workpool_logger, Verbose, "push_worker: "<<&_rws <<" state = "<<static_cast<int>(_rws.state_.load()));
            //_rws.state_.store(WorkerStub::StateE::Wait);
            auto expect_state = WorkerStub::StateE::Notify;
            const bool ok = _rws.state_.compare_exchange_strong(expect_state, WorkerStub::StateE::Wait); 
            solid_assert_logx(ok, workpool_logger, "expect state not Notify, but: "<<static_cast<int>(expect_state));
            doWorkerPush(_rws, thr_id_);
            continue;
        } else if(job_q_ptr_->pop(_rjob)){
            break;
        }else{
            //solid_dbg(workpool_logger, Warning, this << " no more waiting");
            return false;
        }
    }
    ++pop_count_;
    //_rws valid states: Wait, Notify
    
    expect_state = WorkerStub::StateE::Wait;
    if(_rws.state_.compare_exchange_strong(expect_state, WorkerStub::StateE::WaitCancel)){
        
    }else{
        solid_assert_logx(expect_state == WorkerStub::StateE::Notify, workpool_logger, "expect state not Notify but: "<<static_cast<int>(expect_state));
        _rws.state_.store(WorkerStub::StateE::Cancel);
        //solid_dbg(workpool_logger, Warning, "already notified");
        if(doWorkerWake(&_rws)){
            
        }else{
            //solid_dbg(workpool_logger, Warning, "no worker notified!");
        }
    }

    return true;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
void WorkPool<Job, QNBits>::doWorkerPush(WorkerStub& _rws, const size_t _thr_id)
{
    //bool expect = false;
    //const bool stacked_ok = _rws.stacked_.compare_exchange_strong(expect, true);
    //solid_assert_log(stacked_ok, workpool_logger);
    //((void)stacked_ok);
    
    const size_t aba_id = _rws.abaId(_thr_id);
    _rws.next_ = worker_head_.load(std::memory_order_relaxed);
    
    while (!worker_head_.compare_exchange_weak(_rws.next_, aba_id/*,
        std::memory_order_release,
        std::memory_order_relaxed*/))
        ;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
bool WorkPool<Job, QNBits>::doWorkerPop(WorkerStub*& _rpws)
{
    size_t old_head = worker_head_.load();
    while (old_head != InvalidIndex() && !worker_head_.compare_exchange_weak(old_head, worker(WorkerStub::thrId(old_head)).next_/*, std::memory_order_acquire, std::memory_order_relaxed*/))
        ;
    
    if(old_head != InvalidIndex()){
        _rpws = &worker(WorkerStub::thrId(old_head));
        //bool expect = true;
        //const bool stacked_ok = _rpws->stacked_.compare_exchange_strong(expect, false);
        //if(stacked_ok){
        //    _rpws->pop_thr_id_ = std::this_thread::get_id();
        //    _rpws->pop_count_ = _rpws->pop_counter_.fetch_add(1);
        //}
        //solid_assert_logx(stacked_ok, workpool_logger, "last pop thr "<<_rpws->pop_thr_id_<<" vs "<<std::this_thread::get_id()<<" "<<_rpws->pop_count_<<" vs "<<_rpws->pop_counter_);
        //((void)stacked_ok);
        return true;
    }else{
        _rpws = nullptr;
        return false;
    }
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

        for (auto& t : thr_vec_) {
            if(t.pworker_){
                t.pworker_->wake();
            }
        }
        
        thr_cnd_.wait(lock, [this](){return thr_cnt_ == 0;});
        
        for (auto& t : thr_vec_) {
            t.thread_.join();
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

    auto lambda = [_job_handler_fnc, this, _args...](const size_t _id) {
        return std::thread(
            [this](const size_t _id, JobHandlerFnc _job_handler_fnc, Args&&... _args) {
                uint64_t   job_count = 0;
                Job        job;
                WorkerStub stub;
                if(!doRegisterWorker(stub, _id)) return;

                while (doJobPop(stub, _id, job)) {
                    _job_handler_fnc(job, std::forward<Args>(_args)...);
                    solid_statistic_inc(job_count);
                }

                doUnregisterWorker(stub, _id);

                solid_dbg(workpool_logger, Warning, this << " worker exited after handling " << job_count << " jobs");
                solid_statistic_max(statistic_.max_jobs_on_thread_, job_count);
                solid_statistic_min(statistic_.min_jobs_on_thread_, job_count);
            }, _id,
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
            
            thr_vec_.reserve(config_.max_worker_count_);
            
            thr_cnt_ += _start_wkr_cnt;
            for (size_t i = 0; i < _start_wkr_cnt; ++i) {
                thr_vec_.emplace_back(worker_factory_fnc_(thr_vec_.size()));
                solid_statistic_max(statistic_.max_worker_count_, thr_vec_.size());
            }
            thr_cnd_.wait(lock, [this](){return thr_cnt_ == starting_thr_cnt_;});
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
bool WorkPool<Job, QNBits>::doRegisterWorker(WorkerStub& _rws, const size_t _thr_id)
{
    std::unique_lock<std::mutex> lock(thr_mtx_);
    if(running_){
        thr_vec_[_thr_id].pworker_ = &_rws;
        ++starting_thr_cnt_;
        if(starting_thr_cnt_ == thr_cnt_){
            thr_cnd_.notify_all();
        }
        return true;
    }
    if(thr_cnt_.fetch_sub(1) == 1){
        thr_cnd_.notify_all();
    }
    return false;
}
//-----------------------------------------------------------------------------
template <typename Job, size_t QNBits>
void WorkPool<Job, QNBits>::doUnregisterWorker(WorkerStub& _rws, const size_t _thr_id)
{
    std::unique_lock<std::mutex> lock(thr_mtx_);
    ++stopping_thr_cnt_;
    
    if(stopping_thr_cnt_ == thr_cnt_){
        thr_cnd_.notify_all();
    }else{
        thr_cnd_.wait(lock, [this](){return thr_cnt_ <= stopping_thr_cnt_;});
    }
    
    if(thr_cnt_.fetch_sub(1) == 1){
        thr_cnd_.notify_all();
    }
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
} //namespace lockfree
} //namespace solid
