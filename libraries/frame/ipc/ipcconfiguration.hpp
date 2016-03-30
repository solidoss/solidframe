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
#include "frame/ipc/ipcmessage.hpp"
#include "frame/aio/aioreactor.hpp"
#include "frame/aio/openssl/aiosecurecontext.hpp"
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
struct	ConnectionContext;

using AddressVectorT								= std::vector<SocketAddressInet>;

using MessageRegisterFunctionT						= FUNCTION<void(ServiceProxy &)>;
using ResolveCompleteFunctionT						= FUNCTION<void(AddressVectorT &&)>;
using AsyncResolveFunctionT							= FUNCTION<void(const std::string&, ResolveCompleteFunctionT&)>;
using ConnectionStopFunctionT						= FUNCTION<void(ConnectionContext &, ErrorConditionT const&)>;
using ConnectionStartFunctionT						= FUNCTION<void(ConnectionContext &)>;
using AllocateBufferFunctionT						= FUNCTION<char*(const uint16)>;
using FreeBufferFunctionT							= FUNCTION<void(char*)>;
using CompressFunctionT								= FUNCTION<size_t(char*, size_t, ErrorConditionT &)>;
using UncompressFunctionT							= FUNCTION<size_t(char*, const char*, size_t, ErrorConditionT &)>;
using ResetSerializerLimitsFunctionT				= FUNCTION<void(ConnectionContext &, serialization::binary::Limits&)>;

using SecureContextT								= frame::aio::openssl::Context;

using AioSchedulerT									= frame::Scheduler<frame::aio::Reactor>;

using ResponseHandlerFunctionT						= FUNCTION<void(ConnectionContext&, MessagePointerT &, ErrorConditionT const&)>;

using ConnectionEnterActiveCompleteFunctionT		= FUNCTION<MessagePointerT(ConnectionContext&, ErrorConditionT const&)>;
using ConnectionEnterPassiveCompleteFunctionT		= FUNCTION<void(ConnectionContext&, ErrorConditionT const&)>;
using ConnectionSecureHandhakeCompleteFunctionT		= FUNCTION<void(ConnectionContext&, ErrorConditionT const&)>;
using ConnectionSendRawDataCompleteFunctionT		= FUNCTION<void(ConnectionContext&, ErrorConditionT const&)>;
using ConnectionRecvRawDataCompleteFunctionT		= FUNCTION<bool(ConnectionContext&, const char*, size_t, ErrorConditionT const&)>;

enum struct ConnectionState{
	Raw,
	Passive,
	Active
};

struct Configuration{
private:
	Configuration& operator=(const Configuration&) = delete;
	Configuration& operator=(Configuration&&) = default;
public:
	Configuration(
		AioSchedulerT &_rsch
	);
	
	//Only Service can call this constructor
	Configuration(ServiceProxy const&):pscheduler(nullptr){}
	
	Configuration& reset(Configuration &&_ucfg){
		*this = std::move(_ucfg);
		prepare();
		return *this;
	}
	
	template <class F>
	void protocolCallback(F _f);
	
	AioSchedulerT & scheduler(){
		return *pscheduler;
	}
	
	bool isServer()const{
		return listener_address_str.size() != 0;
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
	
	bool hasSecureContext()const{
		return secure_context.isValid();
	}
	
	char* allocateRecvBuffer()const;
	void freeRecvBuffer(char *_pb)const;
	
	char* allocateSendBuffer()const;
	void freeSendBuffer(char *_pb)const;
	
	size_t connectionReconnectTimeoutSeconds()const;
	
	ErrorConditionT check() const;
	

	size_t connetionReconnectTimeoutSeconds()const;
	
	size_t								pool_max_active_connection_count;
	size_t								pool_max_pending_connection_count;
	size_t								pool_max_message_queue_size;
	
	size_t								pools_mutex_count;
	
	size_t								writer_max_message_count_multiplex;
	size_t								writer_max_message_count_response_wait;
	size_t								writer_max_message_continuous_packet_count;
	
	
	size_t								reader_max_message_count_multiplex;
	
	size_t								connection_reconnect_timeout_seconds;
	uint32								connection_inactivity_timeout_seconds;
	uint32								connection_keepalive_timeout_seconds;
	ConnectionState						connection_start_state;
	bool								connection_start_secure;
	
	uint32								connection_inactivity_keepalive_count;	//server error if receives more than inactivity_keepalive_count keep alive 
																				//messages during inactivity_timeout_seconds interval
	
	uint32								connection_recv_buffer_capacity;
	uint32								connection_send_buffer_capacity;
	
	MessageRegisterFunctionT			message_register_fnc;
	AsyncResolveFunctionT				name_resolve_fnc;
	
	ConnectionStartFunctionT			connection_incoming_start_fnc;
	ConnectionStartFunctionT			connection_outgoing_start_fnc;
	ConnectionStopFunctionT				connection_stop_fnc;
	
	AllocateBufferFunctionT				recv_buffer_allocate_fnc;
	AllocateBufferFunctionT				send_buffer_allocate_fnc;
	
	FreeBufferFunctionT					recv_buffer_free_fnc;
	FreeBufferFunctionT					send_buffer_free_fnc;
	
	ResetSerializerLimitsFunctionT		reset_serializer_limits_fnc;
	
	CompressFunctionT					inplace_compress_fnc;
	UncompressFunctionT					uncompress_fnc;
	
	std::string							listener_address_str;
	std::string							listener_default_port_str;
	
	SecureContextT						secure_context;
private:
	void prepare();
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
	
	WriterFunctions						writerfunctionidx;
	AioSchedulerT						*pscheduler;
private:
	//friend class Service;
	friend class MessageWriter;
	Configuration():pscheduler(nullptr){}
};

template <class F>
void Configuration::protocolCallback(F _f){
	message_register_fnc = _f;
}


struct InternetResolverF{
	aio::Resolver		&rresolver;
	std::string			default_service;
	SocketInfo::Family	family;
	
	InternetResolverF(
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
