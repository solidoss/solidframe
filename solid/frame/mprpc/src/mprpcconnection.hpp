// solid/frame/ipc/src/ipcconnection.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/socketdevice.hpp"

#include "solid/system/flags.hpp"
#include "solid/utility/any.hpp"
#include "solid/utility/event.hpp"
#include "solid/utility/queue.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcsocketstub.hpp"

#include "mprpcmessagereader.hpp"
#include "mprpcmessagewriter.hpp"

namespace solid {
namespace frame {
namespace aio {

namespace openssl {
class Context;
} // namespace openssl
} // namespace aio

namespace mprpc {

class Service;

struct ResolveMessage {
    AddressVectorT addrvec;

    bool empty() const
    {
        return addrvec.empty();
    }

    SocketAddressInet const& currentAddress() const
    {
        return addrvec.back();
    }

    void popAddress();

    ResolveMessage(AddressVectorT&& _raddrvec)
        : addrvec(std::move(_raddrvec))
    {
    }

    ResolveMessage(const ResolveMessage&) = delete;

    ResolveMessage(ResolveMessage&& _urm)
        : addrvec(std::move(_urm.addrvec))
    {
    }
};

using MessageIdVectorT = std::vector<MessageId>;

class Connection : public frame::aio::Actor {
public:
    using PointerT = std::shared_ptr<Connection>;

    static EventT eventResolve();
    static EventT eventNewMessage();
    static EventT eventNewMessage(const MessageId&);
    static EventT eventNewQueueMessage();
    static EventT eventCancelConnMessage(const MessageId&);
    static EventT eventCancelPoolMessage(const MessageId&);
    static EventT eventClosePoolMessage(const MessageId&);
    static EventT eventStopping();
    static EventT eventEnterActive(ConnectionEnterActiveCompleteFunctionT&&, const size_t _send_buffer_capacity);
    static EventT eventEnterPassive(ConnectionEnterPassiveCompleteFunctionT&&);
    static EventT eventStartSecure(ConnectionSecureHandhakeCompleteFunctionT&&);
    static EventT eventSendRaw(ConnectionSendRawDataCompleteFunctionT&&, std::string&&);
    static EventT eventRecvRaw(ConnectionRecvRawDataCompleteFunctionT&&);
    static EventT eventPost(ConnectionPostCompleteFunctionT&&);

    Connection(
        Configuration const&    _rconfiguration,
        ConnectionPoolId const& _rpool_id,
        std::string const&      _rpool_name);

    Connection(
        Configuration const&    _rconfiguration,
        SocketDevice&           _rsd,
        ConnectionPoolId const& _rpool_id,
        std::string const&      _rpool_name);

    ~Connection();

    // NOTE: will always accept null message
    bool tryPushMessage(
        Configuration const& _rconfiguration,
        MessageBundle&       _rmsgbundle,
        MessageId&           _rconn_msg_id,
        const MessageId&     _rpool_msg_id);

    const ErrorConditionT& error() const;
    const ErrorCodeT&      systemError() const;

    bool isFull(Configuration const& _rconfiguration) const;
    bool canHandleMore(Configuration const& _rconfiguration) const;

    bool isInPoolWaitingQueue() const;

    void setInPoolWaitingQueue();

    bool isServer() const;
    bool isConnected() const;
    bool isSecured() const;

    bool isWriterEmpty() const;

    const UniqueId& relayId() const;
    void            relayId(const UniqueId& _relay_id);

    MessagePointerT<> fetchRequest(Message const& _rmsg) const;

    ConnectionPoolId const& poolId() const;
    const std::string&      poolName() const;

    MessageIdVectorT const& pendingMessageVector() const
    {
        return pending_message_vec_;
    }

    void pendingMessageVectorEraseFirst(const size_t _count)
    {
        pending_message_vec_.erase(pending_message_vec_.begin(), pending_message_vec_.begin() + _count);
    }

    template <class Fnc>
    void fetchResendableMessages(Service& _rsvc,
        Fnc const&                        _f)
    {
        auto visit_fnc = [this /*, &_rsvc*/, &_f](
                             MessageBundle&   _rmsgbundle,
                             MessageId const& _rmsgid) {
            _f(this->poolId(), _rmsgbundle, _rmsgid);
        };
        MessageWriter::VisitFunctionT fnc(std::cref(visit_fnc));

        msg_writer_.forEveryMessagesNewerToOlder(fnc);
    }

    static void onSendAllRaw(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    static void onRecvSomeRaw(frame::aio::ReactorContext& _rctx, const size_t _sz, EventBase& _revent);

protected:
    template <class Ctx>
    static void onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    template <class Ctx>
    static void onSend(frame::aio::ReactorContext& _rctx);
    template <class Ctx>
    static void onConnect(frame::aio::ReactorContext& _rctx);
    template <class Ctx>
    static void onTimer(frame::aio::ReactorContext& _rctx);
    template <class Ctx>
    static void onSecureConnect(frame::aio::ReactorContext& _rctx);
    template <class Ctx>
    static void onSecureAccept(frame::aio::ReactorContext& _rctx);

    Service&             service(frame::aio::ReactorContext& _rctx) const;
    ActorIdT             uid(frame::aio::ReactorContext& _rctx) const;
    const Configuration& configuration(frame::aio::ReactorContext& _rctx) const;

protected:
    enum class FlagsE : size_t {
        Active,
        Server,
        Keepalive,
        StopPeer,
        PollPool,
        PollRelayEngine,
        Stopping,
        DelayedStopping,
        Secure,
        Raw,
        InPoolWaitQueue,
        Connected, // once set - the flag should not be reset. Is used by pool for restarting
        PauseRecv,
        LastFlag,
    };

    bool isStopping() const;

    void setFlag(const FlagsE _flag);
    void resetFlag(const FlagsE _flag);

    const SharedBuffer& recvBuffer() const;

    void returnRecvBuffer(SharedBuffer&& _buf);

    void ackBufferCountAdd(uint8_t _val);

    size_t cancelRemoveMessageVectorSize() const;
    void   cancelRemoveMessageVectorAppend(const RequestId& _id);

    void updateContextOnCompleteMessage(
        ConnectionContext& _rconctx, MessageBundle& _rmsg_bundle,
        MessageId const& _rpool_msg_id, const Message& _rmsg) const;

    UniqueId& relayId();

    bool shouldPollRelayEngine() const;

    template <class Ctx>
    void doSend(frame::aio::ReactorContext& _rctx);
    template <class Ctx>
    void doStop(frame::aio::ReactorContext& _rctx, const ErrorConditionT& _rerr, const ErrorCodeT& _rsyserr = ErrorCodeT());

    void doHandleEventGeneric(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    template <class Ctx>
    void doHandleEventKill(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    template <class Ctx>
    void doHandleEventStart(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    template <class Ctx>
    void doHandleEventNewPoolMessage(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    template <class Ctx>
    void doHandleEventNewPoolQueueMessage(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    template <class Ctx>
    void doHandleEventNewConnMessage(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    template <class Ctx>
    void doHandleEventCancelConnMessage(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    void doHandleEventCancelPoolMessage(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    void doHandleEventClosePoolMessage(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    template <class Ctx>
    void doHandleEventEnterActive(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    template <class Ctx>
    void doHandleEventEnterPassive(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    template <class Ctx>
    void doHandleEventStartSecure(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    void doHandleEventSendRaw(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    void doHandleEventRecvRaw(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    void doHandleEventPost(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    void doHandleEventStopping(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    void doContinueStopping(frame::aio::ReactorContext& _rctx, const EventT& _revent);
    template <class Ctx>
    void doCompleteAllMessages(frame::aio::ReactorContext& _rctx, size_t _offset);
    template <class Ctx>
    void doResetRecvBuffer(frame::aio::ReactorContext& _rctx, const uint8_t _request_buffer_ack_count, ErrorConditionT& _rerr);

    template <class Ctx>
    bool connect(frame::aio::ReactorContext& _rctx, const SocketAddressInet& _raddr);

    template <class Ctx>
    void doPauseRead(frame::aio::ReactorContext& _rctx);
    template <class Ctx>
    void doResumeRead(frame::aio::ReactorContext& _rctx);

private:
    bool postSendAll(frame::aio::ReactorContext& _rctx, const char* _pbuf, size_t _bufcp, EventBase& _revent);
    template <class Ctx>
    bool postRecvSome(frame::aio::ReactorContext& _rctx, char* _pbuf, size_t _bufcp);
    bool postRecvSome(frame::aio::ReactorContext& _rctx, char* _pbuf, size_t _bufcp, EventBase& _revent);
    template <class Ctx>
    bool recvSome(frame::aio::ReactorContext& _rctx, char* _buf, size_t _bufcp, size_t& _sz);
    template <class Ctx>
    bool sendAll(frame::aio::ReactorContext& _rctx, char* _buf, size_t _bufcp);
    void prepareSocket(frame::aio::ReactorContext& _rctx);

    const NanoTime& minTimeout() const;

    ErrorConditionT pollServicePoolForUpdates(frame::aio::ReactorContext& _rctx, MessageId const& _rpool_msg_id);

private:
    friend struct ConnectionContext;
    friend class RelayEngine;
    friend class Service;

    SocketDevice const& device() const;
    Any<>&              any();

    static bool notify(Manager& _rm, const ActorIdT&, const RelayEngineNotification);

    virtual void onEvent(frame::aio::ReactorContext& _rctx, EventBase&& _uevent) = 0;

    bool shouldSendKeepAlive() const;
    bool shouldPollPool() const;

    bool willAcceptNewMessage(frame::aio::ReactorContext& _rctx) const;

    bool isWaitingKeepAliveTimer() const;
    bool isStopPeer() const;

    // The connection is aware that it is activated
    bool isActiveState() const;
    bool isRawState() const;

    bool isDelayedStopping() const;

    bool hasCompletingMessages() const;

    void onStopped(frame::aio::ReactorContext& _rctx,
        MessageId const&                       _rpool_msg_id,
        MessageBundle&                         _rmsg_local);

    template <class Ctx>
    void doStart(frame::aio::ReactorContext& _rctx, const bool _is_incoming);

    void doActivate(
        frame::aio::ReactorContext& _rctx,
        EventBase&                  _revent);

    void doOptimizeRecvBuffer();
    void doOptimizeRecvBufferForced();
    void doPrepare(frame::aio::ReactorContext& _rctx);
    void doUnprepare(frame::aio::ReactorContext& _rctx);
    template <class Ctx>
    void doResetTimer(frame::aio::ReactorContext& _rctx);

    template <class Ctx>
    ResponseStateE doCheckResponseState(frame::aio::ReactorContext& _rctx, const MessageHeader& _rmsghdr, MessageId& _rrelay_id, const bool _erase_request);
    template <class Ctx>
    bool doCompleteMessage(
        frame::aio::ReactorContext& _rctx, MessagePointerT<>& _rresponse_ptr, const size_t _response_type_id);

    void doCompleteMessage(
        solid::frame::aio::ReactorContext& _rctx,
        MessageId const&                   _rpool_msg_id,
        MessageBundle&                     _rmsg_local,
        ErrorConditionT const&             _rerr);
    template <class Ctx>
    void doCompleteKeepalive(frame::aio::ReactorContext& _rctx);
    template <class Ctx>
    void doCompleteAckCount(frame::aio::ReactorContext& _rctx, uint8_t _count);
    template <class Ctx>
    void doCompleteCancelRequest(frame::aio::ReactorContext& _rctx, const RequestId& _reqid);

    virtual void stop(frame::aio::ReactorContext& _rctx, const ErrorConditionT& _rerr) = 0;
    virtual void pauseRead(frame::aio::ReactorContext& _rctx)                          = 0;
    virtual void resumeRead(frame::aio::ReactorContext& _rctx)                         = 0;

private:
    using TimerT
        = frame::aio::SteadyTimer;
    using FlagsT            = solid::Flags<FlagsE>;
    using RequestIdVectorT  = MessageWriter::RequestIdVectorT;
    using RecvBufferVectorT = std::vector<SharedBuffer>;

    template <class Ctx>
    friend struct ConnectionReceiver;
    template <class Ctx>
    friend struct ConnectionSender;
    template <class Ctx>
    friend struct ConnectionSenderResponse;

    ConnectionPoolId                      pool_id_;
    const std::string&                    rpool_name_;
    TimerT                                timer_;
    FlagsT                                flags_                   = 0;
    size_t                                cons_buf_off_            = 0;
    uint32_t                              recv_keepalive_count_    = 0;
    std::chrono::steady_clock::time_point recv_keepalive_boundary_ = std::chrono::steady_clock::time_point::min();
    uint16_t                              recv_buf_count_          = 0;
    uint8_t                               send_relay_free_count_;
    uint8_t                               ackd_buf_count_ = 0;
    SharedBuffer                          recv_buf_;
    RecvBufferVectorT                     recv_buf_vec_;
    SharedBuffer                          send_buf_;
    MessageIdVectorT                      pending_message_vec_;
    MessageReader                         msg_reader_;
    MessageWriter                         msg_writer_;
    RequestIdVectorT                      cancel_remote_msg_vec_;
    ErrorConditionT                       error_;
    ErrorCodeT                            sys_error_;
    bool                                  poll_pool_more_ = true;
    bool                                  send_posted_    = false;
    Any<>                                 any_data_;
    char                                  socket_emplace_buf_[static_cast<size_t>(ConnectionValues::SocketEmplacementSize)];
    SocketStubPtrT                        sock_ptr_;
    NanoTime                              timeout_recv_      = NanoTime::max(); // client and server
    NanoTime                              timeout_send_soft_ = NanoTime::max(); // client and server
    NanoTime                              timeout_send_hard_ = NanoTime::max(); // client and server
    NanoTime                              timeout_secure_    = NanoTime::max(); // server
    NanoTime                              timeout_active_    = NanoTime::max(); // server
    NanoTime                              timeout_keepalive_ = NanoTime::max(); // client
    UniqueId                              relay_id_;
};

//-----------------------------------------------------------------------------
// ClientConnection
//-----------------------------------------------------------------------------

class ClientConnection final : public Connection {
public:
    ClientConnection(
        Configuration const&    _rconfiguration,
        ConnectionPoolId const& _rpool_id,
        std::string const&      _rpool_name)
        : Connection(_rconfiguration, _rpool_id, _rpool_name)
    {
    }

private:
    void onEvent(frame::aio::ReactorContext& _rctx, EventBase&& _uevent) override;

    void doHandleEventResolve(frame::aio::ReactorContext& _rctx, EventBase& _revent);

    void stop(frame::aio::ReactorContext& _rctx, const ErrorConditionT& _rerr) override;
    void pauseRead(frame::aio::ReactorContext& _rctx) override;
    void resumeRead(frame::aio::ReactorContext& _rctx) override;
};

//-----------------------------------------------------------------------------
// ServerConnection
//-----------------------------------------------------------------------------

class ServerConnection final : public Connection {
public:
    ServerConnection(
        Configuration const&    _rconfiguration,
        SocketDevice&           _rsd,
        ConnectionPoolId const& _rpool_id,
        std::string const&      _rpool_name)
        : Connection(_rconfiguration, _rsd, _rpool_id, _rpool_name)
    {
    }

private:
    void onEvent(frame::aio::ReactorContext& _rctx, EventBase&& _uevent) override;

    void stop(frame::aio::ReactorContext& _rctx, const ErrorConditionT& _rerr) override;
    void pauseRead(frame::aio::ReactorContext& _rctx) override;
    void resumeRead(frame::aio::ReactorContext& _rctx) override;
};

//-----------------------------------------------------------------------------
// RelayConnection
//-----------------------------------------------------------------------------

class RelayConnection final : public Connection {
public:
    RelayConnection(
        Configuration const&    _rconfiguration,
        SocketDevice&           _rsd,
        ConnectionPoolId const& _rpool_id,
        std::string const&      _rpool_name)
        : Connection(_rconfiguration, _rsd, _rpool_id, _rpool_name)
    {
    }

private:
    friend struct RelaySender;
    friend struct RelayReceiver;
    friend struct RelayContext;

    void onEvent(frame::aio::ReactorContext& _rctx, EventBase&& _uevent) override;

    void tryPollRelayEngine(frame::aio::ReactorContext& _rctx, const Configuration& _rconfig, MessageWriter& _rmsgwriter);

    void doCompleteRelayed(
        frame::aio::ReactorContext& _rctx,
        RelayData*                  _prelay_data,
        MessageId const&            _rengine_msg_id);

    void doCancelRelayed(
        frame::aio::ReactorContext& _rctx,
        RelayData*                  _prelay_data,
        MessageId const&            _rengine_msg_id);

    bool doReceiveRelayStart(
        frame::aio::ReactorContext& _rctx,
        MessageHeader&              _rmsghdr,
        const char*                 _pbeg,
        size_t                      _sz,
        MessageId&                  _rrelay_id,
        const bool                  _is_last,
        ErrorConditionT&            _rerror);

    bool doReceiveRelayBody(
        frame::aio::ReactorContext& _rctx,
        const char*                 _pbeg,
        size_t                      _sz,
        const MessageId&            _rrelay_id,
        const bool                  _is_last,
        ErrorConditionT&            _rerror);

    bool doReceiveRelayResponse(
        frame::aio::ReactorContext& _rctx,
        MessageHeader&              _rmsghdr,
        const char*                 _pbeg,
        size_t                      _sz,
        const MessageId&            _rrelay_id,
        const bool                  _is_last,
        ErrorConditionT&            _rerror);

    void doHandleEventRelayNew(frame::aio::ReactorContext& _rctx, EventBase& _revent);
    void doHandleEventRelayDone(frame::aio::ReactorContext& _rctx, EventBase& _revent);

    void stop(frame::aio::ReactorContext& _rctx, const ErrorConditionT& _rerr) override;
    void pauseRead(frame::aio::ReactorContext& _rctx) override;
    void resumeRead(frame::aio::ReactorContext& _rctx) override;
};

//-----------------------------------------------------------------------------
inline Connection::PointerT new_connection(
    Configuration const&    _rconfiguration,
    SocketDevice&           _rsd,
    ConnectionPoolId const& _rpool_id,
    std::string const&      _rpool_name)
{
    if (!_rconfiguration.isEnabledRelayEngine()) {
        return std::make_shared<ServerConnection>(_rconfiguration, _rsd, _rpool_id, _rpool_name);
    } else {
        return std::make_shared<RelayConnection>(_rconfiguration, _rsd, _rpool_id, _rpool_name);
    }
}
//-----------------------------------------------------------------------------
inline Connection::PointerT new_connection(
    Configuration const&    _rconfiguration,
    ConnectionPoolId const& _rpool_id,
    std::string const&      _rpool_name)
{
    return std::make_shared<ClientConnection>(_rconfiguration, _rpool_id, _rpool_name);
}

//-----------------------------------------------------------------------------
// Connection
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool Connection::isActiveState() const
{
    return flags_.has(FlagsE::Active);
}
//-----------------------------------------------------------------------------
inline bool Connection::isRawState() const
{
    return flags_.has(FlagsE::Raw);
}
//-----------------------------------------------------------------------------
inline bool Connection::isStopping() const
{
    return flags_.has(FlagsE::Stopping);
}
//-----------------------------------------------------------------------------
inline bool Connection::isDelayedStopping() const
{
    return flags_.has(FlagsE::DelayedStopping);
}
//-----------------------------------------------------------------------------
inline bool Connection::isServer() const
{
    return flags_.has(FlagsE::Server);
}
//-----------------------------------------------------------------------------
inline bool Connection::isConnected() const
{
    return flags_.has(FlagsE::Connected);
}
//-----------------------------------------------------------------------------
inline bool Connection::isSecured() const
{
    return flags_.has(FlagsE::Secure);
}
//-----------------------------------------------------------------------------
inline bool Connection::shouldSendKeepAlive() const
{
    return flags_.has(FlagsE::Keepalive);
}
//-----------------------------------------------------------------------------
inline bool Connection::shouldPollPool() const
{
    return flags_.has(FlagsE::PollPool);
}
//-----------------------------------------------------------------------------
inline bool Connection::shouldPollRelayEngine() const
{
    return flags_.has(FlagsE::PollRelayEngine);
}
//-----------------------------------------------------------------------------
inline bool Connection::isStopPeer() const
{
    return flags_.has(FlagsE::StopPeer);
}
//-----------------------------------------------------------------------------
inline bool Connection::isInPoolWaitingQueue() const
{
    return flags_.has(FlagsE::InPoolWaitQueue);
}
//-----------------------------------------------------------------------------
inline void Connection::setInPoolWaitingQueue()
{
    flags_.set(FlagsE::InPoolWaitQueue);
}
//-----------------------------------------------------------------------------
inline Any<>& Connection::any()
{
    return any_data_;
}
//-----------------------------------------------------------------------------
inline MessagePointerT<> Connection::fetchRequest(Message const& _rmsg) const
{
    return msg_writer_.fetchRequest(_rmsg.requestId());
}
//-----------------------------------------------------------------------------
inline ConnectionPoolId const& Connection::poolId() const
{
    return pool_id_;
}
//-----------------------------------------------------------------------------
inline const std::string& Connection::poolName() const
{
    return rpool_name_;
}
//-----------------------------------------------------------------------------
inline bool Connection::isWriterEmpty() const
{
    return msg_writer_.isEmpty();
}
//-----------------------------------------------------------------------------
inline const ErrorConditionT& Connection::error() const
{
    return error_;
}
//-----------------------------------------------------------------------------
inline const ErrorCodeT& Connection::systemError() const
{
    return sys_error_;
}
//-----------------------------------------------------------------------------
inline SocketDevice const& Connection::device() const
{
    return sock_ptr_->device();
}
//-----------------------------------------------------------------------------
inline bool Connection::isFull(Configuration const& _rconfiguration) const
{
    return msg_writer_.isFull(_rconfiguration.writer);
}
//-----------------------------------------------------------------------------
inline bool Connection::canHandleMore(Configuration const& _rconfiguration) const
{
    return msg_writer_.canHandleMore(_rconfiguration.writer);
}
//-----------------------------------------------------------------------------
inline const NanoTime& Connection::minTimeout() const
{
    if (isServer()) {
        return std::min(timeout_send_soft_, std::min(timeout_send_hard_, std::min(timeout_recv_, std::min(timeout_secure_, timeout_active_))));
    } else {
        return std::min(timeout_send_soft_, std::min(timeout_send_hard_, std::min(timeout_recv_, timeout_keepalive_)));
    }
}
//-----------------------------------------------------------------------------
inline void Connection::setFlag(const FlagsE _flag)
{
    flags_.set(_flag);
}
//-----------------------------------------------------------------------------
inline void Connection::resetFlag(const FlagsE _flag)
{
    flags_.reset(_flag);
}
//-----------------------------------------------------------------------------
inline const SharedBuffer& Connection::recvBuffer() const
{
    return recv_buf_;
}
//-----------------------------------------------------------------------------
inline void Connection::returnRecvBuffer(SharedBuffer&& _buf)
{
    recv_buf_vec_.emplace_back(std::move(_buf));
}
//-----------------------------------------------------------------------------
inline void Connection::ackBufferCountAdd(uint8_t _val)
{
    ackd_buf_count_ += _val;
}
//-----------------------------------------------------------------------------
inline size_t Connection::cancelRemoveMessageVectorSize() const
{
    return cancel_remote_msg_vec_.size();
}
//-----------------------------------------------------------------------------
inline void Connection::cancelRemoveMessageVectorAppend(const RequestId& _id)
{
    cancel_remote_msg_vec_.push_back(_id);
}
//-----------------------------------------------------------------------------
inline const UniqueId& Connection::relayId() const
{
    return relay_id_;
}
//-----------------------------------------------------------------------------
inline void Connection::relayId(const UniqueId& _relay_id)
{
    relay_id_ = _relay_id;
}
//-----------------------------------------------------------------------------
inline UniqueId& Connection::relayId()
{
    return relay_id_;
}
//-----------------------------------------------------------------------------
inline ErrorConditionT Connection::pollServicePoolForUpdates(frame::aio::ReactorContext& _rctx, MessageId const& _rpool_msg_id)
{
    return service(_rctx).pollPoolForUpdates(*this, uid(_rctx), _rpool_msg_id, poll_pool_more_);
}
//-----------------------------------------------------------------------------
} // namespace mprpc
} // namespace frame
} // namespace solid
