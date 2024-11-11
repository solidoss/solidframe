// solid/frame/mprpc/mprpcconfiguration.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/flags.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"

#include "solid/frame/mprpc/mprpcsocketstub.hpp"

#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpcprotocol.hpp"

#include "solid/frame/scheduler.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/sharedbuffer.hpp"
#include <vector>

namespace solid {

namespace frame {

namespace aio {
class Resolver;
struct ActorProxy;
} // namespace aio

namespace mprpc {

enum struct ConnectionValues : size_t {
    SocketEmplacementSize = 128
};

class Service;
class Connection;
class MessageWriter;
struct ConnectionContext;
class Configuration;

typedef void (*OnSecureConnectF)(frame::aio::ReactorContext&);
typedef void (*OnSecureAcceptF)(frame::aio::ReactorContext&);

enum struct RelayDataFlagsE : uint8_t {
    First,
    Last,
    LastFlag,
};

using RelayDataFlagsT = Flags<RelayDataFlagsE>;

std::ostream& operator<<(std::ostream& _ros, const RelayDataFlagsT& _flags);

struct RelayData {
    SharedBuffer          buffer_;
    const char*           pdata_     = nullptr;
    size_t                data_size_ = 0;
    RelayData*            pnext_     = nullptr;
    RelayDataFlagsT       flags_;
    MessageHeader::FlagsT message_flags_   = 0;
    MessageHeader*        pmessage_header_ = nullptr;

    RelayData() = default;

    RelayData(
        RelayData&& _rrelmsg) noexcept
        : buffer_(std::move(_rrelmsg.buffer_))
        , pdata_(_rrelmsg.pdata_)
        , data_size_(_rrelmsg.data_size_)
        , pnext_(nullptr)
        , flags_(_rrelmsg.flags_)
        , message_flags_(_rrelmsg.message_flags_)
        , pmessage_header_(_rrelmsg.pmessage_header_)
    {
    }

    RelayData& operator=(RelayData&& _rrelmsg) noexcept
    {
        buffer_          = std::move(_rrelmsg.buffer_);
        pdata_           = _rrelmsg.pdata_;
        data_size_       = _rrelmsg.data_size_;
        pnext_           = _rrelmsg.pnext_;
        flags_           = _rrelmsg.flags_;
        message_flags_   = _rrelmsg.message_flags_;
        pmessage_header_ = _rrelmsg.pmessage_header_;
        return *this;
    }

    RelayData(const RelayData&)            = delete;
    RelayData& operator=(const RelayData&) = delete;

    void clear()
    {
        pdata_     = nullptr;
        data_size_ = 0;
        // connection_id_.clear();
        buffer_.reset();
        pnext_ = nullptr;
        flags_.reset();
        message_flags_   = 0;
        pmessage_header_ = nullptr;
    }

    bool isMessageBegin() const
    {
        return flags_.has(RelayDataFlagsE::First);
    }

    bool isMessageEnd() const
    {
        return flags_.has(RelayDataFlagsE::Last);
    }

    bool isMessageLast() const
    {
        return isMessageEnd() && !Message::is_response_part(this->message_flags_);
    }

    bool isMessagePart() const
    {
        return Message::is_response_part(this->message_flags_);
    }

    bool isRequest() const
    {
        return Message::is_awaiting_response(this->message_flags_);
    }

private:
    friend class RelayConnection;
    RelayData(
        const SharedBuffer& _buffer,
        const char*         _pdata,
        size_t              _data_size,
        const bool          _is_last)
        : buffer_(_buffer)
        , pdata_(_pdata)
        , data_size_(_data_size)
    {
        if (_is_last) {
            flags_.set(RelayDataFlagsE::Last);
        }
    }
};

enum struct RelayEngineNotification {
    NewData,
    DoneData,
};

class RelayEngine {

protected:
    using PushFunctionT   = solid_function_t(bool(RelayData*&, const MessageId&, MessageId&, bool&));
    using DoneFunctionT   = solid_function_t(void(SharedBuffer&));
    using CancelFunctionT = solid_function_t(void(const MessageHeader&));

    RelayEngine() {}
    virtual ~RelayEngine();

    virtual bool notifyConnection(Manager& _rm, const ActorIdT& _rrelay_uid, const RelayEngineNotification _what);

private:
    friend class RelayConnection;
    friend class Configuration;
    friend class Service;

    static RelayEngine& instance();

    virtual void stopConnection(const UniqueId& _rrelay_uid);

    // NOTE: we require _rmsghdr parameter because the relay function
    //  will know if it can move it into _rrelay_data.message_header_ (for unicasts)
    //  or copy it in case of multicasts
    virtual bool doRelayStart(
        const ActorIdT&  _rcon_uid,
        UniqueId&        _rrelay_uid,
        MessageHeader&   _rmsghdr,
        RelayData&&      _urelay_data,
        MessageId&       _rrelay_id,
        ErrorConditionT& _rerror);

    virtual bool doRelayResponse(
        const UniqueId&  _rrelay_uid,
        MessageHeader&   _rmsghdr,
        RelayData&&      _urelay_data,
        const MessageId& _rrelay_id,
        ErrorConditionT& _rerror);

    virtual bool doRelay(
        const UniqueId&  _rrelay_uid,
        RelayData&&      _urelay_data,
        const MessageId& _rrelay_id,
        ErrorConditionT& _rerror);

    virtual void doComplete(
        const UniqueId& _rrelay_uid,
        RelayData* /*_prelay_data*/,
        MessageId const& /*_rengine_msg_id*/,
        bool& /*_rmore*/);

    virtual void doCancel(
        const UniqueId& _rrelay_uid,
        RelayData* /*_prelay_data*/,
        MessageId const& /*_rengine_msg_id*/,
        DoneFunctionT& /*_done_fnc*/);

    virtual void doPollNew(const UniqueId& _rrelay_uid, PushFunctionT& /*_try_push_fnc*/, bool& /*_rmore*/);
    virtual void doPollDone(const UniqueId& _rrelay_uid, DoneFunctionT& /*_done_fnc*/, CancelFunctionT& /*_cancel_fnc*/);

    template <class F>
    void pollNew(const UniqueId& _rrelay_uid, F& _try_push, bool& _rmore)
    {
        PushFunctionT try_push_fnc{std::ref(_try_push)};
        doPollNew(_rrelay_uid, try_push_fnc, _rmore);
    }

    template <class DF, class CF>
    void pollDone(const UniqueId& _rrelay_uid, DF& _done_fnc, CF& _cancel_fnc)
    {
        DoneFunctionT   done_fnc{std::ref(_done_fnc)};
        CancelFunctionT cancel_fnc{std::ref(_cancel_fnc)};

        doPollDone(_rrelay_uid, done_fnc, cancel_fnc);
    }

    void complete(const UniqueId& _rrelay_uid, RelayData* _prelay_data, MessageId const& _rengine_msg_id, bool& _rmore)
    {
        doComplete(_rrelay_uid, _prelay_data, _rengine_msg_id, _rmore);
    }

    template <class DF>
    void cancel(const UniqueId& _rrelay_uid, RelayData* _prelay_data, MessageId const& _rengine_msg_id, DF& _done_fnc)
    {
        DoneFunctionT done_fnc{std::ref(_done_fnc)};
        doCancel(_rrelay_uid, _prelay_data, _rengine_msg_id, done_fnc);
    }

    bool relayStart(
        const ActorIdT&  _rcon_uid,
        UniqueId&        _rrelay_uid,
        MessageHeader&   _rmsghdr,
        RelayData&&      _urelay_data,
        MessageId&       _rrelay_id,
        ErrorConditionT& _rerror)
    {
        return doRelayStart(_rcon_uid, _rrelay_uid, _rmsghdr, std::move(_urelay_data), _rrelay_id, _rerror);
    }

    bool relayResponse(
        const UniqueId&  _rrelay_uid,
        MessageHeader&   _rmsghdr,
        RelayData&&      _urelay_data,
        const MessageId& _rrelay_id,
        ErrorConditionT& _rerror)
    {
        return doRelayResponse(_rrelay_uid, _rmsghdr, std::move(_urelay_data), _rrelay_id, _rerror);
    }

    bool relay(
        const UniqueId&  _rrelay_uid,
        RelayData&&      _urelay_data,
        const MessageId& _rrelay_id,
        ErrorConditionT& _rerror)
    {
        return doRelay(_rrelay_uid, std::move(_urelay_data), _rrelay_id, _rerror);
    }
};

using AddressVectorT                            = std::vector<SocketAddressInet>;
using ServerSetupSocketDeviceFunctionT          = solid_function_t(bool(SocketDevice&));
using ClientSetupSocketDeviceFunctionT          = solid_function_t(bool(SocketDevice&));
using ResolveCompleteFunctionT                  = solid_function_t(void(AddressVectorT&&));
using ConnectionStopFunctionT                   = solid_function_t(void(ConnectionContext&));
using ConnectionStartFunctionT                  = solid_function_t(void(ConnectionContext&));
using SendAllocateBufferFunctionT               = solid_function_t(SharedBuffer(const uint32_t));
using RecvAllocateBufferFunctionT               = solid_function_t(SharedBuffer(const uint32_t));
using CompressFunctionT                         = solid_function_t(size_t(char*, size_t, ErrorConditionT&));
using UncompressFunctionT                       = solid_function_t(size_t(char*, const char*, size_t, ErrorConditionT&));
using ConnectionEnterActiveCompleteFunctionT    = solid_function_t(void(ConnectionContext&, ErrorConditionT const&));
using ConnectionPostCompleteFunctionT           = solid_function_t(void(ConnectionContext&));
using ConnectionSendTimeoutSoftFunctionT        = solid_function_t(void(ConnectionContext&));
using ConnectionEnterPassiveCompleteFunctionT   = solid_function_t(void(ConnectionContext&, ErrorConditionT const&));
using ConnectionSecureHandhakeCompleteFunctionT = solid_function_t(void(ConnectionContext&, ErrorConditionT const&));
using ConnectionSendRawDataCompleteFunctionT    = solid_function_t(void(ConnectionContext&, ErrorConditionT const&));
using ConnectionRecvRawDataCompleteFunctionT    = solid_function_t(void(ConnectionContext&, const char*, size_t&, ErrorConditionT const&));
using ConnectionOnEventFunctionT                = solid_function_t(void(ConnectionContext&, EventBase&));
using PoolOnEventFunctionT                      = solid_function_t(void(ConnectionContext&, EventBase&&, const ErrorConditionT&));
using ActorCreateFunctionT                      = solid_function_t(ActorIdT(aio::ActorPointerT&&, frame::Service&, EventBase&&, ErrorConditionT&));

enum struct ConnectionState {
    Raw,
    Passive,
    Active
};

struct ReaderConfiguration {
    ReaderConfiguration();

    size_t              max_message_count_multiplex;
    UncompressFunctionT decompress_fnc;
};

struct WriterConfiguration {
    WriterConfiguration();

    size_t            max_message_count_multiplex;
    size_t            max_message_count_response_wait;
    size_t            max_message_continuous_packet_count;
    CompressFunctionT inplace_compress_fnc;
};

class Configuration {
    friend class Service;

    Configuration(Configuration&&)                 = default;
    Configuration& operator=(const Configuration&) = delete;
    Configuration& operator=(Configuration&&)      = default;
    Configuration()                                = default;

    RelayEngine* prelayengine = nullptr;

public:
    size_t pool_max_active_connection_count;
    size_t pool_max_pending_connection_count;
    size_t pool_max_message_queue_size;

    size_t pool_count;
    size_t pool_mutex_count;
    bool   relay_enabled;

    ReaderConfiguration reader;
    WriterConfiguration writer;

    std::chrono::milliseconds          connection_timeout_recv                  = std::chrono::minutes(10);
    std::chrono::milliseconds          connection_timeout_send_soft             = std::chrono::seconds(10);
    std::chrono::milliseconds          connection_timeout_send_hard             = std::chrono::minutes(5);
    uint8_t                            connection_recv_buffer_start_capacity_kb = 0;
    uint8_t                            connection_recv_buffer_max_capacity_kb   = 64;
    uint8_t                            connection_send_buffer_start_capacity_kb = 0;
    uint8_t                            connection_send_buffer_max_capacity_kb   = 64;
    uint16_t                           connection_relay_buffer_count            = 8;
    ConnectionStopFunctionT            connection_stop_fnc;
    ConnectionOnEventFunctionT         connection_on_event_fnc;
    ConnectionSendTimeoutSoftFunctionT connection_on_send_timeout_soft_ = [](ConnectionContext&) {};
    RecvAllocateBufferFunctionT        connection_recv_buffer_allocate_fnc;
    SendAllocateBufferFunctionT        connection_send_buffer_allocate_fnc;
    Protocol::PointerT                 protocol_ptr;
    ActorCreateFunctionT               actor_create_fnc;

    struct Server {
        using ConnectionCreateSocketFunctionT    = solid_function_t(SocketStubPtrT(Configuration const&, frame::aio::ActorProxy const&, SocketDevice&&, char*));
        using ConnectionSecureHandshakeFunctionT = solid_function_t(void(ConnectionContext&));

        ConnectionCreateSocketFunctionT    connection_create_socket_fnc;
        ConnectionState                    connection_start_state                = ConnectionState::Passive;
        bool                               connection_start_secure               = true;
        uint32_t                           connection_inactivity_keepalive_count = 20; // server error if receives more than inactivity_keepalive_count keep alive messages during inactivity_timeout interval
        ConnectionStartFunctionT           connection_start_fnc;
        ConnectionSecureHandshakeFunctionT connection_on_secure_handshake_fnc;
        std::chrono::milliseconds          connection_timeout_active = std::chrono::seconds(300);
        std::chrono::milliseconds          connection_timeout_secure = std::chrono::seconds(10);
        ServerSetupSocketDeviceFunctionT   socket_device_setup_fnc;
        std::string                        listener_address_str;
        std::string                        listener_service_str;
        Any<>                              secure_any;

        Server()
        {
        }

        bool hasSecureConfiguration() const
        {
            return !secure_any.empty();
        }

        bool hasConnectionTimeoutSecure() const
        {
            return connection_timeout_secure.count() != 0;
        }

        bool hasConnectionTimeoutActive() const
        {
            return connection_timeout_active.count() != 0;
        }

    private:
        friend class Service;
    } server;

    struct Client {
        using ConnectionCreateSocketFunctionT    = solid_function_t(SocketStubPtrT(Configuration const&, frame::aio::ActorProxy const&, char*));
        using AsyncResolveFunctionT              = solid_function_t(void(const std::string&, ResolveCompleteFunctionT&));
        using ConnectionSecureHandshakeFunctionT = solid_function_t(void(ConnectionContext&));

        std::chrono::milliseconds          connection_timeout_reconnect = std::chrono::seconds(10);
        std::chrono::milliseconds          connection_timeout_keepalive = std::chrono::seconds(60);
        uint32_t                           connection_reconnect_steps   = 10;
        ConnectionCreateSocketFunctionT    connection_create_socket_fnc;
        ConnectionState                    connection_start_state  = ConnectionState::Passive;
        bool                               connection_start_secure = true;
        ConnectionStartFunctionT           connection_start_fnc;
        AsyncResolveFunctionT              name_resolve_fnc;
        ConnectionSecureHandshakeFunctionT connection_on_secure_handshake_fnc;
        ClientSetupSocketDeviceFunctionT   socket_device_setup_fnc;
        Any<>                              secure_any;

        bool hasSecureConfiguration() const
        {
            return !secure_any.empty();
        }

        bool hasConnectionTimeoutKeepAlive() const
        {
            return connection_timeout_keepalive.count() != 0;
        }

        std::chrono::milliseconds connectionReconnectTimeout(
            const uint8_t _retry_count,
            const bool    _failed_create_connection_actor,
            const bool    _last_connection_was_connected,
            const bool    _last_connection_was_active,
            const bool    _last_connection_was_secured) const;

    } client;

    template <class Sched, class Proto>
    Configuration(
        Sched&                  _rsch,
        std::shared_ptr<Proto>& _rprotcol_ptr)
        : prelayengine(&RelayEngine::instance())
        , pool_count(1024 * 10)
        , pool_mutex_count(1024)
        , protocol_ptr(std::static_pointer_cast<Protocol>(_rprotcol_ptr))
    {
        actor_create_fnc = [&_rsch](aio::ActorPointerT&& _actor_ptr, frame::Service& _rsvc, EventBase&& _event, ErrorConditionT& _rerror) {
            return _rsch.startActor(std::move(_actor_ptr), _rsvc, std::move(_event), _rerror);
        };
        init();
    }

    template <class Sched, class Proto>
    Configuration(
        Sched&                  _rsch,
        RelayEngine&            _rrelayengine,
        std::shared_ptr<Proto>& _rprotcol_ptr)
        : prelayengine(&_rrelayengine)
        , pool_count(1024 * 10)
        , pool_mutex_count(1024)
        , protocol_ptr(std::static_pointer_cast<Protocol>(_rprotcol_ptr))
    {
        actor_create_fnc = [&_rsch](aio::ActorPointerT&& _actor_ptr, frame::Service& _rsvc, EventBase&& _event, ErrorConditionT& _rerror) {
            return _rsch.startActor(std::move(_actor_ptr), _rsvc, std::move(_event), _rerror);
        };
        init();
    }

    bool isEnabledRelayEngine() const
    {
        return relay_enabled /*  && prelayengine != &RelayEngine::instance() */;
    }

    Configuration& reset(Configuration&& _ucfg)
    {
        *this = std::move(_ucfg);
        prepare();
        return *this;
    }

    void clear(Configuration&& _ucfg)
    {
        _ucfg = std::move(*this);
        Configuration cfg;
        *this = std::move(cfg);
    }

    RelayEngine& relayEngine() const
    {
        return *prelayengine;
    }

    bool isServer() const
    {
        return server.listener_address_str.size() != 0;
    }

    bool isClient() const
    {
        return !solid_function_empty(client.name_resolve_fnc);
    }

    bool isServerOnly() const
    {
        return isServer() && !isClient();
    }

    bool isClientOnly() const
    {
        return !isServer() && isClient();
    }

    SharedBuffer allocateRecvBuffer() const;

    SharedBuffer allocateSendBuffer() const;

    void check() const;

    Protocol& protocol()
    {
        return *protocol_ptr;
    }

    const Protocol& protocol() const
    {
        return *protocol_ptr;
    }

    bool hasConnectionTimeoutRecv() const
    {
        return connection_timeout_recv.count() != 0;
    }

    bool hasConnectionTimeoutSendSoft() const
    {
        return connection_timeout_send_soft.count() != 0;
    }
    bool hasConnectionTimeoutSendHard() const
    {
        return connection_timeout_send_hard.count() != 0;
    }

private:
    void init();
    void prepare();
    void createListenerDevice(SocketDevice& _rsd) const;
};

class InternetResolverF {
    aio::Resolver&           rresolver_;
    const std::string        default_host_;
    const std::string        default_service_;
    const SocketInfo::Family family_;

public:
    InternetResolverF(
        aio::Resolver&     _rresolver,
        const char*        _default_service,
        const char*        _default_host = "localhost",
        SocketInfo::Family _family       = SocketInfo::AnyFamily)
        : rresolver_(_rresolver)
        , default_host_(_default_host)
        , default_service_(_default_service)
        , family_(_family)
    {
    }

    InternetResolverF(
        aio::Resolver&     _rresolver,
        const std::string& _default_service,
        const std::string& _default_host = "localhost",
        SocketInfo::Family _family       = SocketInfo::AnyFamily)
        : rresolver_(_rresolver)
        , default_host_(_default_host)
        , default_service_(_default_service)
        , family_(_family)
    {
    }

    void operator()(const std::string&, ResolveCompleteFunctionT&);
};

} // namespace mprpc
} // namespace frame
} // namespace solid
