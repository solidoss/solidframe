// solid/frame/aio/aiosocketbase.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/aio/aiocommon.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/system/socketdevice.hpp"
#include <expected>
#include <system_error>
#include <tuple>

namespace solid::frame::aio {

class SocketBase {
public:
    SocketBase(SocketDevice&& _rsd)
        : dev_(std::move(_rsd))
    {
        if (device()) {
            solid_check(auto const ret_val{device().makeNonBlocking()}, " unexpected " << ret_val.error() << ": " << ret_val.error().message());
        }
    }

    SocketBase() = default;

    SocketDevice reset(ReactorContext& _rctx, SocketDevice&& _rsd)
    {
        SocketDevice tmpsd = std::move(dev_);
        dev_               = std::move(_rsd);

        if (device()) {
            solid_check(auto const ret_val{device().makeNonBlocking()}, " unexpected " << ret_val.error() << ": " << ret_val.error().message());
            init(_rctx);
        }
        return tmpsd;
    }

    SocketDevice resetAccept(ReactorContext& _rctx, SocketDevice&& _rsd)
    {
        SocketDevice tmpsd = std::move(dev_);
        dev_               = std::move(_rsd);

        if (device()) {
            solid_check(auto const ret_val{device().makeNonBlocking()}, " unexpected " << ret_val.error() << ": " << ret_val.error().message());
            initAccept(_rctx);
        }
        return tmpsd;
    }

    void shutdown()
    {
        solid_check(auto const ret_val{device().makeNonBlocking()}, " unexpected " << ret_val.error() << ": " << ret_val.error().message());
    }

    void initAccept(ReactorContext& _rctx)
    {
#if defined(SOLID_USE_EPOLL) || defined(SOLID_USE_KQUEUE)
        addReactorRequestEvents(_rctx, ReactorWaitRequestE::Read);
#else
        addReactorRequestEvents(_rctx, ReactorWaitRequestE::None);
#endif
    }

    void init(ReactorContext& _rctx)
    {
#if defined(SOLID_USE_EPOLL) || defined(SOLID_USE_KQUEUE)
        addReactorRequestEvents(_rctx, ReactorWaitRequestE::ReadOrWrite);
#else
        addReactorRequestEvents(_rctx, ReactorWaitRequestE::None);
#endif
    }

    //! Accept a new connection
    /*!
        On failure, use SocketDevice::can_retry_accept(error) to check whether
        the accept should be retried once the socket becomes readable.
    */
    [[nodiscard]] std::expected<SocketDevice, std::error_code> accept(ReactorContext& _rctx)
    {
        auto rv = device().accept();
#if defined(SOLID_USE_WSAPOLL)
        if (!rv && SocketDevice::can_retry_accept(rv.error())) {
            modifyReactorRequestEvents(_rctx, ReactorWaitRequestE::Read);
        }
#else
        (void)_rctx;
#endif
        return rv;
    }

    //! Create the socket for the given address family, make it non-blocking and register it with the reactor
    [[nodiscard]] std::expected<void, std::error_code> create(ReactorContext& _rctx, SocketAddressStub const& _rsas)
    {
        if (const auto rv = device().create(_rsas.family()); !rv) {
            return rv;
        }
        const auto rv = device().makeNonBlocking();
#if defined(SOLID_USE_WSAPOLL)
        addReactorRequestEvents(_rctx, ReactorWaitRequestE::None);
#else
        addReactorRequestEvents(_rctx, ReactorWaitRequestE::Write);
#endif
        return rv;
    }

    //! Connect the socket
    /*!
        On failure, use SocketDevice::can_wait_connect(error) to check whether the
        connection is in progress - use checkConnect once the socket becomes writable.
    */
    [[nodiscard]] std::expected<void, std::error_code> connect(ReactorContext& _rctx, SocketAddressStub const& _rsas)
    {
        const auto rv = device().connect(_rsas);
#if defined(SOLID_USE_WSAPOLL)
        if (!rv && SocketDevice::can_wait_connect(rv.error())) {
            modifyReactorRequestEvents(_rctx, ReactorWaitRequestE::Write);
            addReactorConnect(_rctx);
        }
#else
        (void)_rctx;
#endif
        return rv;
    }

    //! Check the outcome of an asynchronous connect
    [[nodiscard]] std::expected<void, std::error_code> checkConnect(ReactorContext& _rctx) const
    {
        if (const auto err = device().error()) {
            return std::unexpected(err);
        }
#if defined(SOLID_USE_EPOLL) || defined(SOLID_USE_KQUEUE)
        modifyReactorRequestEvents(_rctx, ReactorWaitRequestE::ReadOrWrite);
#else
        modifyReactorRequestEvents(_rctx, ReactorWaitRequestE::None);
#endif
        return {};
    }

    //! Check if a failed recv/send/secure operation should be retried once the socket becomes ready
    static bool can_retry(const std::error_code& _err) noexcept
    {
        return SocketDevice::can_retry_io(_err);
    }

    [[nodiscard]] SocketDevice const& device() const
    {
        return dev_;
    }

    SocketDevice& device()
    {
        return dev_;
    }

protected:
    void addReactorRequestEvents(ReactorContext& _rctx, const ReactorWaitRequestE _req) const
    {
        _rctx.reactor().addDevice(_rctx, device(), _req);
    }
    void modifyReactorRequestEvents(ReactorContext& _rctx, const ReactorWaitRequestE _req) const
    {
        _rctx.reactor().modDevice(_rctx, device(), _req);
    }
    void addReactorConnect(ReactorContext& _rctx) const
    {
        _rctx.reactor().addConnect(_rctx);
    }
    void remReactorConnect(ReactorContext& _rctx) const
    {
        _rctx.reactor().remConnect(_rctx);
    }

private:
    SocketDevice dev_;
};

} // namespace solid::frame::aio
