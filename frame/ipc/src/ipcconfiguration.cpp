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
): psch(&_rsch), session_mutex_count(16)
{
	recv_buffer_capacity = 4096;
	send_buffer_capacity = 4096;
	
	
	max_writer_pending_message_count = -1;
	max_writer_multiplex_message_count = 16;
	max_writer_message_continuous_packet_count = 4;
	max_writer_waiting_message_count = -1;
	
	max_reader_multiplex_message_count = max_writer_multiplex_message_count;
	
	inactivity_timeout_seconds = 60 * 10;//ten minutes
	keepalive_timeout_seconds = 60 * 5;//five minutes
	
	inactivity_keepalive_count = 2;
	
	allocate_recv_buffer_fnc = default_allocate_buffer;
	allocate_send_buffer_fnc = default_allocate_buffer;
	
	free_recv_buffer_fnc = default_free_buffer;
	free_send_buffer_fnc = default_free_buffer;
	
	reset_serializer_limits_fnc = empty_reset_serializer_limits;
	
	connection_stop_fnc = empty_connection_stop;
	
	inplace_compress_fnc = default_compress;
	uncompress_fnc = default_uncompress;
}
//-----------------------------------------------------------------------------
ErrorConditionT Configuration::prepare(){
	
	return ErrorConditionT();
}
//-----------------------------------------------------------------------------
char* Configuration::allocateRecvBuffer()const{
	return allocate_recv_buffer_fnc(recv_buffer_capacity);
}
//-----------------------------------------------------------------------------
void Configuration::freeRecvBuffer(char *_pb)const{
	free_recv_buffer_fnc(_pb);
}
//-----------------------------------------------------------------------------
char* Configuration::allocateSendBuffer()const{
	return allocate_send_buffer_fnc(send_buffer_capacity);
}
//-----------------------------------------------------------------------------
void Configuration::freeSendBuffer(char *_pb)const{
	free_send_buffer_fnc(_pb);
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
