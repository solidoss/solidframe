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
namespace frame{

namespace aio{
class Resolver;
}

namespace ipc{

struct	ServiceProxy;
class	Service;
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
	
	AioSchedulerT				*psch;
	size_t						max_per_pool_connection_count;
	size_t						session_mutex_count;
	
	size_t						max_writer_multiplex_message_count;
	size_t						max_writer_pending_message_count;
	
	uint16						recv_buffer_capacity;
	uint16						send_buffer_capacity;
	
	MessageRegisterFunctionT	message_register_fnc;
	AsyncResolveFunctionT		name_resolve_fnc;
	ConnectionStartFunctionT	incoming_connection_start_fnc;
	ConnectionStartFunctionT	outgoing_connection_start_fnc;
	ConnectionStopFunctionT		connection_stop_fnc;
	
	AllocateBufferFunctionT		allocate_recv_buffer_fnc;
	AllocateBufferFunctionT		allocate_send_buffer_fnc;
	
	FreeBufferFunctionT			free_recv_buffer_fnc;
	FreeBufferFunctionT			free_send_buffer_fnc;
	
	std::string					listen_address_str;
	std::string					default_listen_port_str;
private:
	friend class Service;
	Configuration():psch(nullptr){}
};

template <class F>
void Configuration::protocolCallback(F _f){
	message_register_fnc = _f;
}


struct ResolverF{
	aio::Resolver	&rresolver;
	std::string		default_service;
	
	ResolverF(
		aio::Resolver &_rresolver,
		const char *_default_service
	):	rresolver(_rresolver), default_service(_default_service){}
	
	void operator()(const std::string&, ResolveCompleteFunctionT&);
};

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
