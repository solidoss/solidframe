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

class ErrorCategory : public ErrorCategoryT {
public:
    enum {
        AlreadyE = 1,
        WorkerE,
        RunningE,
        ReactorE
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
        case AlreadyE:
            return "Already started";
        case WorkerE:
            return "Failed to start worker";
        case RunningE:
            return "Scheduler not running";
        case ReactorE:
            return "Reactor failure";
        default:
            return "Unknown";
        }
    }
};

const ErrorCategory ec;

inline ErrorConditionT error_already()
{
    return ErrorConditionT(ErrorCategory::AlreadyE, ec);
}
inline ErrorConditionT error_worker()
{
    return ErrorConditionT(ErrorCategory::WorkerE, ec);
}

inline ErrorConditionT error_running()
{
    return ErrorConditionT(ErrorCategory::RunningE, ec);
}

// inline ErrorConditionT error_reactor(){
//  return ErrorConditionT(ErrorCategory::ReactorE, ec);
// }

} //namespace

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

enum Statuses {
    StatusStoppedE = 0,
    StatusStartingWaitE,
    StatusStartingErrorE,
    StatusRunningE,
    StatusStoppingE,
    StatusStoppingWaitE
};

typedef std::atomic<Statuses> AtomicStatuesT;

struct SchedulerBase::Data {
    Data()
        : crtreactoridx(0)
        , reactorcnt(0)
        , stopwaitcnt(0)
        , status(StatusStoppedE)
        , usecnt(0)
    {
    }

    size_t               crtreactoridx;
    size_t               reactorcnt;
    size_t               stopwaitcnt;
    AtomicStatuesT       status;
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

ErrorConditionT SchedulerBase::doStart(
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

        if (impl_->status == StatusRunningE) {
            return error_already();
        }

        if (impl_->status != StatusStoppedE || impl_->stopwaitcnt != 0u) {
            do {
                impl_->cnd.wait(lock);
            } while (impl_->status != StatusStoppedE || impl_->stopwaitcnt != 0u);
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
            //rrs.thrptr.reset((*_pf)(*this, i));

            if (!(*_pf)(*this, i, rrs.thr)) {
                start_err = true;
                break;
            }
        }

        if (!start_err) {
            impl_->status = StatusStartingWaitE;

            do {
                impl_->cnd.wait(lock);
            } while (impl_->status == StatusStartingWaitE && impl_->reactorcnt != impl_->reactorvec.size());

            if (impl_->status == StatusStartingErrorE) {
                impl_->status = StatusStoppingWaitE;
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
            impl_->status = StatusRunningE;
            return ErrorConditionT();
        }
    }

    for (auto& v : impl_->reactorvec) {
        if (v.isActive()) {
            v.thr.join();
            v.clear();
        }
    }

    lock_guard<mutex> lock(impl_->mtx);
    impl_->status = StatusStoppedE;
    impl_->cnd.notify_all();
    return error_worker();
}

void SchedulerBase::doStop(bool _wait /* = true*/)
{
    {
        unique_lock<mutex> lock(impl_->mtx);

        if (impl_->status == StatusStartingWaitE || impl_->status == StatusStartingErrorE) {
            do {
                impl_->cnd.wait(lock);
            } while (impl_->status == StatusStartingWaitE || impl_->status == StatusStartingErrorE);
        }

        if (impl_->status == StatusRunningE) {
            impl_->status = _wait ? StatusStoppingWaitE : StatusStoppingE;

            for (auto& v : impl_->reactorvec) {
                if (v.preactor != nullptr) {
                    v.preactor->stop();
                }
            }
        } else if (impl_->status == StatusStoppingE) {
            impl_->status = _wait ? StatusStoppingWaitE : StatusStoppingE;
        } else if (impl_->status == StatusStoppingWaitE) {
            if (_wait) {
                ++impl_->stopwaitcnt;
                do {
                    impl_->cnd.wait(lock);
                } while (impl_->status != StatusStoppedE);
                --impl_->stopwaitcnt;
                impl_->cnd.notify_one();
            }
            return;
        } else if (impl_->status == StatusStoppedE) {
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
        impl_->status = StatusStoppedE;
        impl_->cnd.notify_all();
    }
}

ObjectIdT SchedulerBase::doStartObject(ObjectBase& _robj, Service& _rsvc, ScheduleFunctionT& _rfct, ErrorConditionT& _rerr)
{
    ++impl_->usecnt;
    ObjectIdT rv;
    if (impl_->status == StatusRunningE) {
        ReactorStub& rrs = impl_->reactorvec[doComputeScheduleReactorIndex()];

        rv = _rsvc.registerObject(_robj, *rrs.preactor, _rfct, _rerr);
    } else {
        _rerr = error_running();
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
    default:
        break;
    }

    const size_t cwi     = impl_->crtreactoridx;
    impl_->crtreactoridx = (cwi + 1) % impl_->reactorvec.size();
    return cwi;
}

bool SchedulerBase::prepareThread(const size_t _idx, ReactorBase& _rreactor, const bool _success)
{
    const bool thrensuccess = impl_->threnfnc();
    {
        lock_guard<mutex> lock(impl_->mtx);
        ReactorStub&      rrs = impl_->reactorvec[_idx];

        if (_success && impl_->status == StatusStartingWaitE && thrensuccess) {
            rrs.preactor = &_rreactor;
            ++impl_->reactorcnt;
            impl_->cnd.notify_all();
            return true;
        }

        impl_->status = StatusStartingErrorE;
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

} //namespace frame
} //namespace solid
