// solid/frame/ipc/src/ipcconfiguration.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "mprpcconnection.hpp"
#include "solid/frame/mprpc/mprpcerror.hpp"
#include "solid/frame/mprpc/mprpcsocketstub_plain.hpp"

#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/memory.hpp"

#include <cstring>

namespace solid {
namespace frame {
namespace mprpc {
//-----------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& _ros, const RelayDataFlagsT& _flags)
{
    bool b = false;
    if (_flags.has(RelayDataFlagsE::First)) {
        _ros << "First";
        b = true;
    }
    if (_flags.has(RelayDataFlagsE::Last)) {
        if (b)
            _ros << "|";
        _ros << "Last";
        b = true;
    }
    return _ros;
}

/*virtual*/ BufferBase::~BufferBase()
{
}

RecvBufferPointerT make_recv_buffer(const size_t _cp)
{
    switch (_cp) {
    case 512:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<512>>());
    case 1 * 1024:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<1 * 1024>>());
    case 2 * 1024:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<2 * 1024>>());
    case 4 * 1024:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<4 * 1024>>());
    case 8 * 1024:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<8 * 1024>>());
    case 16 * 1024:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<16 * 1024>>());
    case 32 * 1024:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<32 * 1024>>());
    case 64 * 1024:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<64 * 1024>>());
    default:
        return std::make_shared<Buffer<0>>(_cp);
    }
}

namespace {
RecvBufferPointerT default_allocate_recv_buffer(const uint32_t _cp)
{
    return make_recv_buffer(_cp);
}

SendBufferPointerT default_allocate_send_buffer(const uint32_t _cp)
{
    return SendBufferPointerT(new char[_cp]);
}

// void empty_reset_serializer_limits(ConnectionContext &, serialization::binary::Limits&){}

void empty_connection_stop(ConnectionContext&) {}

void empty_connection_start(ConnectionContext&) {}

void empty_connection_on_event(ConnectionContext&, Event&) {}

size_t default_compress(char*, size_t, ErrorConditionT&)
{
    return 0;
}

size_t default_decompress(char*, const char*, size_t, ErrorConditionT& _rerror)
{
    // This should never be called
    _rerror = error_compression_unavailable;
    return 0;
}

SocketStubPtrT default_create_client_socket(Configuration const& _rcfg, frame::aio::ActorProxy const& _rproxy, char* _emplace_buf)
{
    return plain::create_client_socket(_rcfg, _rproxy, _emplace_buf);
}

SocketStubPtrT default_create_server_socket(Configuration const& _rcfg, frame::aio::ActorProxy const& _rproxy, SocketDevice&& _usd, char* _emplace_buf)
{
    return plain::create_server_socket(_rcfg, _rproxy, std::move(_usd), _emplace_buf);
}

const char* default_extract_recipient_name(const char* _purl, std::string& _msgurl, std::string& _tmpstr)
{
    solid_assert_log(_purl != nullptr, service_logger());

    const char* const p = strchr(_purl, '/');

    if (p == nullptr) {
        return _purl;
    }
    _msgurl = (p + 1);
    _tmpstr.assign(_purl, p - _purl);
    return _tmpstr.c_str();
}

// const char* default_fast_extract_recipient_name(const char* _purl, std::string& _msgurl, std::string& _tmpstr)
// {
//
//     return _purl;
// }

bool default_setup_socket_device(SocketDevice& _rsd)
{
    _rsd.enableNoDelay();
    _rsd.enableNoSignal();
    return true;
}

} // namespace

ReaderConfiguration::ReaderConfiguration()
{
    max_message_count_multiplex = 64 + 128;

    decompress_fnc = &default_decompress;
}

WriterConfiguration::WriterConfiguration()
{
    max_message_count_multiplex = 64;

    max_message_continuous_packet_count = 4;
    max_message_count_response_wait     = 128;
    inplace_compress_fnc                = &default_compress;
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/*static*/ RelayEngine& RelayEngine::instance()
{
    static RelayEngine eng;
    return eng;
}
//-----------------------------------------------------------------------------
bool RelayEngine::notifyConnection(Manager& _rm, const ActorIdT& _rrelay_uid, const RelayEngineNotification _what)
{
    return Connection::notify(_rm, _rrelay_uid, _what);
}
//-----------------------------------------------------------------------------
/*virtual*/ RelayEngine::~RelayEngine()
{
}
//-----------------------------------------------------------------------------
/*virtual*/ void RelayEngine::stopConnection(const UniqueId& /*_rrelay_uid*/)
{
}
//-----------------------------------------------------------------------------
/*virtual*/ bool RelayEngine::doRelayStart(
    const ActorIdT& /*_rcon_uid*/,
    UniqueId& /*_rrelay_uid*/,
    MessageHeader& /*_rmsghdr*/,
    RelayData&& /*_urelay_data*/,
    MessageId& /*_rrelay_id*/,
    ErrorConditionT& /*_rerror*/)
{
    return false;
}
//-----------------------------------------------------------------------------
/*virtual*/ bool RelayEngine::doRelayResponse(
    const UniqueId& /*_rrelay_uid*/,
    MessageHeader& /*_rmsghdr*/,
    RelayData&& /*_urelay_data*/,
    const MessageId& /*_rrelay_id*/,
    ErrorConditionT& /*_rerror*/)
{
    return false;
}
//-----------------------------------------------------------------------------
/*virtual*/ bool RelayEngine::doRelay(
    const UniqueId& /*_rrelay_uid*/,
    RelayData&& /*_urelay_data*/,
    const MessageId& /*_rrelay_id*/,
    ErrorConditionT& /*_rerror*/)
{
    return false;
}
//-----------------------------------------------------------------------------
/*virtual*/ void RelayEngine::doComplete(
    const UniqueId& /*_rrelay_uid*/,
    RelayData* /*_prelay_data*/,
    MessageId const& /*_rengine_msg_id*/,
    bool& /*_rmore*/
)
{
    solid_throw_log(service_logger(), "should not be called");
}
//-----------------------------------------------------------------------------
/*virtual*/ void RelayEngine::doCancel(
    const UniqueId& /*_rrelay_uid*/,
    RelayData* /*_prelay_data*/,
    MessageId const& /*_rengine_msg_id*/,
    DoneFunctionT& /*_done_fnc*/
)
{
    solid_throw_log(service_logger(), "should not be called");
}
//-----------------------------------------------------------------------------
/*virtual*/ void RelayEngine::doPollNew(const UniqueId& /*_rrelay_uid*/, PushFunctionT& /*_try_push_fnc*/, bool& /*_rmore*/)
{
    solid_throw_log(service_logger(), "should not be called");
}
//-----------------------------------------------------------------------------
/*virtual*/ void RelayEngine::doPollDone(const UniqueId& /*_rrelay_uid*/, DoneFunctionT& /*_done_fnc*/, CancelFunctionT& /*_cancel_fnc*/)
{
    solid_throw_log(service_logger(), "should not be called");
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Configuration::init()
{
    connection_recv_buffer_start_capacity_kb = static_cast<uint8_t>(memory_page_size() / 1024);
    connection_send_buffer_start_capacity_kb = static_cast<uint8_t>(memory_page_size() / 1024);

    connection_recv_buffer_max_capacity_kb = connection_send_buffer_max_capacity_kb = 64;

    connection_timeout_inactivity_seconds = 60 * 10; // ten minutes
    connection_timeout_keepalive_seconds  = 60 * 5; // five minutes
    connection_timeout_reconnect_seconds  = 10;

    connection_relay_buffer_count = 8;

    // server closes connection when receiving more than connection_inactivity_keepalive_count
    // keep-alive requests during connection_timeout_inactivity_seconds
    connection_inactivity_keepalive_count = 10;

    server.connection_start_state                = ConnectionState::Passive;
    server.connection_start_secure               = true;
    server.connection_timeout_activation_seconds = 60 * 5;
    server.connection_timeout_secured_seconds    = 60;

    client.connection_start_state  = ConnectionState::Passive;
    client.connection_start_secure = true;

    connection_recv_buffer_allocate_fnc = &default_allocate_recv_buffer;
    connection_send_buffer_allocate_fnc = &default_allocate_send_buffer;

    connection_stop_fnc = &empty_connection_stop;

    server.connection_start_fnc = &empty_connection_start;
    client.connection_start_fnc = &empty_connection_start;

    connection_on_event_fnc = &empty_connection_on_event;

    client.connection_create_socket_fnc = &default_create_client_socket;
    server.connection_create_socket_fnc = &default_create_server_socket;

    server.socket_device_setup_fnc = &default_setup_socket_device;
    client.socket_device_setup_fnc = &default_setup_socket_device;

    extract_recipient_name_fnc = &default_extract_recipient_name;

    pool_max_active_connection_count  = 1;
    pool_max_pending_connection_count = 1;
    pool_max_message_queue_size       = 1024;
    relay_enabled                     = false;
}
//-----------------------------------------------------------------------------
ulong Configuration::connectionReconnectTimeoutSeconds(
    const uint8_t _retry_count,
    const bool    _failed_create_connection_actor,
    const bool    _last_connection_was_connected,
    const bool    _last_connection_was_active,
    const bool /*_last_connection_was_secured*/) const
{
    if (_failed_create_connection_actor) {
        return connection_timeout_reconnect_seconds / 2;
    }
    if (_last_connection_was_active || _last_connection_was_connected) {
        return 0; // reconnect right away
    }
    ulong retry_count   = _retry_count / 2;
    ulong sleep_seconds = retry_count;

    if (sleep_seconds > connection_timeout_reconnect_seconds) {
        sleep_seconds = connection_timeout_reconnect_seconds;
    }

    return sleep_seconds; // TODO: add entropy - improve algorithm
}
//-----------------------------------------------------------------------------
void Configuration::check() const
{
}
//-----------------------------------------------------------------------------
void Configuration::prepare()
{

    if (pool_max_active_connection_count == 0) {
        pool_max_active_connection_count = 1;
    }
    if (pool_max_pending_connection_count == 0) {
        pool_max_pending_connection_count = 1;
    }

    if (connection_recv_buffer_max_capacity_kb > 64) {
        connection_recv_buffer_max_capacity_kb = 64;
    }

    if (connection_send_buffer_max_capacity_kb > 64) {
        connection_send_buffer_max_capacity_kb = 64;
    }

    if (connection_recv_buffer_start_capacity_kb > connection_recv_buffer_max_capacity_kb) {
        connection_recv_buffer_start_capacity_kb = connection_recv_buffer_max_capacity_kb;
    }

    if (connection_send_buffer_start_capacity_kb > connection_send_buffer_max_capacity_kb) {
        connection_send_buffer_start_capacity_kb = connection_send_buffer_max_capacity_kb;
    }

    if (!server.hasSecureConfiguration()) {
        server.connection_start_secure = false;
    }
    if (!client.hasSecureConfiguration()) {
        client.connection_start_secure = false;
    }
}

//-----------------------------------------------------------------------------

void Configuration::createListenerDevice(SocketDevice& _rsd) const
{
    if (!server.listener_address_str.empty()) {
        std::string tmp;
        const char* hst_name;
        const char* svc_name;

        size_t off = server.listener_address_str.rfind(':');
        if (off != std::string::npos) {
            tmp      = server.listener_address_str.substr(0, off);
            hst_name = tmp.c_str();
            svc_name = server.listener_address_str.c_str() + off + 1;
            if (svc_name[0] == 0) {
                svc_name = server.listener_service_str.c_str();
            }
        } else {
            hst_name = server.listener_address_str.c_str();
            svc_name = server.listener_service_str.c_str();
        }

        ResolveData rd = synchronous_resolve(hst_name, svc_name, 0, -1, SocketInfo::Stream);

        for (auto it = rd.begin(); it != rd.end(); ++it) {
            SocketDevice sd;
            sd.create(it);
            const auto err = sd.prepareAccept(it, SocketInfo::max_listen_backlog_size());
            if (!err) {
                _rsd = std::move(sd);
                return; // SUCCESS
            }
        }

        solid_throw_log(service_logger(), "failed to create listener socket device");
    }
}

//-----------------------------------------------------------------------------
RecvBufferPointerT Configuration::allocateRecvBuffer(uint8_t& _rbuffer_capacity_kb) const
{
    if (_rbuffer_capacity_kb == 0) {
        _rbuffer_capacity_kb = connection_recv_buffer_start_capacity_kb;
    } else if (_rbuffer_capacity_kb > connection_recv_buffer_max_capacity_kb) {
        _rbuffer_capacity_kb = connection_recv_buffer_max_capacity_kb;
    }
    return connection_recv_buffer_allocate_fnc(_rbuffer_capacity_kb * 1024);
}
//-----------------------------------------------------------------------------
SendBufferPointerT Configuration::allocateSendBuffer(uint8_t& _rbuffer_capacity_kb) const
{
    if (_rbuffer_capacity_kb == 0) {
        _rbuffer_capacity_kb = connection_send_buffer_start_capacity_kb;
    } else if (_rbuffer_capacity_kb > connection_send_buffer_max_capacity_kb) {
        _rbuffer_capacity_kb = connection_send_buffer_max_capacity_kb;
    }
    return connection_send_buffer_allocate_fnc(_rbuffer_capacity_kb * 1024);
}
//-----------------------------------------------------------------------------
} // namespace mprpc
} // namespace frame
} // namespace solid
