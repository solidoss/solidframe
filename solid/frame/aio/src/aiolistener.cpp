// solid/frame/aio/src/aiolistener.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/system/log.hpp"
#include "solid/utility/event.hpp"

#include "solid/frame/aio/aiocommon.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioreactor.hpp"

namespace solid {
namespace frame {
namespace aio {

const LoggerT logger("solid::frame::aio");

/*static*/ void Listener::on_init_completion(CompletionHandler& _rch, ReactorContext& _rctx)
{
    Listener& rthis = static_cast<Listener&>(_rch);
    //rthis.completionCallback(on_dummy_completion);
    rthis.completionCallback(&on_completion);
    //rthis.contextBind(_rctx);
    rthis.s.initAccept(_rctx);
}

/*static*/ void Listener::on_completion(CompletionHandler& _rch, ReactorContext& _rctx)
{
    Listener& rthis = static_cast<Listener&>(_rch);

    switch (rthis.reactorEvent(_rctx)) {
    case ReactorEventRecv:
        if (!solid_function_empty(rthis.f)) {
            SocketDevice sd;
            FunctionT    tmpf;
            std::swap(tmpf, rthis.f);
            solid_assert_log(solid_function_empty(rthis.f), logger);
            rthis.doAccept(_rctx, sd);

            tmpf(_rctx, sd);
        }
        break;
    case ReactorEventError:
    case ReactorEventHangup:
        if (!solid_function_empty(rthis.f)) {
            SocketDevice sd;
            FunctionT    tmpf;
            std::swap(tmpf, rthis.f);

            rthis.error(_rctx, error_listener_hangup);

            tmpf(_rctx, sd);
        }
        break;
    case ReactorEventClear:
        rthis.doClear(_rctx);
        break;
    default:
        solid_assert_log(false, logger);
    }
}

/*static*/ void Listener::on_posted_accept(ReactorContext& _rctx, Event&&)
{
    Listener*    pthis = static_cast<Listener*>(completion_handler(_rctx));
    Listener&    rthis = *pthis;
    SocketDevice sd;

    if (!solid_function_empty(rthis.f) && rthis.doTryAccept(_rctx, sd)) {
        FunctionT tmpf;
        std::swap(tmpf, rthis.f);
        tmpf(_rctx, sd);
    }
}

/*static*/ void Listener::on_dummy(ReactorContext&, SocketDevice&)
{
}

SocketDevice Listener::reset(ReactorContext& _rctx, SocketDevice&& _rnewdev)
{
    if (s.device()) {
        remDevice(_rctx, s.device());
    }

    contextBind(_rctx);

    SocketDevice tmpsd = s.resetAccept(_rctx, std::move(_rnewdev));
    if (s.device()) {
        completionCallback(&on_completion);
    }
    return tmpsd;
}

void Listener::doPostAccept(ReactorContext& _rctx)
{
    reactor(_rctx).post(_rctx, Listener::on_posted_accept, Event(), *this);
}

bool Listener::doTryAccept(ReactorContext& _rctx, SocketDevice& _rsd)
{
    bool       can_retry;
    ErrorCodeT err = s.accept(_rctx, _rsd, can_retry);

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
    ErrorCodeT err = s.accept(_rctx, _rsd, can_retry);

    if (!err) {
    } else if (can_retry) {
        solid_assert_log(false, logger);
    } else {
        systemError(_rctx, err);
        error(_rctx, error_listener_system);
    }
}

void Listener::doClear(ReactorContext& _rctx)
{
    solid_function_clear(f);
    remDevice(_rctx, s.device());
    f = &on_dummy;
}

} //namespace aio
} //namespace frame
} //namespace solid
