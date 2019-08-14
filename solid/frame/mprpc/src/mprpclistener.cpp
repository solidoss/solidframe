// solid/frame/ipc/src/ipclistener.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "mprpclistener.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"
#include "solid/utility/event.hpp"

namespace solid {
namespace frame {
namespace mprpc {
namespace {
const LoggerT logger("solid::frame::mprpc::listener");
}

Listener::Listener(
    SocketDevice& _rsd)
    : sock(this->proxy(), std::move(_rsd))
    , timer(this->proxy())
{
    solid_dbg(logger, Info, this);
}
Listener::~Listener()
{
    solid_dbg(logger, Info, this);
}

inline Service& Listener::service(frame::aio::ReactorContext& _rctx)
{
    return static_cast<Service&>(_rctx.service());
}

/*virtual*/ void Listener::onEvent(frame::aio::ReactorContext& _rctx, Event&& _uevent)
{
    solid_dbg(logger, Info, "event = " << _uevent);
    if (
        _uevent == generic_event_start || _uevent == generic_event_timer) {
        sock.postAccept(
            _rctx,
            [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { onAccept(_rctx, _rsd); });
    } else if (_uevent == generic_event_kill) {
        postStop(_rctx);
    }
}

void Listener::onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd)
{
    solid_dbg(logger, Info, "");
    size_t repeatcnt = backlog_size();

    do {
        if (!_rctx.error()) {
            service(_rctx).acceptIncomingConnection(_rsd);
        } else if (_rctx.error() == aio::error_listener_hangup) {
            solid_dbg(logger, Error, "listen hangup" << _rctx.error().message());
            //TODO: maybe you shoud restart the listener.
            postStop(_rctx);
        } else {
            solid_dbg(logger, Info, "listen error" << _rctx.error().message());
            timer.waitFor(
                _rctx, std::chrono::seconds(10),
                [this](frame::aio::ReactorContext& _rctx) { onEvent(_rctx, make_event(GenericEvents::Timer)); });
            break;
        }
        --repeatcnt;
    } while (
        repeatcnt != 0u && sock.accept(
            _rctx, [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { onAccept(_rctx, _rsd); }, _rsd));

    if (repeatcnt == 0u) {
        sock.postAccept(
            _rctx,
            [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { onAccept(_rctx, _rsd); }); //fully asynchronous call
    }
}

} //namespace mprpc
} //namespace frame
} //namespace solid
