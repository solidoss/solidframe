// solid/frame/aio/aiosocket.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/aio/aiosocketbase.hpp"
#include "solid/system/socketdevice.hpp"
#include <cerrno>

namespace solid {
namespace frame {
namespace aio {

class Socket : public SocketBase {
public:
    using VerifyMaskT = unsigned long;

    Socket(SocketDevice&& _rsd)
        : SocketBase(std::move(_rsd))
    {
    }

    Socket() {}

    ReactorEventsE filterReactorEvents(
        const ReactorEventsE _evt) const
    {
        return _evt;
    }

    ssize_t recv(ReactorContext& _rctx, char* _pb, size_t _bl, bool& _can_retry, ErrorCodeT& _rerr)
    {
        const ssize_t rv = device().recv(_pb, _bl, _can_retry, _rerr);
#if defined(SOLID_USE_WSAPOLL)
        if (rv < 0 && _can_retry) {
            modifyReactorRequestEvents(_rctx, ReactorWaitRead);
        }
#endif
        return rv;
    }

    ssize_t send(ReactorContext& _rctx, const char* _pb, size_t _bl, bool& _can_retry, ErrorCodeT& _rerr)
    {
        const ssize_t rv = device().send(_pb, _bl, _can_retry, _rerr);
#if defined(SOLID_USE_WSAPOLL)
        if (rv < 0 && _can_retry) {
            modifyReactorRequestEvents(_rctx, ReactorWaitWrite);
        }
#endif
        return rv;
    }

    ssize_t recvFrom(ReactorContext& _rctx, char* _pb, size_t _bl, SocketAddress& _addr, bool& _can_retry, ErrorCodeT& _rerr)
    {
        const ssize_t rv = device().recv(_pb, _bl, _addr, _can_retry, _rerr);
#if defined(SOLID_USE_WSAPOLL)
        if (rv < 0 && _can_retry) {
            modifyReactorRequestEvents(_rctx, ReactorWaitRead);
        }
#endif
        return rv;
    }

    ssize_t sendTo(ReactorContext& _rctx, const char* _pb, size_t _bl, SocketAddressStub const& _rsas, bool& _can_retry, ErrorCodeT& _rerr)
    {
        const ssize_t rv = device().send(_pb, _bl, _rsas, _can_retry, _rerr);
#if defined(SOLID_USE_WSAPOLL)
        if (rv < 0 && _can_retry) {
            modifyReactorRequestEvents(_rctx, ReactorWaitWrite);
        }
#endif
        return rv;
    }
};

} // namespace aio
} // namespace frame
} // namespace solid
