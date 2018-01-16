// solid/frame/ipc/src/ipclistener.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/system/socketdevice.hpp"

namespace solid {
struct Event;
namespace frame {
namespace aio {
namespace openssl {
class Context;
}
} // namespace aio

namespace mpipc {

class Service;

class Listener final : public Dynamic<Listener, frame::aio::Object> {
public:
    static size_t backlog_size()
    {
        return SocketInfo::max_listen_backlog_size();
    }

    Listener(
        SocketDevice& _rsd);
    ~Listener();

private:
    Service& service(frame::aio::ReactorContext& _rctx);
    void     onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent) override;

    void onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd);

    typedef frame::aio::Listener    ListenerSocketT;
    typedef frame::aio::SteadyTimer TimerT;

    ListenerSocketT sock;
    TimerT          timer;
};

} //namespace mpipc
} //namespace frame
} //namespace solid
