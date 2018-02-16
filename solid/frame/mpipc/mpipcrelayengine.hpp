// solid/frame/mpipc/mpipcrelayengine.hpp
//
// Copyright (c) 2017 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/system/error.hpp"
#include "solid/system/pimpl.hpp"

namespace solid {
namespace frame {
namespace mpipc {
namespace relay {

struct ConnectionStubBase {
    ConnectionStubBase()
        : next_(InvalidIndex())
        , prev_(InvalidIndex())
    {
    }
    ConnectionStubBase(std::string&& _uname)
        : name_(std::move(_uname))
        , next_(InvalidIndex())
        , prev_(InvalidIndex())
    {
    }

    void clear()
    {
        id_.clear();
        name_.clear();
        next_ = InvalidIndex();
        prev_ = InvalidIndex();
    }

    ObjectIdT   id_;
    std::string name_;
    size_t      next_;
    size_t      prev_;
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
public:
    struct Proxy {
        size_t              createConnection();
        ConnectionStubBase& connection(const size_t _idx);
        bool                notifyConnection(const ObjectIdT& _rrelay_con_uid, const RelayEngineNotification _what);
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
    virtual void   unregisterConnectionName(Proxy& _proxy, size_t _conidx) = 0;
    virtual size_t registerConnection(Proxy& _proxy, std::string&& _uname) = 0;

private:
    using ExecuteFunctionT = SOLID_FUNCTION(void(Proxy&));

    void doExecute(ExecuteFunctionT& _rfnc);

    void stopConnection(const UniqueId& _rrelay_con_uid) final;

    void doStopConnection(const size_t _conidx);

    bool doRelayStart(
        const ObjectIdT& _rcon_uid,
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

    size_t doRegisterNamedConnection(std::string&& _uname);
    size_t doRegisterUnnamedConnection(const ObjectIdT& _rcon_uid, UniqueId& _rrelay_con_uid);

    void doRegisterConnectionId(const ConnectionContext& _rconctx, const size_t _idx);

private:
    struct Data;
    PimplT<Data> impl_;
};

} //namespace relay
} //namespace mpipc
} //namespace frame
} //namespace solid
