// solid/frame/mpipc/mpipccontext.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_MPIPC_MPIPC_CONTEXT_HPP
#define SOLID_FRAME_MPIPC_MPIPC_CONTEXT_HPP

#include "solid/system/socketaddress.hpp"

#include "solid/utility/dynamicpointer.hpp"
#include "solid/utility/any.hpp"

#include <ostream>

#include "solid/frame/common.hpp"

namespace solid{
namespace frame{

namespace aio{
struct ReactorContext;
}//namespace aio

namespace mpipc{

struct Message;

//! A structure to uniquely indetify an IPC connection pool
/*!
    <b>Overview:</b><br>
    
    <b>Usage:</b><br>
    
*/
struct ConnectionPoolId: UniqueId{
    ConnectionPoolId(
        const size_t _idx = InvalidIndex(),
        const uint32_t _uid = InvalidIndex()
    ):UniqueId(_idx, _uid){}
    
};

struct RecipientId{
    
    RecipientId(){}
    
    RecipientId(
        const RecipientId &_rid
    ): poolid(_rid.poolid), connectionid(_rid.connectionid){}
    
    explicit RecipientId(
        const ObjectIdT &_rconid
    ):connectionid(_rconid){}
    
    
    bool isInvalid()const{
        return isInvalidConnection() || isInvalidPool();
    }
    
    bool isInvalidConnection()const{
        return connectionid.isInvalid();
    }
    
    bool isValidConnection()const{
        return connectionid.isValid();
    }
    
    bool isInvalidPool()const{
        return poolid.isInvalid();
    }
    
    bool isValidPool()const{
        return poolid.isValid();
    }
    
    ConnectionPoolId const& poolId()const{
        return poolid;
    }
    
    ObjectIdT const& connectionId()const{
        return connectionid;
    }
    
    void clear(){
        poolid.clear();
        connectionid.clear();
    }
    
private:
    friend class Service;
    friend class Connection;
    friend struct ConnectionContext;
    
    RecipientId(
        const ConnectionPoolId &_rpoolid,
        const ObjectIdT &_rconid
    ):poolid(_rpoolid), connectionid(_rconid){}
    
    ConnectionPoolId    poolid;
    ObjectIdT           connectionid;
};


std::ostream& operator<<(std::ostream &_ros, RecipientId const &_rec_id);

struct RequestId{
    uint32_t    index;
    uint32_t    unique;
    
    bool isInvalid()const{
        return index == InvalidIndex();
    }
    
    bool isValid()const{
        return !isInvalid();
    }
    
    RequestId(
        const uint32_t _idx = InvalidIndex(),
        const uint32_t _uid = InvalidIndex()
    ):index(_idx), unique(_uid){}
};

std::ostream& operator<<(std::ostream &_ros, RequestId const &_msguid);

struct MessageId{
    MessageId(): index(InvalidIndex()), unique(0){}
    MessageId(MessageId const &_rmsguid): index(_rmsguid.index), unique(_rmsguid.unique){}
    
    MessageId(RequestId const &_rrequid): index(_rrequid.index), unique(_rrequid.unique){}
    
    bool isInvalid()const{
        return index == InvalidIndex();
    }
    
    bool isValid()const{
        return !isInvalid();
    }
    void clear(){
        index = InvalidIndex();
        unique = 0;
    }
private:
    friend class Service;
    friend class Connection;
    friend class MessageWriter;
    friend struct ConnectionPoolStub;
    
    friend std::ostream& operator<<(std::ostream &_ros, MessageId const &_msguid);
    
    size_t      index;
    uint32_t    unique;

    MessageId(
        const size_t _idx,
        const uint32_t _uid
    ):index(_idx), unique(_uid){}
};


std::ostream& operator<<(std::ostream &_ros, MessageId const &_msguid);

class Service;
class Connection;
struct Configuration;

struct ConnectionProxy{
    //ConnectionProxy(const ConnectionProxy&) = delete;
    //ConnectionProxy(ConnectionProxy&&) = delete;
    
    ConnectionProxy& operator=(const ConnectionProxy&) = delete;
    //ConnectionProxy& operator=(ConnectionProxy&&) = delete;
    
private:
    friend class SocketStub;
    ConnectionProxy(){}
private:
    friend struct ConnectionContext;
    Service& service(frame::aio::ReactorContext &_rctx)const;
    Connection& connection(frame::aio::ReactorContext &_rctx)const;
};


struct ConnectionContext{
    ConnectionContext(
        frame::aio::ReactorContext &_rctx,
        const ConnectionProxy &_rccs
    ):rservice(_rccs.service(_rctx)), rconnection(_rccs.connection(_rctx)), message_flags(0), message_state(0){}
    
    Service& service()const{
        return rservice;
    }
    
    Configuration const & configuration()const;
    
    ObjectIdT connectionId()const;
    
    RecipientId recipientId()const;
    
    const std::string& recipientName()const;
    
    SocketDevice const & device()const;
    
    bool isConnectionActive()const;
    bool isConnectionServer()const;
    
    ulong messageFlags()const{
        return message_flags;
    }
    
    MessageId const& localMessageId()const{
        return message_id;
    }
    
    std::shared_ptr<Message> fetchRequest(Message const &_rmsg)const;
    
    //! Keep any connection data
    Any<>& any();
    
    const ErrorConditionT& error() const;
    const ErrorCodeT& systemError()const;
    
private:
    //not used for now
    RequestId const& requestId()const{
        return request_id;
    }

private:
    friend class Connection;
    friend class MessageWriter;
    friend struct Message;
    friend class TestEntryway;
    
    Service             &rservice;
    Connection          &rconnection;
    
    ulong               message_flags;
    uint8_t             message_state;
    RequestId           request_id;
    MessageId           message_id;
    
    
    
    ConnectionContext(
        Service &_rsrv, Connection &_rcon
    ):rservice(_rsrv), rconnection(_rcon), message_flags(0), message_state(0){}
    
    ConnectionContext(ConnectionContext const&);
    ConnectionContext& operator=(ConnectionContext const&);
};


}//namespace mpipc
}//namespace frame
}//namespace solid
#endif
