// solid/frame/aio/src/aiolistener.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/utility/event.hpp"

#include "solid/frame/aio/aiocommon.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioreactor.hpp"

namespace solid {
namespace frame {
namespace aio {

/*static*/ void Listener::on_init_completion(CompletionHandler& _rch, ReactorContext& _rctx)
{
    Listener& rthis = static_cast<Listener&>(_rch);
    //rthis.completionCallback(on_dummy_completion);
    rthis.completionCallback(&on_completion);
    rthis.addDevice(_rctx, rthis.sd, ReactorWaitRead);
}

/*static*/ void Listener::on_completion(CompletionHandler& _rch, ReactorContext& _rctx)
{
    Listener& rthis = static_cast<Listener&>(_rch);

    switch (rthis.reactorEvent(_rctx)) {
    case ReactorEventRecv:
        if (!SOLID_FUNCTION_EMPTY(rthis.f)) {
            SocketDevice sd;
            FunctionT    tmpf;
            std::swap(tmpf, rthis.f);
            SOLID_ASSERT(SOLID_FUNCTION_EMPTY(rthis.f));
            rthis.doAccept(_rctx, sd);

            tmpf(_rctx, sd);
        }
        break;
    case ReactorEventError:
    case ReactorEventHangup:
        if (!SOLID_FUNCTION_EMPTY(rthis.f)) {
            SocketDevice sd;
            FunctionT    tmpf;
            std::swap(tmpf, rthis.f);
            tmpf(_rctx, sd);
        }
        break;
    case ReactorEventClear:
        rthis.doClear(_rctx);
        break;
    default:
        SOLID_ASSERT(false);
    }
}

/*static*/ void Listener::on_posted_accept(ReactorContext& _rctx, Event&&)
{
    Listener*    pthis = static_cast<Listener*>(completion_handler(_rctx));
    Listener&    rthis = *pthis;
    SocketDevice sd;

    if (!SOLID_FUNCTION_EMPTY(rthis.f) && rthis.doTryAccept(_rctx, sd)) {
        FunctionT tmpf;
        std::swap(tmpf, rthis.f);
        tmpf(_rctx, sd);
    }
}

/*static*/ void Listener::on_dummy(ReactorContext&, SocketDevice&)
{
}

void Listener::doPostAccept(ReactorContext& _rctx)
{
    reactor(_rctx).post(_rctx, Listener::on_posted_accept, Event(), *this);
}

bool Listener::doTryAccept(ReactorContext& _rctx, SocketDevice& _rsd)
{
    bool       can_retry;
    ErrorCodeT err = sd.accept(_rsd, can_retry);

    if (!err) {
    } else if (can_retry) {
        return false;
    } else {
        systemError(_rctx, err);
        error(_rctx, error_listener_system);
    }
    return true;
}

void Listener::doAccept(ReactorContext& _rctx, SocketDevice& _rsd)
{
    bool       can_retry;
    ErrorCodeT err = sd.accept(_rsd, can_retry);

    if (!err) {
    } else if (can_retry) {
        SOLID_ASSERT(false);
    } else {
        systemError(_rctx, err);
        error(_rctx, error_listener_system);
    }
}

void Listener::doClear(ReactorContext& _rctx)
{
    SOLID_FUNCTION_CLEAR(f);
    remDevice(_rctx, sd);
    f = &on_dummy;
}

} //namespace aio
} //namespace frame
} //namespace solid
