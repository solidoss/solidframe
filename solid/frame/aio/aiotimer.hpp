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

struct ActorProxy;
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
            rthis.function_ = &on_dummy;
            break;
        default:
            solid_assert_log(false, generic_logger);
        }
    }
    static void on_dummy(ReactorContext& _rctx)
    {
    }

    typedef solid_function_t(void(ReactorContext&)) FunctionT;

    FunctionT function_;
    size_t    storeidx_;
    NanoTime  expiry_;

public:
    SteadyTimer(
        ActorProxy const& _ract)
        : CompletionHandler(_ract, SteadyTimer::on_init_completion)
        , storeidx_(-1)
    {
    }

    ~SteadyTimer()
    {
        // MUST call here and not in the ~CompletionHandler
        this->deactivate();
    }

    const NanoTime& expiry() const
    {
        return expiry_;
    }

    bool hasPending() const
    {
        return !solid_function_empty(function_);
    }

    // Returns false when the operation is scheduled for completion. On completion _f(...) will be called.
    // Returns true when operation could not be scheduled for completion - e.g. operation already in progress.
    template <class Rep, class Period, typename F>
    bool waitFor(ReactorContext& _rctx, std::chrono::duration<Rep, Period> const& _rd, F&& _function)
    {
        return waitUntil(_rctx, _rctx.nanoTime() + _rd, std::forward<F>(_function));
    }

    // Returns true when the operation completed. Check _rctx.error() for success or fail
    // Returns false when operation is scheduled for completion. On completion _f(...) will be called.
    template <class Clock, class Duration, typename F>
    bool waitUntil(ReactorContext& _rctx, std::chrono::time_point<Clock, Duration> const& _rtp, F&& _function)
    {
        function_ = std::forward<F>(_function);
        expiry_   = NanoTime{time_point_clock_cast<std::chrono::steady_clock>(_rtp)};
        this->addTimer(_rctx, expiry_, storeidx_);
        return false;
    }

    template <typename F>
    bool waitUntil(ReactorContext& _rctx, const NanoTime& _expiry, F&& _function)
    {
        function_ = std::forward<F>(_function);
        expiry_   = _expiry;
        this->addTimer(_rctx, expiry_, storeidx_);
        return false;
    }

    void silentCancel(ReactorContext& _rctx)
    {
        doClear(_rctx);
    }

    void cancel(ReactorContext& _rctx)
    {
        if (!solid_function_empty(function_)) {
            remTimer(_rctx, storeidx_);
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
        std::swap(tmpf, function_);
        storeidx_ = InvalidIndex();
        tmpf(_rctx);
    }
    void doClear(ReactorContext& _rctx)
    {
        solid_function_clear(function_);
        remTimer(_rctx, storeidx_);
        storeidx_ = InvalidIndex();
    }
};

} // namespace aio
} // namespace frame
} // namespace solid
