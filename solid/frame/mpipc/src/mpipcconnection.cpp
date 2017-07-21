// solid/frame/ipc/src/ipcconnection.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "mpipcconnection.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/frame/manager.hpp"
#include "solid/frame/mpipc/mpipcerror.hpp"
#include "solid/frame/mpipc/mpipcservice.hpp"
#include "solid/utility/event.hpp"
#include <cstdio>

namespace solid {
namespace frame {
namespace mpipc {

namespace {

enum class ConnectionEvents {
    Resolve,
    NewPoolMessage,
    NewConnMessage,
    CancelConnMessage,
    CancelPoolMessage,
    EnterActive,
    EnterPassive,
    StartSecure,
    SendRaw,
    RecvRaw,
    Stopping,
    Invalid,
};

const EventCategory<ConnectionEvents> connection_event_category{
    "solid::frame::mpipc::connection_event_category",
    [](const ConnectionEvents _evt) {
        switch (_evt) {
        case ConnectionEvents::Resolve:
            return "Resolve";
        case ConnectionEvents::NewPoolMessage:
            return "NewPoolMessage";
        case ConnectionEvents::NewConnMessage:
            return "NewConnMessage";
        case ConnectionEvents::CancelConnMessage:
            return "CancelConnMessage";
        case ConnectionEvents::CancelPoolMessage:
            return "CancelPoolMessage";
        case ConnectionEvents::EnterActive:
            return "EnterActive";
        case ConnectionEvents::EnterPassive:
            return "EnterPassive";
        case ConnectionEvents::StartSecure:
            return "StartSecure";
        case ConnectionEvents::SendRaw:
            return "SendRaw";
        case ConnectionEvents::RecvRaw:
            return "RecvRaw";
        case ConnectionEvents::Stopping:
            return "Stopping";
        case ConnectionEvents::Invalid:
            return "Invalid";
        default:
            return "unknown";
        }
    }};

} //namespace

//-----------------------------------------------------------------------------
inline Service& Connection::service(frame::aio::ReactorContext& _rctx) const
{
    return static_cast<Service&>(_rctx.service());
}
//-----------------------------------------------------------------------------
inline ObjectIdT Connection::uid(frame::aio::ReactorContext& _rctx) const
{
    return service(_rctx).manager().id(*this);
}
//-----------------------------------------------------------------------------

/*static*/ Event Connection::eventResolve()
{
    return connection_event_category.event(ConnectionEvents::Resolve);
}
//-----------------------------------------------------------------------------
/*static*/ Event Connection::eventNewMessage()
{
    return connection_event_category.event(ConnectionEvents::NewPoolMessage);
}
//-----------------------------------------------------------------------------
/*static*/ Event Connection::eventNewMessage(const MessageId& _rmsgid)
{
    Event event = connection_event_category.event(ConnectionEvents::NewConnMessage);
    event.any() = _rmsgid;
    return event;
}
//-----------------------------------------------------------------------------
/*static*/ Event Connection::eventCancelConnMessage(const MessageId& _rmsgid)
{
    Event event = connection_event_category.event(ConnectionEvents::CancelConnMessage);
    event.any() = _rmsgid;
    return event;
}
//-----------------------------------------------------------------------------
/*static*/ Event Connection::eventCancelPoolMessage(const MessageId& _rmsgid)
{
    Event event = connection_event_category.event(ConnectionEvents::CancelPoolMessage);
    event.any() = _rmsgid;
    return event;
}
//-----------------------------------------------------------------------------
/*static*/ Event Connection::eventStopping()
{
    return connection_event_category.event(ConnectionEvents::Stopping);
}
//-----------------------------------------------------------------------------
struct EnterActive {
    EnterActive(
        ConnectionEnterActiveCompleteFunctionT&& _ucomplete_fnc,
        const size_t                             _send_buffer_capacity)
        : send_buffer_capacity(_send_buffer_capacity)
    {
        std::swap(_ucomplete_fnc, complete_fnc);
    }

    ConnectionEnterActiveCompleteFunctionT complete_fnc;
    const size_t                           send_buffer_capacity;
};
/*static*/ Event Connection::eventEnterActive(ConnectionEnterActiveCompleteFunctionT&& _ucomplete_fnc, const size_t _send_buffer_capacity)
{
    Event event = connection_event_category.event(ConnectionEvents::EnterActive);
    event.any() = EnterActive(std::move(_ucomplete_fnc), _send_buffer_capacity);
    return event;
}
//-----------------------------------------------------------------------------
struct EnterPassive {
    EnterPassive(
        ConnectionEnterPassiveCompleteFunctionT&& _ucomplete_fnc)
    {
        std::swap(complete_fnc, _ucomplete_fnc);
    }

    ConnectionEnterPassiveCompleteFunctionT complete_fnc;
};
/*static*/ Event Connection::eventEnterPassive(ConnectionEnterPassiveCompleteFunctionT&& _ucomplete_fnc)
{
    Event event = connection_event_category.event(ConnectionEvents::EnterPassive);
    event.any() = EnterPassive(std::move(_ucomplete_fnc));
    return event;
}
//-----------------------------------------------------------------------------
struct StartSecure {
    StartSecure(
        ConnectionSecureHandhakeCompleteFunctionT&& _ucomplete_fnc)
    {
        std::swap(complete_fnc, _ucomplete_fnc);
    }

    ConnectionSecureHandhakeCompleteFunctionT complete_fnc;
};
/*static*/ Event Connection::eventStartSecure(ConnectionSecureHandhakeCompleteFunctionT&& _ucomplete_fnc)
{
    Event event = connection_event_category.event(ConnectionEvents::StartSecure);
    event.any() = StartSecure(std::move(_ucomplete_fnc));
    return event;
}
//-----------------------------------------------------------------------------
struct SendRaw {
    SendRaw(
        ConnectionSendRawDataCompleteFunctionT&& _ucomplete_fnc,
        std::string&&                            _udata)
        : data(std::move(_udata))
        , offset(0)
    {
        std::swap(complete_fnc, _ucomplete_fnc);
    }

    ConnectionSendRawDataCompleteFunctionT complete_fnc;
    std::string                            data;
    size_t                                 offset;
};
/*static*/ Event Connection::eventSendRaw(ConnectionSendRawDataCompleteFunctionT&& _ucomplete_fnc, std::string&& _udata)
{
    Event event = connection_event_category.event(ConnectionEvents::SendRaw);
    event.any() = SendRaw(std::move(_ucomplete_fnc), std::move(_udata));
    return event;
}
//-----------------------------------------------------------------------------
struct RecvRaw {
    RecvRaw(
        ConnectionRecvRawDataCompleteFunctionT&& _ucomplete_fnc)
    {
        std::swap(complete_fnc, _ucomplete_fnc);
    }

    ConnectionRecvRawDataCompleteFunctionT complete_fnc;
};
/*static*/ Event Connection::eventRecvRaw(ConnectionRecvRawDataCompleteFunctionT&& _ucomplete_fnc)
{
    Event event = connection_event_category.event(ConnectionEvents::RecvRaw);
    event.any() = RecvRaw(std::move(_ucomplete_fnc));
    return event;
}
//-----------------------------------------------------------------------------
inline void Connection::doOptimizeRecvBuffer()
{
    const size_t cnssz = recv_buf_off_ - cons_buf_off_;
    if (cnssz <= cons_buf_off_) {
        //idbgx(Debug::proto_bin, this<<' '<<"memcopy "<<cnssz<<" rcvoff = "<<receivebufoff<<" cnsoff = "<<consumebufoff);

        memcpy(recv_buf_.get(), recv_buf_.get() + cons_buf_off_, cnssz);
        cons_buf_off_ = 0;
        recv_buf_off_ = cnssz;
    }
}
//-----------------------------------------------------------------------------
inline void Connection::doOptimizeRecvBufferForced()
{
    const size_t cnssz = recv_buf_off_ - cons_buf_off_;

    //idbgx(Debug::proto_bin, this<<' '<<"memcopy "<<cnssz<<" rcvoff = "<<receivebufoff<<" cnsoff = "<<consumebufoff);

    memmove(recv_buf_.get(), recv_buf_.get() + cons_buf_off_, cnssz);
    cons_buf_off_ = 0;
    recv_buf_off_ = cnssz;
}
//-----------------------------------------------------------------------------
Connection::Connection(
    Configuration const&    _rconfiguration,
    ConnectionPoolId const& _rpool_id,
    const std::string&      _rpool_name)
    : pool_id_(_rpool_id)
    , rpool_name_(_rpool_name)
    , timer_(this->proxy())
    , flags_(0)
    , recv_buf_off_(0)
    , cons_buf_off_(0)
    , recv_keepalive_count_(0)
    , recv_buf_count_(0)
    , recv_buf_(nullptr)
    , send_buf_(nullptr)
    , send_buf_vec_sentinel_(0)
    , ackd_buf_count_(0)
    , recv_buf_cp_kb_(0)
    , send_buf_cp_kb_(0)
    , socket_emplace_buf_{0}
    , sock_ptr_(_rconfiguration.client.connection_create_socket_fnc(_rconfiguration, this->proxy(), this->socket_emplace_buf_))
{
    idbgx(Debug::mpipc, this);
}
//-----------------------------------------------------------------------------
Connection::Connection(
    Configuration const&    _rconfiguration,
    SocketDevice&           _rsd,
    ConnectionPoolId const& _rpool_id,
    std::string const&      _rpool_name)
    : pool_id_(_rpool_id)
    , rpool_name_(_rpool_name)
    , timer_(this->proxy())
    , flags_(0)
    , recv_buf_off_(0)
    , cons_buf_off_(0)
    , recv_keepalive_count_(0)
    , recv_buf_count_(0)
    , recv_buf_(nullptr)
    , send_buf_(nullptr)
    , send_buf_vec_sentinel_(0)
    , ackd_buf_count_(0)
    , recv_buf_cp_kb_(0)
    , send_buf_cp_kb_(0)
    , socket_emplace_buf_{0}
    , sock_ptr_(_rconfiguration.server.connection_create_socket_fnc(_rconfiguration, this->proxy(), std::move(_rsd), this->socket_emplace_buf_))
{
    idbgx(Debug::mpipc, this << " (" << local_address(sock_ptr_->device()) << ") -> (" << remote_address(sock_ptr_->device()) << ')');
}
//-----------------------------------------------------------------------------
Connection::~Connection()
{
    idbgx(Debug::mpipc, this);
}
//-----------------------------------------------------------------------------
bool Connection::isFull(Configuration const& _rconfiguration) const
{
    return msg_writer_.full(_rconfiguration.writer);
}
//-----------------------------------------------------------------------------
bool Connection::isInPoolWaitingQueue() const
{
    return flags_.has(FlagsE::InPoolWaitQueue);
}
//-----------------------------------------------------------------------------
void Connection::setInPoolWaitingQueue()
{
    flags_.set(FlagsE::InPoolWaitQueue);
}
//-----------------------------------------------------------------------------
// - called under pool's lock
bool Connection::tryPushMessage(
    Configuration const& _rconfiguration,
    MessageBundle&       _rmsgbundle,
    MessageId&           _rconn_msg_id,
    const MessageId&     _rpool_msg_id)
{
    const bool success = msg_writer_.enqueue(_rconfiguration.writer, _rmsgbundle, _rpool_msg_id, _rconn_msg_id);
    vdbgx(Debug::mpipc, this << " enqueue message " << _rpool_msg_id << " to connection " << this << " retval = " << success);
    return success;
}
//-----------------------------------------------------------------------------
bool Connection::isActiveState() const
{
    return flags_.has(FlagsE::Active);
}
//-----------------------------------------------------------------------------
bool Connection::isRawState() const
{
    return flags_.has(FlagsE::Raw);
}
//-----------------------------------------------------------------------------
bool Connection::isStopping() const
{
    return flags_.has(FlagsE::Stopping);
}
//-----------------------------------------------------------------------------
bool Connection::isDelayedStopping() const
{
    return flags_.has(FlagsE::DelayedStopping);
}
//-----------------------------------------------------------------------------
bool Connection::isServer() const
{
    return flags_.has(FlagsE::Server);
}
//-----------------------------------------------------------------------------
bool Connection::isConnected() const
{
    return flags_.has(FlagsE::Connected);
}
//-----------------------------------------------------------------------------
bool Connection::isSecured() const
{
    return false;
}
//-----------------------------------------------------------------------------
bool Connection::shouldSendKeepalive() const
{
    return flags_.has(FlagsE::Keepalive);
}
//-----------------------------------------------------------------------------
inline bool Connection::shouldPollPool() const
{
    return flags_.has(FlagsE::PollPool);
}
//-----------------------------------------------------------------------------
bool Connection::isWaitingKeepAliveTimer() const
{
    return flags_.has(FlagsE::WaitKeepAliveTimer);
}
//-----------------------------------------------------------------------------
bool Connection::isStopPeer() const
{
    return flags_.has(FlagsE::StopPeer);
}
//-----------------------------------------------------------------------------
void Connection::doPrepare(frame::aio::ReactorContext& _rctx)
{
    recv_buf_       = service(_rctx).configuration().allocateRecvBuffer(recv_buf_cp_kb_);
    send_buf_       = service(_rctx).configuration().allocateSendBuffer(send_buf_cp_kb_);
    recv_buf_count_ = 1;
    msg_reader_.prepare(service(_rctx).configuration().reader);
    msg_writer_.prepare(service(_rctx).configuration().writer);
}
//-----------------------------------------------------------------------------
void Connection::doUnprepare(frame::aio::ReactorContext& _rctx)
{
    vdbgx(Debug::mpipc, this << ' ' << this->id());
    msg_reader_.unprepare();
    msg_writer_.unprepare();
}
//-----------------------------------------------------------------------------
void Connection::doStart(frame::aio::ReactorContext& _rctx, const bool _is_incoming)
{

    ConnectionContext    conctx(service(_rctx), *this);
    Configuration const& config = service(_rctx).configuration();

    doPrepare(_rctx);
    prepareSocket(_rctx);

    const ConnectionState start_state  = _is_incoming ? config.server.connection_start_state : config.client.connection_start_state;
    const bool            start_secure = _is_incoming ? config.server.connection_start_secure : config.client.connection_start_secure;

    if (_is_incoming) {
        flags_.set(FlagsE::Server);
        if (not start_secure) {
            service(_rctx).onIncomingConnectionStart(conctx);
        }
    } else {
        flags_.set(FlagsE::Connected);
        config.client.socket_device_setup_fnc(sock_ptr_->device());
        if (not start_secure) {
            service(_rctx).onOutgoingConnectionStart(conctx);
        }
    }

    switch (start_state) {
    case ConnectionState::Raw:
        if (start_secure) {
            this->post(
                _rctx,
                [this](frame::aio::ReactorContext& _rctx, Event&& _revent) { this->onEvent(_rctx, std::move(_revent)); },
                connection_event_category.event(ConnectionEvents::StartSecure));
        } else {
            flags_.set(FlagsE::Raw);
        }
        break;
    case ConnectionState::Active:
        if (start_secure) {
            this->post(
                _rctx,
                [this](frame::aio::ReactorContext& _rctx, Event&& _revent) { this->onEvent(_rctx, std::move(_revent)); },
                connection_event_category.event(ConnectionEvents::StartSecure));
        } else {
            this->post(
                _rctx,
                [this](frame::aio::ReactorContext& _rctx, Event&& _revent) { this->onEvent(_rctx, std::move(_revent)); },
                connection_event_category.event(ConnectionEvents::EnterActive));
        }
        break;
    case ConnectionState::Passive:
    default:
        flags_.set(FlagsE::Raw);
        if (start_secure) {
            this->post(
                _rctx,
                [this](frame::aio::ReactorContext& _rctx, Event&& _revent) { this->onEvent(_rctx, std::move(_revent)); },
                connection_event_category.event(ConnectionEvents::StartSecure));
        } else {
            this->post(
                _rctx,
                [this](frame::aio::ReactorContext& _rctx, Event&& _revent) { this->onEvent(_rctx, std::move(_revent)); },
                connection_event_category.event(ConnectionEvents::EnterPassive));
        }
        break;
    }
}
//-----------------------------------------------------------------------------
void Connection::doStop(frame::aio::ReactorContext& _rctx, const ErrorConditionT& _rerr, const ErrorCodeT& _rsyserr)
{

    if (not isStopping()) {

        error_     = _rerr;
        sys_error_ = _rsyserr;

        edbgx(Debug::mpipc, this << ' ' << this->id() << " [" << error_.message() << "][" << sys_error_.message() << "]");

        flags_.set(FlagsE::Stopping);

        ErrorConditionT tmp_error(error());
        ObjectIdT       objuid(uid(_rctx));
        ulong           seconds_to_wait = 0;
        MessageBundle   msg_bundle;
        MessageId       pool_msg_id;
        Event           event;
        bool            can_stop = service(_rctx).connectionStopping(*this, objuid, seconds_to_wait, pool_msg_id, msg_bundle, event, tmp_error);

        if (msg_bundle.message_ptr.get() or not SOLID_FUNCTION_EMPTY(msg_bundle.complete_fnc)) {
            doCompleteMessage(_rctx, pool_msg_id, msg_bundle, error_message_connection);
        }

        //at this point we need to start completing all connection's remaining messages

        bool has_no_message = pending_message_vec_.empty() and msg_writer_.empty();

        if (can_stop and has_no_message) {
            idbgx(Debug::mpipc, this << ' ' << this->id() << " postStop");
            //can stop rightaway
            postStop(_rctx,
                [](frame::aio::ReactorContext& _rctx, Event&& /*_revent*/) {
                    Connection& rthis = static_cast<Connection&>(_rctx.object());
                    rthis.onStopped(_rctx);
                }); //there might be events pending which will be delivered, but after this call
            //no event get posted
        } else if (has_no_message) {
            idbgx(Debug::mpipc, this << ' ' << this->id() << " wait " << seconds_to_wait << " seconds");
            if (seconds_to_wait) {
                timer_.waitFor(_rctx,
                    std::chrono::seconds(seconds_to_wait),
                    [event](frame::aio::ReactorContext& _rctx) {
                        Connection& rthis = static_cast<Connection&>(_rctx.object());
                        rthis.doContinueStopping(_rctx, event);
                    });
            } else {
                post(_rctx,
                    [](frame::aio::ReactorContext& _rctx, Event&& _revent) {
                        Connection& rthis = static_cast<Connection&>(_rctx.object());
                        rthis.doContinueStopping(_rctx, _revent);
                    },
                    std::move(event));
            }
        } else {
            idbgx(Debug::mpipc, this << ' ' << this->id() << " post message cleanup");
            size_t offset = 0;
            post(_rctx,
                [seconds_to_wait, can_stop, offset](frame::aio::ReactorContext& _rctx, Event&& _revent) {
                    Connection& rthis = static_cast<Connection&>(_rctx.object());
                    rthis.doCompleteAllMessages(_rctx, offset, can_stop, seconds_to_wait, error_message_connection, _revent);
                },
                std::move(event));
        }
    }
}
//-----------------------------------------------------------------------------
void Connection::doCompleteAllMessages(
    frame::aio::ReactorContext& _rctx,
    size_t                      _offset,
    const bool                  _can_stop,
    const ulong                 _seconds_to_wait,
    ErrorConditionT const&      _rerr,
    Event&                      _revent)
{

    bool has_any_message = not msg_writer_.empty();

    if (has_any_message) {
        idbgx(Debug::mpipc, this);
        //complete msg_writer messages
        MessageBundle msg_bundle;
        MessageId     pool_msg_id;

        if (msg_writer_.cancelOldest(msg_bundle, pool_msg_id)) {
            doCompleteMessage(_rctx, pool_msg_id, msg_bundle, _rerr);
        }

        has_any_message = (not msg_writer_.empty()) or (not pending_message_vec_.empty());

    } else if (_offset < pending_message_vec_.size()) {
        idbgx(Debug::mpipc, this);
        //complete pending messages
        MessageBundle msg_bundle;

        if (service(_rctx).fetchCanceledMessage(*this, pending_message_vec_[_offset], msg_bundle)) {
            doCompleteMessage(_rctx, pending_message_vec_[_offset], msg_bundle, _rerr);
        }
        ++_offset;
        if (_offset == pending_message_vec_.size()) {
            pending_message_vec_.clear();
            has_any_message = false;
        } else {
            has_any_message = true;
        }
    }

    if (has_any_message) {
        idbgx(Debug::mpipc, this);
        post(
            _rctx,
            [_rerr, _seconds_to_wait, _can_stop, _offset](frame::aio::ReactorContext& _rctx, Event&& _revent) {
                Connection& rthis = static_cast<Connection&>(_rctx.object());
                rthis.doCompleteAllMessages(_rctx, _offset, _can_stop, _seconds_to_wait, _rerr, _revent);
            },
            std::move(_revent));
    } else if (_can_stop) {
        idbgx(Debug::mpipc, this);
        //can stop rightaway
        postStop(_rctx,
            [_rerr](frame::aio::ReactorContext& _rctx, Event&& /*_revent*/) {
                Connection& rthis = static_cast<Connection&>(_rctx.object());
                rthis.onStopped(_rctx);
            }); //there might be events pending which will be delivered, but after this call
        //no event get posted
    } else if (_seconds_to_wait) {
        idbgx(Debug::mpipc, this << " secs to wait = " << _seconds_to_wait);
        timer_.waitFor(_rctx,
            std::chrono::seconds(_seconds_to_wait),
            [_rerr, _revent](frame::aio::ReactorContext& _rctx) {
                Connection& rthis = static_cast<Connection&>(_rctx.object());
                rthis.doContinueStopping(_rctx, _revent);
            });
    } else {
        idbgx(Debug::mpipc, this);
        post(_rctx,
            [_rerr](frame::aio::ReactorContext& _rctx, Event&& _revent) {
                Connection& rthis = static_cast<Connection&>(_rctx.object());
                rthis.doContinueStopping(_rctx, _revent);
            },
            std::move(_revent));
    }
}
//-----------------------------------------------------------------------------
void Connection::doContinueStopping(
    frame::aio::ReactorContext& _rctx,
    const Event&                _revent)
{

    idbgx(Debug::mpipc, this << ' ' << this->id() << "");

    ErrorConditionT tmp_error(error());
    ObjectIdT       objuid(uid(_rctx));
    ulong           seconds_to_wait = 0;

    MessageBundle msg_bundle;
    MessageId     pool_msg_id;

    Event event(_revent);

    bool can_stop = service(_rctx).connectionStopping(*this, objuid, seconds_to_wait, pool_msg_id, msg_bundle, event, tmp_error);

    if (msg_bundle.message_ptr or not SOLID_FUNCTION_EMPTY(msg_bundle.complete_fnc)) {
        doCompleteMessage(_rctx, pool_msg_id, msg_bundle, error_message_connection);
    }

    if (can_stop) {
        //can stop rightaway
        postStop(_rctx,
            [](frame::aio::ReactorContext& _rctx, Event&& /*_revent*/) {
                Connection& rthis = static_cast<Connection&>(_rctx.object());
                rthis.onStopped(_rctx);
            }); //there might be events pending which will be delivered, but after this call
        //no event get posted
    } else if (seconds_to_wait) {
        idbgx(Debug::mpipc, this << ' ' << this->id() << " wait for " << seconds_to_wait << " seconds");
        timer_.waitFor(_rctx,
            std::chrono::seconds(seconds_to_wait),
            [_revent](frame::aio::ReactorContext& _rctx) {
                Connection& rthis = static_cast<Connection&>(_rctx.object());
                rthis.doContinueStopping(_rctx, _revent);
            });
    } else {
        post(_rctx,
            [](frame::aio::ReactorContext& _rctx, Event&& _revent) {
                Connection& rthis = static_cast<Connection&>(_rctx.object());
                rthis.doContinueStopping(_rctx, _revent);
            },
            std::move(event));
    }
}
//-----------------------------------------------------------------------------
void Connection::onStopped(frame::aio::ReactorContext& _rctx)
{

    ObjectIdT         objuid(uid(_rctx));
    ConnectionContext conctx(service(_rctx), *this);

    service(_rctx).connectionStop(*this);

    service(_rctx).onConnectionStop(conctx);

    doUnprepare(_rctx);
    (void)objuid;
}
//-----------------------------------------------------------------------------
/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _uevent)
{

    static const EventHandler<void,
        Connection&,
        frame::aio::ReactorContext&>
        event_handler = {
            [](Event& _revt, Connection& _rcon, frame::aio::ReactorContext& _rctx) {
                Connection&       rthis = static_cast<Connection&>(_rctx.object());
                ConnectionContext conctx(rthis.service(_rctx), rthis);
                rthis.service(_rctx).configuration().connection_on_event_fnc(conctx, _revt);
            },
            {

                {make_event(GenericEvents::Start),
                    [](Event& _revt, Connection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventStart(_rctx, _revt);
                    }},
                {make_event(GenericEvents::Kill),
                    [](Event& _revt, Connection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventKill(_rctx, _revt);
                    }},
                {connection_event_category.event(ConnectionEvents::Resolve),
                    [](Event& _revt, Connection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventResolve(_rctx, _revt);
                    }},
                {connection_event_category.event(ConnectionEvents::NewPoolMessage),
                    [](Event& _revt, Connection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventNewPoolMessage(_rctx, _revt);
                    }},
                {connection_event_category.event(ConnectionEvents::NewConnMessage),
                    [](Event& _revt, Connection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventNewConnMessage(_rctx, _revt);
                    }},
                {connection_event_category.event(ConnectionEvents::CancelConnMessage),
                    [](Event& _revt, Connection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventCancelConnMessage(_rctx, _revt);
                    }},
                {connection_event_category.event(ConnectionEvents::CancelPoolMessage),
                    [](Event& _revt, Connection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventCancelPoolMessage(_rctx, _revt);
                    }},
                {connection_event_category.event(ConnectionEvents::EnterActive),
                    [](Event& _revt, Connection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventEnterActive(_rctx, _revt);
                    }},
                {connection_event_category.event(ConnectionEvents::EnterPassive),
                    [](Event& _revt, Connection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventEnterPassive(_rctx, _revt);
                    }},
                {connection_event_category.event(ConnectionEvents::StartSecure),
                    [](Event& _revt, Connection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventStartSecure(_rctx, _revt);
                    }},
                {connection_event_category.event(ConnectionEvents::SendRaw),
                    [](Event& _revt, Connection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventSendRaw(_rctx, _revt);
                    }},
                {connection_event_category.event(ConnectionEvents::RecvRaw),
                    [](Event& _revt, Connection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventRecvRaw(_rctx, _revt);
                    }},
                {connection_event_category.event(ConnectionEvents::Stopping),
                    [](Event& _revt, Connection& _rcon, frame::aio::ReactorContext& _rctx) {
                        idbgx(Debug::mpipc, &_rcon << ' ' << _rcon.id() << " cancel timer");
                        //NOTE: we're trying this way to preserve the
                        //error condition (for which the connection is stopping)
                        //held by the timer callback
                        _rcon.timer_.cancel(_rctx);
                    }},
            }};

    event_handler.handle(_uevent, *this, _rctx);
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventStart(
    frame::aio::ReactorContext& _rctx,
    Event& /*_revent*/
    )
{

    const bool has_valid_socket = this->hasValidSocket();

    idbgx(Debug::mpipc, this << ' ' << this->id() << " Session start: " << (has_valid_socket ? " connected " : "not connected"));

    if (has_valid_socket) {
        doStart(_rctx, true);
    }
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventKill(
    frame::aio::ReactorContext& _rctx,
    Event& /*_revent*/
    )
{
    doStop(_rctx, error_connection_killed);
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventResolve(
    frame::aio::ReactorContext& _rctx,
    Event&                      _revent)
{

    ResolveMessage* presolvemsg = _revent.any().cast<ResolveMessage>();

    if (presolvemsg) {
        idbgx(Debug::mpipc, this << ' ' << this->id() << " Session receive resolve event message of size: " << presolvemsg->addrvec.size());
        if (not presolvemsg->empty()) {
            idbgx(Debug::mpipc, this << ' ' << this->id() << " Connect to " << presolvemsg->currentAddress());

            //initiate connect:

            if (this->connect(_rctx, presolvemsg->currentAddress())) {
                onConnect(_rctx);
            }

            service(_rctx).forwardResolveMessage(poolId(), _revent);
        } else {
            edbgx(Debug::mpipc, this << ' ' << this->id() << " Empty resolve message");
            doStop(_rctx, error_connection_resolve);
        }
    } else {
        SOLID_ASSERT(false);
    }
}
//-----------------------------------------------------------------------------
bool Connection::willAcceptNewMessage(frame::aio::ReactorContext& _rctx) const
{
    return not isStopping() /* and waiting_message_vec.empty() and msg_writer.willAcceptNewMessage(service(_rctx).configuration().writer)*/;
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventNewPoolMessage(frame::aio::ReactorContext& _rctx, Event& _revent)
{

    flags_.reset(FlagsE::InPoolWaitQueue); //reset flag

    if (willAcceptNewMessage(_rctx)) {
        flags_.set(FlagsE::PollPool);
        doSend(_rctx);
    } else {
        service(_rctx).rejectNewPoolMessage(*this);
    }
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventNewConnMessage(frame::aio::ReactorContext& _rctx, Event& _revent)
{

    MessageId* pmsgid = _revent.any().cast<MessageId>();
    SOLID_ASSERT(pmsgid);

    if (pmsgid) {

        if (not this->isStopping()) {
            pending_message_vec_.push_back(*pmsgid);
            if (not this->isRawState()) {
                flags_.set(FlagsE::PollPool);
                doSend(_rctx);
            }
        } else {
            MessageBundle msg_bundle;

            if (service(_rctx).fetchCanceledMessage(*this, *pmsgid, msg_bundle)) {
                doCompleteMessage(_rctx, *pmsgid, msg_bundle, error_connection_stopping);
            }
        }
    }
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventCancelConnMessage(frame::aio::ReactorContext& _rctx, Event& _revent)
{

    MessageId* pmsgid = _revent.any().cast<MessageId>();
    SOLID_ASSERT(pmsgid);

    if (pmsgid) {
        MessageBundle msg_bundle;
        MessageId     pool_msg_id;
        if (msg_writer_.cancel(*pmsgid, msg_bundle, pool_msg_id)) {
            doCompleteMessage(_rctx, pool_msg_id, msg_bundle, error_message_canceled);
        }
    }
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventCancelPoolMessage(frame::aio::ReactorContext& _rctx, Event& _revent)
{

    MessageId* pmsgid = _revent.any().cast<MessageId>();
    SOLID_ASSERT(pmsgid);

    if (pmsgid) {
        MessageBundle msg_bundle;

        if (service(_rctx).fetchCanceledMessage(*this, *pmsgid, msg_bundle)) {
            doCompleteMessage(_rctx, *pmsgid, msg_bundle, error_message_canceled);
        }
    }
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventEnterActive(frame::aio::ReactorContext& _rctx, Event& _revent)
{

    //idbgx(Debug::mpipc, this);

    ConnectionContext conctx(service(_rctx), *this);
    EnterActive*      pdata = _revent.any().cast<EnterActive>();

    if (not isStopping()) {

        ErrorConditionT error = service(_rctx).activateConnection(*this, uid(_rctx));

        if (not error) {
            flags_.set(FlagsE::Active);

            if (pdata) {

                MessagePointerT msg_ptr = pdata->complete_fnc(conctx, error);

                if (msg_ptr) {
                    //TODO: push message to writer
                }
            }

            if (not isServer()) {
                //poll pool only for clients
                flags_.set(FlagsE::PollPool);
            }

            this->post(
                _rctx,
                [this](frame::aio::ReactorContext& _rctx, Event&& /*_revent*/) {
                    doSend(_rctx);
                });

            //start receiving messages
            this->postRecvSome(_rctx, recv_buf_.get(), recvBufferCapacity());

            doResetTimerStart(_rctx);

        } else {

            if (pdata) {

                MessagePointerT msg_ptr = pdata->complete_fnc(conctx, error);

                if (msg_ptr) {
                    //TODO: push message to writer
                    //then push nullptr message - delayed closing
                }
            }

            doStop(_rctx, error);
        }
    } else if (pdata) {
        pdata->complete_fnc(conctx, error_connection_stopping);
    }
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventEnterPassive(frame::aio::ReactorContext& _rctx, Event& _revent)
{

    EnterPassive*     pdata = _revent.any().cast<EnterPassive>();
    ConnectionContext conctx(service(_rctx), *this);

    if (this->isRawState()) {
        flags_.reset(FlagsE::Raw);

        if (pdata) {
            pdata->complete_fnc(conctx, ErrorConditionT());
        }

        if (pending_message_vec_.size()) {
            flags_.set(FlagsE::PollPool);
            doSend(_rctx);
        }
    } else if (pdata) {
        pdata->complete_fnc(conctx, error_connection_invalid_state);
    }

    this->post(
        _rctx,
        [this](frame::aio::ReactorContext& _rctx, Event&& /*_revent*/) {
            doSend(_rctx);
        });

    //start receiving messages
    this->postRecvSome(_rctx, recv_buf_.get(), recvBufferCapacity());

    doResetTimerStart(_rctx);
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventStartSecure(frame::aio::ReactorContext& _rctx, Event& /*_revent*/)
{
    vdbgx(Debug::mpipc, this << "");
    Configuration const& config = service(_rctx).configuration();
    ConnectionContext    conctx(service(_rctx), *this);

    if ((isServer() and config.server.hasSecureConfiguration()) or ((not isServer()) and config.client.hasSecureConfiguration())) {
        ErrorConditionT error;
        bool            done = false;
        vdbgx(Debug::mpipc, this << "");

        if (isServer()) {
            done = sock_ptr_->secureAccept(_rctx, conctx, onSecureAccept, error);
            if (done and not error and not _rctx.error()) {
                onSecureAccept(_rctx);
            }
        } else {
            done = sock_ptr_->secureConnect(_rctx, conctx, onSecureConnect, error);
            if (done and not error and not _rctx.error()) {
                onSecureConnect(_rctx);
            }
        }

        if (_rctx.error()) {
            edbgx(Debug::mpipc, this << " error = [" << error.message() << "]");
            doStop(_rctx, _rctx.error(), _rctx.systemError());
        }

        if (error) {
            edbgx(Debug::mpipc, this << " error = [" << error.message() << "]");
            doStop(_rctx, error);
        }
    } else {
        vdbgx(Debug::mpipc, this << "");
        doStop(_rctx, error_connection_no_secure_configuration);
    }
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onSecureConnect(frame::aio::ReactorContext& _rctx)
{
    Connection&          rthis = static_cast<Connection&>(_rctx.object());
    ConnectionContext    conctx(rthis.service(_rctx), rthis);
    Configuration const& config = rthis.service(_rctx).configuration();

    if (_rctx.error()) {
        edbgx(Debug::mpipc, &rthis << " error = [" << _rctx.error().message() << "] = systemError = [" << _rctx.systemError().message() << "]");
        rthis.doStop(_rctx, _rctx.error(), _rctx.systemError());
        return;
    }

    if (not SOLID_FUNCTION_EMPTY(config.client.connection_on_secure_handshake_fnc)) {
        config.client.connection_on_secure_handshake_fnc(conctx);
    }

    if (not config.client.connection_start_secure) {
        vdbgx(Debug::mpipc, &rthis << "");
        //we need the connection_on_secure_connect_fnc for advancing.
        if (SOLID_FUNCTION_EMPTY(config.client.connection_on_secure_handshake_fnc)) {
            rthis.doStop(_rctx, error_connection_invalid_state); //TODO: add new error
        }
    } else {

        rthis.service(_rctx).onOutgoingConnectionStart(conctx);

        vdbgx(Debug::mpipc, &rthis << "");
        //we continue the connection start process entering the right state
        switch (config.client.connection_start_state) {
        case ConnectionState::Raw:
            break;
        case ConnectionState::Active:
            if (not rthis.isActiveState()) {
                rthis.post(
                    _rctx,
                    [&rthis](frame::aio::ReactorContext& _rctx, Event&& _revent) { rthis.onEvent(_rctx, std::move(_revent)); },
                    connection_event_category.event(ConnectionEvents::EnterActive));
            }
            break;
        case ConnectionState::Passive:
        default:
            if (not rthis.isActiveState() and rthis.isRawState()) {
                rthis.post(
                    _rctx,
                    [&rthis](frame::aio::ReactorContext& _rctx, Event&& _revent) { rthis.onEvent(_rctx, std::move(_revent)); },
                    connection_event_category.event(ConnectionEvents::EnterPassive));
            }
            break;
        }
    }
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onSecureAccept(frame::aio::ReactorContext& _rctx)
{
    Connection&          rthis = static_cast<Connection&>(_rctx.object());
    ConnectionContext    conctx(rthis.service(_rctx), rthis);
    Configuration const& config = rthis.service(_rctx).configuration();

    if (_rctx.error()) {
        edbgx(Debug::mpipc, &rthis << " error = [" << _rctx.error().message() << "] = systemError = [" << _rctx.systemError().message() << "]");
        rthis.doStop(_rctx, _rctx.error(), _rctx.systemError());
        return;
    }

    if (not SOLID_FUNCTION_EMPTY(config.server.connection_on_secure_handshake_fnc)) {
        config.server.connection_on_secure_handshake_fnc(conctx);
    }

    if (not config.server.connection_start_secure) {
        vdbgx(Debug::mpipc, &rthis << "");
        //we need the connection_on_secure_accept_fnc for advancing.
        if (SOLID_FUNCTION_EMPTY(config.server.connection_on_secure_handshake_fnc)) {
            rthis.doStop(_rctx, error_connection_invalid_state); //TODO: add new error
        }
    } else {

        rthis.service(_rctx).onIncomingConnectionStart(conctx);

        vdbgx(Debug::mpipc, &rthis << "");
        //we continue the connection start process entering the right state
        switch (config.server.connection_start_state) {
        case ConnectionState::Raw:
            break;
        case ConnectionState::Active:
            if (not rthis.isActiveState()) {
                rthis.post(
                    _rctx,
                    [&rthis](frame::aio::ReactorContext& _rctx, Event&& _revent) { rthis.onEvent(_rctx, std::move(_revent)); },
                    connection_event_category.event(ConnectionEvents::EnterActive));
            }
            break;
        case ConnectionState::Passive:
        default:
            if (not rthis.isActiveState() and rthis.isRawState()) {
                rthis.post(
                    _rctx,
                    [&rthis](frame::aio::ReactorContext& _rctx, Event&& _revent) { rthis.onEvent(_rctx, std::move(_revent)); },
                    connection_event_category.event(ConnectionEvents::EnterPassive));
            }
            break;
        }
    }
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventSendRaw(frame::aio::ReactorContext& _rctx, Event& _revent)
{
    SendRaw*          pdata = _revent.any().cast<SendRaw>();
    ConnectionContext conctx(service(_rctx), *this);

    SOLID_ASSERT(pdata);

    if (this->isRawState() and pdata) {

        idbgx(Debug::mpipc, this << " datasize = " << pdata->data.size());

        size_t tocopy = this->sendBufferCapacity();

        if (tocopy > pdata->data.size()) {
            tocopy = pdata->data.size();
        }

        memcpy(send_buf_.get(), pdata->data.data(), tocopy);

        if (tocopy) {
            pdata->offset = tocopy;
            if (this->postSendAll(_rctx, send_buf_.get(), tocopy, _revent)) {
                pdata->complete_fnc(conctx, _rctx.error());
            }
        } else {
            pdata->complete_fnc(conctx, ErrorConditionT());
        }

    } else if (pdata) {
        pdata->complete_fnc(conctx, error_connection_invalid_state);
    }
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventRecvRaw(frame::aio::ReactorContext& _rctx, Event& _revent)
{

    RecvRaw*          pdata = _revent.any().cast<RecvRaw>();
    ConnectionContext conctx(service(_rctx), *this);
    size_t            used_size = 0;

    SOLID_ASSERT(pdata);

    idbgx(Debug::mpipc, this);

    if (this->isRawState() and pdata) {
        if (recv_buf_off_ == cons_buf_off_) {
            if (this->postRecvSome(_rctx, recv_buf_.get(), this->recvBufferCapacity(), _revent)) {

                pdata->complete_fnc(conctx, nullptr, used_size, _rctx.error());
            }
        } else {
            used_size = recv_buf_off_ - cons_buf_off_;

            pdata->complete_fnc(conctx, recv_buf_.get() + cons_buf_off_, used_size, _rctx.error());

            if (used_size > (recv_buf_off_ - cons_buf_off_)) {
                used_size = (recv_buf_off_ - cons_buf_off_);
            }

            cons_buf_off_ += used_size;

            if (cons_buf_off_ == recv_buf_off_) {
                cons_buf_off_ = recv_buf_off_ = 0;
            } else {
                SOLID_ASSERT(cons_buf_off_ < recv_buf_off_);
            }
        }
    } else if (pdata) {

        pdata->complete_fnc(conctx, nullptr, used_size, error_connection_invalid_state);
    }
}
//-----------------------------------------------------------------------------
void Connection::doResetTimerStart(frame::aio::ReactorContext& _rctx)
{

    Configuration const& config = service(_rctx).configuration();

    if (isServer()) {
        if (config.connection_inactivity_timeout_seconds) {
            recv_keepalive_count_ = 0;
            flags_.reset(FlagsE::HasActivity);

            idbgx(Debug::mpipc, this << ' ' << this->id() << " wait for " << config.connection_inactivity_timeout_seconds << " seconds");

            timer_.waitFor(_rctx, std::chrono::seconds(config.connection_inactivity_timeout_seconds), onTimerInactivity);
        }
    } else { //client
        if (config.connection_keepalive_timeout_seconds) {
            flags_.set(FlagsE::WaitKeepAliveTimer);

            idbgx(Debug::mpipc, this << ' ' << this->id() << " wait for " << config.connection_keepalive_timeout_seconds << " seconds");

            timer_.waitFor(_rctx, std::chrono::seconds(config.connection_keepalive_timeout_seconds), onTimerKeepalive);
        }
    }
}
//-----------------------------------------------------------------------------
void Connection::doResetTimerSend(frame::aio::ReactorContext& _rctx)
{

    Configuration const& config = service(_rctx).configuration();

    if (isServer()) {
        if (config.connection_inactivity_timeout_seconds) {
            flags_.set(FlagsE::HasActivity);
        }
    } else { //client
        if (config.connection_keepalive_timeout_seconds and isWaitingKeepAliveTimer()) {

            vdbgx(Debug::mpipc, this << ' ' << this->id() << " wait for " << config.connection_keepalive_timeout_seconds << " seconds");

            timer_.waitFor(_rctx, std::chrono::seconds(config.connection_keepalive_timeout_seconds), onTimerKeepalive);
        }
    }
}
//-----------------------------------------------------------------------------
void Connection::doResetTimerRecv(frame::aio::ReactorContext& _rctx)
{

    Configuration const& config = service(_rctx).configuration();

    if (isServer()) {
        if (config.connection_inactivity_timeout_seconds) {
            flags_.set(FlagsE::HasActivity);
        }
    } else { //client
        if (config.connection_keepalive_timeout_seconds and not isWaitingKeepAliveTimer()) {
            flags_.set(FlagsE::WaitKeepAliveTimer);

            idbgx(Debug::mpipc, this << ' ' << this->id() << " wait for " << config.connection_keepalive_timeout_seconds << " seconds");

            timer_.waitFor(_rctx, std::chrono::seconds(config.connection_keepalive_timeout_seconds), onTimerKeepalive);
        }
    }
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onTimerInactivity(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());

    idbgx(Debug::mpipc, &rthis << " " << rthis.flags_.toString() << " " << rthis.recv_keepalive_count_);

    if (rthis.flags_.has(FlagsE::HasActivity)) {

        rthis.flags_.reset(FlagsE::HasActivity);
        rthis.recv_keepalive_count_ = 0;

        Configuration const& config = rthis.service(_rctx).configuration();

        idbgx(Debug::mpipc, &rthis << ' ' << rthis.id() << " wait for " << config.connection_inactivity_timeout_seconds << " seconds");

        rthis.timer_.waitFor(_rctx, std::chrono::seconds(config.connection_inactivity_timeout_seconds), onTimerInactivity);
    } else {
        rthis.doStop(_rctx, error_connection_inactivity_timeout);
    }
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onTimerKeepalive(frame::aio::ReactorContext& _rctx)
{

    Connection& rthis = static_cast<Connection&>(_rctx.object());

    SOLID_ASSERT(not rthis.isServer());
    rthis.flags_.set(FlagsE::Keepalive);
    rthis.flags_.reset(FlagsE::WaitKeepAliveTimer);
    idbgx(Debug::mpipc, &rthis << " post send");
    rthis.post(_rctx, [&rthis](frame::aio::ReactorContext& _rctx, Event const& /*_revent*/) { rthis.doSend(_rctx); });
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onSendAllRaw(frame::aio::ReactorContext& _rctx, Event& _revent)
{

    Connection&       rthis = static_cast<Connection&>(_rctx.object());
    SendRaw*          pdata = _revent.any().cast<SendRaw>();
    ConnectionContext conctx(rthis.service(_rctx), rthis);

    SOLID_ASSERT(pdata);

    if (pdata) {

        if (not _rctx.error()) {

            size_t tocopy = pdata->data.size() - pdata->offset;

            if (tocopy > rthis.sendBufferCapacity()) {
                tocopy = rthis.sendBufferCapacity();
            }

            memcpy(rthis.send_buf_.get(), pdata->data.data() + pdata->offset, tocopy);

            if (tocopy) {
                pdata->offset += tocopy;
                if (rthis.postSendAll(_rctx, rthis.send_buf_.get(), tocopy, _revent)) {
                    pdata->complete_fnc(conctx, _rctx.error());
                }
            } else {
                pdata->complete_fnc(conctx, ErrorConditionT());
            }

        } else {
            pdata->complete_fnc(conctx, _rctx.error());
        }
    }
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onRecvSomeRaw(frame::aio::ReactorContext& _rctx, const size_t _sz, Event& _revent)
{

    Connection&       rthis = static_cast<Connection&>(_rctx.object());
    RecvRaw*          pdata = _revent.any().cast<RecvRaw>();
    ConnectionContext conctx(rthis.service(_rctx), rthis);

    SOLID_ASSERT(pdata);

    if (pdata) {

        size_t used_size = _sz;

        pdata->complete_fnc(conctx, rthis.recv_buf_.get(), used_size, _rctx.error());

        if (used_size > _sz) {
            used_size = _sz;
        }

        if (used_size < _sz) {
            rthis.recv_buf_off_ = _sz;
            rthis.cons_buf_off_ = used_size;
        }
    }
}
//-----------------------------------------------------------------------------
void Connection::doResetRecvBuffer(frame::aio::ReactorContext& _rctx, const uint8_t _request_buffer_ack_count, ErrorConditionT& _rerr)
{
    if (_request_buffer_ack_count == 0) {

    } else if (recv_buf_.use_count() > 1) {
        vdbgx(Debug::mpipc, this << " buffer used for relay - try replace it. vec_size = " << recv_buf_vec_.size() << " count = " << recv_buf_count_);
        RecvBufferPointerT new_buf;
        if (recv_buf_vec_.size()) {
            new_buf = std::move(recv_buf_vec_.back());
            recv_buf_vec_.pop_back();
        } else if (recv_buf_count_ < service(_rctx).configuration().connection_relay_buffer_count) {
            new_buf = service(_rctx).configuration().allocateRecvBuffer(recv_buf_cp_kb_);
        } else {
            recv_buf_.reset();
            _rerr         = error_connection_too_many_recv_buffers;
            cons_buf_off_ = 0;
            recv_buf_off_ = 0;
            return;
        }
        const size_t cnssz = recv_buf_off_ - cons_buf_off_;

        memcpy(new_buf.get(), recv_buf_.get() + cons_buf_off_, cnssz);
        cons_buf_off_ = 0;
        recv_buf_off_ = cnssz;
        recv_buf_     = std::move(new_buf);
    } else {
        //tried to relay received messages/message parts - but all failed
        //so we need to ack the buffer
        vdbgx(Debug::mpipc, this << " send accept for " << (int)_request_buffer_ack_count << " buffers");

        if (ackd_buf_count_ == 0) {
            ackd_buf_count_ += _request_buffer_ack_count;
            this->post(_rctx, [this](frame::aio::ReactorContext& _rctx, Event const& /*_revent*/) { this->doSend(_rctx); });
        } else {
            ackd_buf_count_ += _request_buffer_ack_count;
        }
    }
}
//-----------------------------------------------------------------------------
struct Connection::Receiver : MessageReader::Receiver {
    Connection&                 rcon_;
    frame::aio::ReactorContext& rctx_;

    Receiver(Connection& _rcon, frame::aio::ReactorContext& _rctx)
        : rcon_(_rcon)
        , rctx_(_rctx)
    {
    }

    void receiveMessage(MessagePointerT& _rmsg_ptr, const size_t _msg_type_id) override
    {
        rcon_.doCompleteMessage(rctx_, _rmsg_ptr, _msg_type_id);
        rcon_.flags_.set(FlagsE::PollPool); //reset flag
        rcon_.post(
            rctx_,
            [](frame::aio::ReactorContext& _rctx, Event const& /*_revent*/) {
                Connection& rthis = static_cast<Connection&>(_rctx.object());
                rthis.doSend(_rctx);
            });
    }

    void receiveKeepAlive() override
    {
        rcon_.doCompleteKeepalive(rctx_);
    }

    void receiveAckCount(uint8_t _count) override
    {
        rcon_.doCompleteAckCount(rctx_, _count);
    }
    void receiveCancelRequest(const RequestId& _reqid) override
    {
        rcon_.doCompleteCancelRequest(rctx_, _reqid);
    }

    bool receiveRelayBody(MessageHeader& _rmsghdr, const char* _pbeg, size_t _sz, ObjectIdT& _rrelay_id, const bool _is_last, ErrorConditionT& _rerror) override
    {
        return rcon_.doCompleteRelayBody(rctx_, _rmsghdr, _pbeg, _sz, _rrelay_id, _is_last, _rerror);
    }

    void pushCancelRequest(const RequestId& _reqid) override
    {
        rcon_.cancel_remote_msg_vec_.push_back(_reqid);
        if (rcon_.cancel_remote_msg_vec_.size() == 1) {
            rcon_.post(
                rctx_,
                [](frame::aio::ReactorContext& _rctx, Event const& /*_revent*/) {
                    Connection& rthis = static_cast<Connection&>(_rctx.object());
                    rthis.doSend(_rctx);
                });
        }
    }
};

/*static*/ void Connection::onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
{

    Connection&          rthis = static_cast<Connection&>(_rctx.object());
    ConnectionContext    conctx(rthis.service(_rctx), rthis);
    const Configuration& rconfig   = rthis.service(_rctx).configuration();
    unsigned             repeatcnt = 4;
    char*                pbuf;
    size_t               bufsz;
    const uint32_t       recvbufcp      = rthis.recvBufferCapacity();
    bool                 recv_something = false;
    Receiver             rcvr(rthis, _rctx);
    ErrorConditionT      error;

    rthis.doResetTimerRecv(_rctx);

    do {
        vdbgx(Debug::mpipc, &rthis << " received size " << _sz);

        if (!_rctx.error()) {
            recv_something = true;
            rthis.recv_buf_off_ += _sz;
            pbuf  = rthis.recv_buf_.get() + rthis.cons_buf_off_;
            bufsz = rthis.recv_buf_off_ - rthis.cons_buf_off_;

            rcvr.request_buffer_ack_count_ = 0;

            rthis.cons_buf_off_ += rthis.msg_reader_.read(
                pbuf, bufsz, rcvr, rconfig.reader, rconfig.protocol(), conctx, error);

            vdbgx(Debug::mpipc, &rthis << " consumed size " << rthis.cons_buf_off_ << " of " << bufsz);

            rthis.doResetRecvBuffer(_rctx, rcvr.request_buffer_ack_count_, error);

            if (error) {
                edbgx(Debug::mpipc, &rthis << ' ' << rthis.id() << " parsing " << error.message());
                rthis.doStop(_rctx, error);
                recv_something = false; //prevent calling doResetTimerRecv after doStop
                break;
            } else if (rthis.cons_buf_off_ < bufsz) {
                rthis.doOptimizeRecvBufferForced();
            }
        } else {
            edbgx(Debug::mpipc, &rthis << ' ' << rthis.id() << " receiving " << _rctx.error().message());
            rthis.flags_.set(FlagsE::StopPeer);
            rthis.doStop(_rctx, _rctx.error(), _rctx.systemError());
            recv_something = false; //prevent calling doResetTimerRecv after doStop
            break;
        }
        --repeatcnt;
        rthis.doOptimizeRecvBuffer();
        pbuf  = rthis.recv_buf_.get() + rthis.recv_buf_off_;
        bufsz = recvbufcp - rthis.recv_buf_off_;
        //idbgx(Debug::mpipc, &rthis<<" buffer size "<<bufsz);
    } while (repeatcnt && rthis.recvSome(_rctx, pbuf, bufsz, _sz));

    if (recv_something) {
        rthis.doResetTimerRecv(_rctx);
    }

    if (repeatcnt == 0) {
        bool rv = rthis.postRecvSome(_rctx, pbuf, bufsz); //fully asynchronous call
        SOLID_ASSERT(!rv);
        (void)rv;
    }
}
//-----------------------------------------------------------------------------
inline bool Connection::hasRelayBuffer(const Configuration& _rconfig, char*& _rpbuf)
{
    if (send_buf_vec_sentinel_ < send_buf_vec_.size()) {
        _rpbuf = send_buf_vec_[send_buf_vec_sentinel_].get();
        ++send_buf_vec_sentinel_;
        vdbgx(Debug::mpipc, this << " sentinel = " << (int)send_buf_vec_sentinel_);
        return true;
    } else if (send_buf_vec_.size() < _rconfig.connection_relay_buffer_count) {
        send_buf_vec_.push_back(_rconfig.allocateSendBuffer(recv_buf_cp_kb_));
        _rpbuf = send_buf_vec_[send_buf_vec_sentinel_].get();
        ++send_buf_vec_sentinel_;
        vdbgx(Debug::mpipc, this << " sentinel = " << (int)send_buf_vec_sentinel_);
        return true;
    }
    return false;
}
//-----------------------------------------------------------------------------

struct Connection::Sender : MessageWriter::Sender {
    Connection&                 rcon_;
    frame::aio::ReactorContext& rctx_;

    Sender(Connection& _rcon, frame::aio::ReactorContext& _rctx)
        : rcon_(_rcon)
        , rctx_(_rctx)
    {
    }

    ErrorConditionT completeMessage(MessageBundle& _rmsg_bundle, MessageId const& _rpool_msg_id) override
    {
        rcon_.doCompleteMessage(rctx_, _rpool_msg_id, _rmsg_bundle, ErrorConditionT());
        return rcon_.service(rctx_).pollPoolForUpdates(rcon_, rcon_.uid(rctx_), _rpool_msg_id);
    }

    void releaseRelayBuffer() override
    {
        --rcon_.send_buf_vec_sentinel_;
        vdbgx(Debug::mpipc, this << " sentinel = " << (int)rcon_.send_buf_vec_sentinel_);
    }
};

void Connection::doSend(frame::aio::ReactorContext& _rctx)
{

    vdbgx(Debug::mpipc, this << " isstopping = " << this->isStopping());

    if (not this->isStopping()) {
        ErrorConditionT error;
        //we do a pollPoolForUpdates here because we want to be able to
        //receive a force pool close, even though we are waiting for send.
        if (
            shouldPollPool()) {
            flags_.reset(FlagsE::PollPool); //reset flag
            if ((error = service(_rctx).pollPoolForUpdates(*this, uid(_rctx), MessageId()))) {
                doStop(_rctx, error);
                return;
            }
        }

        if (not this->hasPendingSend()) {
            ConnectionContext          conctx(service(_rctx), *this);
            unsigned                   repeatcnt      = 4;
            const uint32_t             sendbufcp      = sendBufferCapacity();
            const Configuration&       rconfig        = service(_rctx).configuration();
            bool                       sent_something = false;
            Sender                     sender(*this, _rctx);
            MessageWriter::WriteFlagsT write_flags;
            //doResetTimerSend(_rctx);

            while (repeatcnt) {

                if (shouldPollPool()) {
                    
                    flags_.reset(FlagsE::PollPool); //reset flag
                    
                    if ((error = service(_rctx).pollPoolForUpdates(*this, uid(_rctx), MessageId()))) {
                        doStop(_rctx, error);
                        sent_something = false; //prevent calling doResetTimerSend after doStop
                        break;
                    }
                }

                write_flags.reset();

                if (shouldSendKeepalive()) {
                    write_flags.set(MessageWriter::WriteFlagsE::ShouldSendKeepAlive);
                }

                const bool use_relay_buffer = msg_writer_.isFrontRelayMessage();
                char*      buffer           = send_buf_.get();

                if (use_relay_buffer and hasRelayBuffer(rconfig, buffer)) {
                    vdbgx(Debug::mpipc, this << ' ' << id() << " using a relay buffer");
                    write_flags.set(MessageWriter::WriteFlagsE::CanSendRelayedMessages);
                } else {
                    vdbgx(Debug::mpipc, this << ' ' << id() << " using the direct buffer");
                }

                uint32_t sz = msg_writer_.write(
                    buffer, sendbufcp, write_flags, ackd_buf_count_, cancel_remote_msg_vec_, sender, rconfig.writer, rconfig.protocol(), conctx, error);

                flags_.reset(FlagsE::Keepalive);

                if (!error) {

                    ackd_buf_count_ = 0;

                    if (sz && this->sendAll(_rctx, buffer, sz)) {
                        if (_rctx.error()) {
                            edbgx(Debug::mpipc, this << ' ' << id() << " sending " << sz << ": " << _rctx.error().message());
                            flags_.set(FlagsE::StopPeer);
                            doStop(_rctx, _rctx.error(), _rctx.systemError());
                            sent_something = false; //prevent calling doResetTimerSend after doStop
                            break;
                        } else {
                            sent_something = true;
                        }
                    } else {
                        break;
                    }
                } else {
                    edbgx(Debug::mpipc, this << ' ' << id() << " size to send " << sz << " error " << error.message());

                    if (sz) {
                        this->sendAll(_rctx, buffer, sz);
                    }

                    doStop(_rctx, error);
                    sent_something = false; //prevent calling doResetTimerSend after doStop

                    break;
                }
                --repeatcnt;
            }

            if (sent_something) {
                doResetTimerSend(_rctx);
            }

            if (repeatcnt == 0) {
                //idbgx(Debug::mpipc, this<<" post send");
                this->post(_rctx, [this](frame::aio::ReactorContext& _rctx, Event const& /*_revent*/) { this->doSend(_rctx); });
            }
            //idbgx(Debug::mpipc, this<<" done-doSend "<<this->sendmsgvec[0].size()<<" "<<this->sendmsgvec[1].size());

        } //if(not this->hasPendingSend())

    } //if(not this->isStopping())
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onSend(frame::aio::ReactorContext& _rctx)
{

    Connection& rthis = static_cast<Connection&>(_rctx.object());

    if (!_rctx.error()) {
        if (not rthis.isStopping()) {
            idbgx(Debug::mpipc, &rthis);
            rthis.doResetTimerSend(_rctx);
        }
        rthis.doSend(_rctx);
    } else {
        edbgx(Debug::mpipc, &rthis << ' ' << rthis.id() << " sending [" << _rctx.error().message() << "][" << _rctx.systemError().message() << ']');
        rthis.doStop(_rctx, _rctx.error(), _rctx.systemError());
    }
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onConnect(frame::aio::ReactorContext& _rctx)
{

    Connection& rthis = static_cast<Connection&>(_rctx.object());

    if (!_rctx.error()) {
        idbgx(Debug::mpipc, &rthis << ' ' << rthis.id() << " (" << local_address(rthis.sock_ptr_->device()) << ") -> (" << remote_address(rthis.sock_ptr_->device()) << ')');
        rthis.doStart(_rctx, false);
    } else {
        edbgx(Debug::mpipc, &rthis << ' ' << rthis.id() << " connecting [" << _rctx.error().message() << "][" << _rctx.systemError().message() << ']');
        rthis.doStop(_rctx, _rctx.error(), _rctx.systemError());
    }
}
//-----------------------------------------------------------------------------
void Connection::doCompleteMessage(frame::aio::ReactorContext& _rctx, MessagePointerT& _rresponse_ptr, const size_t _response_type_id)
{

    ConnectionContext    conctx(service(_rctx), *this);
    const Configuration& rconfig = service(_rctx).configuration();
    const Protocol&      rproto  = rconfig.protocol();
    ErrorConditionT      error;
    MessageBundle        msg_bundle; //request message

    if (_rresponse_ptr->isBackOnSender() or _rresponse_ptr->isBackOnPeer()) {
        idbgx(Debug::mpipc, this << ' ' << "Completing back on sender message: " << _rresponse_ptr->requestId());

        MessageId pool_msg_id;

        msg_writer_.cancel(_rresponse_ptr->requestId(), msg_bundle, pool_msg_id);

        idbgx(Debug::mpipc, this);

        conctx.message_flags = msg_bundle.message_flags;
        conctx.request_id    = _rresponse_ptr->requestId();
        conctx.message_id    = pool_msg_id;

        if (not SOLID_FUNCTION_EMPTY(msg_bundle.complete_fnc)) {
            idbgx(Debug::mpipc, this);
            msg_bundle.complete_fnc(conctx, msg_bundle.message_ptr, _rresponse_ptr, error);
        } else if (msg_bundle.message_ptr) {
            idbgx(Debug::mpipc, this << " " << msg_bundle.message_type_id);
            rproto[msg_bundle.message_type_id].complete_fnc(conctx, msg_bundle.message_ptr, _rresponse_ptr, error);
        } else {
            idbgx(Debug::mpipc, this << " " << _response_type_id);
            rproto[_response_type_id].complete_fnc(conctx, msg_bundle.message_ptr, _rresponse_ptr, error);
        }

    } else {
        idbgx(Debug::mpipc, this << " " << _response_type_id);
        rproto[_response_type_id].complete_fnc(conctx, msg_bundle.message_ptr, _rresponse_ptr, error);
    }
}
//-----------------------------------------------------------------------------
void Connection::doCompleteMessage(
    solid::frame::aio::ReactorContext& _rctx,
    MessageId const&                   _rpool_msg_id,
    MessageBundle&                     _rmsg_bundle,
    ErrorConditionT const&             _rerror)
{

    ConnectionContext    conctx(service(_rctx), *this);
    const Configuration& rconfig = service(_rctx).configuration();
    const Protocol&      rproto  = rconfig.protocol();
    //const Configuration &rconfig  = service(_rctx).configuration();

    MessagePointerT dummy_recv_msg_ptr;

    conctx.message_flags = _rmsg_bundle.message_flags;
    conctx.message_id    = _rpool_msg_id;

    if (not SOLID_FUNCTION_EMPTY(_rmsg_bundle.complete_fnc)) {
        idbgx(Debug::mpipc, this);
        _rmsg_bundle.complete_fnc(conctx, _rmsg_bundle.message_ptr, dummy_recv_msg_ptr, _rerror);
    } else {
        idbgx(Debug::mpipc, this << " " << _rmsg_bundle.message_type_id);
        rproto[_rmsg_bundle.message_type_id].complete_fnc(conctx, _rmsg_bundle.message_ptr, dummy_recv_msg_ptr, _rerror);
    }
}
//-----------------------------------------------------------------------------
void Connection::doCompleteKeepalive(frame::aio::ReactorContext& _rctx)
{
    if (isServer()) {
        Configuration const& config = service(_rctx).configuration();

        ++recv_keepalive_count_;

        vdbgx(Debug::mpipc, this << "recv_keep_alive_count = " << recv_keepalive_count_);

        if (recv_keepalive_count_ < config.connection_inactivity_keepalive_count) {
            flags_.set(FlagsE::Keepalive);
            idbgx(Debug::mpipc, this << " post send");
            this->post(_rctx, [this](frame::aio::ReactorContext& _rctx, Event const& /*_revent*/) { this->doSend(_rctx); });
        } else {
            idbgx(Debug::mpipc, this << " post stop because of too many keep alive messages");
            recv_keepalive_count_ = 0; //prevent other posting
            this->post(
                _rctx,
                [this](frame::aio::ReactorContext& _rctx, Event const& /*_revent*/) {
                    this->doStop(_rctx, error_connection_too_many_keepalive_packets_received);
                });
            return;
        }
    }
}
//-----------------------------------------------------------------------------
void Connection::doCompleteAckCount(frame::aio::ReactorContext& _rctx, uint8_t _count)
{
    if (_count <= send_buf_vec_sentinel_) {
        send_buf_vec_sentinel_ -= _count;
        vdbgx(Debug::mpipc, this << " count = " << (int)_count << " sentinel = " << (int)send_buf_vec_sentinel_);

        this->post(_rctx, [this](frame::aio::ReactorContext& _rctx, Event const& /*_revent*/) { this->doSend(_rctx); });
    } else {
        vdbgx(Debug::mpipc, this << " count = " << (int)_count << " sentinel = " << (int)send_buf_vec_sentinel_);
        this->post(
            _rctx,
            [this](frame::aio::ReactorContext& _rctx, Event const& /*_revent*/) {
                this->doStop(_rctx, error_connection_ack_count);
            });
    }
}
//-----------------------------------------------------------------------------
void Connection::doCompleteCancelRequest(frame::aio::ReactorContext& _rctx, const RequestId& _reqid)
{
    MessageId     msgid(_reqid);
    MessageBundle msg_bundle;
    MessageId     pool_msg_id;

    if (msg_writer_.cancel(msgid, msg_bundle, pool_msg_id)) {
        doCompleteMessage(_rctx, pool_msg_id, msg_bundle, error_message_canceled_peer);
    }
}
//-----------------------------------------------------------------------------
bool Connection::doCompleteRelayBody(
    frame::aio::ReactorContext& _rctx,
    MessageHeader& _rmsghdr,
    const char* _pbeg,
    size_t _sz,
    ObjectIdT& _rrelay_id,
    const bool _is_last,
    ErrorConditionT& _rerror)
{
    Configuration const& config = service(_rctx).configuration();
    ConnectionContext    conctx(service(_rctx), *this);
    RelayData            relmsg(std::move(recv_buf_), _pbeg, _sz, this->uid(_rctx));

    return config.connection_on_relay_fnc(conctx, _rmsghdr, std::move(relmsg), _rrelay_id, _is_last, _rerror);
}
//-----------------------------------------------------------------------------
/*virtual*/ bool Connection::postSendAll(frame::aio::ReactorContext& _rctx, const char* _pbuf, size_t _bufcp, Event& _revent)
{
    return sock_ptr_->postSendAll(_rctx, Connection::onSendAllRaw, _pbuf, _bufcp, _revent);
}
//-----------------------------------------------------------------------------
/*virtual*/ bool Connection::postRecvSome(frame::aio::ReactorContext& _rctx, char* _pbuf, size_t _bufcp, Event& _revent)
{
    return sock_ptr_->postRecvSome(_rctx, Connection::onRecvSomeRaw, _pbuf, _bufcp, _revent);
}
//-----------------------------------------------------------------------------
/*virtual*/ bool Connection::postRecvSome(frame::aio::ReactorContext& _rctx, char* _pbuf, size_t _bufcp)
{
    return sock_ptr_->postRecvSome(_rctx, Connection::onRecv, _pbuf, _bufcp);
}
//-----------------------------------------------------------------------------
/*virtual*/ bool Connection::hasValidSocket() const
{
    return sock_ptr_->hasValidSocket();
}
//-----------------------------------------------------------------------------
/*virtual*/ bool Connection::connect(frame::aio::ReactorContext& _rctx, const SocketAddressInet& _raddr)
{
    return sock_ptr_->connect(_rctx, Connection::onConnect, _raddr);
}
//-----------------------------------------------------------------------------
/*virtual*/ bool Connection::recvSome(frame::aio::ReactorContext& _rctx, char* _buf, size_t _bufcp, size_t& _sz)
{
    return sock_ptr_->recvSome(_rctx, Connection::onRecv, _buf, _bufcp, _sz);
}
//-----------------------------------------------------------------------------
/*virtual*/ bool Connection::hasPendingSend() const
{
    return sock_ptr_->hasPendingSend();
}
//-----------------------------------------------------------------------------
/*virtual*/ bool Connection::sendAll(frame::aio::ReactorContext& _rctx, char* _buf, size_t _bufcp)
{
    return sock_ptr_->sendAll(_rctx, Connection::onSend, _buf, _bufcp);
}
//-----------------------------------------------------------------------------
/*virtual*/ void Connection::prepareSocket(frame::aio::ReactorContext& _rctx)
{
    sock_ptr_->prepareSocket(_rctx);
}
//-----------------------------------------------------------------------------
//      SecureConnection
//-----------------------------------------------------------------------------
// SecureConnection::SecureConnection(
//  Configuration const& _rconfiguration,
//  SocketDevice &_rsd,
//  ConnectionPoolId const &_rpool_id,
//  std::string const & _rpool_name
// ): Connection(_rconfiguration, _rpool_id, _rpool_name), sock(this->proxy(), std::move(_rsd), _rconfiguration.secure_context)
// {
//  idbgx(Debug::mpipc, this<<' '<<timer.isActive()<<' '<<sock.isActive());
// }
//-----------------------------------------------------------------------------
// SecureConnection::SecureConnection(
//  Configuration const& _rconfiguration,
//  ConnectionPoolId const &_rpool_id,
//  std::string const & _rpool_name
// ): Connection(_rconfiguration, _rpool_id, _rpool_name), sock(this->proxy(), _rconfiguration.secure_context)
// {
//  idbgx(Debug::mpipc, this<<' '<<timer.isActive()<<' '<<sock.isActive());
// }

//-----------------------------------------------------------------------------
//  ConnectionContext
//-----------------------------------------------------------------------------
Any<>& ConnectionContext::any()
{
    return rconnection.any();
}
//-----------------------------------------------------------------------------
MessagePointerT ConnectionContext::fetchRequest(Message const& _rmsg) const
{
    return rconnection.fetchRequest(_rmsg);
}
//-----------------------------------------------------------------------------
RecipientId ConnectionContext::recipientId() const
{
    return RecipientId(rconnection.poolId(), rservice.manager().id(rconnection));
}
//-----------------------------------------------------------------------------
ObjectIdT ConnectionContext::connectionId() const
{
    return rservice.manager().id(rconnection);
}
//-----------------------------------------------------------------------------
const std::string& ConnectionContext::recipientName() const
{
    return rconnection.poolName();
}
//-----------------------------------------------------------------------------
bool ConnectionContext::isConnectionActive() const
{
    return rconnection.isActiveState();
}
//-----------------------------------------------------------------------------
bool ConnectionContext::isConnectionServer() const
{
    return rconnection.isServer();
}
//-----------------------------------------------------------------------------
const ErrorConditionT& ConnectionContext::error() const
{
    return rconnection.error();
}
//-----------------------------------------------------------------------------
const ErrorCodeT& ConnectionContext::systemError() const
{
    return rconnection.systemError();
}
//-----------------------------------------------------------------------------
Configuration const& ConnectionContext::configuration() const
{
    return rservice.configuration();
}
//-----------------------------------------------------------------------------
// SocketStub
//-----------------------------------------------------------------------------
/*virtual*/ bool SocketStub::secureAccept(frame::aio::ReactorContext& /*_rctx*/, ConnectionContext& /*_rconctx*/, OnSecureAcceptF /*_pf*/, ErrorConditionT& _rerror)
{
    _rerror = error_connection_logic;
    return true;
}
//-----------------------------------------------------------------------------
/*virtual*/ bool SocketStub::secureConnect(frame::aio::ReactorContext& /*_rctx*/, ConnectionContext& /*_rconctx*/, OnSecureConnectF /*_pf*/, ErrorConditionT& _rerror)
{
    _rerror = error_connection_logic;
    return true;
}
//-----------------------------------------------------------------------------
ConnectionProxy SocketStub::connectionProxy()
{
    return ConnectionProxy{};
}
//-----------------------------------------------------------------------------
// ConnectionProxy
//-----------------------------------------------------------------------------
Service& ConnectionProxy::service(frame::aio::ReactorContext& _rctx) const
{
    return static_cast<Service&>(_rctx.service());
}
//-----------------------------------------------------------------------------
Connection& ConnectionProxy::connection(frame::aio::ReactorContext& _rctx) const
{
    return static_cast<Connection&>(_rctx.object());
}
//-----------------------------------------------------------------------------
} //namespace mpipc
} //namespace frame
} //namespace solid
