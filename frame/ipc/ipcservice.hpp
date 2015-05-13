// frame/ipc/ipcservice.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_IPCSERVICE_HPP
#define SOLID_FRAME_IPC_IPCSERVICE_HPP

#include "system/function.hpp"
#include "system/exception.hpp"
#include "serialization/binary.hpp"

#include "frame/service.hpp"
#include "frame/ipc/ipccontext.hpp"
#include "frame/ipc/ipcmessage.hpp"

namespace solid{
namespace frame{
namespace ipc{

struct Message;
class Configuration;

enum SendFlags{
	SendRequestFlagE	= 1,
	SendResponseFlagE	= 2,
};

//! Inter Process Communication service
/*!
	Allows exchanging ipc::Messages between processes.
	<br>
*/
class Service: public Dynamic<Service, frame::Service>{
public:
	typedef Dynamic<Service, frame::Service> BaseT;
	
	Service(
		frame::Manager &_rm, frame::Event const &_revt
	);
	
	//! Destructor
	~Service();

	ErrorConditionT reconfigure(Configuration const& _rcfg);
	
	template <class T>
	ErrorConditionT sendMessage(
		const char *_session_name,
		DynamicPointer<T> const &_rmsgptr,
		ulong _flags = 0
	){
		MessagePointerT		msgptr(_rmsgptr);
		ConnectionUid		conuid;
		return doSendMessage(_session_name, conuid, msgptr, nullptr, _flags);
	}
	
	template <class T>
	ErrorConditionT sendMessage(
		const char *_session_name,
		DynamicPointer<T> const &_rmsgptr,
		SessionUid &_rsession_uid,
		ulong _flags = 0
	){
		MessagePointerT		msgptr(_rmsgptr);
		ConnectionUid		conuid;
		return doSendMessage(_session_name, conuid, msgptr, &_rsession_uid, _flags);
	}
	
	template <class T>
	ErrorConditionT sendMessage(
		ConnectionUid const &_rconnection_uid,
		DynamicPointer<T> const &_rmsgptr,
		ulong _flags = 0
	){
		MessagePointerT		msgptr(_rmsgptr);
		return doSendMessage(nullptr, _rconnection_uid, msgptr, nullptr, _flags);
	}
	
	template <class T>
	ErrorConditionT sendMessage(
		SessionUid const &_rsession_uid,
		DynamicPointer<T> const &_rmsgptr,
		ulong _flags = 0
	){
		MessagePointerT		msgptr(_rmsgptr);
		ConnectionUid		conuid(_rsession_uid);
		return doSendMessage(nullptr, conuid, msgptr, nullptr, _flags);
	}
private:
	friend struct ServiceProxy;
	friend class Listener;
	friend class Connection;
	
	typedef FUNCTION<void(ConnectionContext &, MessagePointerT &)>							MessageReceiveFunctionT;
	typedef FUNCTION<void(ConnectionContext &, MessagePointerT &, ErrorConditionT const &)>	MessageCompleteFunctionT;
	typedef FUNCTION<uint32(ConnectionContext &, Message const &)>							MessagePrepareFunctionT;
	
	struct TypeStub{
		MessagePrepareFunctionT		prepare_fnc;
		MessageReceiveFunctionT		receive_fnc;
		MessageCompleteFunctionT	complete_fnc;
	};
	
	typedef serialization::binary::Serializer<ConnectionContext	>						SerializerT;
	typedef serialization::binary::Deserializer<ConnectionContext>						DeserializerT;
	typedef serialization::TypeIdMap<SerializerT, DeserializerT, TypeStub>				TypeIdMapT;
	
	bool isEventStart(Event const&_revent);
	bool isEventStop(Event const&_revent);
	
	void connectionReceive(SocketDevice &_rsd);
	void connectionLeave();
	
	void forwardResolveMessage(SessionUid const &_rssnuid, Event const&_revent);
	
	template <class F, class M>
	struct ReceiveProxy{
		F	f;
		ReceiveProxy(F _f):f(_f){}
		
		void operator()(ConnectionContext &_rctx, MessagePointerT &_rmsgptr){
			DynamicPointer<M>	msgptr(std::move(_rmsgptr));
			f(_rctx, msgptr);
		}
	};
	
	template <class F, class M>
	struct CompleteProxy{
		F	f;
		CompleteProxy(F _f):f(_f){}
		
		void operator()(ConnectionContext &_rctx, MessagePointerT &_rmsgptr, ErrorConditionT const &_err){
			DynamicPointer<M>	msgptr(std::move(_rmsgptr));
			f(_rctx, msgptr, _err);
		}
	};
	
	template <class F, class M>
	struct PrepareProxy{
		F	f;
		PrepareProxy(F _f):f(_f){}
		
		uint32 operator()(ConnectionContext &_rctx, Message const &_rmsg){
			M const	&rmsg = static_cast<M const &>(_rmsg);
			return f(_rctx, rmsg);
		}
	};
	
	template <class T, class FactoryFnc>
	size_t registerType(FactoryFnc _facf, size_t _idx = 0){
		TypeStub ts;
		return tm.registerType<T>(_facf, ts, _idx);
	}
	
	template <class T, class FactoryFnc, class ReceiveFnc, class PrepareFnc, class CompleteFnc>
	size_t registerType(FactoryFnc _facf, ReceiveFnc _rcvf, PrepareFnc _prepf, CompleteFnc _cmpltf, size_t _idx = 0){
		TypeStub ts;
		ts.complete_fnc = MessageCompleteFunctionT(CompleteProxy<CompleteFnc, T>(_cmpltf));
		ts.prepare_fnc = MessagePrepareFunctionT(PrepareProxy<PrepareFnc, T>(_prepf));
		ts.receive_fnc = MessageReceiveFunctionT(ReceiveProxy<ReceiveFnc, T>(_rcvf));
		return tm.registerType<T>(ts, Message::serialize<SerializerT, T>, Message::serialize<DeserializerT, T>, _facf, _idx);
	}
	
	template <class Derived, class Base>
	bool registerCast(){
		return tm.registerCast<Derived, Base>();
	}
	
	template <class Derived, class Base>
	bool registerDownCast(){
		return tm.registerDownCast<Derived, Base>();
	}
	
	
	const TypeIdMapT& typeMap()const{
		return tm;
	}
	
	ErrorConditionT doSendMessage(
		const char *_session_name,
		const ConnectionUid	&_rconuid_in,
		MessagePointerT &_rmsgptr,
		SessionUid *_psession_out,
		ulong _flags
	);
	
	ErrorConditionT doSendMessage(
		ObjectUidT const &_robjuid,
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		SessionUid const &_rsessionuid,
		SessionUid *_psession_out,
		ulong _flags
	);
private:
	struct	Data;
	Data			&d;
	TypeIdMapT		tm;
};

struct ServiceProxy{
	template <class T, class FactoryFnc, class ReceiveFnc, class PrepareFnc, class CompleteFnc>
	size_t registerType(FactoryFnc _facf, ReceiveFnc _rcvf, PrepareFnc _prepf, CompleteFnc _cmpltf, size_t _idx = 0){
		return rservice.registerType<T>(_facf, _rcvf, _prepf, _cmpltf, _idx);
	}
	template <class T, class FactoryFnc>
	size_t registerType(FactoryFnc _facf, size_t _idx = 0){
		return rservice.registerType<T>(_facf, _idx);
	}
private:
	friend class Service;
	friend class Configuration;
	ServiceProxy(Service &_rservice):rservice(_rservice){}
	Service &rservice;
};

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
