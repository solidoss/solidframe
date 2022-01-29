// solid/frame/mprpc/mprpcservice.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/exception.hpp"
#include "solid/utility/event.hpp"
#include "solid/utility/function.hpp"

#include "solid/frame/service.hpp"

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcerror.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpcprotocol.hpp"
#include "solid/system/log.hpp"
#include "solid/system/pimpl.hpp"

namespace solid {
namespace frame {

namespace aio {
struct ReactorContext;
} //namespace aio

namespace mprpc {

extern const Event pool_event_connect;
extern const Event pool_event_disconnect;

struct Message;
struct Configuration;
class Connection;
struct MessageBundle;

//! Message Passing Remote Procedure Call Service
/*!
    Allows exchanging ipc::Messages between processes.
    Synchronous vs Asynchronous messages
    A synchronous message is one sent with MessageFlags::Synchronous flag set:
    sendMessage(..., MessageFlags::Synchronous)
    Messages with MessageFlags::Synchronous flag unset are asynchronous.

    Synchronous messages
        * are always sent one after another, so they reach destination
            in the same order they were sent.
        * if multiple connections through peers are posible, only one is used for
            synchronous messages.

    Asynchronous messages
        * Because the messages are multiplexed, although the messages start being
            serialized on the socket stream in the same order sendMessage was called
            they will reach destination in a different order deppending on the
            message serialization size.


    Example:
        sendMessage("peer_name", m1_500MB, MessageFlags::Synchronous);
        sendMessage("peer_name", m2_100MB, MessageFlags::Synchronous);
        sendMessage("peer_name", m3_10MB, 0);
        sendMessage("peer_name", m4_1MB, 0);

        The order they will reach the peer side is:
        m4_1MB, m3_10MB, m1_500MB, m2_100MB
        or
        m3_10MB, m4_1MB, m1_500MB, m2_100MB

*/

class Service : public frame::Service {
public:
    typedef frame::Service BaseT;

    Service(
        frame::UseServiceShell _force_shell);

    Service(const Service&) = delete;
    Service(Service&&)      = delete;
    Service& operator=(const Service&) = delete;
    Service& operator=(Service&&) = delete;

    //! Destructor
    ~Service();

    Configuration const& configuration() const;

    ErrorConditionT createConnectionPool(const char* _recipient_url, const size_t _persistent_connection_count = 1);

    template <class F>
    ErrorConditionT createConnectionPool(
        const char*  _recipient_url,
        RecipientId& _rrecipient_id,
        const F      _event_fnc,
        const size_t _persistent_connection_count = 1);

    // send message using recipient name --------------------------------------
    template <class T>
    ErrorConditionT sendMessage(
        const char*               _recipient_url,
        std::shared_ptr<T> const& _rmsgptr,
        const MessageFlagsT&      _flags = 0);

    template <class T>
    ErrorConditionT sendMessage(
        const char*               _recipient_url,
        std::shared_ptr<T> const& _rmsgptr,
        RecipientId&              _rrecipient_id,
        const MessageFlagsT&      _flags = 0);

    template <class T>
    ErrorConditionT sendMessage(
        const char*               _recipient_url,
        std::shared_ptr<T> const& _rmsgptr,
        RecipientId&              _rrecipient_id,
        MessageId&                _rmsg_id,
        const MessageFlagsT&      _flags = 0);

    // send message using connection uid  -------------------------------------

    template <class T>
    ErrorConditionT sendMessage(
        RecipientId const&        _rrecipient_id,
        std::shared_ptr<T> const& _rmsgptr,
        const MessageFlagsT&      _flags = 0);

    template <class T>
    ErrorConditionT sendMessage(
        RecipientId const&        _rrecipient_id,
        std::shared_ptr<T> const& _rmsgptr,
        MessageId&                _rmsg_id,
        const MessageFlagsT&      _flags = 0);

    // send request using recipient name --------------------------------------

    template <class T, class Fnc>
    ErrorConditionT sendRequest(
        const char*               _recipient_url,
        std::shared_ptr<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        const MessageFlagsT&      _flags = 0);

    template <class T, class Fnc>
    ErrorConditionT sendRequest(
        const char*               _recipient_url,
        std::shared_ptr<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        RecipientId&              _rrecipient_id,
        const MessageFlagsT&      _flags = 0);

    template <class T, class Fnc>
    ErrorConditionT sendRequest(
        const char*               _recipient_url,
        std::shared_ptr<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        RecipientId&              _rrecipient_id,
        MessageId&                _rmsguid,
        const MessageFlagsT&      _flags = 0);

    // send message using connection uid  -------------------------------------

    template <class T>
    ErrorConditionT sendResponse(
        RecipientId const&        _rrecipient_id,
        std::shared_ptr<T> const& _rmsgptr,
        const MessageFlagsT&      _flags = 0);

    template <class T>
    ErrorConditionT sendResponse(
        RecipientId const&        _rrecipient_id,
        std::shared_ptr<T> const& _rmsgptr,
        MessageId&                _rmsg_id,
        const MessageFlagsT&      _flags = 0);

    // send request using connection uid --------------------------------------

    template <class T, class Fnc>
    ErrorConditionT sendRequest(
        RecipientId const&        _rrecipient_id,
        std::shared_ptr<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        const MessageFlagsT&      _flags = 0);

    template <class T, class Fnc>
    ErrorConditionT sendRequest(
        RecipientId const&        _rrecipient_id,
        std::shared_ptr<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        MessageId&                _rmsguid,
        const MessageFlagsT&      _flags = 0);
    // send message with complete using recipient name ------------------------

    template <class T, class Fnc>
    ErrorConditionT sendMessage(
        const char*               _recipient_url,
        std::shared_ptr<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        const MessageFlagsT&      _flags);

    template <class T, class Fnc>
    ErrorConditionT sendMessage(
        const char*               _recipient_url,
        std::shared_ptr<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        RecipientId&              _rrecipient_id,
        const MessageFlagsT&      _flags);

    template <class T, class Fnc>
    ErrorConditionT sendMessage(
        const char*               _recipient_url,
        std::shared_ptr<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        RecipientId&              _rrecipient_id,
        MessageId&                _rmsguid,
        const MessageFlagsT&      _flags);

    // send message with complete using connection uid ------------------------
    template <class T, class Fnc>
    ErrorConditionT sendMessage(
        RecipientId const&        _rrecipient_id,
        std::shared_ptr<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        const MessageFlagsT&      _flags);

    template <class T, class Fnc>
    ErrorConditionT sendMessage(
        RecipientId const&        _rrecipient_id,
        std::shared_ptr<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        MessageId&                _rmsguid,
        const MessageFlagsT&      _flags);
    //-------------------------------------------------------------------------
    ErrorConditionT sendRelay(const ActorIdT& _rconid, RelayData&& _urelmsg);
    ErrorConditionT sendRelayCancel(RelayData&& _urelmsg);
    //-------------------------------------------------------------------------
    template <typename F>
    ErrorConditionT forceCloseConnectionPool(
        RecipientId const& _rrecipient_id,
        F                  _f);

    template <typename F>
    ErrorConditionT delayCloseConnectionPool(
        RecipientId const& _rrecipient_id,
        F                  _f);

    template <class CompleteFnc>
    ErrorConditionT connectionNotifyEnterActiveState(
        RecipientId const& _rrecipient_id,
        CompleteFnc        _complete_fnc,
        const size_t       _send_buffer_capacity = 0 //0 means: leave as it is
    );

    ErrorConditionT connectionNotifyEnterActiveState(
        RecipientId const& _rrecipient_id,
        const size_t       _send_buffer_capacity = 0 //0 means: leave as it is
    );

    template <class CompleteFnc>
    ErrorConditionT connectionNotifyStartSecureHandshake(
        RecipientId const& _rrecipient_id,
        CompleteFnc        _complete_fnc);

    template <class CompleteFnc>
    ErrorConditionT connectionNotifyEnterPassiveState(
        RecipientId const& _rrecipient_id,
        CompleteFnc        _complete_fnc);

    ErrorConditionT connectionNotifyEnterPassiveState(
        RecipientId const& _rrecipient_id);

    template <class CompleteFnc>
    ErrorConditionT connectionNotifySendAllRawData(
        RecipientId const& _rrecipient_id,
        CompleteFnc        _complete_fnc,
        std::string&&      _rdata);

    template <class CompleteFnc>
    ErrorConditionT connectionNotifyRecvSomeRawData(
        RecipientId const& _rrecipient_id,
        CompleteFnc        _complete_fnc);

    template <class CompleteFnc>
    ErrorConditionT connectionPost(
        RecipientId const& _rrecipient_id,
        CompleteFnc        _complete_fnc);

    template <class CompleteFnc>
    void connectionPostAll(
        CompleteFnc _complete_fnc);

    ErrorConditionT cancelMessage(RecipientId const& _rrecipient_id, MessageId const& _rmsg_id);

    bool closeConnection(RecipientId const& _rrecipient_id);

protected:
    void doStart(Configuration&& _ucfg);
    void doStart();

    template <typename A>
    void doStart(Configuration&& _ucfg, A&& _a)
    {
        Configuration cfg;
        SocketDevice  sd;

        cfg.reset(std::move(_ucfg));
        cfg.check();
        cfg.prepare(sd);

        Service::doStartWithAny(
            std::forward<A>(_a),
            [this, &cfg, &sd]() {
                doFinalizeStart(std::move(cfg), std::move(sd));
            });
    }

private:
    ErrorConditionT doConnectionNotifyEnterActiveState(
        RecipientId const&                       _rrecipient_id,
        ConnectionEnterActiveCompleteFunctionT&& _ucomplete_fnc,
        const size_t                             _send_buffer_capacity);
    ErrorConditionT doConnectionNotifyStartSecureHandshake(
        RecipientId const&                          _rrecipient_id,
        ConnectionSecureHandhakeCompleteFunctionT&& _ucomplete_fnc);

    ErrorConditionT doConnectionNotifyEnterPassiveState(
        RecipientId const&                        _rrecipient_id,
        ConnectionEnterPassiveCompleteFunctionT&& _ucomplete_fnc);
    ErrorConditionT doConnectionNotifySendRawData(
        RecipientId const&                       _rrecipient_id,
        ConnectionSendRawDataCompleteFunctionT&& _ucomplete_fnc,
        std::string&&                            _rdata);
    ErrorConditionT doConnectionNotifyRecvRawData(
        RecipientId const&                       _rrecipient_id,
        ConnectionRecvRawDataCompleteFunctionT&& _ucomplete_fnc);
    ErrorConditionT doConnectionNotifyPost(
        RecipientId const&                _rrecipient_id,
        ConnectionPostCompleteFunctionT&& _ucomplete_fnc);
    void doConnectionNotifyPostAll(
        ConnectionPostCompleteFunctionT&& _ucomplete_fnc);

private:
    friend class Listener;
    friend class Connection;

    //void doStop();

    void doFinalizeStart(Configuration&& _ucfg, SocketDevice&& _usd);
    void doFinalizeStart();

    void acceptIncomingConnection(SocketDevice& _rsd);

    ErrorConditionT activateConnection(ConnectionContext& _rconctx, ActorIdT const& _ractui);

    void connectionStop(ConnectionContext& _rconctx);

    bool connectionStopping(
        ConnectionContext& _rconctx,
        ActorIdT const&    _ractuid,
        ulong&             _rseconds_to_wait,
        MessageId&         _rmsg_id,
        MessageBundle*     _pmsg_bundle,
        Event&             _revent_context,
        ErrorConditionT&   _rerror);

    bool doNonMainConnectionStopping(
        Connection& _rcon, ActorIdT const& _ractuid,
        ulong&           _rseconds_to_wait,
        MessageId&       _rmsg_id,
        MessageBundle*   _pmsg_bundle,
        Event&           _revent_context,
        ErrorConditionT& _rerror);

    bool doMainConnectionStoppingNotLast(
        Connection& _rcon, ActorIdT const& /*_ractuid*/,
        ulong&      _rseconds_to_wait,
        MessageId& /*_rmsg_id*/,
        MessageBundle* /*_pmsg_bundle*/,
        Event&           _revent_context,
        ErrorConditionT& _rerror);

    bool doMainConnectionStoppingCleanOneShot(
        Connection& _rcon, ActorIdT const& _ractuid,
        ulong&           _rseconds_to_wait,
        MessageId&       _rmsg_id,
        MessageBundle*   _rmsg_bundle,
        Event&           _revent_context,
        ErrorConditionT& _rerror);

    bool doMainConnectionStoppingCleanAll(
        Connection& _rcon, ActorIdT const& _ractuid,
        ulong&           _rseconds_to_wait,
        MessageId&       _rmsg_id,
        MessageBundle*   _rmsg_bundle,
        Event&           _revent_context,
        ErrorConditionT& _rerror);

    bool doMainConnectionStoppingPrepareCleanOneShot(
        Connection& _rcon, ActorIdT const& /*_ractuid*/,
        ulong& /*_rseconds_to_wait*/,
        MessageId& /*_rmsg_id*/,
        MessageBundle* /*_rmsg_bundle*/,
        Event& _revent_context,
        ErrorConditionT& /*_rerror*/);

    bool doMainConnectionStoppingPrepareCleanAll(
        Connection& _rcon, ActorIdT const& /*_ractuid*/,
        ulong& /*_rseconds_to_wait*/,
        MessageId& /*_rmsg_id*/,
        MessageBundle* /*_rmsg_bundle*/,
        Event&           _revent_context,
        ErrorConditionT& _rerror);

    bool doMainConnectionRestarting(
        Connection& _rcon, ActorIdT const& /*_ractuid*/,
        ulong& /*_rseconds_to_wait*/,
        MessageId& /*_rmsg_id*/,
        MessageBundle* /*_rmsg_bundle*/,
        Event&           _revent_context,
        ErrorConditionT& _rerror);

    void onIncomingConnectionStart(ConnectionContext& _rconctx);
    void onOutgoingConnectionStart(ConnectionContext& _rconctx);

    ErrorConditionT pollPoolForUpdates(
        Connection&      _rcon,
        ActorIdT const&  _ractuid,
        MessageId const& _rmsgid);

    void rejectNewPoolMessage(Connection const& _rcon);

    bool fetchMessage(Connection& _rcon, ActorIdT const& _ractuid, MessageId const& _rmsg_id);

    bool fetchCanceledMessage(Connection const& _rcon, MessageId const& _rmsg_id, MessageBundle& _rmsg_bundle);

    bool doTryPushMessageToConnection(
        Connection&     _rcon,
        ActorIdT const& _ractuid,
        const size_t    _pool_idx,
        const size_t    msg_idx);

    bool doTryPushMessageToConnection(
        Connection&      _rcon,
        ActorIdT const&  _ractuid,
        const size_t     _pool_idx,
        const MessageId& _rmsg_id);

    void forwardResolveMessage(ConnectionPoolId const& _rconpoolid, Event& _revent);

    void doPushFrontMessageToPool(
        const ConnectionPoolId& _rpool_id,
        MessageBundle&          _rmsgbundle,
        MessageId const&        _rmsgid);

    ErrorConditionT doSendMessage(
        const char*               _recipient_url,
        const RecipientId&        _rrecipient_id_in,
        MessagePointerT&          _rmsgptr,
        MessageCompleteFunctionT& _rcomplete_fnc,
        RecipientId*              _precipient_id_out,
        MessageId*                _pmsg_id_out,
        const MessageFlagsT&      _flags);

    ErrorConditionT doSendMessageToNewPool(
        const char*               _recipient_url,
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        RecipientId*              _precipient_id_out,
        MessageId*                _pmsguid_out,
        const MessageFlagsT&      _flags,
        std::string&              _msg_url);

    ErrorConditionT doSendMessageToConnection(
        const RecipientId&        _rrecipient_id_in,
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        MessageId*                _pmsg_id_out,
        MessageFlagsT             _flags,
        std::string&              _msg_url);

    bool doTryCreateNewConnectionForPool(const size_t _pool_index, ErrorConditionT& _rerror);

    void doFetchResendableMessagesFromConnection(
        Connection& _rcon);

    size_t doPushNewConnectionPool();

    void pushFrontMessageToConnectionPool(
        ConnectionPoolId& _rconpoolid,
        MessageBundle&    _rmsgbundle,
        MessageId const&  _rmsgid);

    bool doTryNotifyPoolWaitingConnection(const size_t _conpoolindex);

    ErrorConditionT doCreateConnectionPool(
        const char*           _recipient_url,
        RecipientId&          _rrecipient_id_out,
        PoolOnEventFunctionT& _event_fnc,
        const size_t          _persistent_connection_count);

    ErrorConditionT doForceCloseConnectionPool(
        RecipientId const&        _rrecipient_id,
        MessageCompleteFunctionT& _rcomplete_fnc);

    ErrorConditionT doDelayCloseConnectionPool(
        RecipientId const&        _rrecipient_id,
        MessageCompleteFunctionT& _rcomplete_fnc);

private:
    struct Data;
    PimplT<Data> impl_;
};

//-------------------------------------------------------------------------

using ServiceT = frame::ServiceShell<Service>;

//-------------------------------------------------------------------------
template <class T>
ErrorConditionT Service::sendMessage(
    const char*               _recipient_url,
    std::shared_ptr<T> const& _rmsgptr,
    const MessageFlagsT&      _flags)
{
    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    RecipientId              recipient_id;
    MessageCompleteFunctionT complete_handler;
    return doSendMessage(_recipient_url, recipient_id, msgptr, complete_handler, nullptr, nullptr, _flags);
}
//-------------------------------------------------------------------------
template <class T>
ErrorConditionT Service::sendMessage(
    const char*               _recipient_url,
    std::shared_ptr<T> const& _rmsgptr,
    RecipientId&              _rrecipient_id,
    const MessageFlagsT&      _flags)
{
    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    RecipientId              recipient_id;
    MessageCompleteFunctionT complete_handler;
    return doSendMessage(_recipient_url, recipient_id, msgptr, complete_handler, &_rrecipient_id, nullptr, _flags);
}
//-------------------------------------------------------------------------
template <class T>
ErrorConditionT Service::sendMessage(
    const char*               _recipient_url,
    std::shared_ptr<T> const& _rmsgptr,
    RecipientId&              _rrecipient_id,
    MessageId&                _rmsg_id,
    const MessageFlagsT&      _flags)
{
    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    RecipientId              recipient_id;
    MessageCompleteFunctionT complete_handler;
    return doSendMessage(_recipient_url, recipient_id, msgptr, complete_handler, &_rrecipient_id, &_rmsg_id, _flags);
}
// send message using connection uid  -------------------------------------
template <class T>
ErrorConditionT Service::sendMessage(
    RecipientId const&        _rrecipient_id,
    std::shared_ptr<T> const& _rmsgptr,
    const MessageFlagsT&      _flags)
{
    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    MessageCompleteFunctionT complete_handler;
    return doSendMessage(nullptr, _rrecipient_id, msgptr, complete_handler, nullptr, nullptr, _flags);
}
//-------------------------------------------------------------------------
template <class T>
ErrorConditionT Service::sendMessage(
    RecipientId const&        _rrecipient_id,
    std::shared_ptr<T> const& _rmsgptr,
    MessageId&                _rmsg_id,
    const MessageFlagsT&      _flags)
{
    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    MessageCompleteFunctionT complete_handler;
    return doSendMessage(nullptr, _rrecipient_id, msgptr, complete_handler, nullptr, &_rmsg_id, _flags);
}
//-------------------------------------------------------------------------
// send request using recipient name --------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendRequest(
    const char*               _recipient_url,
    std::shared_ptr<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    RecipientId              recipient_id;
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(_recipient_url, recipient_id, msgptr, complete_handler, nullptr, nullptr, _flags | MessageFlagsE::AwaitResponse);
}
//-------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendRequest(
    const char*               _recipient_url,
    std::shared_ptr<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    RecipientId&              _rrecipient_id,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    RecipientId              recipient_id;
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(_recipient_url, recipient_id, msgptr, complete_handler, &_rrecipient_id, nullptr, _flags | MessageFlagsE::AwaitResponse);
}
//-------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendRequest(
    const char*               _recipient_url,
    std::shared_ptr<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    RecipientId&              _rrecipient_id,
    MessageId&                _rmsguid,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    RecipientId              recipient_id;
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(_recipient_url, recipient_id, msgptr, complete_handler, &_rrecipient_id, &_rmsguid, _flags | MessageFlagsE::AwaitResponse);
}
//-------------------------------------------------------------------------
// send request using connection uid --------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendRequest(
    RecipientId const&        _rrecipient_id,
    std::shared_ptr<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(nullptr, _rrecipient_id, msgptr, complete_handler, nullptr, nullptr, _flags | MessageFlagsE::AwaitResponse);
}
//-------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendRequest(
    RecipientId const&        _rrecipient_id,
    std::shared_ptr<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    MessageId&                _rmsguid,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(nullptr, _rrecipient_id, msgptr, complete_handler, nullptr, &_rmsguid, _flags | MessageFlagsE::AwaitResponse);
}

//-------------------------------------------------------------------------
// send response using recipient id ---------------------------------------

template <class T>
ErrorConditionT Service::sendResponse(
    RecipientId const&        _rrecipient_id,
    std::shared_ptr<T> const& _rmsgptr,
    const MessageFlagsT&      _flags)
{
    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    MessageCompleteFunctionT complete_handler;
    return doSendMessage(nullptr, _rrecipient_id, msgptr, complete_handler, nullptr, nullptr, _flags | MessageFlagsE::Response);
}

//-------------------------------------------------------------------------

template <class T>
ErrorConditionT Service::sendResponse(
    RecipientId const&        _rrecipient_id,
    std::shared_ptr<T> const& _rmsgptr,
    MessageId&                _rmsg_id,
    const MessageFlagsT&      _flags)
{
    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    MessageCompleteFunctionT complete_handler;
    return doSendMessage(nullptr, _rrecipient_id, msgptr, complete_handler, nullptr, &_rmsg_id, _flags | MessageFlagsE::Response);
}

//-------------------------------------------------------------------------
// send message with complete using recipient name ------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendMessage(
    const char*               _recipient_url,
    std::shared_ptr<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    RecipientId              recipient_id;
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(_recipient_url, recipient_id, msgptr, complete_handler, nullptr, nullptr, _flags);
}
//-------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendMessage(
    const char*               _recipient_url,
    std::shared_ptr<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    RecipientId&              _rrecipient_id,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    RecipientId              recipient_id;
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(_recipient_url, recipient_id, msgptr, complete_handler, &_rrecipient_id, nullptr, _flags);
}
//-------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendMessage(
    const char*               _recipient_url,
    std::shared_ptr<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    RecipientId&              _rrecipient_id,
    MessageId&                _rmsguid,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    RecipientId              recipient_id;
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(_recipient_url, recipient_id, msgptr, complete_handler, &_rrecipient_id, &_rmsguid, _flags);
}
//-------------------------------------------------------------------------
// send message with complete using connection uid ------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendMessage(
    RecipientId const&        _rrecipient_id,
    std::shared_ptr<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(nullptr, _rrecipient_id, msgptr, complete_handler, nullptr, nullptr, _flags);
}
//-------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendMessage(
    RecipientId const&        _rrecipient_id,
    std::shared_ptr<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    MessageId&                _rmsguid,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    MessagePointerT          msgptr(std::static_pointer_cast<Message>(_rmsgptr));
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(nullptr, _rrecipient_id, msgptr, complete_handler, nullptr, &_rmsguid, _flags);
}
//-------------------------------------------------------------------------
template <typename F>
ErrorConditionT Service::forceCloseConnectionPool(
    RecipientId const& _rrecipient_id,
    F                  _f)
{
    auto fnc = [_f](ConnectionContext& _rctx, MessagePointerT& /*_rmsgptr*/, MessagePointerT&, ErrorConditionT const& /*_err*/) {
        _f(_rctx);
    };

    MessageCompleteFunctionT complete_handler(std::move(fnc));
    return doForceCloseConnectionPool(_rrecipient_id, complete_handler);
}
//-------------------------------------------------------------------------
template <typename F>
ErrorConditionT Service::delayCloseConnectionPool(
    RecipientId const& _rrecipient_id,
    F                  _f)
{
    auto fnc = [_f](ConnectionContext& _rctx, MessagePointerT& /*_rmsgptr*/, MessagePointerT&, ErrorConditionT const& /*_err*/) {
        _f(_rctx);
    };

    MessageCompleteFunctionT complete_handler(std::move(fnc));
    return doDelayCloseConnectionPool(_rrecipient_id, complete_handler);
}
//-------------------------------------------------------------------------
template <class CompleteFnc>
ErrorConditionT Service::connectionNotifyEnterActiveState(
    RecipientId const& _rrecipient_id,
    CompleteFnc        _complete_fnc,
    const size_t       _send_buffer_capacity /* = 0*/ //0 means: leave as it is
)
{
    ConnectionEnterActiveCompleteFunctionT complete_fnc(std::move(_complete_fnc));
    return doConnectionNotifyEnterActiveState(_rrecipient_id, std::move(complete_fnc), _send_buffer_capacity);
}
//-------------------------------------------------------------------------
template <class CompleteFnc>
ErrorConditionT Service::connectionPost(
    RecipientId const& _rrecipient_id,
    CompleteFnc        _complete_fnc)
{
    ConnectionPostCompleteFunctionT complete_fnc(std::move(_complete_fnc));
    return doConnectionNotifyPost(_rrecipient_id, std::move(complete_fnc));
}
//-------------------------------------------------------------------------
template <class CompleteFnc>
void Service::connectionPostAll(
    CompleteFnc _complete_fnc)
{
    ConnectionPostCompleteFunctionT complete_fnc(std::move(_complete_fnc));
    doConnectionNotifyPostAll(std::move(complete_fnc));
}
//-------------------------------------------------------------------------
inline ErrorConditionT Service::connectionNotifyEnterActiveState(
    RecipientId const& _rrecipient_id,
    const size_t       _send_buffer_capacity //0 means: leave as it is
)
{
    ConnectionEnterActiveCompleteFunctionT complete_fnc([](ConnectionContext&, ErrorConditionT const&) {});
    return doConnectionNotifyEnterActiveState(_rrecipient_id, std::move(complete_fnc), _send_buffer_capacity);
}
//-------------------------------------------------------------------------
template <class CompleteFnc>
ErrorConditionT Service::connectionNotifyStartSecureHandshake(
    RecipientId const& _rrecipient_id,
    CompleteFnc        _complete_fnc)
{
    ConnectionSecureHandhakeCompleteFunctionT complete_fnc(_complete_fnc);
    return doConnectionNotifyStartSecureHandshake(_rrecipient_id, std::move(complete_fnc));
}
//-------------------------------------------------------------------------
template <class CompleteFnc>
ErrorConditionT Service::connectionNotifyEnterPassiveState(
    RecipientId const& _rrecipient_id,
    CompleteFnc        _complete_fnc)
{
    ConnectionEnterPassiveCompleteFunctionT complete_fnc(_complete_fnc);
    return doConnectionNotifyEnterPassiveState(_rrecipient_id, std::move(complete_fnc));
}
//-------------------------------------------------------------------------
inline ErrorConditionT Service::connectionNotifyEnterPassiveState(
    RecipientId const& _rrecipient_id)
{
    ConnectionEnterPassiveCompleteFunctionT complete_fnc([](ConnectionContext&, ErrorConditionT const&) {});
    return doConnectionNotifyEnterPassiveState(_rrecipient_id, std::move(complete_fnc));
}
//-------------------------------------------------------------------------
template <class CompleteFnc>
ErrorConditionT Service::connectionNotifySendAllRawData(
    RecipientId const& _rrecipient_id,
    CompleteFnc        _complete_fnc,
    std::string&&      _rdata)
{
    ConnectionSendRawDataCompleteFunctionT complete_fnc(std::move(_complete_fnc));
    return doConnectionNotifySendRawData(_rrecipient_id, std::move(complete_fnc), std::move(_rdata));
}
//-------------------------------------------------------------------------
template <class CompleteFnc>
ErrorConditionT Service::connectionNotifyRecvSomeRawData(
    RecipientId const& _rrecipient_id,
    CompleteFnc        _complete_fnc)
{
    ConnectionRecvRawDataCompleteFunctionT complete_fnc(std::move(_complete_fnc));
    return doConnectionNotifyRecvRawData(_rrecipient_id, std::move(complete_fnc));
}
//-------------------------------------------------------------------------
inline ErrorConditionT Service::createConnectionPool(const char* _recipient_url, const size_t _persistent_connection_count)
{
    RecipientId          recipient_id;
    PoolOnEventFunctionT fnc([](ConnectionContext& _rctx, Event&&, const ErrorConditionT&) {});
    return doCreateConnectionPool(_recipient_url, recipient_id, fnc, _persistent_connection_count);
}
//-------------------------------------------------------------------------
template <class F>
ErrorConditionT Service::createConnectionPool(
    const char*  _recipient_url,
    RecipientId& _rrecipient_id,
    const F      _event_fnc,
    const size_t _persistent_connection_count)
{
    PoolOnEventFunctionT fnc(_event_fnc);
    return doCreateConnectionPool(_recipient_url, _rrecipient_id, fnc, _persistent_connection_count);
}
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
} //namespace mprpc
} //namespace frame
} //namespace solid
