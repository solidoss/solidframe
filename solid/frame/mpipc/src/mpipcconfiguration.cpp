// solid/frame/ipc/src/ipcconfiguration.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcerror.hpp"
#include "solid/frame/mpipc/mpipcsocketstub_plain.hpp"

#include "solid/system/cassert.hpp"
#include "solid/system/memory.hpp"

#include <cstring>

namespace solid {
namespace frame {
namespace mpipc {
//-----------------------------------------------------------------------------
namespace {
RecvBufferPointerT default_allocate_recv_buffer(const uint32_t _cp)
{
    return RecvBufferPointerT(new char[_cp]);
}

SendBufferPointerT default_allocate_send_buffer(const uint32_t _cp)
{
    return SendBufferPointerT(new char[_cp]);
}

//void empty_reset_serializer_limits(ConnectionContext &, serialization::binary::Limits&){}

void empty_connection_stop(ConnectionContext&) {}

void empty_connection_start(ConnectionContext&) {}

void empty_connection_on_event(ConnectionContext&, Event&) {}

size_t default_compress(char*, size_t, ErrorConditionT&)
{
    return 0;
}

size_t default_decompress(char*, const char*, size_t, ErrorConditionT& _rerror)
{
    //This should never be called
    SOLID_ASSERT(false);
    _rerror = error_compression_unavailable;
    return 0;
}

SocketStubPtrT default_create_client_socket(Configuration const& _rcfg, frame::aio::ObjectProxy const& _rproxy, char* _emplace_buf)
{
    return plain::create_client_socket(_rcfg, _rproxy, _emplace_buf);
}

SocketStubPtrT default_create_server_socket(Configuration const& _rcfg, frame::aio::ObjectProxy const& _rproxy, SocketDevice&& _usd, char* _emplace_buf)
{
    return plain::create_server_socket(_rcfg, _rproxy, std::move(_usd), _emplace_buf);
}

const char* default_extract_recipient_name(const char* _purl, std::string& _msgurl, std::string& _tmpstr)
{
    if (_purl == nullptr)
        return nullptr;

    const char* p = strchr(_purl, '/');

    if (p == nullptr) {
        return _purl;
    } else {
        _msgurl = (p + 1);
        _tmpstr.assign(_purl, p - _purl);
        return _tmpstr.c_str();
    }
}

const char* default_fast_extract_recipient_name(const char* _purl, std::string& _msgurl, std::string& _tmpstr)
{

    return _purl;
}

bool default_setup_socket_device(SocketDevice& _rsd)
{
    _rsd.enableNoDelay();
    _rsd.enableNoSignal();
    return true;
}

} //namespace

ReaderConfiguration::ReaderConfiguration()
{
    max_message_count_multiplex = 64;

    decompress_fnc = default_decompress;
}

WriterConfiguration::WriterConfiguration()
{
    max_message_count_multiplex = 64;

    max_message_continuous_packet_count = 4;
    max_message_count_response_wait     = 128;

    inplace_compress_fnc = default_compress;
    //reset_serializer_limits_fnc = empty_reset_serializer_limits;
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/*static*/ RelayEngineBase& RelayEngineBase::instance()
{
    static RelayEngineBase eng;
    return eng;
}
//-----------------------------------------------------------------------------
/*virtual*/ RelayEngineBase::~RelayEngineBase()
{
}
//-----------------------------------------------------------------------------
/*virtual*/ bool RelayEngineBase::doRelayStart(
    const ObjectIdT& _rconuid,
    NotifyFunctionT& _rnotify_fnc,
    MessageHeader&   _rmsghdr,
    RelayData&&      _urelay_data,
    MessageId&       _rrelay_id,
    ErrorConditionT& _rerror)
{
    return false;
}
//-----------------------------------------------------------------------------
/*virtual*/ bool RelayEngineBase::doRelayResponse(
    const ObjectIdT& _rconuid,
    NotifyFunctionT& _rnotify_fnc,
    MessageHeader&   _rmsghdr,
    RelayData&&      _urelay_data,
    const MessageId& _rrelay_id,
    ErrorConditionT& _rerror)
{
    return false;
}
//-----------------------------------------------------------------------------
/*virtual*/ bool RelayEngineBase::doRelay(
    const ObjectIdT& _rconuid,
    NotifyFunctionT& _rnotify_fnc,
    RelayData&&      _urelay_data,
    const MessageId& _rrelay_id,
    ErrorConditionT& _rerror)
{
    return false;
}
//-----------------------------------------------------------------------------
/*virtual*/ void RelayEngineBase::doPollNew(const ObjectIdT& _rconuid, PushFunctionT& /*_try_push_fnc*/, bool& /*_rmore*/)
{
    SOLID_THROW("should not be called");
}
//-----------------------------------------------------------------------------
/*virtual*/ void RelayEngineBase::doPollDone(const ObjectIdT& _rconuid, DoneFunctionT& /*_done_fnc*/)
{
    SOLID_THROW("should not be called");
}
//-----------------------------------------------------------------------------
/*virtual*/ void RelayEngineBase::doComplete(const ObjectIdT& _rconuid, NotifyFunctionT&, RelayData* /*_prelay_data*/, MessageId const& /*_rengine_msg_id*/, bool& /*_rmore*/)
{
    SOLID_THROW("should not be called");
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Configuration::init()
{
    connection_recv_buffer_start_capacity_kb = memory_page_size() / 1024;
    connection_send_buffer_start_capacity_kb = memory_page_size() / 1024;

    connection_recv_buffer_max_capacity_kb = connection_send_buffer_max_capacity_kb = 64;

    connection_inactivity_timeout_seconds = 60 * 10; //ten minutes
    connection_keepalive_timeout_seconds  = 60 * 5; //five minutes
    connection_reconnect_timeout_seconds  = 10;

    connection_relay_buffer_count = 8;

    connection_inactivity_keepalive_count = 2;

    server.connection_start_state  = ConnectionState::Passive;
    server.connection_start_secure = true;

    client.connection_start_state  = ConnectionState::Passive;
    client.connection_start_secure = true;

    connection_recv_buffer_allocate_fnc = default_allocate_recv_buffer;
    connection_send_buffer_allocate_fnc = default_allocate_send_buffer;

    connection_stop_fnc = empty_connection_stop;

    server.connection_start_fnc = empty_connection_start;
    client.connection_start_fnc = empty_connection_start;

    connection_on_event_fnc = empty_connection_on_event;

    client.connection_create_socket_fnc = default_create_client_socket;
    server.connection_create_socket_fnc = default_create_server_socket;

    server.socket_device_setup_fnc = default_setup_socket_device;
    client.socket_device_setup_fnc = default_setup_socket_device;

    extract_recipient_name_fnc = default_extract_recipient_name;

    pool_max_active_connection_count  = 1;
    pool_max_pending_connection_count = 1;
    pool_max_message_queue_size       = 1024;
}
//-----------------------------------------------------------------------------
size_t Configuration::connectionReconnectTimeoutSeconds(
    const uint8_t _retry_count,
    const bool    _failed_create_connection_object,
    const bool    _last_connection_was_connected,
    const bool    _last_connection_was_active,
    const bool    _last_connection_was_secured) const
{
    if (_failed_create_connection_object) {
        return connection_reconnect_timeout_seconds / 2;
    }
    if (_last_connection_was_active or _last_connection_was_connected) {
        return 0; //reconnect right away
    }
    size_t retry_count   = _retry_count / 2;
    size_t sleep_seconds = retry_count;

    if (sleep_seconds > connection_reconnect_timeout_seconds) {
        sleep_seconds = connection_reconnect_timeout_seconds;
    }

    return sleep_seconds; //TODO: add entropy - improve algorithm
}
//-----------------------------------------------------------------------------
ErrorConditionT Configuration::check() const
{
    //TODO:
    return ErrorConditionT();
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

    if (not server.hasSecureConfiguration()) {
        server.connection_start_secure = false;
    }
    if (not client.hasSecureConfiguration()) {
        client.connection_start_secure = false;
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
} //namespace mpipc
} //namespace frame
} //namespace solid
