// frame/ipc/ipcconfiguration.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_IPCCONFIGURATION_HPP
#define SOLID_FRAME_IPC_IPCCONFIGURATION_HPP

#include <vector>
#include "system/function.hpp"
#include "system/socketaddress.hpp"
#include "frame/aio/aioreactor.hpp"
#include "frame/aio/aioreactorcontext.hpp"
#include "frame/scheduler.hpp"



namespace solid{

namespace serialization{ namespace binary{
struct Limits;
}/*namespace binary*/}/*namespace serialization*/

namespace frame{

namespace aio{
class Resolver;
}

namespace ipc{

struct	ServiceProxy;
class	Service;
class	MessageWriter;
struct ConnectionContext;

typedef frame::Scheduler<frame::aio::Reactor> 								AioSchedulerT;

typedef std::vector<SocketAddressInet>										AddressVectorT;
	
typedef FUNCTION<void(ServiceProxy &)>										MessageRegisterFunctionT;
typedef FUNCTION<void(AddressVectorT &)>									ResolveCompleteFunctionT;
typedef FUNCTION<void(const std::string&, ResolveCompleteFunctionT&)>		AsyncResolveFunctionT;
typedef FUNCTION<void(ConnectionContext &, ErrorConditionT const&)>			ConnectionStopFunctionT;
typedef FUNCTION<void(ConnectionContext &)>									ConnectionStartFunctionT;
typedef FUNCTION<char*(const uint16)>										AllocateBufferFunctionT;
typedef FUNCTION<void(char*)>												FreeBufferFunctionT;
typedef FUNCTION<size_t(char*, size_t, ErrorConditionT &)>					CompressFunctionT;
typedef FUNCTION<size_t(char*, const char*, size_t, ErrorConditionT &)>		UncompressFunctionT;
typedef FUNCTION<void(ConnectionContext &, serialization::binary::Limits&)>	ResetSerializerLimitsFunctionT;

struct Configuration{
	
	Configuration(
		AioSchedulerT &_rsch
	);
	
	template <class F>
	void protocolCallback(F _f);
	
	AioSchedulerT & scheduler(){
		return *psch;
	}
	
	bool isServer()const{
		return listen_address_str.size() != 0;
	}
	
	bool isClient()const{
		return not FUNCTION_EMPTY(name_resolve_fnc);
	}
	
	bool isServerOnly()const{
		return isServer() && !isClient();
	}
	
	bool isClientOnly()const{
		return !isServer() && isClient();
	}
	
	char* allocateRecvBuffer()const;
	void freeRecvBuffer(char *_pb)const;
	
	char* allocateSendBuffer()const;
	void freeSendBuffer(char *_pb)const;
	
	AioSchedulerT						*psch;
	size_t								max_per_pool_connection_count;
	size_t								session_mutex_count;
	
	size_t								max_writer_multiplex_message_count;
	size_t								max_writer_message_count;
	size_t								max_writer_message_continuous_packet_count;
	
	
	size_t								max_reader_multiplex_message_count;
	
	uint32								inactivity_timeout_seconds;
	uint32								keepalive_timeout_seconds;
	
	uint32								inactivity_keepalive_count;	//server error if receives more than inactivity_keepalive_count keep alive 
																	//messages during inactivity_timeout_seconds interval
	
	uint32								recv_buffer_capacity;
	uint32								send_buffer_capacity;
	
	MessageRegisterFunctionT			message_register_fnc;
	AsyncResolveFunctionT				name_resolve_fnc;
	ConnectionStartFunctionT			incoming_connection_start_fnc;
	ConnectionStartFunctionT			outgoing_connection_start_fnc;
	ConnectionStopFunctionT				connection_stop_fnc;
	
	AllocateBufferFunctionT				allocate_recv_buffer_fnc;
	AllocateBufferFunctionT				allocate_send_buffer_fnc;
	
	FreeBufferFunctionT					free_recv_buffer_fnc;
	FreeBufferFunctionT					free_send_buffer_fnc;
	
	ResetSerializerLimitsFunctionT		reset_serializer_limits_fnc;
	
	CompressFunctionT					inplace_compress_fnc;
	UncompressFunctionT					uncompress_fnc;
	
	std::string							listen_address_str;
	std::string							default_listen_port_str;
	
	ErrorConditionT prepare();
	
private:
	enum WriterFunctions{
		WriterNoCompressE = 0,
		WriterCompress01kE,
		WriterCompress02kE,
		WriterCompress04kE,
		WriterCompress08kE,
		WriterCompress12kE,
		WriterCompress16kE,
		WriterCompress20kE,
		WriterCompress24kE,
		WriterCompress32kE,
		WriterCompress36kE,
		WriterCompress64kE,
		WriterCompress96kE,
	};
	
	WriterFunctions		writerfunctionidx;
private:
	friend class Service;
	friend class MessageWriter;
	Configuration():psch(nullptr){}
};

template <class F>
void Configuration::protocolCallback(F _f){
	message_register_fnc = _f;
}


struct ResolverF{
	aio::Resolver		&rresolver;
	std::string			default_service;
	SocketInfo::Family	family;
	
	ResolverF(
		aio::Resolver &_rresolver,
		const char *_default_service,
		SocketInfo::Family	_family = SocketInfo::AnyFamily
	):	rresolver(_rresolver), default_service(_default_service), family(_family){}
	
	void operator()(const std::string&, ResolveCompleteFunctionT&);
};

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
