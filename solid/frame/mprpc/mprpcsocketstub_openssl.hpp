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

//NOTE: on Windows, when compiling against BoringSSL
//we need to include "openssl/ssl.hpp" before any
//inclusion of Windows.h
#include "solid/frame/aio/openssl/aiosecurecontext.hpp"
#include "solid/frame/aio/openssl/aiosecuresocket.hpp"

#include "solid/system/socketdevice.hpp"
#include "solid/utility/function.hpp"

#include "solid/utility/any.hpp"
#include "solid/utility/event.hpp"

#include "solid/frame/aio/aiostream.hpp"

#include "solid/frame/mprpc/mprpcsocketstub.hpp"

#include "solid/frame/mprpc/mprpcservice.hpp"

namespace solid {
namespace frame {
namespace mprpc {
namespace openssl {

using ContextT      = frame::aio::openssl::Context;
using StreamSocketT = frame::aio::Stream<frame::aio::openssl::Socket>;

using ConnectionPrepareServerFunctionT = solid_function_t(unsigned long(frame::aio::ReactorContext&, ConnectionContext&, StreamSocketT&, ErrorConditionT&));
using ConnectionPrepareClientFunctionT = solid_function_t(unsigned long(frame::aio::ReactorContext&, ConnectionContext&, StreamSocketT&, ErrorConditionT&));
using ConnectionServerVerifyFunctionT  = solid_function_t(bool(frame::aio::ReactorContext&, ConnectionContext&, StreamSocketT&, bool, frame::aio::openssl::VerifyContext&));
using ConnectionClientVerifyFunctionT  = solid_function_t(bool(frame::aio::ReactorContext&, ConnectionContext&, StreamSocketT&, bool, frame::aio::openssl::VerifyContext&));

struct ClientConfiguration {
    ClientConfiguration()
        : context{ContextT::create()}
    {
    }

    ContextT context;

    ConnectionPrepareClientFunctionT connection_prepare_secure_fnc;
    ConnectionClientVerifyFunctionT  connection_verify_fnc;
};

struct ServerConfiguration {
    ServerConfiguration()
        : context{ContextT::create()}
    {
    }

    ContextT context;

    ConnectionPrepareServerFunctionT connection_prepare_secure_fnc;
    ConnectionServerVerifyFunctionT  connection_verify_fnc;
};

class SocketStub final : public mprpc::SocketStub {
public:
    SocketStub(frame::aio::ActorProxy const& _rproxy, ContextT& _rsecure_ctx)
        : sock(_rproxy, _rsecure_ctx)
    {
    }

    SocketStub(
        frame::aio::ActorProxy const& _rproxy, SocketDevice&& _usd, ContextT& _rsecure_ctx)
        : sock(_rproxy, std::move(_usd), _rsecure_ctx)
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

    bool secureAccept(
        frame::aio::ReactorContext& _rctx, ConnectionContext& _rconctx, OnSecureAcceptF _pf, ErrorConditionT& _rerror) override final
    {
        Service&                   rservice = static_cast<Service&>(_rctx.service());
        const ServerConfiguration& rconfig  = *rservice.configuration().server.secure_any.cast<ServerConfiguration>();

        const unsigned long verify_mode = rconfig.connection_prepare_secure_fnc(_rctx, _rconctx, sock, _rerror);

        auto lambda = [this](frame::aio::ReactorContext& _rctx, bool _preverified, frame::aio::openssl::VerifyContext& _rverify_ctx) {
            Service&                   rservice = static_cast<Service&>(_rctx.service());
            const ServerConfiguration& rconfig  = *rservice.configuration().server.secure_any.cast<ServerConfiguration>();
            ConnectionContext          conctx(_rctx, connectionProxy());

            return rconfig.connection_verify_fnc(_rctx, conctx, sock, _preverified, _rverify_ctx);
        };

        sock.secureSetVerifyCallback(_rctx, verify_mode, lambda);

        return sock.secureAccept(_rctx, _pf);
    }

    bool secureConnect(
        frame::aio::ReactorContext& _rctx, ConnectionContext& _rconctx, OnSecureConnectF _pf, ErrorConditionT& _rerror) override final
    {
        Service&                   rservice = static_cast<Service&>(_rctx.service());
        const ClientConfiguration& rconfig  = *rservice.configuration().client.secure_any.cast<ClientConfiguration>();

        const unsigned long verify_mode = rconfig.connection_prepare_secure_fnc(_rctx, _rconctx, sock, _rerror);

        auto lambda = [this](frame::aio::ReactorContext& _rctx, bool _preverified, frame::aio::openssl::VerifyContext& _rverify_ctx) {
            Service&                   rservice = static_cast<Service&>(_rctx.service());
            const ClientConfiguration& rconfig  = *rservice.configuration().client.secure_any.cast<ClientConfiguration>();
            ConnectionContext          conctx(_rctx, connectionProxy());

            return rconfig.connection_verify_fnc(_rctx, conctx, sock, _preverified, _rverify_ctx);
        };

        sock.secureSetVerifyCallback(_rctx, verify_mode, lambda);

        return sock.secureConnect(_rctx, _pf);
    }

    StreamSocketT& socket()
    {
        return sock;
    }
    StreamSocketT const& socket() const
    {
        return sock;
    }

private:
    StreamSocketT sock;
};

inline SocketStubPtrT create_client_socket(mprpc::Configuration const& _rcfg, frame::aio::ActorProxy const& _rproxy, char* _emplace_buf)
{

    if (sizeof(SocketStub) > static_cast<size_t>(ConnectionValues::SocketEmplacementSize)) {
        return SocketStubPtrT(new SocketStub(_rproxy, const_cast<ClientConfiguration*>(_rcfg.client.secure_any.cast<ClientConfiguration>())->context), SocketStub::delete_deleter);
    } else {
        return SocketStubPtrT(new (_emplace_buf) SocketStub(_rproxy, const_cast<ClientConfiguration*>(_rcfg.client.secure_any.cast<ClientConfiguration>())->context), SocketStub::emplace_deleter);
    }
}

inline SocketStubPtrT create_server_socket(mprpc::Configuration const& _rcfg, frame::aio::ActorProxy const& _rproxy, SocketDevice&& _usd, char* _emplace_buf)
{

    if (sizeof(SocketStub) > static_cast<size_t>(ConnectionValues::SocketEmplacementSize)) {
        return SocketStubPtrT(new SocketStub(_rproxy, std::move(_usd), const_cast<ServerConfiguration*>(_rcfg.server.secure_any.cast<ServerConfiguration>())->context), SocketStub::delete_deleter);
    } else {
        return SocketStubPtrT(new (_emplace_buf) SocketStub(_rproxy, std::move(_usd), const_cast<ServerConfiguration*>(_rcfg.server.secure_any.cast<ServerConfiguration>())->context), SocketStub::emplace_deleter);
    }
}

namespace impl {

} //namespace impl

inline unsigned long basic_secure_start(
    frame::aio::ReactorContext& _rctx, ConnectionContext& _rconctx, StreamSocketT& _rsock, ErrorConditionT& _rerr)
{
    return aio::openssl::VerifyModePeer;
}

inline bool basic_secure_verify(
    frame::aio::ReactorContext& _rctx, ConnectionContext& /*_rctx*/, StreamSocketT&, bool _preverified, frame::aio::openssl::VerifyContext& /*_rverify_ctx*/
)
{
    return _preverified;
}

struct NameCheckSecureStart {
    std::string name;

    NameCheckSecureStart() {}

    NameCheckSecureStart(const std::string& _name)
        : name(_name)
    {
    }
    NameCheckSecureStart(std::string&& _name)
        : name(std::move(_name))
    {
    }

    unsigned long operator()(frame::aio::ReactorContext& _rctx, ConnectionContext& _rconctx, StreamSocketT& _rsock, ErrorConditionT& _rerr) const
    {
        if (!name.empty()) {
            _rsock.secureSetCheckHostName(_rctx, name);
            solid_check_log(!_rctx.error(), service_logger());
        } else {
            _rsock.secureSetCheckHostName(_rctx, _rconctx.recipientName());
        }
        return aio::openssl::VerifyModePeer;
    }
};

enum ClientSetupFlags {
    ClientSetupIgnoreCertificateErrors = 1,
    ClientSetupNoCheckServerName       = 2
};

template <class ContextSetupFunction,
    class ConnectionStartSecureFunction = unsigned long (*)(frame::aio::ReactorContext&, ConnectionContext&, StreamSocketT&, ErrorConditionT&),
    class ConnectionOnSecureVerify      = bool (*)(frame::aio::ReactorContext&, ConnectionContext&, StreamSocketT&, bool, frame::aio::openssl::VerifyContext&)>
inline void setup_client(
    mprpc::Configuration&         _rcfg,
    const ContextSetupFunction&   _ctx_fnc,
    ConnectionStartSecureFunction _start_fnc  = basic_secure_start,
    ConnectionOnSecureVerify      _verify_fnc = basic_secure_verify)
{

    if (_rcfg.client.secure_any.empty()) {
        _rcfg.client.secure_any = make_any<ClientConfiguration>();
    }

    _rcfg.client.connection_create_socket_fnc = &create_client_socket;

    ClientConfiguration& rsecure_cfg = *_rcfg.client.secure_any.cast<ClientConfiguration>();

    rsecure_cfg.context = ContextT::create();

    _ctx_fnc(rsecure_cfg.context);

    rsecure_cfg.connection_prepare_secure_fnc = std::move(_start_fnc);
    rsecure_cfg.connection_verify_fnc         = std::move(_verify_fnc);
}

template <class ContextSetupFunction,
    class ConnectionStartSecureFunction = unsigned long (*)(frame::aio::ReactorContext&, ConnectionContext&, StreamSocketT&, ErrorConditionT&),
    class ConnectionOnSecureVerify      = bool (*)(frame::aio::ReactorContext&, ConnectionContext&, StreamSocketT&, bool, frame::aio::openssl::VerifyContext&)>
inline void setup_server(
    mprpc::Configuration&         _rcfg,
    const ContextSetupFunction&   _ctx_fnc,
    ConnectionStartSecureFunction _start_fnc  = basic_secure_start,
    ConnectionOnSecureVerify      _verify_fnc = basic_secure_verify)
{

    if (_rcfg.server.secure_any.empty()) {
        _rcfg.server.secure_any = make_any<ServerConfiguration>();
    }

    _rcfg.server.connection_create_socket_fnc = &create_server_socket;

    ServerConfiguration& rsecure_cfg = *_rcfg.server.secure_any.cast<ServerConfiguration>();

    rsecure_cfg.context = ContextT::create();

    _ctx_fnc(rsecure_cfg.context);

    rsecure_cfg.connection_prepare_secure_fnc = std::move(_start_fnc);
    rsecure_cfg.connection_verify_fnc         = std::move(_verify_fnc);
}

} //namespace openssl
} //namespace mprpc
} //namespace frame
} //namespace solid
