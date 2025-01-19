// solid/frame/mprpc/mprpccontext.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <ostream>

#include "solid/system/log.hpp"
#include "solid/system/socketaddress.hpp"

#include "solid/utility/any.hpp"

#include "solid/frame/mprpc/mprpcid.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpcmessageflags.hpp"

#include "solid/reflection/reflection.hpp"

namespace solid {
namespace frame {

namespace aio {
class ReactorContext;
} // namespace aio

namespace mprpc {

struct Message;
class Service;
struct MessageHeader;
struct MessageRelayHeader;

namespace relay {
class EngineCore;
}

const LoggerT& service_logger();

class Service;
class Connection;
class Configuration;

struct ConnectionProxy {
    ConnectionProxy& operator=(const ConnectionProxy&) = delete;

private:
    friend class SocketStub;
    ConnectionProxy() {}

private:
    friend struct ConnectionContext;
    Service&    service(frame::aio::ReactorContext& _rctx) const;
    Connection& connection(frame::aio::ReactorContext& _rctx) const;
};

class ConnectionContext : NonCopyable {
    frame::aio::ReactorContext& rreactor_context_;
    Service&                    rservice_;
    Connection&                 rconnection_;
    MessageHeader*              pmessage_header_{nullptr};
    MessageFlagsT               message_flags_{0};
    RequestId                   request_id_;
    MessageId                   message_id_;
    MessageRelayHeader*         pmessage_relay_header_{nullptr}; // we cannot make it const - serializer constraint
public:
    ConnectionContext(
        frame::aio::ReactorContext& _rctx,
        const ConnectionProxy&      _rccs)
        : rreactor_context_(_rctx)
        , rservice_(_rccs.service(_rctx))
        , rconnection_(_rccs.connection(_rctx))
        , message_flags_(0)
    {
    }

    Service& service() const
    {
        return rservice_;
    }

    Configuration const& configuration() const;

    ActorIdT connectionId() const;

    const UniqueId& relayId() const;

    RecipientId recipientId() const;

    const std::string& recipientName() const;

    SocketDevice const& device() const;

    bool isConnectionActive() const;
    bool isConnectionServer() const;

    const MessageFlagsT& messageFlags() const
    {
        return message_flags_;
    }

    MessageId const& localMessageId() const
    {
        return message_id_;
    }

    MessagePointerT<> fetchRequest(Message const& _rmsg) const;

    //! Keep any connection data
    Any<>& any();

    const ErrorConditionT& error() const;
    const ErrorCodeT&      systemError() const;

    void stop(const ErrorConditionT& _err);

    void pauseRead();
    void resumeRead();

private:
    // not used for now
    RequestId const& requestId() const
    {
        return request_id_;
    }

    void relayId(const UniqueId& _relay_id) const;

private:
    friend class Connection;
    friend class RelayConnection;
    friend class MessageWriter;
    friend class MessageReader;
    friend struct MessageHeader;
    friend struct MessageRelayHeader;
    friend struct Message;
    friend class TestEntryway;
    friend class relay::EngineCore;
    friend class Service;
    template <class Ctx>
    friend struct ConnectionSenderResponse;

    ConnectionContext(
        frame::aio::ReactorContext& _rctx,
        Service&                    _rsvc,
        Connection&                 _rcon)
        : rreactor_context_(_rctx)
        , rservice_(_rsvc)
        , rconnection_(_rcon)
    {
    }

    ConnectionContext(ConnectionContext const&)            = delete;
    ConnectionContext& operator=(ConnectionContext const&) = delete;

    Connection& connection()
    {
        return rconnection_;
    }
    auto& reactorContext()
    {
        return rreactor_context_;
    }
};

} // namespace mprpc
} // namespace frame
} // namespace solid
