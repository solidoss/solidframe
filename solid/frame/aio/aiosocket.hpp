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
#include <span>

namespace solid::frame::aio {

class Socket : public SocketBase {
public:
    using VerifyMaskT = unsigned long;

    Socket(SocketDevice&& _rsd)
        : SocketBase(std::move(_rsd))
    {
    }

    Socket() = default;

    [[nodiscard]] ReactorEventE filterReactorEvents(
        const ReactorEventE _evt) const
    {
        return _evt;
    }

    //! Receive data - on failure use can_retry(error) to check whether the operation should be retried
    [[nodiscard]] std::expected<std::size_t, std::error_code> recv(ReactorContext& _rctx, std::span<std::byte> _buf)
    {
        const auto rv = device().recv(_buf);
#if defined(SOLID_USE_WSAPOLL)
        if (!rv && can_retry(rv.error())) {
            modifyReactorRequestEvents(_rctx, ReactorWaitRequestE::Read);
        }
#else
        (void)_rctx;
#endif
        return rv;
    }

    //! Send data - on failure use can_retry(error) to check whether the operation should be retried
    [[nodiscard]] std::expected<std::size_t, std::error_code> send(ReactorContext& _rctx, std::span<const std::byte> _buf)
    {
        const auto rv = device().send(_buf);
#if defined(SOLID_USE_WSAPOLL)
        if (!rv && can_retry(rv.error())) {
            modifyReactorRequestEvents(_rctx, ReactorWaitRequestE::Write);
        }
#else
        (void)_rctx;
#endif
        return rv;
    }

    //! Receive a datagram - on failure use can_retry(error) to check whether the operation should be retried
    [[nodiscard]] std::expected<std::size_t, std::error_code> recvFrom(ReactorContext& _rctx, std::span<std::byte> _buf, SocketAddress& _raddr)
    {
        const auto rv = device().recv(_buf, _raddr);
#if defined(SOLID_USE_WSAPOLL)
        if (!rv && can_retry(rv.error())) {
            modifyReactorRequestEvents(_rctx, ReactorWaitRequestE::Read);
        }
#else
        (void)_rctx;
#endif
        return rv;
    }

    //! Send a datagram - on failure use can_retry(error) to check whether the operation should be retried
    [[nodiscard]] std::expected<std::size_t, std::error_code> sendTo(ReactorContext& _rctx, std::span<const std::byte> _buf, SocketAddressStub const& _rsas)
    {
        const auto rv = device().send(_buf, _rsas);
#if defined(SOLID_USE_WSAPOLL)
        if (!rv && can_retry(rv.error())) {
            modifyReactorRequestEvents(_rctx, ReactorWaitRequestE::Write);
        }
#else
        (void)_rctx;
#endif
        return rv;
    }
};

} // namespace solid::frame::aio
