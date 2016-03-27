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
#include "system/cassert.hpp"

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
	
	void empty_reset_serializer_limits(ConnectionContext &, serialization::binary::Limits&){}
	
	void empty_connection_stop(ConnectionContext &, ErrorConditionT const&){}
	
	
	size_t default_compress(char*, size_t, ErrorConditionT &){
		return 0;
	}
	
	size_t default_uncompress(char*, const char*, size_t, ErrorConditionT &_rerror){
		//This should never be called
		cassert(false);
		_rerror.assign(-1, _rerror.category());//TODO:
		return 0;
	}
	
}//namespace

Configuration::Configuration(
	AioSchedulerT &_rsch
): pscheduler(&_rsch), pools_mutex_count(16)
{
	connection_recv_buffer_capacity = 4096;
	connection_send_buffer_capacity = 4096;
	
	
	writer_max_message_count_multiplex = 64;
	
	writer_max_message_continuous_packet_count = 4;
	writer_max_message_count_response_wait = 128;
	
	reader_max_message_count_multiplex = writer_max_message_count_multiplex;
	
	connection_inactivity_timeout_seconds = 60 * 10;//ten minutes
	connection_keepalive_timeout_seconds = 60 * 5;//five minutes
	connection_reconnect_timeout_seconds = 10;
	
	connection_inactivity_keepalive_count = 2;
	
	connection_start_state = ConnectionState::Passive;
	connection_start_secure = true;
	
	recv_buffer_allocate_fnc = default_allocate_buffer;
	send_buffer_allocate_fnc = default_allocate_buffer;
	
	recv_buffer_free_fnc = default_free_buffer;
	send_buffer_free_fnc = default_free_buffer;
	
	reset_serializer_limits_fnc = empty_reset_serializer_limits;
	
	connection_stop_fnc = empty_connection_stop;
	
	inplace_compress_fnc = default_compress;
	uncompress_fnc = default_uncompress;
	
	
	pool_max_active_connection_count = 1;
	pool_max_pending_connection_count = 1;
}
//-----------------------------------------------------------------------------
size_t Configuration::connectionReconnectTimeoutSeconds()const{
	return connection_reconnect_timeout_seconds;//TODO: add entropy
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
}
//-----------------------------------------------------------------------------
char* Configuration::allocateRecvBuffer()const{
	return recv_buffer_allocate_fnc(connection_recv_buffer_capacity);
}
//-----------------------------------------------------------------------------
void Configuration::freeRecvBuffer(char *_pb)const{
	recv_buffer_free_fnc(_pb);
}
//-----------------------------------------------------------------------------
char* Configuration::allocateSendBuffer()const{
	return send_buffer_allocate_fnc(connection_send_buffer_capacity);
}
//-----------------------------------------------------------------------------
void Configuration::freeSendBuffer(char *_pb)const{
	send_buffer_free_fnc(_pb);
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
