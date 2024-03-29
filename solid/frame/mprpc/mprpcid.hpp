#pragma once

#include "solid/frame/common.hpp"

namespace solid {
namespace frame {
namespace mprpc {
namespace relay {
class EngineCore;
}

class Service;
struct ConnectionContext;

//! A structure to uniquely indetify an IPC connection pool
/*!
    <b>Overview:</b><br>

    <b>Usage:</b><br>

*/
struct ConnectionPoolId : UniqueId {
    ConnectionPoolId(
        const size_t   _idx = InvalidIndex(),
        const uint32_t _uid = InvalidIndex())
        : UniqueId(_idx, _uid)
    {
    }
};

class RecipientId {
    friend class Service;
    friend class Connection;
    friend struct ConnectionContext;

    ConnectionPoolId pool_id_;
    ActorIdT         connection_id_;

    RecipientId(
        const ConnectionPoolId& _pool_id,
        const ActorIdT&         _connection_id)
        : pool_id_(_pool_id)
        , connection_id_(_connection_id)
    {
    }

public:
    RecipientId() {}

    RecipientId(
        const RecipientId& _other)
        : pool_id_(_other.pool_id_)
        , connection_id_(_other.connection_id_)
    {
    }

    explicit RecipientId(
        const ActorIdT& _connection_id)
        : connection_id_(_connection_id)
    {
    }

    bool isValid() const
    {
        return isValidConnection() && isValidPool();
    }

    bool empty() const
    {
        return isInvalidConnection() && isInvalidPool();
    }

    bool isInvalid() const
    {
        return isInvalidConnection() || isInvalidPool();
    }

    bool isInvalidConnection() const
    {
        return connection_id_.isInvalid();
    }

    bool isValidConnection() const
    {
        return connection_id_.isValid();
    }

    bool isInvalidPool() const
    {
        return pool_id_.isInvalid();
    }

    bool isValidPool() const
    {
        return pool_id_.isValid();
    }

    ConnectionPoolId const& poolId() const
    {
        return pool_id_;
    }

    ActorIdT const& connectionId() const
    {
        return connection_id_;
    }

    void clear()
    {
        pool_id_.clear();
        connection_id_.clear();
    }

    SOLID_REFLECT_V1(_r, _rthis, _rctx)
    {
        _r.add(_rthis.pool_id_, _rctx, 1, "pool_id");
        _r.add(_rthis.connection_id_, _rctx, 2, "connection_id");
    }
};

inline bool operator<(RecipientId const& _rec_id1, RecipientId const& _rec_id2)
{
    if (_rec_id1.connectionId() < _rec_id2.connectionId()) {
        return true;
    } else if (_rec_id2.connectionId() < _rec_id1.connectionId()) {
        return false;
    } else {
        return _rec_id1.poolId() < _rec_id2.poolId();
    }
}

inline bool operator==(RecipientId const& _rec_id1, RecipientId const& _rec_id2)
{
    return _rec_id1.connectionId() == _rec_id2.connectionId() && _rec_id1.poolId() == _rec_id2.poolId();
}

std::ostream& operator<<(std::ostream& _ros, RecipientId const& _rec_id);

struct RequestId {
    uint32_t index;
    uint32_t unique;

    bool isInvalid() const
    {
        return index == 0;
    }

    bool isValid() const
    {
        return !isInvalid();
    }

    void clear()
    {
        index  = 0;
        unique = 0;
    }

    bool operator==(const RequestId& _reqid) const
    {
        return index == _reqid.index && unique == _reqid.unique;
    }

    RequestId(
        const uint32_t _idx = 0,
        const uint32_t _uid = 0)
        : index(_idx)
        , unique(_uid)
    {
    }
};

std::ostream& operator<<(std::ostream& _ros, RequestId const& _msguid);

struct MessageId {
    MessageId()
        : index(InvalidIndex())
        , unique(0)
    {
    }
    MessageId(MessageId const& _rmsguid)
        : index(_rmsguid.index)
        , unique(_rmsguid.unique)
    {
    }

    MessageId(RequestId const& _rrequid)
        : index(_rrequid.index)
        , unique(_rrequid.unique)
    {
        if (_rrequid.isInvalid()) {
            index = InvalidIndex();
        } else {
            --index;
        }
    }

    bool isInvalid() const
    {
        return index == InvalidIndex();
    }

    bool isValid() const
    {
        return !isInvalid();
    }
    void clear()
    {
        index  = InvalidIndex();
        unique = 0;
    }

private:
    friend class Service;
    friend class Connection;
    friend class MessageWriter;
    friend struct ConnectionPoolStub;
    friend class relay::EngineCore;

    friend std::ostream& operator<<(std::ostream& _ros, MessageId const& _msguid);

    size_t   index;
    uint32_t unique;

    MessageId(
        const size_t   _idx,
        const uint32_t _uid)
        : index(_idx)
        , unique(_uid)
    {
    }
};

std::ostream& operator<<(std::ostream& _ros, MessageId const& _msguid);

} // namespace mprpc
} // namespace frame
} // namespace solid