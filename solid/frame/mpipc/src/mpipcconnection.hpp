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

#include "solid/utility/any.hpp"
#include "solid/utility/event.hpp"
#include "solid/utility/flags.hpp"
#include "solid/utility/queue.hpp"

#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcsocketstub.hpp"

#include "mpipcmessagereader.hpp"
#include "mpipcmessagewriter.hpp"

namespace solid {
namespace frame {
namespace aio {

namespace openssl {
class Context;
} //namespace openssl

} //namespace aio

namespace mpipc {

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

class Connection final : public Dynamic<Connection, frame::aio::Object> {
public:
    static Event eventResolve();
    static Event eventNewMessage();
    static Event eventNewMessage(const MessageId&);
    static Event eventCancelConnMessage(const MessageId&);
    static Event eventCancelPoolMessage(const MessageId&);
    static Event eventStopping();
    static Event eventEnterActive(ConnectionEnterActiveCompleteFunctionT&&, const size_t _send_buffer_capacity);
    static Event eventEnterPassive(ConnectionEnterPassiveCompleteFunctionT&&);
    static Event eventStartSecure(ConnectionSecureHandhakeCompleteFunctionT&&);
    static Event eventSendRaw(ConnectionSendRawDataCompleteFunctionT&&, std::string&&);
    static Event eventRecvRaw(ConnectionRecvRawDataCompleteFunctionT&&);

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

    Any<>& any();

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
    void forEveryMessagesNewerToOlder(
        Fnc const& _f)
    {
        auto visit_fnc = [this, &_f](
            MessageBundle&   _rmsgbundle,
            MessageId const& _rmsgid) {
            _f(this->poolId(), _rmsgbundle, _rmsgid);
        };
        MessageWriter::VisitFunctionT fnc(std::cref(visit_fnc));

        msg_writer_.forEveryMessagesNewerToOlder(fnc);
    }

    static void onSendAllRaw(frame::aio::ReactorContext& _rctx, Event& _revent);
    static void onRecvSomeRaw(frame::aio::ReactorContext& _rctx, const size_t _sz, Event& _revent);

protected:
    static void onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onSend(frame::aio::ReactorContext& _rctx);
    static void onConnect(frame::aio::ReactorContext& _rctx);
    static void onTimerInactivity(frame::aio::ReactorContext& _rctx);
    static void onTimerKeepalive(frame::aio::ReactorContext& _rctx);
    static void onSecureConnect(frame::aio::ReactorContext& _rctx);
    static void onSecureAccept(frame::aio::ReactorContext& _rctx);

private:
    friend struct ConnectionContext;
    friend class Service;

    Service& service(frame::aio::ReactorContext& _rctx) const;
    ObjectIdT uid(frame::aio::ReactorContext& _rctx) const;

    void onEvent(frame::aio::ReactorContext& _rctx, Event&& _uevent) override;

    bool shouldSendKeepalive() const;
    bool shouldPollPool() const;

    bool willAcceptNewMessage(frame::aio::ReactorContext& _rctx) const;

    bool isWaitingKeepAliveTimer() const;
    bool isStopPeer() const;

    //The connection is aware that it is activated
    bool isActiveState() const;
    bool isRawState() const;

    bool isStopping() const;
    bool isDelayedStopping() const;

    bool hasCompletingMessages() const;

    void onStopped(frame::aio::ReactorContext& _rctx);

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

    void doCompleteMessage(
        frame::aio::ReactorContext& _rctx, MessagePointerT& _rresponse_ptr, const size_t _response_type_id);

    void doCompleteMessage(
        solid::frame::aio::ReactorContext& _rctx,
        MessageId const&                   _rpool_msg_id,
        MessageBundle&                     _rmsg_local,
        ErrorConditionT const&             _rerr);

    void doCompleteKeepalive(frame::aio::ReactorContext& _rctx);
    void doCompleteAckCount(frame::aio::ReactorContext& _rctx, uint8_t _count);

    void doHandleEventKill(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventStart(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventResolve(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventNewPoolMessage(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventNewConnMessage(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventCancelConnMessage(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventCancelPoolMessage(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventEnterActive(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventEnterPassive(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventStartSecure(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventSendRaw(frame::aio::ReactorContext& _rctx, Event& _revent);
    void doHandleEventRecvRaw(frame::aio::ReactorContext& _rctx, Event& _revent);

    void doContinueStopping(
        frame::aio::ReactorContext& _rctx,
        const Event&                _revent);

    void doCompleteAllMessages(
        frame::aio::ReactorContext& _rctx,
        size_t                      _offset,
        const bool                  _can_stop,
        const ulong                 _seconds_to_wait,
        ErrorConditionT const&      _rerr,
        Event&                      _revent);
private:
    bool hasRelayBuffer(const Configuration& _rconfig, char*& _rpbuf);
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
    using TimerT        = frame::aio::SteadyTimer;
    using BufferVectorT = std::vector<BufferPointerT>;

    enum class FlagsE : size_t {
        Active,
        Server,
        Keepalive,
        WaitKeepAliveTimer,
        StopPeer,
        HasActivity,
        PollPool,
        Stopping,
        DelayedStopping,
        Secure,
        Raw,
        InPoolWaitQueue,
        Connected, //once set - the flag should not be reset. Is used by pool for restarting
        LastFlag,
    };

    using FlagsT = solid::Flags<FlagsE>;

    struct Receiver;
    friend struct Receiver;

    ConnectionPoolId   pool_id_;
    const std::string& rpool_name_;
    TimerT             timer_;
    FlagsT             flags_;
    uint32_t           recv_buf_off_;
    uint32_t           cons_buf_off_;
    uint32_t           recv_keepalive_count_;
    BufferPointerT     recv_buf_;
    BufferPointerT     send_buf_;
    BufferVectorT      send_buf_vec_;
    uint8_t           send_buf_vec_sentinel_;
    uint8_t           ackd_buf_count_;
    uint8_t            recv_buf_cp_kb_; //kilobytes
    uint8_t            send_buf_cp_kb_; //kilobytes
    MessageIdVectorT   pending_message_vec_;
    MessageReader      msg_reader_;
    MessageWriter      msg_writer_;
    ErrorConditionT    error_;
    ErrorCodeT         sys_error_;
    Any<>              any_data_;
    char               socket_emplace_buf_[static_cast<size_t>(ConnectionValues::SocketEmplacementSize)];
    SocketStubPtrT     sock_ptr_;
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

inline Connection* new_connection(
    Configuration const&    _rconfiguration,
    SocketDevice&           _rsd,
    ConnectionPoolId const& _rpool_id,
    std::string const&      _rpool_name)
{
    return new Connection(_rconfiguration, _rsd, _rpool_id, _rpool_name);
}

inline Connection* new_connection(
    Configuration const&    _rconfiguration,
    ConnectionPoolId const& _rpool_id,
    std::string const&      _rpool_name)
{
    return new Connection(_rconfiguration, _rpool_id, _rpool_name);
}

} //namespace mpipc
} //namespace frame
} //namespace solid
