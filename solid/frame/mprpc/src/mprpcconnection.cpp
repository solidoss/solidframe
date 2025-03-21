// solid/frame/ipc/src/ipcconnection.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "mprpcconnection.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/frame/manager.hpp"
#include "solid/frame/mprpc/mprpcerror.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"
#include "solid/utility/event.hpp"
#include <chrono>
#include <cstdio>

namespace solid {
namespace frame {
namespace mprpc {
namespace {

const LoggerT logger("solid::frame::mprpc::connection");

enum class ConnectionEvents {
    Resolve,
    NewPoolMessage,
    NewPoolQueueMessage,
    NewConnMessage,
    CancelConnMessage,
    CancelPoolMessage,
    ClosePoolMessage,
    EnterActive,
    EnterPassive,
    StartSecure,
    SendRaw,
    RecvRaw,
    Stopping,
    RelayNew,
    RelayDone,
    Post,
    Invalid,
};

const EventCategory<ConnectionEvents> connection_event_category{
    "solid::frame::mprpc::connection_event_category",
    [](const ConnectionEvents _evt) {
        switch (_evt) {
        case ConnectionEvents::Resolve:
            return "Resolve";
        case ConnectionEvents::NewPoolMessage:
            return "NewPoolMessage";
        case ConnectionEvents::NewPoolQueueMessage:
            return "NewPoolQueueMessage";
        case ConnectionEvents::NewConnMessage:
            return "NewConnMessage";
        case ConnectionEvents::CancelConnMessage:
            return "CancelConnMessage";
        case ConnectionEvents::CancelPoolMessage:
            return "CancelPoolMessage";
        case ConnectionEvents::ClosePoolMessage:
            return "ClosePoolMessage";
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
        case ConnectionEvents::RelayNew:
            return "RelayNew";
        case ConnectionEvents::RelayDone:
            return "RelayDone";
        case ConnectionEvents::Post:
            return "Post";
        case ConnectionEvents::Invalid:
            return "Invalid";
        default:
            return "unknown";
        }
    }};

} // namespace

void ResolveMessage::popAddress()
{
    solid_assert_log(addrvec.size(), logger);
    addrvec.pop_back();
}

//-----------------------------------------------------------------------------
inline Service& Connection::service(frame::aio::ReactorContext& _rctx) const
{
    return static_cast<Service&>(_rctx.service());
}
//-----------------------------------------------------------------------------
inline ActorIdT Connection::uid(frame::aio::ReactorContext& _rctx) const
{
    return service(_rctx).id(*this);
}
//-----------------------------------------------------------------------------
inline const Configuration& Connection::configuration(frame::aio::ReactorContext& _rctx) const
{
    return service(_rctx).configuration();
}
//-----------------------------------------------------------------------------

/*static*/ EventT Connection::eventResolve()
{
    return make_event(connection_event_category, ConnectionEvents::Resolve);
}
//-----------------------------------------------------------------------------
/*static*/ EventT Connection::eventNewMessage()
{
    return make_event(connection_event_category, ConnectionEvents::NewPoolMessage);
}
//-----------------------------------------------------------------------------
/*static*/ EventT Connection::eventNewQueueMessage()
{
    return make_event(connection_event_category, ConnectionEvents::NewPoolQueueMessage);
}
//-----------------------------------------------------------------------------
/*static*/ EventT Connection::eventNewMessage(const MessageId& _rmsgid)
{
    return make_event(connection_event_category, ConnectionEvents::NewConnMessage, _rmsgid);
}
//-----------------------------------------------------------------------------
/*static*/ EventT Connection::eventCancelConnMessage(const MessageId& _rmsgid)
{
    return make_event(connection_event_category, ConnectionEvents::CancelConnMessage, _rmsgid);
}
//-----------------------------------------------------------------------------
/*static*/ EventT Connection::eventCancelPoolMessage(const MessageId& _rmsgid)
{
    return make_event(connection_event_category, ConnectionEvents::CancelPoolMessage, _rmsgid);
}
//-----------------------------------------------------------------------------
/*static*/ EventT Connection::eventClosePoolMessage(const MessageId& _rmsgid)
{
    return make_event(connection_event_category, ConnectionEvents::ClosePoolMessage, _rmsgid);
}
//-----------------------------------------------------------------------------
/*static*/ EventT Connection::eventStopping()
{
    return make_event(connection_event_category, ConnectionEvents::Stopping);
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
/*static*/ EventT Connection::eventEnterActive(ConnectionEnterActiveCompleteFunctionT&& _ucomplete_fnc, const size_t _send_buffer_capacity)
{
    return make_event(connection_event_category, ConnectionEvents::EnterActive, EnterActive(std::move(_ucomplete_fnc), _send_buffer_capacity));
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
/*static*/ EventT Connection::eventEnterPassive(ConnectionEnterPassiveCompleteFunctionT&& _ucomplete_fnc)
{
    return make_event(connection_event_category, ConnectionEvents::EnterPassive, EnterPassive(std::move(_ucomplete_fnc)));
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
/*static*/ EventT Connection::eventStartSecure(ConnectionSecureHandhakeCompleteFunctionT&& _ucomplete_fnc)
{
    return make_event(connection_event_category, ConnectionEvents::StartSecure, StartSecure(std::move(_ucomplete_fnc)));
}
//-----------------------------------------------------------------------------
/*static*/ EventT Connection::eventPost(ConnectionPostCompleteFunctionT&& _ucomplete_fnc)
{
    return make_event(connection_event_category, ConnectionEvents::Post, ConnectionPostCompleteFunctionT(std::move(_ucomplete_fnc)));
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
/*static*/ EventT Connection::eventSendRaw(ConnectionSendRawDataCompleteFunctionT&& _ucomplete_fnc, std::string&& _udata)
{
    return make_event(connection_event_category, ConnectionEvents::SendRaw, SendRaw(std::move(_ucomplete_fnc), std::move(_udata)));
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
/*static*/ EventT Connection::eventRecvRaw(ConnectionRecvRawDataCompleteFunctionT&& _ucomplete_fnc)
{
    return make_event(connection_event_category, ConnectionEvents::RecvRaw, RecvRaw(std::move(_ucomplete_fnc)));
}
//-----------------------------------------------------------------------------
inline void Connection::doOptimizeRecvBuffer()
{
    const size_t remaining_size = recv_buf_.size() - cons_buf_off_;
    if (remaining_size <= cons_buf_off_) {
        memcpy(recv_buf_.data(), recv_buf_.data() + cons_buf_off_, remaining_size);
        cons_buf_off_ = 0;
        recv_buf_.resize(remaining_size);
    }
}
//-----------------------------------------------------------------------------
inline void Connection::doOptimizeRecvBufferForced()
{
    const size_t remaining_size = recv_buf_.size() - cons_buf_off_;

    memmove(recv_buf_.data(), recv_buf_.data() + cons_buf_off_, remaining_size);
    cons_buf_off_ = 0;
    recv_buf_.resize(remaining_size);
}
//-----------------------------------------------------------------------------
Connection::Connection(
    Configuration const&    _rconfiguration,
    ConnectionPoolId const& _rpool_id,
    const std::string&      _rpool_name)
    : pool_id_(_rpool_id)
    , rpool_name_(_rpool_name)
    , timer_(this->proxy())
    , send_relay_free_count_(static_cast<uint8_t>(_rconfiguration.connection_relay_buffer_count))
    , sock_ptr_(_rconfiguration.client.connection_create_socket_fnc(_rconfiguration, this->proxy(), this->socket_emplace_buf_))
{
    solid_log(logger, Info, this);
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
    , send_relay_free_count_(static_cast<uint8_t>(_rconfiguration.connection_relay_buffer_count))
    , sock_ptr_(_rconfiguration.server.connection_create_socket_fnc(_rconfiguration, this->proxy(), std::move(_rsd), this->socket_emplace_buf_))
{
    solid_log(logger, Info, this << " (" << local_endpoint(sock_ptr_->device()) << ") -> (" << remote_endpoint(sock_ptr_->device()) << ')');
}
//-----------------------------------------------------------------------------
Connection::~Connection()
{
    solid_log(logger, Info, this);
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
    solid_log(logger, Verbose, this << " enqueue message " << _rpool_msg_id << " to connection " << this << " retval = " << success);
    return success;
}
//-----------------------------------------------------------------------------
void Connection::doPrepare(frame::aio::ReactorContext& _rctx)
{
    Configuration const& config = service(_rctx).configuration();

    recv_buf_       = service(_rctx).configuration().allocateRecvBuffer();
    send_buf_       = service(_rctx).configuration().allocateSendBuffer();
    recv_buf_count_ = 1;
    msg_reader_.prepare(service(_rctx).configuration().reader);
    msg_writer_.prepare(service(_rctx).configuration().writer);
    const auto crt_time      = _rctx.steadyTime();
    recv_keepalive_boundary_ = crt_time + config.server.connection_inactivity_keepalive_interval;
}
//-----------------------------------------------------------------------------
void Connection::doUnprepare(frame::aio::ReactorContext& /*_rctx*/)
{
    solid_log(logger, Verbose, this << ' ' << this->id());
    msg_reader_.unprepare();
    msg_writer_.unprepare();
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doStart(frame::aio::ReactorContext& _rctx, const bool _is_incoming)
{

    ConnectionContext    conctx(_rctx, service(_rctx), *this);
    Configuration const& config = service(_rctx).configuration();

    doPrepare(_rctx);
    prepareSocket(_rctx);

    const ConnectionState start_state  = _is_incoming ? config.server.connection_start_state : config.client.connection_start_state;
    const bool            start_secure = _is_incoming ? config.server.connection_start_secure : config.client.connection_start_secure;

    if (_is_incoming) {
        flags_.set(FlagsE::Server);
        if (!start_secure) {
            service(_rctx).onIncomingConnectionStart(conctx);
            timeout_secure_ = NanoTime::max();
        } else {
            if (config.server.hasConnectionTimeoutSecure()) {
                timeout_secure_ = _rctx.nanoTime() + config.server.connection_timeout_secure;
            } else {
                timeout_secure_ = NanoTime::max();
            }
        }
        if (start_state != ConnectionState::Active && config.server.hasConnectionTimeoutActive()) {
            timeout_active_ = _rctx.nanoTime() + config.server.connection_timeout_active;
        } else {
            timeout_active_ = NanoTime::max();
        }
        doResetTimer<Ctx>(_rctx);
    } else {
        flags_.set(FlagsE::Connected);
        config.client.socket_device_setup_fnc(sock_ptr_->device());
        if (!start_secure) {
            service(_rctx).onOutgoingConnectionStart(conctx);
        }
    }

    doResetTimer<Ctx>(_rctx);

    switch (start_state) {
    case ConnectionState::Raw:
        if (start_secure) {
            this->post(
                _rctx,
                [this](frame::aio::ReactorContext& _rctx, EventBase&& _revent) { this->onEvent(_rctx, std::move(_revent)); },
                make_event(connection_event_category, ConnectionEvents::StartSecure));
        } else {
            flags_.set(FlagsE::Raw);
        }
        break;
    case ConnectionState::Active:
        if (start_secure) {
            this->post(
                _rctx,
                [this](frame::aio::ReactorContext& _rctx, EventBase&& _revent) { this->onEvent(_rctx, std::move(_revent)); },
                make_event(connection_event_category, ConnectionEvents::StartSecure));
        } else {
            this->post(
                _rctx,
                [this](frame::aio::ReactorContext& _rctx, EventBase&& _revent) { this->onEvent(_rctx, std::move(_revent)); },
                make_event(connection_event_category, ConnectionEvents::EnterActive));
        }
        break;
    case ConnectionState::Passive:
    default:
        flags_.set(FlagsE::Raw);
        if (start_secure) {
            this->post(
                _rctx,
                [this](frame::aio::ReactorContext& _rctx, EventBase&& _revent) { this->onEvent(_rctx, std::move(_revent)); },
                make_event(connection_event_category, ConnectionEvents::StartSecure));
        } else {
            this->post(
                _rctx,
                [this](frame::aio::ReactorContext& _rctx, EventBase&& _revent) { this->onEvent(_rctx, std::move(_revent)); },
                make_event(connection_event_category, ConnectionEvents::EnterPassive));
        }
        break;
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doStop(frame::aio::ReactorContext& _rctx, const ErrorConditionT& _rerr, const ErrorCodeT& _rsyserr)
{

    if (!isStopping()) {

        error_     = _rerr;
        sys_error_ = _rsyserr;

        solid_log(logger, Info, this << ' ' << this->id() << " [" << error_.message() << "][" << sys_error_.message() << "]");

        flags_.set(FlagsE::Stopping);

        ConnectionContext         conctx(_rctx, service(_rctx), *this);
        ErrorConditionT           tmp_error(error());
        ActorIdT                  actuid(uid(_rctx));
        std::chrono::milliseconds wait_duration = std::chrono::milliseconds(0);
        MessageBundle             msg_bundle;
        MessageId                 pool_msg_id;
        EventT                    event;
        const bool                has_no_message = pending_message_vec_.empty() && msg_writer_.isEmpty();
        const bool                can_stop       = service(_rctx).connectionStopping(conctx, actuid, wait_duration, pool_msg_id, has_no_message ? &msg_bundle : nullptr, event, tmp_error);

        if (can_stop) {
            solid_assert_log(has_no_message, logger);
            solid_log(logger, Info, this << ' ' << this->id() << " postStop");

            this->relay_id_.clear(); // the connection was unregistered from RelayEngine on Service::connectionStopping

            auto lambda = [msg_b = std::move(msg_bundle), pool_msg_id](frame::aio::ReactorContext& _rctx, EventBase&& /*_revent*/) mutable {
                Connection& rthis = static_cast<Connection&>(_rctx.actor());
                rthis.onStopped(_rctx, pool_msg_id, msg_b);
            };
            // Can stop rightaway - we will handle the last message
            // from service on onStopped method.
            // There might be events pending which will be delivered, but after this call
            postStop(_rctx, std::move(lambda));
            // no event get posted
        } else if (has_no_message) {
            // try handle the returned message if any
            if (msg_bundle.message_ptr || !solid_function_empty(msg_bundle.complete_fnc)) {
                doCompleteMessage(_rctx, pool_msg_id, msg_bundle, error_message_connection);
            }
            solid_log(logger, Info, this << ' ' << this->id() << " wait " << wait_duration.count());

            if (wait_duration != std::chrono::milliseconds(0)) {
                solid_log(logger, Info, this << ' ' << this->id() << " wait for " << wait_duration.count());
                timer_.waitFor(_rctx,
                    wait_duration,
                    [event = std::move(event)](frame::aio::ReactorContext& _rctx) {
                        Connection& rthis = static_cast<Connection&>(_rctx.actor());
                        rthis.doContinueStopping(_rctx, event);
                    });
            } else {
                post(
                    _rctx,
                    [](frame::aio::ReactorContext& _rctx, EventBase&& _revent) {
                        Connection& rthis = static_cast<Connection&>(_rctx.actor());
                        rthis.doContinueStopping(_rctx, _revent);
                    },
                    std::move(event));
            }

        } else {
            // we have initiated the stopping process but we cannot finish it right away because
            // we have some local messages to handle
            solid_assert_log(event.empty(), logger);
            size_t offset = 0;
            post(_rctx,
                [offset](frame::aio::ReactorContext& _rctx, EventBase&& _revent) {
                    Connection& rthis = static_cast<Connection&>(_rctx.actor());
                    rthis.doCompleteAllMessages<Ctx>(_rctx, offset);
                });
        }
    } // if(!isStopping())
}
//-----------------------------------------------------------------------------
void Connection::doContinueStopping(
    frame::aio::ReactorContext& _rctx,
    const EventT&               _revent)
{

    ErrorConditionT           tmp_error(error());
    ActorIdT                  actuid(uid(_rctx));
    std::chrono::milliseconds wait_duration = std::chrono::milliseconds(0);
    MessageBundle             msg_bundle;
    MessageId                 pool_msg_id;
    EventT                    event(_revent);
    ConnectionContext         conctx(_rctx, service(_rctx), *this);
    const bool                can_stop = service(_rctx).connectionStopping(conctx, actuid, wait_duration, pool_msg_id, &msg_bundle, event, tmp_error);

    solid_log(logger, Info, this << ' ' << this->id() << ' ' << can_stop);

    if (can_stop) {
        // can stop rightaway
        solid_log(logger, Info, this << ' ' << this->id() << " postStop");

        this->relay_id_.clear(); // the connection was unregistered from RelayEngine on Service::connectionStopping

        auto lambda = [msg_bundle = std::move(msg_bundle), pool_msg_id](frame::aio::ReactorContext& _rctx, EventBase&& /*_revent*/) mutable {
            Connection& rthis = static_cast<Connection&>(_rctx.actor());
            rthis.onStopped(_rctx, pool_msg_id, msg_bundle);
        };
        // Can stop rightaway - we will handle the last message
        // from service on onStopped method.
        // There might be events pending which will be delivered, but after this call
        postStop(_rctx, std::move(lambda));
        // no event get posted
    } else {
        if (msg_bundle.message_ptr || !solid_function_empty(msg_bundle.complete_fnc)) {
            doCompleteMessage(_rctx, pool_msg_id, msg_bundle, error_message_connection);
        }
        if (wait_duration != std::chrono::milliseconds(0)) {
            solid_log(logger, Info, this << ' ' << this->id() << " wait for " << wait_duration.count());
            timer_.waitFor(_rctx,
                wait_duration,
                [_revent](frame::aio::ReactorContext& _rctx) {
                    Connection& rthis = static_cast<Connection&>(_rctx.actor());
                    rthis.doContinueStopping(_rctx, _revent);
                });
        } else {
            post(
                _rctx,
                [](frame::aio::ReactorContext& _rctx, EventBase&& _revent) {
                    Connection& rthis = static_cast<Connection&>(_rctx.actor());
                    rthis.doContinueStopping(_rctx, _revent);
                },
                std::move(event));
        }
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
struct ConnectionSender : MessageWriterSender {
    Connection&                 rcon_;
    frame::aio::ReactorContext& rctx_;
    ErrorConditionT             err_;

    ConnectionSender(
        Connection&                 _rcon,
        frame::aio::ReactorContext& _rctx,
        WriterConfiguration const&  _rconfig,
        Protocol const&             _rproto,
        ConnectionContext&          _rconctx,
        const ErrorConditionT&      _rerr = ErrorConditionT{})
        : MessageWriterSender(_rconfig, _rproto, _rconctx)
        , rcon_(_rcon)
        , rctx_(_rctx)
        , err_(_rerr)
    {
    }

    ErrorConditionT completeMessage(MessageBundle& _rmsg_bundle, MessageId const& _rpool_msg_id) override
    {
        rcon_.doCompleteMessage(rctx_, _rpool_msg_id, _rmsg_bundle, err_);
        if (_rpool_msg_id.isValid()) {
            return rcon_.pollServicePoolForUpdates(rctx_, _rpool_msg_id);
        } else {
            return ErrorConditionT{};
        }
    }

    bool cancelMessage(MessageBundle& _rmsg_bundle, MessageId const& _rpool_msg_id) override
    {
        rcon_.doCompleteMessage(rctx_, _rpool_msg_id, _rmsg_bundle, err_);
        return true;
    }
};

template <class Ctx>
void Connection::doCompleteAllMessages(
    frame::aio::ReactorContext& _rctx,
    size_t                      _offset)
{

    bool has_any_message = !msg_writer_.isEmpty();

    if (has_any_message) {
        solid_log(logger, Info, this);
        // complete msg_writer messages
        ConnectionContext     conctx(_rctx, service(_rctx), *this);
        Configuration const&  rconfig = service(_rctx).configuration();
        typename Ctx::SenderT sender(*this, _rctx, rconfig.writer, rconfig.protocol(), conctx, error_message_connection);

        msg_writer_.cancelOldest(sender);

        has_any_message = (!msg_writer_.isEmpty()) || (!pending_message_vec_.empty());

    } else if (_offset < pending_message_vec_.size()) {
        solid_log(logger, Info, this);
        // complete pending messages
        MessageBundle msg_bundle;

        if (service(_rctx).fetchCanceledMessage(*this, pending_message_vec_[_offset], msg_bundle)) {
            doCompleteMessage(_rctx, pending_message_vec_[_offset], msg_bundle, error_message_connection);
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
        solid_log(logger, Info, this);
        post(
            _rctx,
            [_offset](frame::aio::ReactorContext& _rctx, EventBase&& _revent) {
                solid_assert_log(_revent.empty(), logger);
                Connection& rthis = static_cast<Connection&>(_rctx.actor());
                rthis.doCompleteAllMessages<Ctx>(_rctx, _offset);
            });
    } else {
        solid_log(logger, Info, this);
        post(_rctx,
            [](frame::aio::ReactorContext& _rctx, EventBase&& _revent) {
                Connection& rthis = static_cast<Connection&>(_rctx.actor());
                rthis.doContinueStopping(_rctx, _revent);
            });
    }
}
//-----------------------------------------------------------------------------
void Connection::onStopped(
    frame::aio::ReactorContext& _rctx,
    MessageId const&            _rpool_msg_id,
    MessageBundle&              _rmsg_bundle)
{

    ActorIdT          actuid(uid(_rctx));
    ConnectionContext conctx(_rctx, service(_rctx), *this);

    service(_rctx).connectionStop(conctx);

    if (_rmsg_bundle.message_ptr || !solid_function_empty(_rmsg_bundle.complete_fnc)) {
        doCompleteMessage(_rctx, _rpool_msg_id, _rmsg_bundle, error_message_connection);
    }

    doUnprepare(_rctx);
    (void)actuid;
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventGeneric(frame::aio::ReactorContext& _rctx, EventBase& _revent)
{
    ConnectionContext conctx(_rctx, service(_rctx), *this);
    configuration(_rctx).connection_on_event_fnc(conctx, _revent);
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventStopping(frame::aio::ReactorContext& _rctx, EventBase& _revent)
{
    solid_log(logger, Info, this << ' ' << id() << " cancel timer");
    // NOTE: we're trying this way to preserve the
    // error condition (for which the connection is stopping)
    // held by the timer callback
    timer_.cancel(_rctx);
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doHandleEventStart(
    frame::aio::ReactorContext& _rctx,
    EventBase& /*_revent*/
)
{

    const bool has_valid_socket = sock_ptr_->hasValidSocket();

    solid_log(logger, Info, this << ' ' << this->id() << " Session start: " << (has_valid_socket ? " connected " : "not connected"));

    if (has_valid_socket) {
        doStart<Ctx>(_rctx, true);
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doHandleEventKill(
    frame::aio::ReactorContext& _rctx,
    EventBase& /*_revent*/
)
{
    doStop<Ctx>(_rctx, error_connection_killed);
}
//-----------------------------------------------------------------------------
bool Connection::willAcceptNewMessage(frame::aio::ReactorContext& /*_rctx*/) const
{
    return !isStopping() /* && waiting_message_vec.empty() && msg_writer.willAcceptNewMessage(service(_rctx).configuration().writer)*/;
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doHandleEventNewPoolQueueMessage(frame::aio::ReactorContext& _rctx, EventBase& _revent)
{

    flags_.reset(FlagsE::InPoolWaitQueue); // reset flag

    doHandleEventNewPoolMessage<Ctx>(_rctx, _revent);
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doHandleEventNewPoolMessage(frame::aio::ReactorContext& _rctx, EventBase& /*_revent*/)
{

    if (willAcceptNewMessage(_rctx)) {
        solid_statistic_inc(service(_rctx).wstatistic().connection_new_pool_message_count_);
        flags_.set(FlagsE::PollPool);
        poll_pool_more_ = true;
        if (!send_posted_) {
            doSend<Ctx>(_rctx);
        }
    } else {
        service(_rctx).rejectNewPoolMessage(*this);
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doHandleEventNewConnMessage(frame::aio::ReactorContext& _rctx, EventBase& _revent)
{

    MessageId* pmsgid = _revent.cast<MessageId>();
    solid_assert_log(pmsgid, logger);

    if (pmsgid != nullptr) {

        if (!this->isStopping()) {
            pending_message_vec_.push_back(*pmsgid);
            if (!this->isRawState()) {
                flags_.set(FlagsE::PollPool);
                doSend<Ctx>(_rctx);
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
template <class Ctx>
void Connection::doHandleEventCancelConnMessage(frame::aio::ReactorContext& _rctx, EventBase& _revent)
{

    MessageId* pmsgid = _revent.cast<MessageId>();
    solid_assert_log(pmsgid, logger);

    if (pmsgid != nullptr) {
        ConnectionContext     conctx(_rctx, service(_rctx), *this);
        Configuration const&  rconfig = service(_rctx).configuration();
        typename Ctx::SenderT sender(*this, _rctx, rconfig.writer, rconfig.protocol(), conctx, error_message_canceled);
        msg_writer_.cancel(*pmsgid, sender);
    }
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventCancelPoolMessage(frame::aio::ReactorContext& _rctx, EventBase& _revent)
{

    MessageId* pmsgid = _revent.cast<MessageId>();
    solid_assert_log(pmsgid, logger);

    if (pmsgid != nullptr) {
        MessageBundle msg_bundle;

        if (service(_rctx).fetchCanceledMessage(*this, *pmsgid, msg_bundle)) {
            doCompleteMessage(_rctx, *pmsgid, msg_bundle, error_message_canceled);
        }
    }
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventClosePoolMessage(frame::aio::ReactorContext& _rctx, EventBase& _revent)
{

    MessageId* pmsgid = _revent.cast<MessageId>();
    solid_assert_log(pmsgid, logger);

    if (pmsgid != nullptr) {
        MessageBundle msg_bundle;

        if (service(_rctx).fetchCanceledMessage(*this, *pmsgid, msg_bundle)) {
            doCompleteMessage(_rctx, *pmsgid, msg_bundle, error_message_connection);
        }
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doHandleEventEnterActive(frame::aio::ReactorContext& _rctx, EventBase& _revent)
{

    solid_log(logger, Info, this);

    ConnectionContext    conctx(_rctx, service(_rctx), *this);
    Configuration const& rconfig = service(_rctx).configuration();
    EnterActive*         pdata   = _revent.cast<EnterActive>();

    timeout_active_ = NanoTime::max();

    if (!isStopping()) {

        ErrorConditionT error = service(_rctx).activateConnection(conctx, uid(_rctx));

        if (!error) {
            flags_.set(FlagsE::Active);

            if (pdata != nullptr) {
                pdata->complete_fnc(conctx, error);
            }

            if (!isServer()) {
                // poll pool only for clients
                flags_.set(FlagsE::PollPool);
            }

            doSend<Ctx>(_rctx);

            if (!isStopping()) {
                // start receiving messages
                this->postRecvSome<Ctx>(_rctx, recv_buf_.data(), recv_buf_.capacity());

                if (rconfig.hasConnectionTimeoutRecv()) {
                    timeout_recv_ = _rctx.nanoTime() + rconfig.connection_timeout_recv;
                    solid_log(logger, Verbose, this << " timeout_recv = " << timeout_recv_);
                } else {
                    solid_log(logger, Verbose, this << " timeout_recv = " << timeout_recv_);
                    timeout_recv_ = NanoTime::max();
                }
                if (!isServer()) {
                    if (rconfig.client.hasConnectionTimeoutKeepAlive()) {
                        timeout_keepalive_ = _rctx.nanoTime() + rconfig.client.connection_timeout_keepalive;
                        solid_log(logger, Verbose, this << " timeout_keepalive = " << timeout_keepalive_);
                    } else {
                        timeout_keepalive_ = NanoTime::max();
                        solid_log(logger, Verbose, this << " timeout_keepalive = " << timeout_keepalive_);
                    }
                }
                doResetTimer<Ctx>(_rctx);
            }
        } else {

            if (pdata != nullptr) {
                pdata->complete_fnc(conctx, error);
            }

            doStop<Ctx>(_rctx, error);
        }
    } else if (pdata != nullptr) {
        pdata->complete_fnc(conctx, error_connection_stopping);
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doHandleEventEnterPassive(frame::aio::ReactorContext& _rctx, EventBase& _revent)
{
    Configuration const& config = service(_rctx).configuration();
    EnterPassive*        pdata  = _revent.cast<EnterPassive>();
    ConnectionContext    conctx(_rctx, service(_rctx), *this);
    if (!isStopping()) {
        if (this->isRawState()) {
            flags_.reset(FlagsE::Raw);

            if (pdata != nullptr) {
                pdata->complete_fnc(conctx, ErrorConditionT());
            }

            if (!pending_message_vec_.empty()) {
                flags_.set(FlagsE::PollPool);
                doSend<Ctx>(_rctx);
            }
        } else if (pdata != nullptr) {
            pdata->complete_fnc(conctx, error_connection_invalid_state);
        }

        this->post(
            _rctx,
            [this](frame::aio::ReactorContext& _rctx, EventBase&& /*_revent*/) {
                doSend<Ctx>(_rctx);
            });

        // start receiving messages
        this->postRecvSome<Ctx>(_rctx, recv_buf_.data(), recv_buf_.capacity());

        if (isServer() && config.server.hasConnectionTimeoutActive()) {
            timeout_active_ = _rctx.nanoTime() + config.server.connection_timeout_active;
        } else {
            timeout_active_ = NanoTime::max();
        }
        doResetTimer<Ctx>(_rctx);
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doHandleEventStartSecure(frame::aio::ReactorContext& _rctx, EventBase& /*_revent*/)
{
    solid_log(logger, Verbose, this << "");
    Configuration const& config = service(_rctx).configuration();
    ConnectionContext    conctx(_rctx, service(_rctx), *this);
    if (!isStopping()) {
        if ((isServer() && config.server.hasSecureConfiguration()) || ((!isServer()) && config.client.hasSecureConfiguration())) {
            ErrorConditionT error;
            bool            done = false;
            solid_log(logger, Verbose, this << "");

            if (isServer()) {
                done = sock_ptr_->secureAccept(_rctx, conctx, onSecureAccept<Ctx>, error);
                if (done && !error && !_rctx.error()) {
                    onSecureAccept<Ctx>(_rctx);
                }
            } else {
                done = sock_ptr_->secureConnect(_rctx, conctx, onSecureConnect<Ctx>, error);
                if (done && !error && !_rctx.error()) {
                    onSecureConnect<Ctx>(_rctx);
                }
            }

            if (_rctx.error()) {
                solid_log(logger, Error, this << " error = [" << error.message() << "]");
                doStop<Ctx>(_rctx, _rctx.error(), _rctx.systemError());
            }

            if (error) {
                solid_log(logger, Error, this << " error = [" << error.message() << "]");
                doStop<Ctx>(_rctx, error);
            }
        } else {
            solid_log(logger, Verbose, this << "");
            doStop<Ctx>(_rctx, error_connection_no_secure_configuration);
        }
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
/*static*/ void Connection::onSecureConnect(frame::aio::ReactorContext& _rctx)
{
    Connection&          rthis = static_cast<Connection&>(_rctx.actor());
    ConnectionContext    conctx(_rctx, rthis.service(_rctx), rthis);
    Configuration const& config = rthis.service(_rctx).configuration();

    if (_rctx.error()) {
        solid_log(logger, Error, &rthis << " error = [" << _rctx.error().message() << "] = systemError = [" << _rctx.systemError().message() << "]");
        rthis.doStop<Ctx>(_rctx, _rctx.error(), _rctx.systemError());
        return;
    }

    if (!solid_function_empty(config.client.connection_on_secure_handshake_fnc)) {
        config.client.connection_on_secure_handshake_fnc(conctx);
    }

    if (!config.client.connection_start_secure) {
        solid_log(logger, Verbose, &rthis << "");
        // we need the connection_on_secure_connect_fnc for advancing.
        if (solid_function_empty(config.client.connection_on_secure_handshake_fnc)) {
            rthis.doStop<Ctx>(_rctx, error_connection_invalid_state); // TODO: add new error
        }
    } else {
        rthis.flags_.set(FlagsE::Secure);
        rthis.service(_rctx).onOutgoingConnectionStart(conctx);

        solid_log(logger, Verbose, &rthis << "");
        // we continue the connection start process entering the right state
        switch (config.client.connection_start_state) {
        case ConnectionState::Raw:
            break;
        case ConnectionState::Active:
            if (!rthis.isActiveState()) {
                rthis.post(
                    _rctx,
                    [&rthis](frame::aio::ReactorContext& _rctx, EventBase&& _revent) { rthis.onEvent(_rctx, std::move(_revent)); },
                    make_event(connection_event_category, ConnectionEvents::EnterActive));
            }
            break;
        case ConnectionState::Passive:
        default:
            if (!rthis.isActiveState() && rthis.isRawState()) {
                rthis.post(
                    _rctx,
                    [&rthis](frame::aio::ReactorContext& _rctx, EventBase&& _revent) { rthis.onEvent(_rctx, std::move(_revent)); },
                    make_event(connection_event_category, ConnectionEvents::EnterPassive));
            }
            break;
        }
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
/*static*/ void Connection::onSecureAccept(frame::aio::ReactorContext& _rctx)
{
    Connection&          rthis = static_cast<Connection&>(_rctx.actor());
    ConnectionContext    conctx(_rctx, rthis.service(_rctx), rthis);
    Configuration const& config = rthis.service(_rctx).configuration();

    if (_rctx.error()) {
        solid_log(logger, Error, &rthis << " error = [" << _rctx.error().message() << "] = systemError = [" << _rctx.systemError().message() << "]");
        rthis.doStop<Ctx>(_rctx, _rctx.error(), _rctx.systemError());
        return;
    }

    if (!solid_function_empty(config.server.connection_on_secure_handshake_fnc)) {
        config.server.connection_on_secure_handshake_fnc(conctx);
    }

    if (!config.server.connection_start_secure) {
        solid_log(logger, Verbose, &rthis << "");
        // we need the connection_on_secure_accept_fnc for advancing.
        if (solid_function_empty(config.server.connection_on_secure_handshake_fnc)) {
            rthis.doStop<Ctx>(_rctx, error_connection_invalid_state); // TODO: add new error
        }
    } else {
        rthis.flags_.set(FlagsE::Secure);
        rthis.timeout_secure_ = NanoTime::max();
        rthis.doResetTimer<Ctx>(_rctx);
        rthis.service(_rctx).onIncomingConnectionStart(conctx);

        solid_log(logger, Verbose, &rthis << "");
        // we continue the connection start process entering the right state
        switch (config.server.connection_start_state) {
        case ConnectionState::Raw:
            break;
        case ConnectionState::Active:
            if (!rthis.isActiveState()) {
                rthis.post(
                    _rctx,
                    [&rthis](frame::aio::ReactorContext& _rctx, EventBase&& _revent) { rthis.onEvent(_rctx, std::move(_revent)); },
                    make_event(connection_event_category, ConnectionEvents::EnterActive));
            }
            break;
        case ConnectionState::Passive:
        default:
            if (!rthis.isActiveState() && rthis.isRawState()) {
                rthis.post(
                    _rctx,
                    [&rthis](frame::aio::ReactorContext& _rctx, EventBase&& _revent) { rthis.onEvent(_rctx, std::move(_revent)); },
                    make_event(connection_event_category, ConnectionEvents::EnterPassive));
            }
            break;
        }
    }
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventSendRaw(frame::aio::ReactorContext& _rctx, EventBase& _revent)
{
    SendRaw*          pdata = _revent.cast<SendRaw>();
    ConnectionContext conctx(_rctx, service(_rctx), *this);

    solid_assert_log(pdata, logger);

    if (this->isRawState() && pdata != nullptr) {

        solid_log(logger, Info, this << " datasize = " << pdata->data.size());

        size_t tocopy = send_buf_.capacity();

        if (tocopy > pdata->data.size()) {
            tocopy = pdata->data.size();
        }

        memcpy(send_buf_.data(), pdata->data.data(), tocopy);

        if (tocopy != 0u) {
            pdata->offset = tocopy;
            if (this->postSendAll(_rctx, send_buf_.data(), tocopy, _revent)) {
                pdata->complete_fnc(conctx, _rctx.error());
            }
        } else {
            pdata->complete_fnc(conctx, ErrorConditionT());
        }

    } else if (pdata != nullptr) {
        pdata->complete_fnc(conctx, error_connection_invalid_state);
    }
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventRecvRaw(frame::aio::ReactorContext& _rctx, EventBase& _revent)
{
    RecvRaw*          pdata = _revent.cast<RecvRaw>();
    ConnectionContext conctx(_rctx, service(_rctx), *this);
    size_t            used_size = 0;

    solid_assert_log(pdata, logger);

    solid_log(logger, Info, this);

    if (this->isRawState() && pdata != nullptr) {
        if (recv_buf_.size() == cons_buf_off_) {
            if (this->postRecvSome(_rctx, recv_buf_.data(), recv_buf_.capacity(), _revent)) {

                pdata->complete_fnc(conctx, nullptr, used_size, _rctx.error());
            }
        } else {
            used_size = recv_buf_.size() - cons_buf_off_;

            pdata->complete_fnc(conctx, recv_buf_.data() + cons_buf_off_, used_size, _rctx.error());

            if (used_size > (recv_buf_.size() - cons_buf_off_)) {
                used_size = (recv_buf_.size() - cons_buf_off_);
            }

            cons_buf_off_ += used_size;

            if (cons_buf_off_ == recv_buf_.size()) {
                cons_buf_off_ = 0;
                recv_buf_.resize(0);
            } else {
                solid_assert_log(cons_buf_off_ < recv_buf_.size(), logger);
            }
        }
    } else if (pdata != nullptr) {

        pdata->complete_fnc(conctx, nullptr, used_size, error_connection_invalid_state);
    }
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventPost(frame::aio::ReactorContext& _rctx, EventBase& _revent)
{
    ConnectionContext                conctx(_rctx, service(_rctx), *this);
    ConnectionPostCompleteFunctionT* pdata = _revent.cast<ConnectionPostCompleteFunctionT>();
    solid_check(pdata != nullptr);
    (*pdata)(conctx);
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doResetTimer(frame::aio::ReactorContext& _rctx)
{
    const auto& min_timeout = minTimeout();
    timer_.waitUntil(_rctx, min_timeout, onTimer<Ctx>);
}
//-----------------------------------------------------------------------------
template <class Ctx>
/*static*/ void Connection::onTimer(frame::aio::ReactorContext& _rctx)
{
    Connection&          rthis    = static_cast<Connection&>(_rctx.actor());
    Configuration const& rconfig  = rthis.service(_rctx).configuration();
    const auto&          crt_time = _rctx.nanoTime();

    if (crt_time >= rthis.timeout_recv_) {
        solid_log(logger, Info, &rthis << " " << rthis.flags_.toString() << " recv timeout = " << rthis.timeout_recv_ << " crt_time = " << crt_time);
        rthis.timeout_recv_ = NanoTime::max();
        rthis.doStop<Ctx>(_rctx, error_connection_inactivity_timeout);
        return;
    }

    if (crt_time >= rthis.timeout_send_hard_) {
        solid_log(logger, Info, &rthis << " " << rthis.flags_.toString() << " send hard timeout = " << rthis.timeout_send_hard_ << " crt_time = " << crt_time);
        rthis.timeout_send_hard_ = NanoTime::max();
        rthis.doStop<Ctx>(_rctx, error_connection_inactivity_timeout);
        return;
    }

    if (crt_time >= rthis.timeout_secure_) {
        solid_log(logger, Info, &rthis << " " << rthis.flags_.toString() << " secure timeout = " << rthis.timeout_secure_ << " crt_time = " << crt_time);
        solid_assert(rthis.isServer());
        rthis.timeout_secure_ = NanoTime::max();
        if (!rthis.isSecured()) {
            rthis.doStop<Ctx>(_rctx, error_connection_inactivity_timeout);
            return;
        }
    }

    if (crt_time >= rthis.timeout_active_) {
        solid_log(logger, Info, &rthis << " " << rthis.flags_.toString() << " active timeout = " << rthis.timeout_active_ << " crt_time = " << crt_time);
        solid_assert(rthis.isServer());
        rthis.timeout_active_ = NanoTime::max();
        if (!rthis.isActiveState()) {
            rthis.doStop<Ctx>(_rctx, error_connection_inactivity_timeout);
            return;
        }
    }

    if (crt_time >= rthis.timeout_send_soft_) {
        solid_log(logger, Info, &rthis << " " << rthis.flags_.toString() << " send soft timeout = " << rthis.timeout_send_soft_ << " crt_time = " << crt_time);
        ConnectionContext conctx(_rctx, rthis.service(_rctx), rthis);
        rthis.timeout_send_soft_ = NanoTime::max();
        rconfig.connection_on_send_timeout_soft_(conctx);
    }

    if (crt_time >= rthis.timeout_keepalive_) {
        solid_log(logger, Info, &rthis << " " << rthis.flags_.toString() << " keep alive timeout = " << rthis.timeout_keepalive_ << " crt_time = " << crt_time);
        solid_assert(!rthis.isServer());
        rthis.flags_.set(FlagsE::Keepalive);
        rthis.timeout_keepalive_ = NanoTime::max();
        rthis.post(_rctx, [&rthis](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) { rthis.doSend<Ctx>(_rctx); });
    }

    const auto& min_timeout = rthis.minTimeout();
    rthis.timer_.waitUntil(_rctx, min_timeout, onTimer<Ctx>);
}

//-----------------------------------------------------------------------------
/*static*/ void Connection::onSendAllRaw(frame::aio::ReactorContext& _rctx, EventBase& _revent)
{

    Connection&       rthis = static_cast<Connection&>(_rctx.actor());
    SendRaw*          pdata = _revent.cast<SendRaw>();
    ConnectionContext conctx(_rctx, rthis.service(_rctx), rthis);

    solid_assert_log(pdata, logger);

    if (pdata != nullptr) {

        if (!_rctx.error()) {

            size_t tocopy = pdata->data.size() - pdata->offset;

            if (tocopy > rthis.send_buf_.capacity()) {
                tocopy = rthis.send_buf_.capacity();
            }

            memcpy(rthis.send_buf_.data(), pdata->data.data() + pdata->offset, tocopy);

            if (tocopy != 0u) {
                pdata->offset += tocopy;
                if (rthis.postSendAll(_rctx, rthis.send_buf_.data(), tocopy, _revent)) {
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
/*static*/ void Connection::onRecvSomeRaw(frame::aio::ReactorContext& _rctx, const size_t _sz, EventBase& _revent)
{

    Connection&       rthis = static_cast<Connection&>(_rctx.actor());
    RecvRaw*          pdata = _revent.cast<RecvRaw>();
    ConnectionContext conctx(_rctx, rthis.service(_rctx), rthis);

    solid_assert_log(pdata, logger);

    if (pdata != nullptr) {

        size_t used_size = _sz;

        pdata->complete_fnc(conctx, rthis.recv_buf_.data(), used_size, _rctx.error());

        if (used_size > _sz) {
            used_size = _sz;
        }

        if (used_size < _sz) {
            rthis.recv_buf_.resize(_sz);
            rthis.cons_buf_off_ = used_size;
        }
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doResetRecvBuffer(frame::aio::ReactorContext& _rctx, const uint8_t _request_buffer_ack_count, ErrorConditionT& _rerr)
{
    if (_request_buffer_ack_count == 0) {

    } else if (recv_buf_.useCount() > 1) {
        solid_log(logger, Verbose, this << " buffer used for relay - try replace it. vec_size = " << recv_buf_vec_.size() << " count = " << recv_buf_count_);
        SharedBuffer new_buf;
        if (!recv_buf_vec_.empty()) {
            new_buf = std::move(recv_buf_vec_.back());
            recv_buf_vec_.pop_back();
        } else if (recv_buf_count_ < service(_rctx).configuration().connection_relay_buffer_count) {
            new_buf = service(_rctx).configuration().allocateRecvBuffer();
        } else {
            recv_buf_.reset();
            _rerr         = error_connection_too_many_recv_buffers;
            cons_buf_off_ = 0;
            return;
        }
        const size_t cnssz = recv_buf_.size() - cons_buf_off_;

        memcpy(new_buf.data(), recv_buf_.data() + cons_buf_off_, cnssz);
        cons_buf_off_ = 0;
        new_buf.resize(cnssz);
        recv_buf_ = std::move(new_buf);
    } else {
        // tried to relay received messages/message parts - but all failed
        // so we need to ack the buffer
        solid_assert_log(recv_buf_.useCount(), logger);
        solid_log(logger, Info, this << " send accept for " << (int)_request_buffer_ack_count << " buffers");

        if (ackd_buf_count_ == 0) {
            ackd_buf_count_ += _request_buffer_ack_count;
            this->post(_rctx, [this](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) { this->doSend<Ctx>(_rctx); });
        } else {
            ackd_buf_count_ += _request_buffer_ack_count;
        }
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
struct ConnectionReceiver : MessageReaderReceiver {
    Connection&                 rcon_;
    frame::aio::ReactorContext& rctx_;

    ConnectionReceiver(
        Connection&                 _rcon,
        frame::aio::ReactorContext& _rctx,
        ReaderConfiguration const&  _rconfig,
        Protocol const&             _rproto,
        ConnectionContext&          _rconctx,
        const bool                  _is_relay_enabled = false)
        : MessageReaderReceiver(_rconfig, _rproto, _rconctx, _is_relay_enabled)
        , rcon_(_rcon)
        , rctx_(_rctx)
    {
    }

    void receiveMessage(MessagePointerT<>& _rmsg_ptr, const size_t _msg_type_id) override
    {
        rcon_.doCompleteMessage<Ctx>(rctx_, _rmsg_ptr, _msg_type_id);
        rcon_.flags_.set(Connection::FlagsE::PollPool);
        if (!rcon_.send_posted_) {
            rcon_.send_posted_ = true;
            rcon_.post(
                rctx_,
                [](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) {
                    Connection& rthis  = static_cast<Connection&>(_rctx.actor());
                    rthis.send_posted_ = false;
                    rthis.doSend<Ctx>(_rctx);
                });
        }
    }

    void receiveKeepAlive() override
    {
        rcon_.doCompleteKeepalive<Ctx>(rctx_);
    }

    void receiveAckCount(uint8_t _count) override
    {
        rcon_.doCompleteAckCount<Ctx>(rctx_, _count);
    }
    void receiveCancelRequest(const RequestId& _reqid) override
    {
        rcon_.doCompleteCancelRequest<Ctx>(rctx_, _reqid);
    }

    void pushCancelRequest(const RequestId& _reqid) override
    {
        solid_log(logger, Info, this << " cancel_remote_msg = " << _reqid);
        rcon_.cancel_remote_msg_vec_.push_back(_reqid);
        if (rcon_.cancel_remote_msg_vec_.size() == 1) {
            rcon_.post(
                rctx_,
                [](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) {
                    Connection& rthis = static_cast<Connection&>(_rctx.actor());
                    rthis.doSend<Ctx>(_rctx);
                });
        }
    }

    ResponseStateE checkResponseState(const MessageHeader& _rmsghdr, MessageId& _rrelay_id, const bool _erase_request) const override
    {
        return rcon_.doCheckResponseState<Ctx>(rctx_, _rmsghdr, _rrelay_id, _erase_request);
    }
};

template <class Ctx>
/*static*/ void Connection::onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    Connection&             rthis = static_cast<Connection&>(_rctx.actor());
    ConnectionContext       conctx(_rctx, rthis.service(_rctx), rthis);
    const Configuration&    rconfig   = rthis.service(_rctx).configuration();
    unsigned                repeatcnt = 4;
    char*                   pbuf      = nullptr;
    size_t                  bufsz     = 0;
    const uint32_t          recvbufcp = rthis.recv_buf_.capacity();
    typename Ctx::ReceiverT rcvr(rthis, _rctx, rconfig.reader, rconfig.protocol(), conctx);
    ErrorConditionT         error;

    do {
        solid_log(logger, Verbose, &rthis << " received size " << _sz);
        rthis.service(_rctx).wstatistic().connectionRecvBufferSize(_sz, rthis.recv_buf_.capacity());

        if (!_rctx.error()) {
            rthis.recv_buf_.append(_sz);
            pbuf  = rthis.recv_buf_.data() + rthis.cons_buf_off_;
            bufsz = rthis.recv_buf_.size() - rthis.cons_buf_off_;

            rcvr.request_buffer_ack_count_ = 0;

            rthis.cons_buf_off_ += rthis.msg_reader_.read(pbuf, bufsz, rcvr, error);

            solid_log(logger, Verbose, &rthis << " consumed size " << rthis.cons_buf_off_ << " of " << bufsz);

            rthis.doResetRecvBuffer<Ctx>(_rctx, rcvr.request_buffer_ack_count_, error);

            if (error) {
                solid_log(logger, Error, &rthis << ' ' << rthis.id() << " parsing " << error.message());
                rthis.doStop<Ctx>(_rctx, error);
                return;
            }

            if (rthis.cons_buf_off_ < bufsz) {
                rthis.doOptimizeRecvBufferForced();
            }
        } else {
            solid_log(logger, Info, &rthis << ' ' << rthis.id() << " receiving " << _rctx.error().message());
            rthis.flags_.set(FlagsE::StopPeer);
            rthis.doStop<Ctx>(_rctx, _rctx.error(), _rctx.systemError());
            return;
        }
        --repeatcnt;
        rthis.doOptimizeRecvBuffer();

        solid_assert_log(rthis.recv_buf_, logger);

        pbuf = rthis.recv_buf_.data() + rthis.recv_buf_.size();

        bufsz = recvbufcp - rthis.recv_buf_.size();
        // solid_log(logger, Info, &rthis<<" buffer size "<<bufsz);
    } while (repeatcnt != 0u && !rthis.flags_.isSet(FlagsE::PauseRecv) && rthis.recvSome<Ctx>(_rctx, pbuf, bufsz, _sz));

    if (rconfig.hasConnectionTimeoutRecv()) {
        rthis.timeout_recv_ = _rctx.nanoTime() + rconfig.connection_timeout_recv;
        solid_log(logger, Verbose, &rthis << " timeout_recv = " << rthis.timeout_recv_);
        rthis.doResetTimer<Ctx>(_rctx);
    }

    if (repeatcnt == 0 && !rthis.flags_.isSet(FlagsE::PauseRecv)) {
        const bool rv = rthis.postRecvSome<Ctx>(_rctx, pbuf, bufsz); // fully asynchronous call
        solid_assert_log(!rv, logger);
        (void)rv;
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doSend(frame::aio::ReactorContext& _rctx)
{

    solid_log(logger, Verbose, this << " isstopping = " << this->isStopping());

    solid_statistic_inc(service(_rctx).wstatistic().connection_do_send_count_);

    if (isStopping()) {
        return;
    }
    ErrorConditionT      error;
    const Configuration& rconfig = service(_rctx).configuration();
    ConnectionContext    conctx(_rctx, service(_rctx), *this);
    // we do a pollPoolForUpdates here because we want to be able to
    // receive a force pool close, even though we are waiting for send.

    {
        flags_.reset(FlagsE::PollPool); // reset flag
        error = service(_rctx).pollPoolForUpdates(*this, uid(_rctx), MessageId(), poll_pool_more_);

        if (error) {
            doStop<Ctx>(_rctx, error);
            return;
        }
    }

    Ctx::pollOther(_rctx, rconfig, msg_writer_);

    if (!sock_ptr_->hasPendingSend()) {
        unsigned                   repeatcnt = 1;
        typename Ctx::SenderT      sender(*this, _rctx, rconfig.writer, rconfig.protocol(), conctx);
        MessageWriter::WriteFlagsT write_flags;
        bool                       sent_something = false;

        timeout_send_soft_ = timeout_send_hard_ = NanoTime::max();

        while (repeatcnt != 0u) {
            write_flags.reset();

            if (shouldSendKeepAlive()) {
                write_flags.set(MessageWriter::WriteFlagsE::ShouldSendKeepAlive);
            }

            WriteBuffer buffer{send_buf_.data(), send_buf_.capacity()};

            error = msg_writer_.write(
                buffer, write_flags, ackd_buf_count_, cancel_remote_msg_vec_, send_relay_free_count_, sender);

            flags_.reset(FlagsE::Keepalive);

            if (!error) {
                service(_rctx).wstatistic().connectionSendBufferSize(buffer.size(), send_buf_.capacity());
                if (!buffer.empty() && this->sendAll<Ctx>(_rctx, buffer.data(), buffer.size())) {
                    sent_something = true;
                    if (_rctx.error()) {
                        solid_log(logger, Error, this << ' ' << id() << " sending " << buffer.size() << ": " << _rctx.error().message());
                        flags_.set(FlagsE::StopPeer);
                        doStop<Ctx>(_rctx, _rctx.error(), _rctx.systemError());
                        return;
                    }
                    solid_statistic_inc(service(_rctx).wstatistic().connection_send_done_count_);
                } else if (buffer.empty()) {
                    if (poll_pool_more_ && isServer()) {
                        solid_assert(msg_writer_.isEmpty());
                        repeatcnt = 0;
                        solid_statistic_inc(service(_rctx).wstatistic().connection_send_wait_count_);
                        break;
                    } else {
                        sent_something = true;
                        break;
                    }
                } else {
                    break;
                }
            } else {
                solid_log(logger, Error, this << ' ' << id() << " size to send " << buffer.size() << " error " << error.message());

                if (!buffer.empty()) {
                    this->sendAll<Ctx>(_rctx, buffer.data(), buffer.size());
                }

                doStop<Ctx>(_rctx, error);
                return;
            }
            --repeatcnt;
        } // while

        if (repeatcnt == 0) {
            if (sent_something) {
                if (!isServer()) {
                    if (rconfig.client.hasConnectionTimeoutKeepAlive()) {
                        timeout_keepalive_ = _rctx.nanoTime() + rconfig.client.connection_timeout_keepalive;
                        solid_log(logger, Verbose, this << " timeout_keepalive = " << timeout_keepalive_);
                        doResetTimer<Ctx>(_rctx);
                    }
                }
            }
            if (!send_posted_) {
                solid_statistic_inc(service(_rctx).wstatistic().connection_send_posted_);
                send_posted_ = true;
                this->post(_rctx, [this](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) { send_posted_ = false; doSend<Ctx>(_rctx); });
            }
        } else if (sock_ptr_->hasPendingSend()) {
            // blocking send
            if (rconfig.hasConnectionTimeoutSendSoft()) {
                timeout_send_soft_ = _rctx.nanoTime() + rconfig.connection_timeout_send_soft;
            }
            if (rconfig.hasConnectionTimeoutSendHard()) {
                timeout_send_hard_ = _rctx.nanoTime() + rconfig.connection_timeout_send_hard;
            }
            if (!isServer()) {
                timeout_keepalive_ = NanoTime::max();
                solid_log(logger, Verbose, this << " timeout_keepalive = " << timeout_keepalive_);
            }
            doResetTimer<Ctx>(_rctx);
        }

        if (repeatcnt == 0 && !send_posted_) {
            // solid_log(logger, Info, this<<" post send");
            solid_statistic_inc(service(_rctx).wstatistic().connection_send_posted_);
            send_posted_ = true;
            this->post(_rctx, [this](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) { send_posted_ = false; doSend<Ctx>(_rctx); });
        }
        // solid_log(logger, Info, this<<" done-doSend "<<this->sendmsgvec[0].size()<<" "<<this->sendmsgvec[1].size());

    } else {
        solid_log(logger, Info, this << " hasPendingSend");
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
/*static*/ void Connection::onSend(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());

    if (!_rctx.error()) {
        rthis.timeout_send_soft_ = rthis.timeout_send_hard_ = NanoTime::max();
        if (!rthis.isServer()) {
            const Configuration& rconfig = rthis.service(_rctx).configuration();
            if (rconfig.client.hasConnectionTimeoutKeepAlive()) {
                rthis.timeout_keepalive_ = _rctx.nanoTime() + rconfig.client.connection_timeout_keepalive;
                solid_log(logger, Verbose, &rthis << " timeout_keepalive = " << rthis.timeout_keepalive_);
            }
        }
        rthis.doResetTimer<Ctx>(_rctx);
        rthis.doSend<Ctx>(_rctx);
    } else if (!rthis.isStopping()) {
        solid_log(logger, Error, &rthis << ' ' << rthis.id() << " sending [" << _rctx.error().message() << "][" << _rctx.systemError().message() << ']');

        rthis.timeout_send_soft_ = rthis.timeout_send_hard_ = NanoTime::max();
        rthis.doResetTimer<Ctx>(_rctx);
        rthis.doStop<Ctx>(_rctx, _rctx.error(), _rctx.systemError());
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
/*static*/ void Connection::onConnect(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());

    if (!_rctx.error()) {
        solid_log(logger, Info, &rthis << ' ' << rthis.id() << " (" << local_address(rthis.sock_ptr_->device()) << ") -> (" << remote_address(rthis.sock_ptr_->device()) << ')');
        rthis.doStart<Ctx>(_rctx, false);
    } else {
        solid_log(logger, Error, &rthis << ' ' << rthis.id() << " connecting [" << _rctx.error().message() << "][" << _rctx.systemError().message() << ']');
        rthis.doStop<Ctx>(_rctx, _rctx.error(), _rctx.systemError());
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
struct ConnectionSenderResponse : ConnectionSender<Ctx> {
    MessagePointerT<>& rresponse_ptr_;
    bool               request_found_ = false;

    ConnectionSenderResponse(
        Connection&                 _rcon,
        frame::aio::ReactorContext& _rctx,
        WriterConfiguration const&  _rconfig,
        Protocol const&             _rproto,
        ConnectionContext&          _rconctx,
        MessagePointerT<>&          _rresponse_ptr)
        : ConnectionSender<Ctx>(_rcon, _rctx, _rconfig, _rproto, _rconctx)
        , rresponse_ptr_(_rresponse_ptr)
    {
    }

    bool cancelMessage(MessageBundle& _rmsg_bundle, MessageId const& _rpool_msg_id) override
    {
        this->rcon_.updateContextOnCompleteMessage(this->context(), _rmsg_bundle, _rpool_msg_id, *rresponse_ptr_);

        const bool must_clear_request = !rresponse_ptr_->isResponsePart(); // do not clear the request if the response is a partial one

        if (!solid_function_empty(_rmsg_bundle.complete_fnc)) {
            solid_log(logger, Info, this);
            _rmsg_bundle.complete_fnc(this->context(), _rmsg_bundle.message_ptr, rresponse_ptr_, this->err_);
            request_found_ = true;
        } else if (_rmsg_bundle.message_ptr) {
            solid_log(logger, Info, this << " " << _rmsg_bundle.message_type_id);
            this->rproto_.complete(_rmsg_bundle.message_type_id, this->context(), _rmsg_bundle.message_ptr, rresponse_ptr_, this->err_);
            this->request_found_ = true;
        }

        return must_clear_request;
    }
};

void Connection::updateContextOnCompleteMessage(
    ConnectionContext& _rconctx, MessageBundle& _rmsg_bundle,
    MessageId const& _rpool_msg_id, const Message& _rmsg) const
{
    _rconctx.message_flags_ = _rmsg_bundle.message_flags;
    _rconctx.request_id_    = _rmsg.requestId();
    _rconctx.message_id_    = _rpool_msg_id;
}

template <class Ctx>
bool Connection::doCompleteMessage(frame::aio::ReactorContext& _rctx, MessagePointerT<>& _rresponse_ptr, const size_t _response_type_id)
{
    ConnectionContext    conctx(_rctx, service(_rctx), *this);
    const Configuration& rconfig = service(_rctx).configuration();
    const Protocol&      rproto  = rconfig.protocol();
    ErrorConditionT      error;

    if (_rresponse_ptr->isBackOnSender() || _rresponse_ptr->isBackOnPeer()) {
        solid_log(logger, Info, this << ' ' << "Completing back on sender message: " << _rresponse_ptr->requestId());
        typename Ctx::SenderResponseT sender(*this, _rctx, rconfig.writer, rproto, conctx, _rresponse_ptr);

        msg_writer_.cancel(_rresponse_ptr->requestId(), sender, true /*force*/);

        if (sender.request_found_) {
            return true;
        }
    }
    MessageBundle empty_msg_bundle; // request message

    solid_log(logger, Info, this << " " << _response_type_id);
    rproto.complete(_response_type_id, conctx, empty_msg_bundle.message_ptr, _rresponse_ptr, error);
    return false;
}
//-----------------------------------------------------------------------------
void Connection::doCompleteMessage(
    solid::frame::aio::ReactorContext& _rctx,
    MessageId const&                   _rpool_msg_id,
    MessageBundle&                     _rmsg_bundle,
    ErrorConditionT const&             _rerror)
{

    ConnectionContext    conctx(_rctx, service(_rctx), *this);
    const Configuration& rconfig = service(_rctx).configuration();
    const Protocol&      rproto  = rconfig.protocol();
    MessagePointerT<>    dummy_recv_msg_ptr;

    conctx.message_flags_ = _rmsg_bundle.message_flags;
    conctx.message_id_    = _rpool_msg_id;

    if (!solid_function_empty(_rmsg_bundle.complete_fnc)) {
        solid_log(logger, Info, this);
        _rmsg_bundle.complete_fnc(conctx, _rmsg_bundle.message_ptr, dummy_recv_msg_ptr, _rerror);
    } else {
        solid_log(logger, Info, this << " " << _rmsg_bundle.message_type_id);
        rproto.complete(_rmsg_bundle.message_type_id, conctx, _rmsg_bundle.message_ptr, dummy_recv_msg_ptr, _rerror);
    }
}
//-----------------------------------------------------------------------------
/*static*/ bool Connection::notify(Manager& _rm, const ActorIdT& _conuid, const RelayEngineNotification _what)
{
    switch (_what) {
    case RelayEngineNotification::NewData:
        return _rm.notify(_conuid, make_event(connection_event_category, ConnectionEvents::RelayNew));
    case RelayEngineNotification::DoneData:
        return _rm.notify(_conuid, make_event(connection_event_category, ConnectionEvents::RelayDone));
    default:
        break;
    }
    return false;
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doCompleteKeepalive(frame::aio::ReactorContext& _rctx)
{
    if (isServer()) {
        Configuration const& config = service(_rctx).configuration();

        ++recv_keepalive_count_;

        solid_log(logger, Verbose, this << " recv_keep_alive_count = " << recv_keepalive_count_);

        if (recv_keepalive_count_ < config.server.connection_inactivity_keepalive_count) {
            flags_.set(FlagsE::Keepalive);
            solid_log(logger, Info, this << " post send");
            this->post(_rctx, [this](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) { this->doSend<Ctx>(_rctx); });
        } else {
            const auto crt_time   = _rctx.steadyTime();
            recv_keepalive_count_ = 0; // prevent other posting
            if (crt_time >= recv_keepalive_boundary_) {
                recv_keepalive_boundary_ = crt_time + config.server.connection_inactivity_keepalive_interval;

                flags_.set(FlagsE::Keepalive);
                solid_log(logger, Info, this << " post send");
                this->post(_rctx, [this](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) { this->doSend<Ctx>(_rctx); });
            } else {

                solid_log(logger, Info, this << " post stop because of too many keep alive messages");
                this->post(
                    _rctx,
                    [this](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) {
                        this->doStop<Ctx>(_rctx, error_connection_too_many_keepalive_packets_received);
                    });
                return;
            }
        }
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doCompleteAckCount(frame::aio::ReactorContext& _rctx, uint8_t _count)
{
    int                  new_count = static_cast<int>(send_relay_free_count_) + _count;
    Configuration const& config    = service(_rctx).configuration();

    if (new_count <= config.connection_relay_buffer_count) {
        send_relay_free_count_ = static_cast<uint8_t>(new_count);
        solid_log(logger, Verbose, this << " count = " << (int)_count << " sentinel = " << (int)send_relay_free_count_);

        this->post(_rctx, [this](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) { this->doSend<Ctx>(_rctx); });
    } else {
        solid_log(logger, Error, this << " count = " << (int)_count << " sentinel = " << (int)send_relay_free_count_);
        this->post(
            _rctx,
            [this](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) {
                this->doStop<Ctx>(_rctx, error_connection_ack_count);
            });
    }
}
// //-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doCompleteCancelRequest(frame::aio::ReactorContext& _rctx, const RequestId& _reqid)
{
    MessageId             msgid(_reqid);
    MessageBundle         msg_bundle;
    MessageId             pool_msg_id;
    ConnectionContext     conctx(_rctx, service(_rctx), *this);
    Configuration const&  rconfig = service(_rctx).configuration();
    typename Ctx::SenderT sender(*this, _rctx, rconfig.writer, rconfig.protocol(), conctx, error_message_canceled_peer);

    msg_writer_.cancel(msgid, sender);
    this->post(_rctx, [this](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) { this->doSend<Ctx>(_rctx); });
}
//-----------------------------------------------------------------------------
template <class Ctx>
ResponseStateE Connection::doCheckResponseState(frame::aio::ReactorContext& _rctx, const MessageHeader& _rmsghdr, MessageId& _rrelay_id, const bool _erase_request)
{
    ResponseStateE rv = msg_writer_.checkResponseState(_rmsghdr.recipient_request_id_, _rrelay_id, _erase_request);
    if (rv == ResponseStateE::Invalid) {
        this->post(
            _rctx,
            [this](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) {
                this->doStop<Ctx>(_rctx, error_connection_invalid_response_state);
            });
        return ResponseStateE::Cancel;
    }

    if (rv == ResponseStateE::Cancel) {
        if (_rmsghdr.sender_request_id_.isValid()) {
            solid_log(logger, Info, this << " cancel_remote_msg = " << _rmsghdr.sender_request_id_);
            cancel_remote_msg_vec_.push_back(_rmsghdr.sender_request_id_);
            if (cancel_remote_msg_vec_.size() == 1) {
                post(_rctx, [this](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) { doSend<Ctx>(_rctx); });
            }
        }
        return ResponseStateE::Cancel;
    }
    return rv;
}
//-----------------------------------------------------------------------------
bool Connection::postSendAll(frame::aio::ReactorContext& _rctx, const char* _pbuf, size_t _bufcp, EventBase& _revent)
{
    return sock_ptr_->postSendAll(_rctx, Connection::onSendAllRaw, _pbuf, _bufcp, _revent);
}
//-----------------------------------------------------------------------------
bool Connection::postRecvSome(frame::aio::ReactorContext& _rctx, char* _pbuf, size_t _bufcp, EventBase& _revent)
{
    return sock_ptr_->postRecvSome(_rctx, Connection::onRecvSomeRaw, _pbuf, _bufcp, _revent);
}
//-----------------------------------------------------------------------------
template <class Ctx>
bool Connection::postRecvSome(frame::aio::ReactorContext& _rctx, char* _pbuf, size_t _bufcp)
{
    return sock_ptr_->postRecvSome(_rctx, Connection::onRecv<Ctx>, _pbuf, _bufcp);
}
//-----------------------------------------------------------------------------
template <class Ctx>
bool Connection::connect(frame::aio::ReactorContext& _rctx, const SocketAddressInet& _raddr)
{
    return sock_ptr_->connect(_rctx, Connection::onConnect<Ctx>, _raddr);
}
//-----------------------------------------------------------------------------
template <class Ctx>
bool Connection::recvSome(frame::aio::ReactorContext& _rctx, char* _buf, size_t _bufcp, size_t& _sz)
{
    return sock_ptr_->recvSome(_rctx, Connection::onRecv<Ctx>, _buf, _bufcp, _sz);
}
//-----------------------------------------------------------------------------
template <class Ctx>
bool Connection::sendAll(frame::aio::ReactorContext& _rctx, char* _buf, size_t _bufcp)
{
    solid_check(!isStopping());
    return sock_ptr_->sendAll(_rctx, Connection::onSend<Ctx>, _buf, _bufcp);
}
//-----------------------------------------------------------------------------
void Connection::prepareSocket(frame::aio::ReactorContext& _rctx)
{
    sock_ptr_->prepareSocket(_rctx);
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doPauseRead(frame::aio::ReactorContext& _rctx)
{
    if (!flags_.isSet(FlagsE::PauseRecv)) {
        flags_.set(FlagsE::PauseRecv);
        sock_ptr_->cancelRecv(_rctx);
    }
}
//-----------------------------------------------------------------------------
template <class Ctx>
void Connection::doResumeRead(frame::aio::ReactorContext& _rctx)
{
    if (flags_.isSet(FlagsE::PauseRecv)) {
        flags_.reset(FlagsE::PauseRecv);

        const auto pbuf  = recv_buf_.data() + recv_buf_.size();
        const auto bufsz = recv_buf_.capacity() - recv_buf_.size();

        const bool rv = postRecvSome<Ctx>(_rctx, pbuf, bufsz); // fully asynchronous call

        solid_assert_log(!rv, logger);
        (void)rv;
    }
}
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//  ClientConnection
//-----------------------------------------------------------------------------
struct ClientContext {
    using SenderT         = ConnectionSender<ClientContext>;
    using ReceiverT       = ConnectionReceiver<ClientContext>;
    using SenderResponseT = ConnectionSenderResponse<ClientContext>;

    static void pollOther(frame::aio::ReactorContext& _rctx, const Configuration& _rconfig, MessageWriter& _rmsgwriter)
    {
    }
};

void ClientConnection::onEvent(frame::aio::ReactorContext& _rctx, EventBase&& _uevent)
{
    static const EventHandler<void,
        ClientConnection&,
        frame::aio::ReactorContext&>
        event_handler = {
            [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                _rcon.doHandleEventGeneric(_rctx, _revt);
            },
            {
                {make_event(GenericEventE::Start),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventStart<ClientContext>(_rctx, _revt);
                    }},
                {make_event(GenericEventE::Kill),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventKill<ClientContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::Resolve),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventResolve(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::NewPoolMessage),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventNewPoolMessage<ClientContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::NewPoolQueueMessage),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventNewPoolQueueMessage<ClientContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::NewConnMessage),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventNewConnMessage<ClientContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::CancelConnMessage),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventCancelConnMessage<ClientContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::CancelPoolMessage),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventCancelPoolMessage(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::ClosePoolMessage),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventClosePoolMessage(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::EnterActive),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventEnterActive<ClientContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::EnterPassive),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventEnterPassive<ClientContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::StartSecure),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventStartSecure<ClientContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::SendRaw),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventSendRaw(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::RecvRaw),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventRecvRaw(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::Post),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventPost(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::Stopping),
                    [](EventBase& _revt, ClientConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventStopping(_rctx, _revt);
                    }},
            }};

    solid_log(logger, Verbose, this << ' ' << this->id() << " " << _uevent);
    event_handler.handle(_uevent, *this, _rctx);
}
//-----------------------------------------------------------------------------
void ClientConnection::doHandleEventResolve(
    frame::aio::ReactorContext& _rctx,
    EventBase&                  _revent)
{
    ResolveMessage* presolvemsg = _revent.cast<ResolveMessage>();

    if (presolvemsg != nullptr) {
        if (!isStopping()) {
            solid_log(logger, Info, this << ' ' << this->id() << " Session receive resolve event message of size: " << presolvemsg->addrvec.size());
            if (!presolvemsg->empty()) {
                solid_log(logger, Info, this << ' ' << this->id() << " Connect to " << presolvemsg->currentAddress());

                // initiate connect:

                if (this->connect<ClientContext>(_rctx, presolvemsg->currentAddress())) {
                    onConnect<ClientContext>(_rctx);
                }

                presolvemsg->popAddress();

                service(_rctx).forwardResolveMessage(poolId(), _revent);
            } else {
                solid_log(logger, Warning, this << ' ' << this->id() << " Empty resolve message");
                doStop<ClientContext>(_rctx, error_connection_resolve);
            }
        }
    } else {
        solid_assert_log(false, logger);
    }
}
//-----------------------------------------------------------------------------
void ClientConnection::stop(frame::aio::ReactorContext& _rctx, const ErrorConditionT& _rerr)
{
    doStop<ClientContext>(_rctx, _rerr);
}
//-----------------------------------------------------------------------------
void ClientConnection::pauseRead(frame::aio::ReactorContext& _rctx)
{
    doPauseRead<ClientContext>(_rctx);
}
//-----------------------------------------------------------------------------
void ClientConnection::resumeRead(frame::aio::ReactorContext& _rctx)
{
    doResumeRead<ClientContext>(_rctx);
}
//-----------------------------------------------------------------------------
//  ServerConnection
//-----------------------------------------------------------------------------
struct ServerContext {
    using SenderT         = ConnectionSender<ServerContext>;
    using ReceiverT       = ConnectionReceiver<ServerContext>;
    using SenderResponseT = ConnectionSenderResponse<ServerContext>;

    static void pollOther(frame::aio::ReactorContext& _rctx, const Configuration& _rconfig, MessageWriter& _rmsgwriter)
    {
    }
};
void ServerConnection::onEvent(frame::aio::ReactorContext& _rctx, EventBase&& _uevent)
{
    static const EventHandler<void,
        ServerConnection&,
        frame::aio::ReactorContext&>
        event_handler = {
            [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                _rcon.doHandleEventGeneric(_rctx, _revt);
            },
            {
                {make_event(GenericEventE::Start),
                    [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventStart<ServerContext>(_rctx, _revt);
                    }},
                {make_event(GenericEventE::Kill),
                    [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventKill<ServerContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::NewPoolMessage),
                    [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventNewPoolMessage<ServerContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::NewPoolQueueMessage),
                    [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventNewPoolQueueMessage<ServerContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::NewConnMessage),
                    [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventNewConnMessage<ServerContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::CancelConnMessage),
                    [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventCancelConnMessage<ServerContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::CancelPoolMessage),
                    [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventCancelPoolMessage(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::ClosePoolMessage),
                    [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventClosePoolMessage(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::EnterActive),
                    [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventEnterActive<ServerContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::EnterPassive),
                    [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventEnterPassive<ServerContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::StartSecure),
                    [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventStartSecure<ServerContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::SendRaw),
                    [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventSendRaw(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::RecvRaw),
                    [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventRecvRaw(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::Post),
                    [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventPost(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::Stopping),
                    [](EventBase& _revt, ServerConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventStopping(_rctx, _revt);
                    }},
            }};

    solid_log(logger, Verbose, this << ' ' << this->id() << " " << _uevent);
    event_handler.handle(_uevent, *this, _rctx);
}
//-----------------------------------------------------------------------------
void ServerConnection::stop(frame::aio::ReactorContext& _rctx, const ErrorConditionT& _rerr)
{
    doStop<ServerContext>(_rctx, _rerr);
}
//-----------------------------------------------------------------------------
void ServerConnection::pauseRead(frame::aio::ReactorContext& _rctx)
{
    doPauseRead<ServerContext>(_rctx);
}
//-----------------------------------------------------------------------------
void ServerConnection::resumeRead(frame::aio::ReactorContext& _rctx)
{
    doResumeRead<ServerContext>(_rctx);
}
//-----------------------------------------------------------------------------
//  RelayConnection
//-----------------------------------------------------------------------------

struct RelayContext;

struct RelaySender : ConnectionSender<RelayContext> {
    RelaySender(
        Connection&                 _rcon,
        frame::aio::ReactorContext& _rctx,
        WriterConfiguration const&  _rconfig,
        Protocol const&             _rproto,
        ConnectionContext&          _rconctx,
        const ErrorConditionT&      _rerr = ErrorConditionT{})
        : ConnectionSender<RelayContext>(_rcon, _rctx, _rconfig, _rproto, _rconctx, _rerr)
    {
    }

    void completeRelayed(RelayData* _prelay_data, MessageId const& _rmsgid) override
    {
        static_cast<RelayConnection&>(rcon_).doCompleteRelayed(rctx_, _prelay_data, _rmsgid);
    }

    void cancelRelayed(RelayData* _prelay_data, MessageId const& _rmsgid) override
    {
        static_cast<RelayConnection&>(rcon_).doCancelRelayed(rctx_, _prelay_data, _rmsgid);
    }
};

struct RelayReceiver : ConnectionReceiver<RelayContext> {

    RelayReceiver(
        Connection&                 _rcon,
        frame::aio::ReactorContext& _rctx,
        ReaderConfiguration const&  _rconfig,
        Protocol const&             _rproto,
        ConnectionContext&          _rconctx)
        : ConnectionReceiver<RelayContext>(_rcon, _rctx, _rconfig, _rproto, _rconctx, true)
    {
    }

    bool receiveRelayStart(MessageHeader& _rmsghdr, const char* _pbeg, size_t _sz, MessageId& _rrelay_id, const bool _is_last, ErrorConditionT& _rerror) override
    {
        return static_cast<RelayConnection&>(rcon_).doReceiveRelayStart(rctx_, _rmsghdr, _pbeg, _sz, _rrelay_id, _is_last, _rerror);
    }

    bool receiveRelayBody(const char* _pbeg, size_t _sz, const MessageId& _rrelay_id, const bool _is_last, ErrorConditionT& _rerror) override
    {
        return static_cast<RelayConnection&>(rcon_).doReceiveRelayBody(rctx_, _pbeg, _sz, _rrelay_id, _is_last, _rerror);
    }

    bool receiveRelayResponse(MessageHeader& _rmsghdr, const char* _pbeg, size_t _sz, const MessageId& _rrelay_id, const bool _is_last, ErrorConditionT& _rerror) override
    {
        return static_cast<RelayConnection&>(rcon_).doReceiveRelayResponse(rctx_, _rmsghdr, _pbeg, _sz, _rrelay_id, _is_last, _rerror);
    }

    void cancelRelayed(const solid::frame::mprpc::MessageId& _rrelay_id) override
    {
        static_cast<RelayConnection&>(rcon_).doCancelRelayed(rctx_, nullptr, _rrelay_id);
    }
};

struct RelayContext {
    using SenderT         = RelaySender;
    using ReceiverT       = RelayReceiver;
    using SenderResponseT = ConnectionSenderResponse<RelayContext>;

    static void pollOther(frame::aio::ReactorContext& _rctx, const Configuration& _rconfig, MessageWriter& _rmsgwriter)
    {
        RelayConnection& rcon = static_cast<RelayConnection&>(_rctx.actor());
        rcon.tryPollRelayEngine(_rctx, _rconfig, _rmsgwriter);
    }
};

void RelayConnection::tryPollRelayEngine(frame::aio::ReactorContext& _rctx, const Configuration& _rconfig, MessageWriter& _rmsgwriter)
{
    if (shouldPollRelayEngine()) {
        auto relay_poll_push_lambda = [this, &_rconfig, &_rmsgwriter](
                                          RelayData*&      _rprelay_data,
                                          const MessageId& _rengine_msg_id,
                                          MessageId& _rconn_msg_id, bool& _rmore) -> bool {
            return _rmsgwriter.enqueue(_rconfig.writer, _rprelay_data, _rengine_msg_id, _rconn_msg_id, _rmore);
        };
        bool more = false;
        _rconfig.relayEngine().pollNew(relayId(), relay_poll_push_lambda, more);

        if (!more) {
            resetFlag(FlagsE::PollRelayEngine); // reset flag
        }
        solid_log(logger, Verbose, this << ' ' << id() << " shouldPollRelayEngine = " << shouldPollRelayEngine());
    }
}

void RelayConnection::onEvent(frame::aio::ReactorContext& _rctx, EventBase&& _uevent)
{
    static const EventHandler<void,
        RelayConnection&,
        frame::aio::ReactorContext&>
        event_handler = {
            [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                _rcon.doHandleEventGeneric(_rctx, _revt);
            },
            {
                {make_event(GenericEventE::Start),
                    [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventStart<RelayContext>(_rctx, _revt);
                    }},
                {make_event(GenericEventE::Kill),
                    [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventKill<RelayContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::NewPoolMessage),
                    [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventNewPoolMessage<RelayContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::NewPoolQueueMessage),
                    [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventNewPoolQueueMessage<RelayContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::NewConnMessage),
                    [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventNewConnMessage<RelayContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::CancelConnMessage),
                    [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventCancelConnMessage<RelayContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::CancelPoolMessage),
                    [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventCancelPoolMessage(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::ClosePoolMessage),
                    [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventClosePoolMessage(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::EnterActive),
                    [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventEnterActive<RelayContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::EnterPassive),
                    [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventEnterPassive<RelayContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::StartSecure),
                    [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventStartSecure<RelayContext>(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::RelayNew),
                    [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventRelayNew(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::RelayDone),
                    [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventRelayDone(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::Post),
                    [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventPost(_rctx, _revt);
                    }},
                {make_event(connection_event_category, ConnectionEvents::Stopping),
                    [](EventBase& _revt, RelayConnection& _rcon, frame::aio::ReactorContext& _rctx) {
                        _rcon.doHandleEventStopping(_rctx, _revt);
                    }},
            }};

    solid_log(logger, Verbose, this << ' ' << this->id() << " " << _uevent);
    event_handler.handle(_uevent, *this, _rctx);
}
//-----------------------------------------------------------------------------
void RelayConnection::doCompleteRelayed(
    frame::aio::ReactorContext& _rctx,
    RelayData*                  _prelay_data,
    MessageId const&            _rengine_msg_id)
{

    const Configuration& rconfig = configuration(_rctx);
    bool                 more    = false;

    rconfig.relayEngine().complete(relayId(), _prelay_data, _rengine_msg_id, more);

    if (more) {
        setFlag(FlagsE::PollRelayEngine); // set flag

        this->post(_rctx, [this](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) { this->doSend<RelayContext>(_rctx); });
    } else {
        resetFlag(FlagsE::PollRelayEngine); // reset flag
    }
}
//-----------------------------------------------------------------------------
void RelayConnection::doCancelRelayed(
    frame::aio::ReactorContext& _rctx,
    RelayData*                  _prelay_data,
    MessageId const&            _rengine_msg_id)
{

    const Configuration& rconfig     = configuration(_rctx);
    size_t               ack_buf_cnt = 0;

    const auto done_lambda = [this, &ack_buf_cnt](SharedBuffer& _rbuf) {
        if (_rbuf.useCount() == 1) {
            ++ack_buf_cnt;
            returnRecvBuffer(std::move(_rbuf));
        }
    };

    rconfig.relayEngine().cancel(relayId(), _prelay_data, _rengine_msg_id, done_lambda);

    if (ack_buf_cnt != 0u) {
        ackBufferCountAdd(static_cast<uint8_t>(ack_buf_cnt));
        setFlag(FlagsE::PollRelayEngine); // set flag
        this->post(_rctx, [this](frame::aio::ReactorContext& _rctx, EventBase const& /*_revent*/) { this->doSend<RelayContext>(_rctx); });
    }
}
//-----------------------------------------------------------------------------
bool RelayConnection::doReceiveRelayStart(
    frame::aio::ReactorContext& _rctx,
    MessageHeader&              _rmsghdr,
    const char*                 _pbeg,
    size_t                      _sz,
    MessageId&                  _rrelay_id,
    const bool                  _is_last,
    ErrorConditionT&            _rerror)
{
    Configuration const& config = configuration(_rctx);
    ConnectionContext    conctx{_rctx, service(_rctx), *this};
    RelayData            relmsg{recvBuffer(), _pbeg, _sz /*, this->uid(_rctx)*/, _is_last};

    return config.relayEngine().relayStart(uid(_rctx), relayId(), _rmsghdr, std::move(relmsg), _rrelay_id, _rerror);
}
//-----------------------------------------------------------------------------
bool RelayConnection::doReceiveRelayBody(
    frame::aio::ReactorContext& _rctx,
    const char*                 _pbeg,
    size_t                      _sz,
    const MessageId&            _rrelay_id,
    const bool                  _is_last,
    ErrorConditionT&            _rerror)
{
    Configuration const& config = configuration(_rctx);
    ConnectionContext    conctx{_rctx, service(_rctx), *this};
    RelayData            relmsg{recvBuffer(), _pbeg, _sz /*, this->uid(_rctx)*/, _is_last};

    return config.relayEngine().relay(relayId(), std::move(relmsg), _rrelay_id, _rerror);
}
//-----------------------------------------------------------------------------
bool RelayConnection::doReceiveRelayResponse(
    frame::aio::ReactorContext& _rctx,
    MessageHeader&              _rmsghdr,
    const char*                 _pbeg,
    size_t                      _sz,
    const MessageId&            _rrelay_id,
    const bool                  _is_last,
    ErrorConditionT&            _rerror)
{
    Configuration const& config = configuration(_rctx);
    ConnectionContext    conctx{_rctx, service(_rctx), *this};
    RelayData            relmsg{recvBuffer(), _pbeg, _sz /*, this->uid(_rctx)*/, _is_last};

    return config.relayEngine().relayResponse(relayId(), _rmsghdr, std::move(relmsg), _rrelay_id, _rerror);
}

//-----------------------------------------------------------------------------
void RelayConnection::doHandleEventRelayNew(frame::aio::ReactorContext& _rctx, EventBase& /*_revent*/)
{
    // The connection may get unregistered from RelayEngine (see Service::connectionStopping)
    // after an Relay* event get posted but before it being handled by connection.
    // That is why we need to check if the relay_id_ is valid.
    if (relayId().isValid()) {
        setFlag(FlagsE::PollRelayEngine);
        doSend<RelayContext>(_rctx);
    }
}
//-----------------------------------------------------------------------------
void RelayConnection::doHandleEventRelayDone(frame::aio::ReactorContext& _rctx, EventBase& /*_revent*/)
{
    // The connection may get unregistered from RelayEngine (see Service::connectionStopping)
    // after an Relay* event get posted but before it being handled by connection.
    // That is why we need to check if the relay_id_ is valid.
    if (relayId().isValid()) {
        Configuration const& config      = configuration(_rctx);
        size_t               ack_buf_cnt = 0;

        const auto done_lambda = [this, &ack_buf_cnt](SharedBuffer& _rbuf) {
            if (_rbuf.useCount() == 1) {
                ++ack_buf_cnt;
                returnRecvBuffer(std::move(_rbuf));
            }
        };
        const auto cancel_lambda = [this](const MessageHeader& _rmsghdr) {
            // we must request the remote side to stop sending the message
            solid_log(logger, Info, this << " cancel_remote_msg sreqid =  " << _rmsghdr.sender_request_id_ << " rreqid = " << _rmsghdr.recipient_request_id_);
            cancelRemoveMessageVectorAppend(_rmsghdr.recipient_request_id_);
            // we do nothing here because the message cancel will be discovered onto messagereader
            // when calling receiveRelayBody which will return false

            // TODO:!!!!
        };

        config.relayEngine().pollDone(relayId(), done_lambda, cancel_lambda);

        if (ack_buf_cnt != 0u || cancelRemoveMessageVectorSize() != 0) {
            solid_log(logger, Info, this << " ack_buf_cnt = " << ack_buf_cnt << " cancel_remote_msg_vec_size = " << cancelRemoveMessageVectorSize());
            ackBufferCountAdd(static_cast<uint8_t>(ack_buf_cnt));
            doSend<RelayContext>(_rctx);
        }
    }
}
//-----------------------------------------------------------------------------
void RelayConnection::stop(frame::aio::ReactorContext& _rctx, const ErrorConditionT& _rerr)
{
    doStop<RelayContext>(_rctx, _rerr);
}
//-----------------------------------------------------------------------------
void RelayConnection::pauseRead(frame::aio::ReactorContext& _rctx)
{
    doPauseRead<RelayContext>(_rctx);
}
//-----------------------------------------------------------------------------
void RelayConnection::resumeRead(frame::aio::ReactorContext& _rctx)
{
    doResumeRead<RelayContext>(_rctx);
}
//-----------------------------------------------------------------------------
//  ConnectionContext
//-----------------------------------------------------------------------------
Any<>& ConnectionContext::any()
{
    return rconnection_.any();
}
//-----------------------------------------------------------------------------
MessagePointerT<> ConnectionContext::fetchRequest(Message const& _rmsg) const
{
    return rconnection_.fetchRequest(_rmsg);
}
//-----------------------------------------------------------------------------
RecipientId ConnectionContext::recipientId() const
{
    return RecipientId(rconnection_.poolId(), rservice_.manager().id(rconnection_));
}
//-----------------------------------------------------------------------------
ActorIdT ConnectionContext::connectionId() const
{
    return rservice_.manager().id(rconnection_);
}
//-----------------------------------------------------------------------------
const UniqueId& ConnectionContext::relayId() const
{
    return rconnection_.relayId();
}
//-----------------------------------------------------------------------------
void ConnectionContext::relayId(const UniqueId& _relay_id) const
{
    rconnection_.relayId(_relay_id);
}
//-----------------------------------------------------------------------------
const std::string& ConnectionContext::recipientName() const
{
    return rconnection_.poolName();
}
//-----------------------------------------------------------------------------
SocketDevice const& ConnectionContext::device() const
{
    return rconnection_.device();
}
//-----------------------------------------------------------------------------
bool ConnectionContext::isConnectionActive() const
{
    return rconnection_.isActiveState();
}
//-----------------------------------------------------------------------------
bool ConnectionContext::isConnectionServer() const
{
    return rconnection_.isServer();
}
//-----------------------------------------------------------------------------
const ErrorConditionT& ConnectionContext::error() const
{
    return rconnection_.error();
}
//-----------------------------------------------------------------------------
const ErrorCodeT& ConnectionContext::systemError() const
{
    return rconnection_.systemError();
}
//-----------------------------------------------------------------------------
Configuration const& ConnectionContext::configuration() const
{
    return rservice_.configuration();
}
//-----------------------------------------------------------------------------
void ConnectionContext::stop(const ErrorConditionT& _err)
{
    rconnection_.stop(reactorContext(), _err);
}
//-----------------------------------------------------------------------------
void ConnectionContext::pauseRead()
{
    rconnection_.pauseRead(reactorContext());
}
//-----------------------------------------------------------------------------
void ConnectionContext::resumeRead()
{
    rconnection_.resumeRead(reactorContext());
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
    return static_cast<Connection&>(_rctx.actor());
}
//-----------------------------------------------------------------------------
// Message
//-----------------------------------------------------------------------------

void Message::header(frame::mprpc::ConnectionContext& _rctx)
{
    header_ = std::move(*_rctx.pmessage_header_);
}

} // namespace mprpc
} // namespace frame
} // namespace solid
