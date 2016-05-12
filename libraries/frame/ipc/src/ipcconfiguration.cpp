// frame/ipc/src/ipcconfiguration.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/ipc/ipcconfiguration.hpp"
#include "frame/ipc/ipcerror.hpp"
#include "system/cassert.hpp"
#include "system/memory.hpp"

namespace solid{
namespace frame{
namespace ipc{
//-----------------------------------------------------------------------------
namespace{
	char * default_allocate_buffer(const uint16 _cp){
		char *rv = new char[_cp];
		return rv;
	}
	void default_free_buffer(char *_pbuf){
		delete []_pbuf;
	}
	
	//void empty_reset_serializer_limits(ConnectionContext &, serialization::binary::Limits&){}
	
	void empty_connection_stop(ConnectionContext &, ErrorConditionT const&){}
	
	void empty_connection_on_event(ConnectionContext &, Event &){}
	
	size_t default_compress(char*, size_t, ErrorConditionT &){
		return 0;
	}
	
	size_t default_decompress(char*, const char*, size_t, ErrorConditionT &_rerror){
		//This should never be called
		SOLID_ASSERT(false);
		_rerror = error_compression_unavailable;
		return 0;
	}
	
}//namespace


ReaderConfiguration::ReaderConfiguration(){
	max_message_count_multiplex = 64;
	
	decompress_fnc = default_decompress;
}

WriterConfiguration::WriterConfiguration(){
	max_message_count_multiplex = 64;
	
	max_message_continuous_packet_count = 4;
	max_message_count_response_wait = 128;
	
	inplace_compress_fnc = default_compress;
	//reset_serializer_limits_fnc = empty_reset_serializer_limits;
}


Configuration::Configuration(
	AioSchedulerT &_rsch,
	ipc::Protocol *_pproto
): pools_mutex_count(16), protocol_ptr(_pproto), pscheduler(&_rsch)
{
	connection_recv_buffer_start_capacity_kb = memory_page_size()/1024;
	connection_send_buffer_start_capacity_kb = memory_page_size()/1024;
	
	connection_recv_buffer_max_capacity_kb = connection_send_buffer_max_capacity_kb = 64;
	
	
	connection_inactivity_timeout_seconds = 60 * 10;//ten minutes
	connection_keepalive_timeout_seconds = 60 * 5;//five minutes
	connection_reconnect_timeout_seconds = 10;
	
	connection_inactivity_keepalive_count = 2;
	
	connection_start_state = ConnectionState::Passive;
	connection_start_secure = true;
	
	connection_recv_buffer_allocate_fnc = default_allocate_buffer;
	connection_send_buffer_allocate_fnc = default_allocate_buffer;
	
	connection_recv_buffer_free_fnc = default_free_buffer;
	connection_send_buffer_free_fnc = default_free_buffer;
	
	connection_stop_fnc = empty_connection_stop;
	
	connection_on_event_fnc = empty_connection_on_event;
	
	pool_max_active_connection_count = 1;
	pool_max_pending_connection_count = 1;
	pool_max_message_queue_size = 1024;
	
	listener_port = -1;
}
//-----------------------------------------------------------------------------
size_t Configuration::connectionReconnectTimeoutSeconds(
	const uint8 _retry_count,
	const bool _failed_create_connection_object,
	const bool _last_connection_was_connected,
	const bool _last_connection_was_active,
	const bool _last_connection_was_secured
)const{
	if(_failed_create_connection_object){
		return connection_reconnect_timeout_seconds / 2;
	}
	if(_last_connection_was_active or _last_connection_was_connected){
		return 0;//reconnect right away
	}
	size_t retry_count = _retry_count / 2;
	size_t sleep_seconds = retry_count;
	
	if(sleep_seconds > connection_reconnect_timeout_seconds){
		sleep_seconds = connection_reconnect_timeout_seconds;
	}
	
	return sleep_seconds;//TODO: add entropy - improve algorithm
}
//-----------------------------------------------------------------------------
ErrorConditionT Configuration::check() const {
	//TODO:
	return ErrorConditionT();
}
//-----------------------------------------------------------------------------
void Configuration::prepare(){
	
	if(pool_max_active_connection_count == 0){
		pool_max_active_connection_count = 1;
	}
	if(pool_max_pending_connection_count == 0){
		pool_max_pending_connection_count = 1;
	}
	
	if(connection_recv_buffer_max_capacity_kb > 64){
		connection_recv_buffer_max_capacity_kb = 64;
	}
	
	if(connection_send_buffer_max_capacity_kb > 64){
		connection_send_buffer_max_capacity_kb = 64;
	}
	
	if(connection_recv_buffer_start_capacity_kb > connection_recv_buffer_max_capacity_kb){
		connection_recv_buffer_start_capacity_kb = connection_recv_buffer_max_capacity_kb;
	}
	
	if(connection_send_buffer_start_capacity_kb > connection_send_buffer_max_capacity_kb){
		connection_send_buffer_start_capacity_kb = connection_send_buffer_max_capacity_kb;
	}
}
//-----------------------------------------------------------------------------
char* Configuration::allocateRecvBuffer(uint8 &_rbuffer_capacity_kb)const{
	if(_rbuffer_capacity_kb == 0){
		_rbuffer_capacity_kb = connection_recv_buffer_start_capacity_kb;
	}else if(_rbuffer_capacity_kb > connection_recv_buffer_max_capacity_kb){
		_rbuffer_capacity_kb = connection_recv_buffer_max_capacity_kb;
	}
	return connection_recv_buffer_allocate_fnc(_rbuffer_capacity_kb * 1024);
}
//-----------------------------------------------------------------------------
void Configuration::freeRecvBuffer(char *_pb)const{
	connection_recv_buffer_free_fnc(_pb);
}
//-----------------------------------------------------------------------------
char* Configuration::allocateSendBuffer(uint8 &_rbuffer_capacity_kb)const{
	if(_rbuffer_capacity_kb == 0){
		_rbuffer_capacity_kb = connection_send_buffer_start_capacity_kb;
	}else if(_rbuffer_capacity_kb > connection_send_buffer_max_capacity_kb){
		_rbuffer_capacity_kb = connection_send_buffer_max_capacity_kb;
	}
	return connection_send_buffer_allocate_fnc(_rbuffer_capacity_kb * 1024);
}
//-----------------------------------------------------------------------------
void Configuration::freeSendBuffer(char *_pb)const{
	connection_send_buffer_free_fnc(_pb);
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
