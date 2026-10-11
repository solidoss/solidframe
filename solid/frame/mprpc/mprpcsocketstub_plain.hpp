// solid/frame/mprpc/mprpcsocketstub.hpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <span>

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/system/socketdevice.hpp"

#include "solid/utility/event.hpp"

#include "solid/frame/aio/aiosocket.hpp"
#include "solid/frame/aio/aiostream.hpp"
#include "solid/frame/mprpc/mprpcsocketstub.hpp"

namespace solid::frame::mprpc::plain {

class SocketStub final : public mprpc::SocketStub {
public:
    SocketStub(frame::aio::ActorProxy const& _rproxy)
        : sock(_rproxy)
    {
    }
    SocketStub(frame::aio::ActorProxy const& _rproxy, SocketDevice&& _usd)
        : sock(_rproxy, std::move(_usd))
    {
    }

private:
    ~SocketStub()
    {
    }

    SocketDevice const& device() const final
    {
        return sock.device();
    }

    SocketDevice& device() final
    {
        return sock.device();
    }

    bool postSendAll(
        frame::aio::ReactorContext& _rctx, OnSendAllRawF _pf, std::span<const std::byte> buf, EventBase& _revent) final
    {
        struct Closure {
            const OnSendAllRawF pf;
            EventT              event;

            Closure(OnSendAllRawF _pf, EventBase const& _revent)
                : pf(_pf)
                , event(_revent)
            {
            }

            void operator()(frame::aio::ReactorContext& _rctx)
            {
                (*pf)(_rctx, event);
            }

        } lambda(_pf, _revent);

        return sock.postSendAll(_rctx, buf, lambda);
    }

    bool postRecvSome(
        frame::aio::ReactorContext& _rctx, OnRecvF _pf, std::span<std::byte> buf) final
    {
        return sock.postRecvSome(_rctx, buf, _pf);
    }

    bool postRecvSome(
        frame::aio::ReactorContext& _rctx, OnRecvSomeRawF _pf, std::span<std::byte> buf, EventBase& _revent) final
    {
        struct Closure {
            const OnRecvSomeRawF pf;
            EventT               event;

            Closure(OnRecvSomeRawF _pf, EventBase const& _revent)
                : pf(_pf)
                , event(_revent)
            {
            }

            void operator()(frame::aio::ReactorContext& _rctx, size_t _sz)
            {
                (*pf)(_rctx, _sz, event);
            }

        } lambda(_pf, _revent);

        return sock.postRecvSome(_rctx, buf, lambda);
    }

    bool hasValidSocket() const final
    {
        return static_cast<bool>(sock.device());
    }

    bool connect(
        frame::aio::ReactorContext& _rctx, OnConnectF _pf, const SocketAddressInet& _raddr) final
    {
        return sock.connect(_rctx, _raddr, _pf);
    }

    bool recvSome(
        frame::aio::ReactorContext& _rctx, OnRecvF _pf, std::span<std::byte> buf, size_t& _sz) final
    {
        return sock.recvSome(_rctx, buf, _pf, _sz);
    }

    bool hasPendingSend() const final
    {
        return sock.hasPendingSend();
    }

    bool sendAll(
        frame::aio::ReactorContext& _rctx, OnSendF _pf, std::span<const std::byte> buf) final
    {
        return sock.sendAll(_rctx, buf, _pf);
    }

    void prepareSocket(
        frame::aio::ReactorContext& _rctx) final
    {
    }

    void cancelRecv(frame::aio::ReactorContext& _rctx) override
    {
        sock.cancelRecv(_rctx);
    }

private:
    using StreamSocketT = frame::aio::Stream<frame::aio::Socket>;

    StreamSocketT sock;
};

inline SocketStubPtrT create_client_socket(Configuration const& /*_rcfg*/, frame::aio::ActorProxy const& _rproxy, char* _emplace_buf)
{
#ifdef SOLID_HAS_ASSERT
    static_assert(sizeof(SocketStub) <= socket_emplace_size);
    static_assert(alignof(SocketStub) <= socket_emplace_align);
#endif
    if constexpr (sizeof(SocketStub) > socket_emplace_size and alignof(SocketStub) > socket_emplace_align) {
        return SocketStubPtrT(new SocketStub(_rproxy), SocketStub::delete_deleter);
    } else {
        return SocketStubPtrT(new (_emplace_buf) SocketStub(_rproxy), SocketStub::emplace_deleter);
    }
}

inline SocketStubPtrT create_server_socket(Configuration const& /*_rcfg*/, frame::aio::ActorProxy const& _rproxy, SocketDevice&& _usd, char* _emplace_buf)
{
#ifdef SOLID_HAS_ASSERT
    static_assert(sizeof(SocketStub) <= socket_emplace_size);
    static_assert(alignof(SocketStub) <= socket_emplace_align);
#endif
    if constexpr (sizeof(SocketStub) > socket_emplace_size and alignof(SocketStub) > socket_emplace_align) {
        return SocketStubPtrT(new SocketStub(_rproxy, std::move(_usd)), SocketStub::delete_deleter);
    } else {
        return SocketStubPtrT(new (_emplace_buf) SocketStub(_rproxy, std::move(_usd)), SocketStub::emplace_deleter);
    }
}

} // namespace solid::frame::mprpc::plain
