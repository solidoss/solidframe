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
    const char* name() const noexcept(true)
    {
        return "solid::frame::Scheduler";
    }

    std::string message(int _ev) const
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

    ReactorStub(ReactorStub&& _urs)
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
    : impl(std::make_unique<Data>())
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
        unique_lock<mutex> lock(impl->mtx);

        if (impl->status == StatusRunningE) {
            return error_already();
        } else if (impl->status != StatusStoppedE || impl->stopwaitcnt) {
            do {
                impl->cnd.wait(lock);
            } while (impl->status != StatusStoppedE || impl->stopwaitcnt);
        }

        impl->reactorvec.resize(_reactorcnt);

        if (not SOLID_FUNCTION_EMPTY(_renf)) {
            SOLID_FUNCTION_CLEAR(impl->threnfnc);
            std::swap(impl->threnfnc, _renf);
        } else {
            impl->threnfnc = dummy_thread_enter;
        }

        if (not SOLID_FUNCTION_EMPTY(_rexf)) {
            SOLID_FUNCTION_CLEAR(impl->threxfnc);
            std::swap(impl->threxfnc, _rexf);
        } else {
            impl->threxfnc = dummy_thread_exit;
        }

        for (size_t i = 0; i < _reactorcnt; ++i) {
            ReactorStub& rrs = impl->reactorvec[i];
            //rrs.thrptr.reset((*_pf)(*this, i));

            if (not(*_pf)(*this, i, rrs.thr)) {
                start_err = true;
                break;
            }
        }

        if (!start_err) {
            impl->status = StatusStartingWaitE;

            do {
                impl->cnd.wait(lock);
            } while (impl->status == StatusStartingWaitE && impl->reactorcnt != impl->reactorvec.size());

            if (impl->status == StatusStartingErrorE) {
                impl->status = StatusStoppingWaitE;
                start_err    = true;
            }
        }

        if (start_err) {
            for (auto it = impl->reactorvec.begin(); it != impl->reactorvec.end(); ++it) {
                if (it->preactor) {
                    it->preactor->stop();
                }
            }
        } else {
            impl->status = StatusRunningE;
            return ErrorConditionT();
        }
    }

    for (auto it = impl->reactorvec.begin(); it != impl->reactorvec.end(); ++it) {
        if (it->isActive()) {
            it->thr.join();
            it->clear();
        }
    }

    unique_lock<mutex> lock(impl->mtx);
    impl->status = StatusStoppedE;
    impl->cnd.notify_all();
    return error_worker();
}

void SchedulerBase::doStop(bool _wait /* = true*/)
{
    {
        unique_lock<mutex> lock(impl->mtx);

        if (impl->status == StatusStartingWaitE || impl->status == StatusStartingErrorE) {
            do {
                impl->cnd.wait(lock);
            } while (impl->status == StatusStartingWaitE || impl->status == StatusStartingErrorE);
        }

        if (impl->status == StatusRunningE) {
            impl->status = _wait ? StatusStoppingWaitE : StatusStoppingE;
            for (auto it = impl->reactorvec.begin(); it != impl->reactorvec.end(); ++it) {
                if (it->preactor) {
                    it->preactor->stop();
                }
            }
        } else if (impl->status == StatusStoppingE) {
            impl->status = _wait ? StatusStoppingWaitE : StatusStoppingE;
        } else if (impl->status == StatusStoppingWaitE) {
            if (_wait) {
                ++impl->stopwaitcnt;
                do {
                    impl->cnd.wait(lock);
                } while (impl->status != StatusStoppedE);
                --impl->stopwaitcnt;
                impl->cnd.notify_one();
            }
            return;
        } else if (impl->status == StatusStoppedE) {
            return;
        }
    }

    if (_wait) {
        while (impl->usecnt) {
            this_thread::yield();
        }
        for (auto it = impl->reactorvec.begin(); it != impl->reactorvec.end(); ++it) {
            if (it->isActive()) {
                it->thr.join();
                it->clear();
            }
        }
        unique_lock<mutex> lock(impl->mtx);
        impl->status = StatusStoppedE;
        impl->cnd.notify_all();
    }
}

ObjectIdT SchedulerBase::doStartObject(ObjectBase& _robj, Service& _rsvc, ScheduleFunctionT& _rfct, ErrorConditionT& _rerr)
{
    ++impl->usecnt;
    ObjectIdT rv;
    if (impl->status == StatusRunningE) {
        ReactorStub& rrs = impl->reactorvec[doComputeScheduleReactorIndex()];

        rv = _rsvc.registerObject(_robj, *rrs.preactor, _rfct, _rerr);
    } else {
        _rerr = error_running();
    }
    --impl->usecnt;
    return rv;
}

bool less_cmp(ReactorStub const& _rrs1, ReactorStub const& _rrs2)
{
    return _rrs1.preactor->load() < _rrs2.preactor->load();
}

size_t SchedulerBase::doComputeScheduleReactorIndex()
{
    switch (impl->reactorvec.size()) {
    case 1:
        return 0;
    case 2: {
        return find_cmp(impl->reactorvec.begin(), less_cmp, SizeToType<2>());
    }
    case 3: {
        return find_cmp(impl->reactorvec.begin(), less_cmp, SizeToType<3>());
    }
    case 4: {
        return find_cmp(impl->reactorvec.begin(), less_cmp, SizeToType<4>());
    }
    case 5: {
        return find_cmp(impl->reactorvec.begin(), less_cmp, SizeToType<5>());
    }
    case 6: {
        return find_cmp(impl->reactorvec.begin(), less_cmp, SizeToType<6>());
    }
    case 7: {
        return find_cmp(impl->reactorvec.begin(), less_cmp, SizeToType<7>());
    }
    case 8: {
        return find_cmp(impl->reactorvec.begin(), less_cmp, SizeToType<8>());
    }
    default:
        break;
    }

    const size_t cwi    = impl->crtreactoridx;
    impl->crtreactoridx = (cwi + 1) % impl->reactorvec.size();
    return cwi;
}

bool SchedulerBase::prepareThread(const size_t _idx, ReactorBase& _rreactor, const bool _success)
{
    const bool thrensuccess = impl->threnfnc();
    {
        unique_lock<mutex> lock(impl->mtx);
        ReactorStub&       rrs = impl->reactorvec[_idx];

        if (_success && impl->status == StatusStartingWaitE && thrensuccess) {
            rrs.preactor = &_rreactor;
            ++impl->reactorcnt;
            impl->cnd.notify_all();
            return true;
        }

        impl->status = StatusStartingErrorE;
        impl->cnd.notify_one();
    }

    impl->threxfnc();

    return false;
}

void SchedulerBase::unprepareThread(const size_t _idx, ReactorBase& _rreactor)
{
    {
        unique_lock<mutex> lock(impl->mtx);
        ReactorStub&       rrs = impl->reactorvec[_idx];
        rrs.preactor           = nullptr;
        --impl->reactorcnt;
        impl->cnd.notify_one();
    }
    impl->threxfnc();
}

} //namespace frame
} //namespace solid
