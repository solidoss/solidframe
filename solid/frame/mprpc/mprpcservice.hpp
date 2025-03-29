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
#include <optional>

#include "solid/system/exception.hpp"
#include "solid/system/statistic.hpp"

#include "solid/utility/cast.hpp"
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
#include "solid/system/statistic.hpp"

namespace solid {
namespace frame {

namespace aio {
class ReactorContext;
} // namespace aio

namespace mprpc {

namespace openssl {
class SocketStub;
} // namespace openssl

constexpr size_t event_small_size = 32;
using EventT                      = Event<event_small_size>;

extern const Event<> pool_event_connect;
extern const Event<> pool_event_disconnect;
extern const Event<> pool_event_connection_start;
extern const Event<> pool_event_connection_activate;
extern const Event<> pool_event_connection_stop;
extern const Event<> pool_event_pool_disconnect;
extern const Event<> pool_event_pool_stop;

struct Message;
class Configuration;
class Connection;
struct MessageBundle;

struct ServiceStartStatus {
    std::vector<SocketAddress> listen_addr_vec_;
};

struct ServiceStatistic : solid::Statistic {
    std::atomic<uint64_t> poll_pool_count_;
    std::atomic<uint64_t> poll_pool_more_count_;
    std::atomic<uint64_t> poll_pool_fetch_count_00_;
    std::atomic<uint64_t> poll_pool_fetch_count_01_;
    std::atomic<uint64_t> poll_pool_fetch_count_05_;
    std::atomic<uint64_t> poll_pool_fetch_count_10_;
    std::atomic<uint64_t> poll_pool_fetch_count_40_;
    std::atomic<uint64_t> poll_pool_fetch_count_50_;
    std::atomic<uint64_t> poll_pool_fetch_count_60_;
    std::atomic<uint64_t> poll_pool_fetch_count_70_;
    std::atomic<uint64_t> poll_pool_fetch_count_80_;
    std::atomic<uint64_t> send_message_count_;
    std::atomic<uint64_t> send_message_context_count_;
    std::atomic<uint64_t> send_message_to_connection_count_;
    std::atomic<uint64_t> send_message_to_pool_count_;
    std::atomic<uint64_t> reject_new_pool_message_count_;
    std::atomic<uint64_t> connection_new_pool_message_count_;
    std::atomic<uint64_t> connection_do_send_count_;
    std::atomic<uint64_t> connection_send_wait_count_;
    std::atomic<uint64_t> connection_send_done_count_;
    std::atomic<uint64_t> connection_send_buff_size_max_;
    std::atomic<uint64_t> connection_send_buff_size_count_00_;
    std::atomic<uint64_t> connection_send_buff_size_count_01_;
    std::atomic<uint64_t> connection_send_buff_size_count_02_;
    std::atomic<uint64_t> connection_send_buff_size_count_03_;
    std::atomic<uint64_t> connection_send_buff_size_count_04_;
    std::atomic<uint64_t> connection_recv_buff_size_max_;
    std::atomic<uint64_t> connection_recv_buff_size_count_00_;
    std::atomic<uint64_t> connection_recv_buff_size_count_01_;
    std::atomic<uint64_t> connection_recv_buff_size_count_02_;
    std::atomic<uint64_t> connection_recv_buff_size_count_03_;
    std::atomic<uint64_t> connection_recv_buff_size_count_04_;
    std::atomic<uint64_t> connection_send_posted_;
    std::atomic<uint64_t> max_fetch_size_;
    std::atomic<uint64_t> min_fetch_size_;

    void fetchCount(const uint64_t _count, const bool _more)
    {
#ifdef SOLID_HAS_STATISTICS
        if (_count == 0) {
            solid_statistic_inc(poll_pool_fetch_count_00_);
        } else if (_count <= 1) {
            solid_statistic_inc(poll_pool_fetch_count_01_);
            poll_pool_more_count_ += _more;
        } else if (_count <= 5) {
            solid_statistic_inc(poll_pool_fetch_count_05_);
        } else if (_count <= 10) {
            solid_statistic_inc(poll_pool_fetch_count_10_);
        } else if (_count <= 40) {
            solid_statistic_inc(poll_pool_fetch_count_40_);
        } else if (_count <= 50) {
            solid_statistic_inc(poll_pool_fetch_count_50_);
        } else if (_count <= 60) {
            solid_statistic_inc(poll_pool_fetch_count_60_);
        } else if (_count <= 70) {
            solid_statistic_inc(poll_pool_fetch_count_70_);
        } else {
            solid_statistic_inc(poll_pool_fetch_count_80_);
        }

#else
        (void)_count;
#endif
    }

    void connectionSendBufferSize(const uint64_t _size, const uint64_t _capacity)
    {
#ifdef SOLID_HAS_STATISTICS
        solid_statistic_max(connection_send_buff_size_max_, _size);
        if (_size == 0) {
            solid_statistic_inc(connection_send_buff_size_count_00_);
        } else if (_size <= (_capacity / 4)) {
            solid_statistic_inc(connection_send_buff_size_count_01_);
        } else if (_size <= (_capacity / 2)) {
            solid_statistic_inc(connection_send_buff_size_count_02_);
        } else if (_size <= (_capacity)) {
            solid_statistic_inc(connection_send_buff_size_count_03_);
        } else {
            solid_statistic_inc(connection_send_buff_size_count_04_);
        }
#endif
    }

    void connectionRecvBufferSize(const uint64_t _size, const uint64_t _capacity)
    {
#ifdef SOLID_HAS_STATISTICS
        solid_statistic_max(connection_recv_buff_size_max_, _size);
        if (_size == 0) {
            solid_statistic_inc(connection_recv_buff_size_count_00_);
        } else if (_size <= (_capacity / 4)) {
            solid_statistic_inc(connection_recv_buff_size_count_01_);
        } else if (_size <= (_capacity / 2)) {
            solid_statistic_inc(connection_recv_buff_size_count_02_);
        } else if (_size <= (_capacity)) {
            solid_statistic_inc(connection_recv_buff_size_count_03_);
        } else {
            solid_statistic_inc(connection_recv_buff_size_count_04_);
        }
#endif
    }

    ServiceStatistic();
    std::ostream& print(std::ostream& _ros) const override;
};

class RecipientUrl final {
    friend class Service;
    using ImplOptionalRelayT = std::optional<MessageRelayHeader>;
    using ImplURLDataT       = std::optional<std::variant<RecipientId, std::string_view>>;
    const ImplURLDataT       url_var_opt_;
    ConnectionContext* const pctx_ = nullptr;
    const ImplOptionalRelayT relay_;

    bool hasRecipientId() const
    {
        return url_var_opt_.has_value() && std::get_if<RecipientId>(&url_var_opt_.value());
    }
    const RecipientId* recipientId() const
    {
        return url_var_opt_.has_value() ? std::get_if<RecipientId>(&url_var_opt_.value()) : nullptr;
    }
    bool hasURL() const
    {
        return url_var_opt_.has_value() && std::get_if<std::string_view>(&url_var_opt_.value());
        return false;
    }

    bool hasURLNonEmpty() const
    {
        if (url_var_opt_.has_value()) {
            if (auto* psv = std::get_if<std::string_view>(&url_var_opt_.value())) {
                return !psv->empty();
            }
        }
        return false;
    }

    std::string_view url() const
    {
        if (url_var_opt_.has_value()) {
            if (auto* psv = std::get_if<std::string_view>(&url_var_opt_.value())) {
                return *psv;
            }
        }
        return {};
    }

public:
    using RelayT         = MessageRelayHeader;
    using OptionalRelayT = ImplOptionalRelayT;

    RecipientUrl(const RecipientUrl&) = delete;
    RecipientUrl(RecipientUrl&&)      = delete;

    RecipientUrl& operator=(const RecipientUrl&) = delete;
    RecipientUrl& operator=(RecipientUrl&&)      = delete;

    RecipientUrl(
        RecipientId const& _id)
        : url_var_opt_(_id)
    {
    }

    RecipientUrl(
        const std::string_view& _url)
        : url_var_opt_(_url)
    {
    }

    RecipientUrl(
        ConnectionContext& _rctx)
        : pctx_(&_rctx)
    {
    }

    RecipientUrl(
        RecipientId const& _id, const RelayT& _relay)
        : url_var_opt_(_id)
        , relay_(_relay)
    {
    }

    RecipientUrl(
        const std::string_view& _url, const RelayT& _relay)
        : url_var_opt_(_url)
        , relay_(_relay)
    {
    }

    RecipientUrl(
        ConnectionContext& _rctx, const RelayT& _relay)
        : pctx_(&_rctx)
        , relay_(_relay)
    {
    }

private:
    RecipientUrl(RecipientId const& _id, const OptionalRelayT& _relay)
        : url_var_opt_(_id)
        , relay_(_relay)
    {
    }
};

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
    struct Data;
    std::shared_ptr<Data> pimpl_;

public:
    typedef frame::Service BaseT;

    Service(
        frame::UseServiceShell _force_shell);

    //! Destructor
    ~Service();

    const ServiceStatistic& statistic() const;

    ErrorConditionT createConnectionPool(const std::string_view& _url, const size_t _persistent_connection_count = 1);

    template <class F>
    ErrorConditionT createConnectionPool(
        const std::string_view& _url,
        RecipientId&            _rrecipient_id,
        const F                 _event_fnc,
        const size_t            _persistent_connection_count = 1);

    // send message using recipient name --------------------------------------
    template <class T>
    ErrorConditionT sendMessage(
        const RecipientUrl&       _recipient_url,
        MessagePointerT<T> const& _rmsgptr,
        const MessageFlagsT&      _flags = 0);

    template <class T>
    ErrorConditionT sendMessage(
        const RecipientUrl&       _recipient_url,
        MessagePointerT<T> const& _rmsgptr,
        RecipientId&              _rrecipient_id,
        const MessageFlagsT&      _flags = 0);

    template <class T>
    ErrorConditionT sendMessage(
        const RecipientUrl&       _recipient_url,
        MessagePointerT<T> const& _rmsgptr,
        RecipientId&              _rrecipient_id,
        MessageId&                _rmsg_id,
        const MessageFlagsT&      _flags = 0);

    // send message using connection uid  -------------------------------------
    template <class T>
    ErrorConditionT sendMessage(
        const RecipientUrl&       _recipient_url,
        MessagePointerT<T> const& _rmsgptr,
        MessageId&                _rmsg_id,
        const MessageFlagsT&      _flags = 0);

    // send request using recipient name --------------------------------------

    template <class T, class Fnc>
    ErrorConditionT sendRequest(
        const RecipientUrl&       _recipient_url,
        MessagePointerT<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        const MessageFlagsT&      _flags = 0);

    template <class T, class Fnc>
    ErrorConditionT sendRequest(
        const RecipientUrl&       _recipient_url,
        MessagePointerT<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        RecipientId&              _rrecipient_id,
        const MessageFlagsT&      _flags = 0);

    template <class T, class Fnc>
    ErrorConditionT sendRequest(
        const RecipientUrl&       _recipient_url,
        MessagePointerT<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        RecipientId&              _rrecipient_id,
        MessageId&                _rmsguid,
        const MessageFlagsT&      _flags = 0);

    // send message using connection uid  -------------------------------------
    template <class T>
    ErrorConditionT sendResponse(
        RecipientId const&        _rrecipient_id,
        MessagePointerT<T> const& _rmsgptr,
        const MessageFlagsT&      _flags = 0);

    template <class T>
    ErrorConditionT sendResponse(
        RecipientId const&        _rrecipient_id,
        MessagePointerT<T> const& _rmsgptr,
        MessageId&                _rmsg_id,
        const MessageFlagsT&      _flags = 0);
    // send message with complete using recipient name ------------------------

    template <class T, class Fnc>
    ErrorConditionT sendMessage(
        const RecipientUrl&       _recipient_url,
        MessagePointerT<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        const MessageFlagsT&      _flags);

    template <class T, class Fnc>
    ErrorConditionT sendMessage(
        const RecipientUrl&       _recipient_url,
        MessagePointerT<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        RecipientId&              _rrecipient_id,
        const MessageFlagsT&      _flags);

    template <class T, class Fnc>
    ErrorConditionT sendMessage(
        const RecipientUrl&       _recipient_url,
        MessagePointerT<T> const& _rmsgptr,
        Fnc                       _complete_fnc,
        RecipientId&              _rrecipient_id,
        MessageId&                _rmsguid,
        const MessageFlagsT&      _flags);

    // send message using ConnectionContext  ----------------------------------

    template <class T>
    ErrorConditionT sendResponse(
        ConnectionContext&        _rctx,
        MessagePointerT<T> const& _rmsgptr,
        const MessageFlagsT&      _flags = 0);
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
        const size_t       _send_buffer_capacity = 0 // 0 means: leave as it is
    );

    ErrorConditionT connectionNotifyEnterActiveState(
        RecipientId const& _rrecipient_id,
        const size_t       _send_buffer_capacity = 0 // 0 means: leave as it is
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
    void doStart(Configuration&& _ucfg)
    {
        ServiceStartStatus status;
        doStart(status, std::move(_ucfg));
    }
    void doStart()
    {
        ServiceStartStatus status;
        doStart(status);
    }

    template <typename A>
    void doStart(Configuration&& _ucfg, A&& _a)
    {
        ServiceStartStatus status;
        doStart(status, std::move(_ucfg), std::forward<A>(_a));
    }

    void doStart(ServiceStartStatus& _status, Configuration&& _ucfg);
    void doStart(ServiceStartStatus& _status);

    template <typename A>
    void doStart(ServiceStartStatus& _status, Configuration&& _ucfg, A&& _a)
    {
        Configuration cfg;
        SocketDevice  sd;

        cfg.reset(std::move(_ucfg));
        cfg.check();
        cfg.prepare();
        cfg.createListenerDevice(sd);

        Service::doStartWithAny(
            std::forward<A>(_a),
            [this, &cfg, &sd, &_status](std::unique_lock<std::mutex>& _lock) {
                doFinalizeStart(_status, std::move(cfg), std::move(sd), _lock);
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
    friend class ClientConnection;
    friend class openssl::SocketStub;
    friend struct ConnectionContext;

    Configuration const& configuration() const;
    ServiceStatistic&    wstatistic();

    std::shared_ptr<Data> acquire(std::unique_lock<std::mutex>& _lock);
    std::shared_ptr<Data> acquire();

    void doFinalizeStart(ServiceStartStatus& _status, Configuration&& _ucfg, SocketDevice&& _usd, std::unique_lock<std::mutex>& _lock);
    void doFinalizeStart(ServiceStartStatus& _status, std::unique_lock<std::mutex>& _lock);

    void acceptIncomingConnection(SocketDevice& _rsd);

    ErrorConditionT activateConnection(ConnectionContext& _rconctx, ActorIdT const& _ractui);

    void connectionStop(ConnectionContext& _rconctx);

    bool connectionStopping(
        ConnectionContext&         _rconctx,
        ActorIdT const&            _ractuid,
        std::chrono::milliseconds& _rwait_duration,
        MessageId&                 _rmsg_id,
        MessageBundle*             _pmsg_bundle,
        EventT&                    _revent_context,
        ErrorConditionT&           _rerror);

    void onIncomingConnectionStart(ConnectionContext& _rconctx);
    void onOutgoingConnectionStart(ConnectionContext& _rconctx);

    ErrorConditionT pollPoolForUpdates(
        Connection&      _rcon,
        ActorIdT const&  _ractuid,
        MessageId const& _rmsgid, bool& _rmore);

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

    void forwardResolveMessage(ConnectionPoolId const& _rconpoolid, EventBase& _revent);

    ErrorConditionT doSendMessage(
        const RecipientUrl&       _recipient_url,
        MessagePointerT<>&        _rmsgptr,
        MessageCompleteFunctionT& _rcomplete_fnc,
        RecipientId*              _precipient_id_out,
        MessageId*                _pmsg_id_out,
        const MessageFlagsT&      _flags);
    ErrorConditionT doSendMessageUsingConnectionContext(
        const RecipientUrl&       _recipient_url,
        MessagePointerT<>&        _rmsgptr,
        MessageCompleteFunctionT& _rcomplete_fnc,
        RecipientId*              _precipient_id_out,
        MessageId*                _pmsg_id_out,
        MessageFlagsT             _flags);

    ErrorConditionT doCreateConnectionPool(
        std::string_view      _url,
        RecipientId&          _rrecipient_id_out,
        PoolOnEventFunctionT& _event_fnc,
        const size_t          _persistent_connection_count);

    ErrorConditionT doForceCloseConnectionPool(
        RecipientId const&        _rrecipient_id,
        MessageCompleteFunctionT& _rcomplete_fnc);

    ErrorConditionT doDelayCloseConnectionPool(
        RecipientId const&        _rrecipient_id,
        MessageCompleteFunctionT& _rcomplete_fnc);
    void onLockedStoppingBeforeActors() override;
};

//-------------------------------------------------------------------------

using ServiceT = frame::ServiceShell<Service>;

//-------------------------------------------------------------------------
template <class T>
ErrorConditionT Service::sendMessage(
    const RecipientUrl&       _recipient_url,
    MessagePointerT<T> const& _rmsgptr,
    const MessageFlagsT&      _flags)
{
    auto                     msgptr(solid::static_pointer_cast<Message>(_rmsgptr));
    MessageCompleteFunctionT complete_handler;
    return doSendMessage(_recipient_url, msgptr, complete_handler, nullptr, nullptr, _flags);
}
//-------------------------------------------------------------------------
template <class T>
ErrorConditionT Service::sendMessage(
    const RecipientUrl&       _recipient_url,
    MessagePointerT<T> const& _rmsgptr,
    RecipientId&              _rrecipient_id,
    const MessageFlagsT&      _flags)
{
    auto                     msgptr(solid::static_pointer_cast<Message>(_rmsgptr));
    MessageCompleteFunctionT complete_handler;
    return doSendMessage(_recipient_url, msgptr, complete_handler, &_rrecipient_id, nullptr, _flags);
}
//-------------------------------------------------------------------------
template <class T>
ErrorConditionT Service::sendMessage(
    const RecipientUrl&       _recipient_url,
    MessagePointerT<T> const& _rmsgptr,
    RecipientId&              _rrecipient_id,
    MessageId&                _rmsg_id,
    const MessageFlagsT&      _flags)
{
    auto                     msgptr(solid::static_pointer_cast<Message>(_rmsgptr));
    MessageCompleteFunctionT complete_handler;
    return doSendMessage(_recipient_url, msgptr, complete_handler, &_rrecipient_id, &_rmsg_id, _flags);
}
//-------------------------------------------------------------------------
template <class T>
ErrorConditionT Service::sendMessage(
    const RecipientUrl&       _recipient_url,
    MessagePointerT<T> const& _rmsgptr,
    MessageId&                _rmsg_id,
    const MessageFlagsT&      _flags)
{
    auto                     msgptr(solid::static_pointer_cast<Message>(_rmsgptr));
    MessageCompleteFunctionT complete_handler;
    return doSendMessage(_recipient_url, msgptr, complete_handler, nullptr, &_rmsg_id, _flags);
}
//-------------------------------------------------------------------------
// send request using recipient name --------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendRequest(
    const RecipientUrl&       _recipient_url,
    MessagePointerT<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    auto                     msgptr(solid::static_pointer_cast<Message>(_rmsgptr));
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(_recipient_url, msgptr, complete_handler, nullptr, nullptr, _flags | MessageFlagsE::AwaitResponse);
}
//-------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendRequest(
    const RecipientUrl&       _recipient_url,
    MessagePointerT<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    RecipientId&              _rrecipient_id,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    auto                     msgptr(solid::static_pointer_cast<Message>(_rmsgptr));
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(_recipient_url, msgptr, complete_handler, &_rrecipient_id, nullptr, _flags | MessageFlagsE::AwaitResponse);
}
//-------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendRequest(
    const RecipientUrl&       _recipient_url,
    MessagePointerT<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    RecipientId&              _rrecipient_id,
    MessageId&                _rmsguid,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    auto                     msgptr(solid::static_pointer_cast<Message>(_rmsgptr));
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(_recipient_url, msgptr, complete_handler, &_rrecipient_id, &_rmsguid, _flags | MessageFlagsE::AwaitResponse);
}
//-------------------------------------------------------------------------
// send response using recipient id ---------------------------------------

template <class T>
ErrorConditionT Service::sendResponse(
    RecipientId const&        _rrecipient_id,
    MessagePointerT<T> const& _rmsgptr,
    const MessageFlagsT&      _flags)
{
    auto                     msgptr(solid::static_pointer_cast<Message>(_rmsgptr));
    MessageCompleteFunctionT complete_handler;
    return doSendMessage(_rrecipient_id, msgptr, complete_handler, nullptr, nullptr, _flags | MessageFlagsE::Response);
}

//-------------------------------------------------------------------------

template <class T>
ErrorConditionT Service::sendResponse(
    RecipientId const&        _rrecipient_id,
    MessagePointerT<T> const& _rmsgptr,
    MessageId&                _rmsg_id,
    const MessageFlagsT&      _flags)
{
    auto                     msgptr(solid::static_pointer_cast<Message>(_rmsgptr));
    MessageCompleteFunctionT complete_handler;
    return doSendMessage(_rrecipient_id, msgptr, complete_handler, nullptr, &_rmsg_id, _flags | MessageFlagsE::Response);
}

//-------------------------------------------------------------------------
// send message with complete using recipient name ------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendMessage(
    const RecipientUrl&       _recipient_url,
    MessagePointerT<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    auto                     msgptr(solid::static_pointer_cast<Message>(_rmsgptr));
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(_recipient_url, msgptr, complete_handler, nullptr, nullptr, _flags);
}
//-------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendMessage(
    const RecipientUrl&       _recipient_url,
    MessagePointerT<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    RecipientId&              _rrecipient_id,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    auto                     msgptr(solid::static_pointer_cast<Message>(_rmsgptr));
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(_recipient_url, msgptr, complete_handler, &_rrecipient_id, nullptr, _flags);
}
//-------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendMessage(
    const RecipientUrl&       _recipient_url,
    MessagePointerT<T> const& _rmsgptr,
    Fnc                       _complete_fnc,
    RecipientId&              _rrecipient_id,
    MessageId&                _rmsguid,
    const MessageFlagsT&      _flags)
{
    using CompleteHandlerT = CompleteHandler<Fnc,
        typename message_complete_traits<decltype(_complete_fnc)>::send_type,
        typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

    auto                     msgptr(solid::static_pointer_cast<Message>(_rmsgptr));
    CompleteHandlerT         fnc(std::forward<Fnc>(_complete_fnc));
    MessageCompleteFunctionT complete_handler(std::move(fnc));

    return doSendMessage(_recipient_url, msgptr, complete_handler, &_rrecipient_id, &_rmsguid, _flags);
}
//-------------------------------------------------------------------------
// send message using ConnectionContext  ----------------------------------

template <class T>
ErrorConditionT Service::sendResponse(
    ConnectionContext&        _rctx,
    MessagePointerT<T> const& _rmsgptr,
    const MessageFlagsT&      _flags)
{
    auto                     msgptr(solid::static_pointer_cast<Message>(_rmsgptr));
    MessageCompleteFunctionT complete_handler;
    return doSendMessage(_rctx, msgptr, complete_handler, nullptr, nullptr, _flags | MessageFlagsE::Response);
}

//-------------------------------------------------------------------------
template <typename F>
ErrorConditionT Service::forceCloseConnectionPool(
    RecipientId const& _rrecipient_id,
    F                  _f)
{
    auto fnc = [_f](ConnectionContext& _rctx, MessagePointerT<>& /*_rmsgptr*/, MessagePointerT<>&, ErrorConditionT const& /*_err*/) {
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
    auto fnc = [_f](ConnectionContext& _rctx, MessagePointerT<>& /*_rmsgptr*/, MessagePointerT<>&, ErrorConditionT const& /*_err*/) {
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
    const size_t       _send_buffer_capacity /* = 0*/ // 0 means: leave as it is
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
    const size_t       _send_buffer_capacity // 0 means: leave as it is
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
inline ErrorConditionT Service::createConnectionPool(const std::string_view& _url, const size_t _persistent_connection_count)
{
    RecipientId          recipient_id;
    PoolOnEventFunctionT fnc([](ConnectionContext& _rctx, EventBase&&, const ErrorConditionT&) {});
    return doCreateConnectionPool(_url, recipient_id, fnc, _persistent_connection_count);
}
//-------------------------------------------------------------------------
template <class F>
ErrorConditionT Service::createConnectionPool(
    const std::string_view& _url,
    RecipientId&            _rrecipient_id,
    const F                 _event_fnc,
    const size_t            _persistent_connection_count)
{
    PoolOnEventFunctionT fnc(_event_fnc);
    return doCreateConnectionPool(_url, _rrecipient_id, fnc, _persistent_connection_count);
}
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
} // namespace mprpc
} // namespace frame
} // namespace solid
