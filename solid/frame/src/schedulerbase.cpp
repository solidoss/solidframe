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

typedef Queue<UniqueId>     UidQueueT;
typedef Stack<UniqueId>     UidStackT;
typedef std::atomic<size_t> AtomicSizeT;

struct ReactorStub {
    ReactorStub(ReactorBase* _preactor = nullptr)
        : preactor(_preactor)
    {
    }

    ReactorStub(ReactorStub&& _urs) noexcept
        : thr(std::move(_urs.thr))
        , preactor(_urs.preactor)
    {
        _urs.preactor = nullptr;
    }

    bool isActive() const
    {
        static const std::thread empty_thread{};
        return thr.get_id() != empty_thread.get_id();
    }

    void clear()
    {
        preactor = nullptr;
    }

    thread       thr;
    ReactorBase* preactor;
};

typedef std::vector<ReactorStub> ReactorVectorT;

enum struct StatusE {
    Stopped = 0,
    StartingWait,
    StartingError,
    Running,
    Stopping,
    StoppingWait
};

typedef std::atomic<StatusE> AtomicStatusT;

struct SchedulerBase::Data {
    Data()
        : crtreactoridx(0)
        , reactorcnt(0)
        , stopwaitcnt(0)
        , status(StatusE::Stopped)
        , usecnt(0)
    {
    }

    AtomicSizeT          crtreactoridx;
    AtomicSizeT          reactorcnt;
    size_t               stopwaitcnt;
    AtomicStatusT        status;
    AtomicSizeT          usecnt;
    ThreadEnterFunctionT threnfnc;
    ThreadExitFunctionT  threxfnc;
    ReactorVectorT       reactorvec;
    mutex                mtx;
    condition_variable   cnd;
};

SchedulerBase::SchedulerBase()
    : impl_(make_pimpl<Data>())
{
}

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

size_t SchedulerBase::workerCount()const
{
    return impl_->reactorcnt.load(std::memory_order_relaxed);
}

void SchedulerBase::doStart(
    CreateWorkerF         _pf,
    ThreadEnterFunctionT& _renf, ThreadExitFunctionT& _rexf,
    size_t _reactorcnt)
{
    if (_reactorcnt == 0) {
        _reactorcnt = thread::hardware_concurrency();
    }
    bool start_err = false;

    {
        unique_lock<mutex> lock(impl_->mtx);

        solid_check_log(impl_->status != StatusE::Running, logger, "Scheduler already running");

        if (impl_->status != StatusE::Stopped || impl_->stopwaitcnt != 0u) {
            do {
                impl_->cnd.wait(lock);
            } while (impl_->status != StatusE::Stopped || impl_->stopwaitcnt != 0u);
        }

        impl_->reactorvec.resize(_reactorcnt);

        if (!solid_function_empty(_renf)) {
            solid_function_clear(impl_->threnfnc);
            std::swap(impl_->threnfnc, _renf);
        } else {
            impl_->threnfnc = &dummy_thread_enter;
        }

        if (!solid_function_empty(_rexf)) {
            solid_function_clear(impl_->threxfnc);
            std::swap(impl_->threxfnc, _rexf);
        } else {
            impl_->threxfnc = &dummy_thread_exit;
        }

        for (size_t i = 0; i < _reactorcnt; ++i) {
            ReactorStub& rrs = impl_->reactorvec[i];

            if (!(*_pf)(*this, i, rrs.thr)) {
                start_err = true;
                break;
            }
        }

        if (!start_err) {
            impl_->status = StatusE::StartingWait;

            do {
                impl_->cnd.wait(lock);
            } while (impl_->status == StatusE::StartingWait && impl_->reactorcnt != impl_->reactorvec.size());

            if (impl_->status == StatusE::StartingError) {
                impl_->status = StatusE::StoppingWait;
                start_err     = true;
            }
        }

        if (start_err) {
            for (auto& v : impl_->reactorvec) {
                if (v.preactor != nullptr) {
                    v.preactor->stop();
                }
            }
        } else {
            impl_->status = StatusE::Running;
            return;
        }
    }

    for (auto& v : impl_->reactorvec) {
        if (v.isActive()) {
            v.thr.join();
            v.clear();
        }
    }

    lock_guard<mutex> lock(impl_->mtx);
    impl_->status = StatusE::Stopped;
    impl_->cnd.notify_all();
    solid_throw_log(logger, "Failed starting worker");
}

void SchedulerBase::doStop(bool _wait /* = true*/)
{
    {
        unique_lock<mutex> lock(impl_->mtx);

        if (impl_->status == StatusE::StartingWait || impl_->status == StatusE::StartingError) {
            do {
                impl_->cnd.wait(lock);
            } while (impl_->status == StatusE::StartingWait || impl_->status == StatusE::StartingError);
        }

        if (impl_->status == StatusE::Running) {
            impl_->status = _wait ? StatusE::StoppingWait : StatusE::Stopping;

            for (auto& v : impl_->reactorvec) {
                if (v.preactor != nullptr) {
                    v.preactor->stop();
                }
            }
        } else if (impl_->status == StatusE::Stopping) {
            impl_->status = _wait ? StatusE::StoppingWait : StatusE::Stopping;
        } else if (impl_->status == StatusE::StoppingWait) {
            if (_wait) {
                ++impl_->stopwaitcnt;
                do {
                    impl_->cnd.wait(lock);
                } while (impl_->status != StatusE::Stopped);
                --impl_->stopwaitcnt;
                impl_->cnd.notify_one();
            }
            return;
        } else if (impl_->status == StatusE::Stopped) {
            return;
        }
    }

    if (_wait) {
        while (impl_->usecnt != 0u) {
            this_thread::yield();
        }
        for (auto& v : impl_->reactorvec) {
            if (v.isActive()) {
                v.thr.join();
                v.clear();
            }
        }
        lock_guard<mutex> lock(impl_->mtx);
        impl_->status = StatusE::Stopped;
        impl_->cnd.notify_all();
    }
}

ActorIdT SchedulerBase::doStartActor(ActorBase& _ract, Service& _rsvc, ScheduleFunctionT& _rfct, ErrorConditionT& _rerr)
{
    ++impl_->usecnt;
    ActorIdT rv;
    if (impl_->status == StatusE::Running) {
        ReactorStub& rrs = impl_->reactorvec[doComputeScheduleReactorIndex()];

        rv = _rsvc.registerActor(_ract, *rrs.preactor, _rfct, _rerr);
    } else {
        _rerr = error_running;
    }
    --impl_->usecnt;
    return rv;
}

ActorIdT SchedulerBase::doStartActor(ActorBase& _ract, Service& _rsvc, const size_t _workerIndex, ScheduleFunctionT& _rfct, ErrorConditionT& _rerr)
{
    ++impl_->usecnt;
    ActorIdT rv;
    if (impl_->status == StatusE::Running) {
        if(_workerIndex < workerCount()){
            ReactorStub& rrs = impl_->reactorvec[_workerIndex];
            rv = _rsvc.registerActor(_ract, *rrs.preactor, _rfct, _rerr);
        }else{
            _rerr = error_worker_index;
        }
    } else {
        _rerr = error_running;
    }
    --impl_->usecnt;
    return rv;
}


bool less_cmp(ReactorStub const& _rrs1, ReactorStub const& _rrs2)
{
    return _rrs1.preactor->load() < _rrs2.preactor->load();
}

size_t SchedulerBase::doComputeScheduleReactorIndex()
{
    switch (impl_->reactorvec.size()) {
    case 1:
        return 0;
    case 2: {
        return find_cmp(impl_->reactorvec.begin(), less_cmp, std::integral_constant<size_t, 2>());
    }
    case 3: {
        return find_cmp(impl_->reactorvec.begin(), less_cmp, std::integral_constant<size_t, 3>());
    }
    case 4: {
        return find_cmp(impl_->reactorvec.begin(), less_cmp, std::integral_constant<size_t, 4>());
    }
    case 5: {
        return find_cmp(impl_->reactorvec.begin(), less_cmp, std::integral_constant<size_t, 5>());
    }
    case 6: {
        return find_cmp(impl_->reactorvec.begin(), less_cmp, std::integral_constant<size_t, 6>());
    }
    case 7: {
        return find_cmp(impl_->reactorvec.begin(), less_cmp, std::integral_constant<size_t, 7>());
    }
    case 8: {
        return find_cmp(impl_->reactorvec.begin(), less_cmp, std::integral_constant<size_t, 8>());
    }
    case 9: {
        return find_cmp(impl_->reactorvec.begin(), less_cmp, std::integral_constant<size_t, 9>());
    }
    case 10: {
        return find_cmp(impl_->reactorvec.begin(), less_cmp, std::integral_constant<size_t, 10>());
    }
    case 11: {
        return find_cmp(impl_->reactorvec.begin(), less_cmp, std::integral_constant<size_t, 11>());
    }
    case 12: {
        return find_cmp(impl_->reactorvec.begin(), less_cmp, std::integral_constant<size_t, 12>());
    }
    case 13: {
        return find_cmp(impl_->reactorvec.begin(), less_cmp, std::integral_constant<size_t, 13>());
    }
    case 14: {
        return find_cmp(impl_->reactorvec.begin(), less_cmp, std::integral_constant<size_t, 14>());
    }
    case 15: {
        return find_cmp(impl_->reactorvec.begin(), less_cmp, std::integral_constant<size_t, 15>());
    }
    case 16: {
        return find_cmp(impl_->reactorvec.begin(), less_cmp, std::integral_constant<size_t, 16>());
    }
    default:
        break;
    }

    const size_t cwi  = impl_->crtreactoridx.fetch_add(1);
    return cwi % impl_->reactorcnt.load(std::memory_order_relaxed);
}

bool SchedulerBase::prepareThread(const size_t _idx, ReactorBase& _rreactor, const bool _success)
{
    const bool thrensuccess = impl_->threnfnc();
    {
        lock_guard<mutex> lock(impl_->mtx);
        ReactorStub&      rrs = impl_->reactorvec[_idx];

        if (_success && impl_->status == StatusE::StartingWait && thrensuccess) {
            rrs.preactor = &_rreactor;
            ++impl_->reactorcnt;
            impl_->cnd.notify_all();
            return true;
        }

        impl_->status = StatusE::StartingError;
        impl_->cnd.notify_one();
    }

    impl_->threxfnc();

    return false;
}

void SchedulerBase::unprepareThread(const size_t _idx, ReactorBase& /*_rreactor*/)
{
    {
        lock_guard<mutex> lock(impl_->mtx);
        ReactorStub&      rrs = impl_->reactorvec[_idx];
        rrs.preactor          = nullptr;
        --impl_->reactorcnt;
        impl_->cnd.notify_one();
    }
    impl_->threxfnc();
}

} // namespace frame
} // namespace solid
