// solid/frame/src/execscheduler.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/frame/schedulerbase.hpp"
#include "solid/frame/manager.hpp"
#include "solid/frame/reactorbase.hpp"
#include "solid/frame/service.hpp"
#include "solid/system/cassert.hpp"
#include "solid/utility/algorithm.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include <memory>

using namespace std;

namespace solid {
namespace frame {

namespace {
const LoggerT logger("solid::frame::scheduler");

class ErrorCategory : public ErrorCategoryT {
public:
    enum {
        Already = 1,
        Worker,
        Running,
        Reactor,
        WorkerIndex,
    };

    ErrorCategory() {}

private:
    const char* name() const noexcept override
    {
        return "solid::frame::Scheduler";
    }

    std::string message(int _ev) const override
    {
        switch (_ev) {
        case Already:
            return "Already started";
        case Worker:
            return "Failed to start worker";
        case Running:
            return "Scheduler not running";
        case Reactor:
            return "Reactor failure";
        case WorkerIndex:
            return "Invalid Worker Index";
        default:
            return "Unknown";
        }
    }
};

const ErrorCategory ec;

const ErrorConditionT error_already{ErrorCategory::Already, ec};
const ErrorConditionT error_worker{ErrorCategory::Worker, ec};
const ErrorConditionT error_running{ErrorCategory::Running, ec};
const ErrorConditionT error_worker_index{ErrorCategory::WorkerIndex, ec};

} // namespace

using AtomicSizeT = std::atomic<size_t>;

struct ReactorStub {
    thread       thread_;
    ReactorBase* preactor_;

    ReactorStub(ReactorBase* _preactor = nullptr)
        : preactor_(_preactor)
    {
    }

    ReactorStub(ReactorStub&& _other) noexcept
        : thread_(std::move(_other.thread_))
        , preactor_(_other.preactor_)
    {
        _other.preactor_ = nullptr;
    }

    bool isActive() const
    {
        static const std::thread empty_thread{};
        return thread_.get_id() != empty_thread.get_id();
    }

    void clear()
    {
        preactor_ = nullptr;
    }
};

using ReactorVectorT = std::vector<ReactorStub>;

enum struct StatusE {
    Stopped = 0,
    StartingWait,
    StartingError,
    Running,
    Stopping,
    StoppingWait
};

using AtomicStatusT = std::atomic<StatusE>;

struct SchedulerBase::Data {
    AtomicSizeT          crt_reactor_idx_;
    AtomicSizeT          reactor_cnt_;
    size_t               stop_wait_cnt_;
    AtomicStatusT        status_;
    AtomicSizeT          use_cnt_;
    ThreadEnterFunctionT thread_enter_fnc_;
    ThreadExitFunctionT  thread_exit_fnc_;
    ReactorVectorT       reactor_vec_;
    mutex                mtx_;
    condition_variable   cnd_;

    Data()
        : crt_reactor_idx_(0)
        , reactor_cnt_(0)
        , stop_wait_cnt_(0)
        , status_(StatusE::Stopped)
        , use_cnt_(0)
    {
    }
};

SchedulerBase::SchedulerBase() = default;

SchedulerBase::~SchedulerBase()
{
    doStop(true);
}

bool dummy_thread_enter()
{
    return true;
}
void dummy_thread_exit()
{
}

size_t SchedulerBase::workerCount() const
{
    return pimpl_->reactor_cnt_.load(std::memory_order_relaxed);
}

void SchedulerBase::doStart(
    CreateWorkerF         _pf,
    ThreadEnterFunctionT& _renter_fnc, ThreadExitFunctionT& _rexit_fnc,
    size_t _reactorcnt, const size_t _wake_capacity)
{
    if (_reactorcnt == 0) {
        _reactorcnt = thread::hardware_concurrency();
    }
    bool start_err = false;

    {
        unique_lock<mutex> lock(pimpl_->mtx_);

        solid_check_log(pimpl_->status_ != StatusE::Running, logger, "Scheduler already running");

        if (pimpl_->status_ != StatusE::Stopped || pimpl_->stop_wait_cnt_ != 0u) {
            do {
                pimpl_->cnd_.wait(lock);
            } while (pimpl_->status_ != StatusE::Stopped || pimpl_->stop_wait_cnt_ != 0u);
        }

        pimpl_->reactor_vec_.resize(_reactorcnt);

        if (!solid_function_empty(_renter_fnc)) {
            solid_function_clear(pimpl_->thread_enter_fnc_);
            std::swap(pimpl_->thread_enter_fnc_, _renter_fnc);
        } else {
            pimpl_->thread_enter_fnc_ = &dummy_thread_enter;
        }

        if (!solid_function_empty(_rexit_fnc)) {
            solid_function_clear(pimpl_->thread_exit_fnc_);
            std::swap(pimpl_->thread_exit_fnc_, _rexit_fnc);
        } else {
            pimpl_->thread_exit_fnc_ = &dummy_thread_exit;
        }

        for (size_t i = 0; i < _reactorcnt; ++i) {
            ReactorStub& rrs = pimpl_->reactor_vec_[i];

            if (!(*_pf)(*this, i, rrs.thread_, _wake_capacity)) {
                start_err = true;
                break;
            }
        }

        if (!start_err) {
            pimpl_->status_ = StatusE::StartingWait;

            do {
                pimpl_->cnd_.wait(lock);
            } while (pimpl_->status_ == StatusE::StartingWait && pimpl_->reactor_cnt_ != pimpl_->reactor_vec_.size());

            if (pimpl_->status_ == StatusE::StartingError) {
                pimpl_->status_ = StatusE::StoppingWait;
                start_err       = true;
            }
        }

        if (start_err) {
            for (auto& v : pimpl_->reactor_vec_) {
                if (v.preactor_ != nullptr) {
                    v.preactor_->stop();
                }
            }
        } else {
            pimpl_->status_ = StatusE::Running;
            return;
        }
    }

    for (auto& v : pimpl_->reactor_vec_) {
        if (v.isActive()) {
            v.thread_.join();
            v.clear();
        }
    }

    lock_guard<mutex> lock(pimpl_->mtx_);
    pimpl_->status_ = StatusE::Stopped;
    pimpl_->cnd_.notify_all();
    solid_throw_log(logger, "Failed starting worker");
}

void SchedulerBase::doStop(bool _wait /* = true*/)
{
    {
        unique_lock<mutex> lock(pimpl_->mtx_);

        if (pimpl_->status_ == StatusE::StartingWait || pimpl_->status_ == StatusE::StartingError) {
            do {
                pimpl_->cnd_.wait(lock);
            } while (pimpl_->status_ == StatusE::StartingWait || pimpl_->status_ == StatusE::StartingError);
        }

        if (pimpl_->status_ == StatusE::Running) {
            pimpl_->status_ = _wait ? StatusE::StoppingWait : StatusE::Stopping;

            for (auto& v : pimpl_->reactor_vec_) {
                if (v.preactor_ != nullptr) {
                    v.preactor_->stop();
                }
            }
        } else if (pimpl_->status_ == StatusE::Stopping) {
            pimpl_->status_ = _wait ? StatusE::StoppingWait : StatusE::Stopping;
        } else if (pimpl_->status_ == StatusE::StoppingWait) {
            if (_wait) {
                ++pimpl_->stop_wait_cnt_;
                do {
                    pimpl_->cnd_.wait(lock);
                } while (pimpl_->status_ != StatusE::Stopped);
                --pimpl_->stop_wait_cnt_;
                pimpl_->cnd_.notify_one();
            }
            return;
        } else if (pimpl_->status_ == StatusE::Stopped) {
            return;
        }
    }

    if (_wait) {
        while (pimpl_->use_cnt_ != 0u) {
            this_thread::yield();
        }
        for (auto& v : pimpl_->reactor_vec_) {
            if (v.isActive()) {
                v.thread_.join();
                v.clear();
            }
        }
        lock_guard<mutex> lock(pimpl_->mtx_);
        pimpl_->status_ = StatusE::Stopped;
        pimpl_->cnd_.notify_all();
    }
}

ActorIdT SchedulerBase::doStartActor(ActorBase& _ract, Service& _rsvc, ScheduleFunctionT& _rfnc, ErrorConditionT& _rerr)
{
    ++pimpl_->use_cnt_;
    ActorIdT rv;
    if (pimpl_->status_ == StatusE::Running) {
        ReactorStub& rrs = pimpl_->reactor_vec_[doComputeScheduleReactorIndex()];

        rv = _rsvc.registerActor(_ract, *rrs.preactor_, _rfnc, _rerr);
    } else {
        _rerr = error_running;
    }
    --pimpl_->use_cnt_;
    return rv;
}

ActorIdT SchedulerBase::doStartActor(ActorBase& _ract, Service& _rsvc, const size_t _workerIndex, ScheduleFunctionT& _rfnc, ErrorConditionT& _rerr)
{
    ++pimpl_->use_cnt_;
    ActorIdT rv;
    if (pimpl_->status_ == StatusE::Running) {
        if (_workerIndex < workerCount()) {
            ReactorStub& rrs = pimpl_->reactor_vec_[_workerIndex];
            rv               = _rsvc.registerActor(_ract, *rrs.preactor_, _rfnc, _rerr);
        } else {
            _rerr = error_worker_index;
        }
    } else {
        _rerr = error_running;
    }
    --pimpl_->use_cnt_;
    return rv;
}

bool less_cmp(ReactorStub const& _rrs1, ReactorStub const& _rrs2)
{
    return _rrs1.preactor_->load() < _rrs2.preactor_->load();
}

size_t SchedulerBase::doComputeScheduleReactorIndex()
{
    switch (pimpl_->reactor_vec_.size()) {
    case 1:
        return 0;
    case 2: {
        return find_cmp(pimpl_->reactor_vec_.begin(), less_cmp, std::integral_constant<size_t, 2>());
    }
    case 3: {
        return find_cmp(pimpl_->reactor_vec_.begin(), less_cmp, std::integral_constant<size_t, 3>());
    }
    case 4: {
        return find_cmp(pimpl_->reactor_vec_.begin(), less_cmp, std::integral_constant<size_t, 4>());
    }
    case 5: {
        return find_cmp(pimpl_->reactor_vec_.begin(), less_cmp, std::integral_constant<size_t, 5>());
    }
    case 6: {
        return find_cmp(pimpl_->reactor_vec_.begin(), less_cmp, std::integral_constant<size_t, 6>());
    }
    case 7: {
        return find_cmp(pimpl_->reactor_vec_.begin(), less_cmp, std::integral_constant<size_t, 7>());
    }
    case 8: {
        return find_cmp(pimpl_->reactor_vec_.begin(), less_cmp, std::integral_constant<size_t, 8>());
    }
    case 9: {
        return find_cmp(pimpl_->reactor_vec_.begin(), less_cmp, std::integral_constant<size_t, 9>());
    }
    case 10: {
        return find_cmp(pimpl_->reactor_vec_.begin(), less_cmp, std::integral_constant<size_t, 10>());
    }
    case 11: {
        return find_cmp(pimpl_->reactor_vec_.begin(), less_cmp, std::integral_constant<size_t, 11>());
    }
    case 12: {
        return find_cmp(pimpl_->reactor_vec_.begin(), less_cmp, std::integral_constant<size_t, 12>());
    }
    case 13: {
        return find_cmp(pimpl_->reactor_vec_.begin(), less_cmp, std::integral_constant<size_t, 13>());
    }
    case 14: {
        return find_cmp(pimpl_->reactor_vec_.begin(), less_cmp, std::integral_constant<size_t, 14>());
    }
    case 15: {
        return find_cmp(pimpl_->reactor_vec_.begin(), less_cmp, std::integral_constant<size_t, 15>());
    }
    case 16: {
        return find_cmp(pimpl_->reactor_vec_.begin(), less_cmp, std::integral_constant<size_t, 16>());
    }
    default:
        break;
    }

    const size_t cwi = pimpl_->crt_reactor_idx_.fetch_add(1);
    return cwi % pimpl_->reactor_cnt_.load(std::memory_order_relaxed);
}

bool SchedulerBase::prepareThread(const size_t _idx, ReactorBase& _rreactor, const bool _success)
{
    const bool thrensuccess = pimpl_->thread_enter_fnc_();
    {
        lock_guard<mutex> lock(pimpl_->mtx_);
        ReactorStub&      rrs = pimpl_->reactor_vec_[_idx];

        if (_success && pimpl_->status_ == StatusE::StartingWait && thrensuccess) {
            rrs.preactor_ = &_rreactor;
            ++pimpl_->reactor_cnt_;
            pimpl_->cnd_.notify_all();
            return true;
        }

        pimpl_->status_ = StatusE::StartingError;
        pimpl_->cnd_.notify_one();
    }

    pimpl_->thread_exit_fnc_();

    return false;
}

void SchedulerBase::unprepareThread(const size_t _idx, ReactorBase& /*_rreactor*/)
{
    {
        lock_guard<mutex> lock(pimpl_->mtx_);
        ReactorStub&      rrs = pimpl_->reactor_vec_[_idx];
        rrs.preactor_         = nullptr;
        --pimpl_->reactor_cnt_;
        pimpl_->cnd_.notify_one();
    }
    pimpl_->thread_exit_fnc_();
}

} // namespace frame
} // namespace solid
