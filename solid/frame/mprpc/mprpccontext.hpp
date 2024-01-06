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

struct ConnectionContext : NonCopyable {
    ConnectionContext(
        frame::aio::ReactorContext& _rctx,
        const ConnectionProxy&      _rccs)
        : rservice(_rccs.service(_rctx))
        , rconnection(_rccs.connection(_rctx))
        , message_flags(0)
    {
    }

    Service& service() const
    {
        return rservice;
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
        return message_flags;
    }

    MessageId const& localMessageId() const
    {
        return message_id;
    }

    MessagePointerT<> fetchRequest(Message const& _rmsg) const;

    //! Keep any connection data
    Any<>& any();

    const ErrorConditionT& error() const;
    const ErrorCodeT&      systemError() const;

private:
    // not used for now
    RequestId const& requestId() const
    {
        return request_id;
    }

    void relayId(const UniqueId& _relay_id) const;

private:
    friend class Connection;
    friend class MessageWriter;
    friend class MessageReader;
    friend struct MessageHeader;
    friend struct MessageRelayHeader;
    friend struct Message;
    friend class TestEntryway;
    friend class relay::EngineCore;
    friend class Service;

    Service&            rservice;
    Connection&         rconnection;
    MessageHeader*      pmessage_header{nullptr};
    MessageFlagsT       message_flags{0};
    RequestId           request_id;
    MessageId           message_id;
    MessageRelayHeader* pmessage_relay_header_{nullptr}; // we cannot make it const - serializer constraint

    ConnectionContext(
        Service& _rsrv, Connection& _rcon)
        : rservice(_rsrv)
        , rconnection(_rcon)
    {
    }

    ConnectionContext(ConnectionContext const&);
    ConnectionContext& operator=(ConnectionContext const&);

    Connection& connection()
    {
        return rconnection;
    }
};

} // namespace mprpc
} // namespace frame
} // namespace solid
