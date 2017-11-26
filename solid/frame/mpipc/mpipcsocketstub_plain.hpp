// solid/frame/mpipc/mpipcsocketstub.hpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/socketdevice.hpp"

#include "solid/utility/event.hpp"

#include "solid/frame/aio/aiosocket.hpp"
#include "solid/frame/aio/aiostream.hpp"
#include "solid/frame/mpipc/mpipcsocketstub.hpp"

namespace solid {
namespace frame {
namespace mpipc {
namespace plain {

class SocketStub final : public mpipc::SocketStub {
public:
    SocketStub(frame::aio::ObjectProxy const& _rproxy)
        : sock(_rproxy)
    {
    }
    SocketStub(frame::aio::ObjectProxy const& _rproxy, SocketDevice&& _usd)
        : sock(_rproxy, std::move(_usd))
    {
    }

private:
    ~SocketStub()
    {
    }

    SocketDevice const& device() const override final
    {
        return sock.device();
    }

    SocketDevice& device() override final
    {
        return sock.device();
    }

    bool postSendAll(
        frame::aio::ReactorContext& _rctx, OnSendAllRawF _pf, const char* _pbuf, size_t _bufcp, Event& _revent) override final
    {
        struct Closure {
            OnSendAllRawF pf;
            Event         event;

            Closure(OnSendAllRawF _pf, Event const& _revent)
                : pf(_pf)
                , event(_revent)
            {
            }

            void operator()(frame::aio::ReactorContext& _rctx)
            {
                (*pf)(_rctx, event);
            }

        } lambda(_pf, _revent);

        //TODO: find solution for costly event copy

        return sock.postSendAll(_rctx, _pbuf, _bufcp, lambda);
    }

    bool postRecvSome(
        frame::aio::ReactorContext& _rctx, OnRecvF _pf, char* _pbuf, size_t _bufcp) override final
    {
        return sock.postRecvSome(_rctx, _pbuf, _bufcp, _pf);
    }

    bool postRecvSome(
        frame::aio::ReactorContext& _rctx, OnRecvSomeRawF _pf, char* _pbuf, size_t _bufcp, Event& _revent) override final
    {
        struct Closure {
            OnRecvSomeRawF pf;
            Event          event;

            Closure(OnRecvSomeRawF _pf, Event const& _revent)
                : pf(_pf)
                , event(_revent)
            {
            }

            void operator()(frame::aio::ReactorContext& _rctx, size_t _sz)
            {
                (*pf)(_rctx, _sz, event);
            }

        } lambda(_pf, _revent);

        //TODO: find solution for costly event copy

        return sock.postRecvSome(_rctx, _pbuf, _bufcp, lambda);
    }

    bool hasValidSocket() const override final
    {
        return static_cast<bool>(sock.device());
    }

    bool connect(
        frame::aio::ReactorContext& _rctx, OnConnectF _pf, const SocketAddressInet& _raddr) override final
    {
        return sock.connect(_rctx, _raddr, _pf);
    }

    bool recvSome(
        frame::aio::ReactorContext& _rctx, OnRecvF _pf, char* _buf, size_t _bufcp, size_t& _sz) override final
    {
        return sock.recvSome(_rctx, _buf, _bufcp, _pf, _sz);
    }

    bool hasPendingSend() const override final
    {
        return sock.hasPendingSend();
    }

    bool sendAll(
        frame::aio::ReactorContext& _rctx, OnSendF _pf, char* _buf, size_t _bufcp) override final
    {
        return sock.sendAll(_rctx, _buf, _bufcp, _pf);
    }

    void prepareSocket(
        frame::aio::ReactorContext& _rctx) override final
    {
    }

private:
    using StreamSocketT = frame::aio::Stream<frame::aio::Socket>;

    StreamSocketT sock;
};

inline SocketStubPtrT create_client_socket(Configuration const& /*_rcfg*/, frame::aio::ObjectProxy const& _rproxy, char* _emplace_buf)
{
    if (sizeof(SocketStub) > static_cast<size_t>(ConnectionValues::SocketEmplacementSize)) {
        return SocketStubPtrT(new SocketStub(_rproxy), SocketStub::delete_deleter);
    } else {
        return SocketStubPtrT(new (_emplace_buf) SocketStub(_rproxy), SocketStub::emplace_deleter);
    }
}

inline SocketStubPtrT create_server_socket(Configuration const& /*_rcfg*/, frame::aio::ObjectProxy const& _rproxy, SocketDevice&& _usd, char* _emplace_buf)
{

    if (sizeof(SocketStub) > static_cast<size_t>(ConnectionValues::SocketEmplacementSize)) {
        return SocketStubPtrT(new SocketStub(_rproxy, std::move(_usd)), SocketStub::delete_deleter);
    } else {
        return SocketStubPtrT(new (_emplace_buf) SocketStub(_rproxy, std::move(_usd)), SocketStub::emplace_deleter);
    }
}

} //namespace plain
} //namespace mpipc
} //namespace frame
} //namespace solid
