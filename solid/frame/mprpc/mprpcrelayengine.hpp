// solid/frame/mprpc/mprpcrelayengine.hpp
//
// Copyright (c) 2017 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/system/error.hpp"
#include "solid/system/pimpl.hpp"

namespace solid {
namespace frame {
namespace mprpc {
namespace relay {

struct ConnectionStubBase {
    ActorIdT id_;
    uint32_t group_id_   = InvalidIndex();
    uint16_t replica_id_ = 0;
    size_t   next_       = InvalidIndex();
    size_t   prev_       = InvalidIndex();

    ConnectionStubBase() = default;

    ConnectionStubBase(const uint32_t _group_id, const uint16_t _replica_id)
        : group_id_(_group_id)
        , replica_id_(_replica_id)
    {
    }

    void clear()
    {
        id_.clear();
        group_id_   = InvalidIndex();
        replica_id_ = 0;
        next_       = InvalidIndex();
        prev_       = InvalidIndex();
    }
};

class EngineCore;

struct ConnectionPrintStub {
    const EngineCore&         re_;
    const ConnectionStubBase& rc_;
    ConnectionPrintStub(const EngineCore& _re, const ConnectionStubBase& _rc)
        : re_(_re)
        , rc_(_rc)
    {
    }
};

std::ostream& operator<<(std::ostream& _ros, const ConnectionPrintStub& _rps);

class EngineCore : public RelayEngine {
    struct Data;
    Pimpl<Data, 480> impl_;

public:
    struct Proxy {
        size_t              createConnection();
        ConnectionStubBase& connection(const size_t _idx);
        bool                notifyConnection(const ActorIdT& _rrelay_con_uid, const RelayEngineNotification _what);
        void                stopConnection(const size_t _idx);
        void                registerConnectionId(const ConnectionContext& _rconctx, const size_t _idx);

    private:
        friend class EngineCore;

        EngineCore& re_;
        Proxy(EngineCore& _re)
            : re_(_re)
        {
        }
        Proxy(const Proxy&) = delete;
        Proxy(Proxy&&)      = delete;
    };

    void     debugDump();
    Manager& manager();

    virtual std::ostream& print(std::ostream& _ros, const ConnectionStubBase& _rcon) const = 0;

protected:
    EngineCore(Manager& _rm);
    ~EngineCore();

    template <class F>
    void execute(F& _rf)
    {
        ExecuteFunctionT f(std::ref(_rf));
        doExecute(f);
    }

    ConnectionPrintStub plot(const ConnectionStubBase& _rcon) const
    {
        return ConnectionPrintStub(*this, _rcon);
    }

private:
    virtual void   unregisterConnectionName(Proxy& _proxy, size_t _conidx)                                 = 0;
    virtual size_t registerConnection(Proxy& _proxy, const uint32_t _group_id, const uint16_t _replica_id) = 0;

private:
    using ExecuteFunctionT = solid_function_t(void(Proxy&));

    void doExecute(ExecuteFunctionT& _rfnc);

    void stopConnection(const UniqueId& _rrelay_con_uid) final;

    void doStopConnection(const size_t _conidx);

    bool doRelayStart(
        const ActorIdT&  _rcon_uid,
        UniqueId&        _rrelay_con_uid,
        MessageHeader&   _rmsghdr,
        RelayData&&      _rrelmsg,
        MessageId&       _rrelay_id,
        ErrorConditionT& _rerror) final;

    bool doRelayResponse(
        const UniqueId&  _rrelay_con_uid,
        MessageHeader&   _rmsghdr,
        RelayData&&      _rrelmsg,
        const MessageId& _rrelay_id,
        ErrorConditionT& _rerror) final;

    bool doRelay(
        const UniqueId&  _rrelay_con_uid,
        RelayData&&      _rrelmsg,
        const MessageId& _rrelay_id,
        ErrorConditionT& _rerror) final;

    void doComplete(
        const UniqueId&  _rrelay_con_uid,
        RelayData*       _prelay_data,
        MessageId const& _rengine_msg_id,
        bool&            _rmore) final;

    void doCancel(
        const UniqueId&  _rrelay_con_uid,
        RelayData*       _prelay_data,
        MessageId const& _rengine_msg_id,
        DoneFunctionT&   _done_fnc) final;

    void doPollNew(const UniqueId& _rrelay_con_uid, PushFunctionT& _try_push_fnc, bool& _rmore) final;
    void doPollDone(const UniqueId& _rrelay_con_uid, DoneFunctionT& _done_fnc, CancelFunctionT& _cancel_fnc) final;

    size_t doRegisterNamedConnection(MessageRelayHeader&& _relay);
    size_t doRegisterUnnamedConnection(const ActorIdT& _rcon_uid, UniqueId& _rrelay_con_uid);

    void doRegisterConnectionId(const ConnectionContext& _rconctx, const size_t _idx);
};

} // namespace relay
} // namespace mprpc
} // namespace frame
} // namespace solid
