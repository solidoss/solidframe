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
} //namespace openssl

} //namespace aio

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

class Connection final : public frame::aio::Actor {
public:
    using PointerT = std::shared_ptr<Connection>;

    static Event eventResolve();
    static Event eventNewMessage();
    static Event eventNewMessage(const MessageId&);
    static Event eventNewQueueMessage();
    static Event eventCancelConnMessage(const MessageId&);
    static Event eventCancelPoolMessage(const MessageId&);
    static Event eventClosePoolMessage(const MessageId&);
    static Event eventStopping();
    static Event eventEnterActive(ConnectionEnterActiveCompleteFunctionT&&, const size_t _send_buffer_capacity);
    static Event eventEnterPassive(ConnectionEnterPassiveCompleteFunctionT&&);
    static Event eventStartSecure(ConnectionSecureHandhakeCompleteFunctionT&&);
    static Event eventSendRaw(ConnectionSendRawDataCompleteFunctionT&&, std::string&&);
    static Event eventRecvRaw(ConnectionRecvRawDataCompleteFunctionT&&);
    static Event eventPost(ConnectionPostCompleteFunctionT&&);

    Connection(
        Configuration const&    _rconfiguration,
        ConnectionPoolId const& _rpool_id,
        std::string const&      _rpool_name);

    Connection(
        Configuration const&    _rconfiguration,
        SocketDevice&           _rsd,
        ConnectionPoolId const& _rpool_id,
        std::string const&      _rpool_name);

    virtual ~Connection();

    //NOTE: will always accept null message
    bool tryPushMessage(
        Configuration const& _rconfiguration,
        MessageBundle&       _rmsgbundle,
        MessageId&           _rconn_msg_id,
        const MessageId&     _rpool_msg_id);

    const ErrorConditionT& error() const;
    const ErrorCodeT&      systemError() const;

    bool isFull(Configuration const& _rconfiguration) const;

    bool isInPoolWaitingQueue() const;

    void setInPoolWaitingQueue();

    bool isServer() const;
    bool isConnected() const;
    bool isSecured() const;

    bool isWriterEmpty() const;

    SocketDevice const& device() const;

    uint32_t peerVersionMajor() const;
    uint32_t peerVersionMinor() const;

    uint32_t& peerVersionMajor();
    uint32_t& peerVersionMinor();

    Any<>& any();

    const UniqueId& relayId() const;

    void relayId(const UniqueId& _relay_id);

    MessagePointerT fetchRequest(Message const& _rmsg) const;

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
#if 0
    template <class Fnc>
    void forEveryMessagesNewerToOlder(
        Fnc const& _f)
    {
        auto visit_fnc = [this, &_f](
            MessageBundle&   _rmsgbundle,
            MessageId const& _rmsgid,
            RelayData* /*_prelay_data*/
        ) {
            _f(this->poolId(), _rmsgbundle, _rmsgid);
        };
        MessageWriter::VisitFunctionT fnc(std::cref(visit_fnc));

        msg_writer_.forEveryMessagesNewerToOlder(fnc);
    }
#endif

    static void onSendAllRaw(frame::aio::ReactorContext& _rctx, Event& _revent);
    static void onRecvSomeRaw(frame::aio::ReactorContext& _rctx, const size_t _sz, Event& _revent);

protected:
    static void onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onSend(frame::aio::ReactorContext& _rctx);
    static void onConnect(frame::aio::ReactorContext& _rctx);
    static void onTimerInactivity(frame::aio::ReactorContext& _rctx);
    static void onTimerKeepalive(frame::aio::ReactorContext& _rctx);
    static void onTimerWaitSecured(frame::aio::ReactorContext& _rctx);
    static void onTimerWaitActivation(frame::aio::ReactorContext& _rctx);
    static void onSecureConnect(frame::aio::ReactorContext& _rctx);
    static void onSecureAccept(frame::aio::ReactorContext& _rctx);

private:
    friend struct ConnectionContext;
    friend class RelayEngine;
    friend class Service;

    static bool notify(Manager& _rm, const ActorIdT&, const RelayEngineNotification);

    Service& service(frame::aio::ReactorContext& _rctx) const;
    ActorIdT uid(frame::aio::ReactorContext& _rctx) const;

    void onEvent(frame::aio::ReactorContext& _rctx, Event&& _uevent) override;

    bool shouldSendKeepalive() const;
    bool shouldPollPool() const;
    bool shouldPollRelayEngine() const;

    bool willAcceptNewMessage(frame::aio::ReactorContext& _rctx) const;

    bool isWaitingKeepAliveTimer() const;
    bool isStopPeer() const;

    //The connection is aware that it is activated
    bool isActiveState() const;
    bool isRawState() const;

    bool isStopping() const;
    bool isDelayedStopping() const;

    bool hasCompletingMessages() const;

    void onStopped(frame::aio::ReactorContext& _rctx,
        MessageId const&                       _rpool_msg_id,
        MessageBundle&                         _rmsg_local);

    void doStart(frame::aio::ReactorContext& _rctx, const bool _is_incoming);

    void doStop(frame::aio::ReactorContext& _rctx, const ErrorConditionT& _rerr, const ErrorCodeT& _rsyserr = ErrorCodeT());

    void doSend(frame::aio::ReactorContext& _rctx);

    //  SocketDevice const & device()const{
    //      return sock.device();
    //  }

    void doActivate(
        frame::aio::ReactorContext& _rctx,
        Event&                      _revent);

    void doOptimizeRecvBuffer();
    void doOptimizeRecvBufferForced();
    void doPrepare(frame::aio::ReactorContext& _rctx);
    void doUnprepare(frame::aio::ReactorContext& _rctx);
    void doResetTimerStart(frame::aio::ReactorContext& _rctx);
    void doResetTimerSend(frame::aio::ReactorContext& _rctx);
    void doResetTimerRecv(frame::aio::ReactorContext& _rctx);

    ResponseStateE doCheckResponseState(frame::aio::ReactorContext& _rctx, const MessageHeader& _rmsghdr, MessageId& _rrelay_id, const bool _erase_request);

    void doCompleteMessage(
        frame::aio::ReactorContext& _rctx, MessagePointerT& _rresponse_ptr, const size_t _response_type_id);

    void doCompleteMessage(
        solid::frame::aio::ReactorContext& _rctx,
        MessageId const&                   _rpool_msg_id,
        MessageBundle&                     _rmsg_local,
        ErrorConditionT const&             _rerr);

    void doCompleteRelayed(
        frame::aio::ReactorContext& _rctx,
        RelayData*                  _prelay_data,
        MessageId const&            _rengine_msg_id);

    void doCancelRelayed(
        frame::aio::ReactorContext& _rctx,
        RelayData*                  _prelay_data,
        MessageId const&            _rengine_msg_id);

    void doCompleteKeepalive(frame::aio::ReactorContext& _rctx);
    void doCompleteAckCount(frame::aio::ReactorContext& _rctx, uint8_t _count);
    void doCompleteCancelRequest(frame::aio::ReactorContext& _rctx, const RequestId& _reqid);

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

    void doHandleEventKill(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventStart(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventResolve(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventNewPoolMessage(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventNewPoolQueueMessage(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventNewConnMessage(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventCancelConnMessage(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventCancelPoolMessage(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventClosePoolMessage(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventEnterActive(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventEnterPassive(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventStartSecure(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventSendRaw(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventRecvRaw(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventRelayNew(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventRelayDone(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventPost(frame::aio::ReactorContext& _rctx, Event& _revent);

    void doContinueStopping(
        frame::aio::ReactorContext& _rctx,
        const Event&                _revent);

    void doCompleteAllMessages(
        frame::aio::ReactorContext& _rctx,
        size_t                      _offset);
    void doResetRecvBuffer(frame::aio::ReactorContext& _rctx, const uint8_t _request_buffer_ack_count, ErrorConditionT& _rerr);

private:
    bool postSendAll(frame::aio::ReactorContext& _rctx, const char* _pbuf, size_t _bufcp, Event& _revent);
    bool postRecvSome(frame::aio::ReactorContext& _rctx, char* _pbuf, size_t _bufcp);
    bool postRecvSome(frame::aio::ReactorContext& _rctx, char* _pbuf, size_t _bufcp, Event& _revent);
    bool hasValidSocket() const;
    bool connect(frame::aio::ReactorContext& _rctx, const SocketAddressInet& _raddr);
    bool recvSome(frame::aio::ReactorContext& _rctx, char* _buf, size_t _bufcp, size_t& _sz);
    bool hasPendingSend() const;
    bool sendAll(frame::aio::ReactorContext& _rctx, char* _buf, size_t _bufcp);
    void prepareSocket(frame::aio::ReactorContext& _rctx);

    uint32_t recvBufferCapacity() const
    {
        return recv_buf_cp_kb_ * 1024;
    }
    uint32_t sendBufferCapacity() const
    {
        return send_buf_cp_kb_ * 1024;
    }

private:
    enum class FlagsE : size_t {
        Active,
        Server,
        Keepalive,
        WaitKeepAliveTimer,
        StopPeer,
        HasActivity,
        PollPool,
        PollRelayEngine,
        Stopping,
        DelayedStopping,
        Secure,
        Raw,
        InPoolWaitQueue,
        Connected, //once set - the flag should not be reset. Is used by pool for restarting
        LastFlag,
    };

    using TimerT            = frame::aio::SteadyTimer;
    using SendBufferVectorT = std::vector<SendBufferPointerT>;
    using FlagsT            = solid::Flags<FlagsE>;
    using RequestIdVectorT  = MessageWriter::RequestIdVectorT;
    using RecvBufferVectorT = std::vector<RecvBufferPointerT>;

    struct Receiver;
    friend struct Receiver;
    struct Sender;
    struct SenderResponse;
    friend struct Sender;
    friend struct SenderResponse;

    ConnectionPoolId   pool_id_;
    const std::string& rpool_name_;
    TimerT             timer_;
    FlagsT             flags_;
    size_t             recv_buf_off_;
    size_t             cons_buf_off_;
    uint32_t           recv_keepalive_count_;
    uint16_t           recv_buf_count_;
    RecvBufferPointerT recv_buf_;
    RecvBufferVectorT  recv_buf_vec_;
    SendBufferPointerT send_buf_;
    uint8_t            send_relay_free_count_;
    uint8_t            ackd_buf_count_;
    uint8_t            recv_buf_cp_kb_; //kilobytes
    uint8_t            send_buf_cp_kb_; //kilobytes
    MessageIdVectorT   pending_message_vec_;
    MessageReader      msg_reader_;
    MessageWriter      msg_writer_;
    RequestIdVectorT   cancel_remote_msg_vec_;
    ErrorConditionT    error_;
    ErrorCodeT         sys_error_;
    Any<>              any_data_;
    char               socket_emplace_buf_[static_cast<size_t>(ConnectionValues::SocketEmplacementSize)];
    SocketStubPtrT     sock_ptr_;
    UniqueId           relay_id_;
    uint32_t           peer_version_major_ = InvalidIndex();
    uint32_t           peer_version_minor_ = InvalidIndex();
};

inline Any<>& Connection::any()
{
    return any_data_;
}

inline MessagePointerT Connection::fetchRequest(Message const& _rmsg) const
{
    return msg_writer_.fetchRequest(_rmsg.requestId());
}

inline ConnectionPoolId const& Connection::poolId() const
{
    return pool_id_;
}

inline const std::string& Connection::poolName() const
{
    return rpool_name_;
}

inline bool Connection::isWriterEmpty() const
{
    return msg_writer_.empty();
}

inline const ErrorConditionT& Connection::error() const
{
    return error_;
}
inline const ErrorCodeT& Connection::systemError() const
{
    return sys_error_;
}

inline const UniqueId& Connection::relayId() const
{
    return relay_id_;
}

inline void Connection::relayId(const UniqueId& _relay_id)
{
    relay_id_ = _relay_id;
}

inline SocketDevice const& Connection::device() const
{
    return sock_ptr_->device();
}

inline Connection::PointerT new_connection(
    Configuration const&    _rconfiguration,
    SocketDevice&           _rsd,
    ConnectionPoolId const& _rpool_id,
    std::string const&      _rpool_name)
{
    return std::make_shared<Connection>(_rconfiguration, _rsd, _rpool_id, _rpool_name);
}

inline Connection::PointerT new_connection(
    Configuration const&    _rconfiguration,
    ConnectionPoolId const& _rpool_id,
    std::string const&      _rpool_name)
{
    return std::make_shared<Connection>(_rconfiguration, _rpool_id, _rpool_name);
}

inline uint32_t Connection::peerVersionMajor() const
{
    return peer_version_major_;
}
inline uint32_t Connection::peerVersionMinor() const
{
    return peer_version_minor_;
}

inline uint32_t& Connection::peerVersionMajor()
{
    return peer_version_major_;
}
inline uint32_t& Connection::peerVersionMinor()
{
    return peer_version_minor_;
}

} //namespace mprpc
} //namespace frame
} //namespace solid
