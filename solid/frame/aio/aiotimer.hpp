// solid/frame/aio/aiotimer.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/common.hpp"
#include "solid/system/socketdevice.hpp"

#include "aiocompletion.hpp"
#include "aioerror.hpp"
#include "aioreactorcontext.hpp"

namespace solid {
namespace frame {
namespace aio {

struct ObjectProxy;
struct ReactorContext;

class SteadyTimer : public CompletionHandler {
    typedef SteadyTimer ThisT;

    static void on_init_completion(CompletionHandler& _rch, ReactorContext& _rctx)
    {
        ThisT& rthis = static_cast<ThisT&>(_rch);
        rthis.completionCallback(SteadyTimer::on_completion);
    }

    static void on_completion(CompletionHandler& _rch, ReactorContext& _rctx)
    {
        ThisT& rthis = static_cast<ThisT&>(_rch);

        switch (rthis.reactorEvent(_rctx)) {
        case ReactorEventTimer:
            rthis.doExec(_rctx);
            break;
        case ReactorEventClear:
            rthis.doClear(_rctx);
            rthis.f = &on_dummy;
            break;
        default:
            SOLID_ASSERT(false);
        }
    }
    static void on_dummy(ReactorContext& _rctx)
    {
    }

public:
    SteadyTimer(
        ObjectProxy const& _robj)
        : CompletionHandler(_robj, SteadyTimer::on_init_completion)
        , storeidx(-1)
    {
    }

    ~SteadyTimer()
    {
        //MUST call here and not in the ~CompletionHandler
        this->deactivate();
    }

    //Returns false when the operation is scheduled for completion. On completion _f(...) will be called.
    //Returns true when operation could not be scheduled for completion - e.g. operation already in progress.
    template <class Rep, class Period, typename F>
    bool waitFor(ReactorContext& _rctx, std::chrono::duration<Rep, Period> const& _rd, F _f)
    {
        return waitUntil(_rctx, _rctx.steadyTime() + _rd, _f);
    }

    //Returns true when the operation completed. Check _rctx.error() for success or fail
    //Returns false when operation is scheduled for completion. On completion _f(...) will be called.
    template <class Clock, class Duration, typename F>
    bool waitUntil(ReactorContext& _rctx, std::chrono::time_point<Clock, Duration> const& _rtp, F _f)
    {
        f = _f;
        NanoTime steady_nt{time_point_clock_cast<std::chrono::steady_clock>(_rtp)};
        this->addTimer(_rctx, steady_nt, storeidx);
        return false;
    }

    void silentCancel(ReactorContext& _rctx)
    {
        doClear(_rctx);
    }

    void cancel(ReactorContext& _rctx)
    {
        if (!SOLID_FUNCTION_EMPTY(f)) {
            remTimer(_rctx, storeidx);
            error(_rctx, error_timer_cancel);
            doExec(_rctx);
        } else {
            doClear(_rctx);
        }
    }

private:
    friend class Reactor;
    void doExec(ReactorContext& _rctx)
    {
        FunctionT tmpf;
        std::swap(tmpf, f);
        storeidx = InvalidIndex();
        tmpf(_rctx);
    }
    void doClear(ReactorContext& _rctx)
    {
        SOLID_FUNCTION_CLEAR(f);
        remTimer(_rctx, storeidx);
        storeidx = InvalidIndex();
    }

private:
    typedef SOLID_FUNCTION(void(ReactorContext&)) FunctionT;

    FunctionT f;
    size_t    storeidx;
};

} //namespace aio
} //namespace frame
} //namespace solid
