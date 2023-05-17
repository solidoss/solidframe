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

#include "solid/frame/mprpc/mprpccontext.hpp"

#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/frame/common.hpp"

namespace solid {
namespace frame {
namespace mprpc {

struct ConnectionContext;

class SocketStub {
public:
    typedef void (*OnSendAllRawF)(frame::aio::ReactorContext&, EventBase&);
    typedef void (*OnRecvSomeRawF)(frame::aio::ReactorContext&, const size_t, EventBase&);
    typedef void (*OnConnectF)(frame::aio::ReactorContext&);
    typedef void (*OnRecvF)(frame::aio::ReactorContext&, size_t);
    typedef void (*OnSendF)(frame::aio::ReactorContext&);
    typedef void (*OnSecureAcceptF)(frame::aio::ReactorContext&);
    typedef void (*OnSecureConnectF)(frame::aio::ReactorContext&);

    static void emplace_deleter(SocketStub* _pss)
    {
        _pss->~SocketStub();
    }

    static void delete_deleter(SocketStub* _pss)
    {
        delete _pss;
    }

    virtual ~SocketStub()
    {
    }

    virtual SocketDevice const& device() const = 0;
    virtual SocketDevice&       device()       = 0;

    virtual bool postSendAll(
        frame::aio::ReactorContext& _rctx, OnSendAllRawF _pf, const char* _pbuf, size_t _bufcp, EventBase& _revent)
        = 0;

    virtual bool postRecvSome(
        frame::aio::ReactorContext& _rctx, OnRecvF _pf, char* _pbuf, size_t _bufcp)
        = 0;

    virtual bool postRecvSome(
        frame::aio::ReactorContext& _rctx, OnRecvSomeRawF _pf, char* _pbuf, size_t _bufcp, EventBase& _revent)
        = 0;

    virtual bool hasValidSocket() const = 0;

    virtual bool connect(
        frame::aio::ReactorContext& _rctx, OnConnectF _pf, const SocketAddressInet& _raddr)
        = 0;

    virtual bool recvSome(
        frame::aio::ReactorContext& _rctx, OnRecvF _pf, char* _buf, size_t _bufcp, size_t& _sz)
        = 0;

    virtual bool hasPendingSend() const = 0;

    virtual bool sendAll(
        frame::aio::ReactorContext& _rctx, OnSendF _pf, char* _buf, size_t _bufcp)
        = 0;

    virtual void prepareSocket(
        frame::aio::ReactorContext& _rctx)
        = 0;

    virtual bool secureAccept(
        frame::aio::ReactorContext& _rctx, ConnectionContext& _rconctx, OnSecureAcceptF _pf, ErrorConditionT& _rerror);
    virtual bool secureConnect(
        frame::aio::ReactorContext& _rctx, ConnectionContext& _rconctx, OnSecureConnectF _pf, ErrorConditionT& _rerror);

protected:
    static ConnectionProxy connectionProxy();
};

typedef void (*SocketStubDeleteF)(SocketStub*);

using SocketStubPtrT = std::unique_ptr<SocketStub, SocketStubDeleteF>;

} // namespace mprpc
} // namespace frame
} // namespace solid
