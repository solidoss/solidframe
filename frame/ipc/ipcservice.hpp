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

#include "frame/service.hpp"
#include "frame/ipc/ipccontext.hpp"
#include "frame/ipc/ipcmessage.hpp"
#include "frame/ipc/ipcserialization.hpp"

namespace solid{
namespace frame{

namespace aio{
struct ReactorContext;
}//namespace aio

namespace ipc{

//https://functionalcpp.wordpress.com/2013/08/05/function-traits/
#if 0
template<class F>
struct function_traits;
 
// function pointer
template<class R, class... Args>
struct function_traits<R(*)(Args...)> : public function_traits<R(Args...)>
{};
 
template<class R, class... Args>
struct function_traits<R(Args...)>
{
    using return_type = R;
 
    static constexpr std::size_t arity = sizeof...(Args);
 
    template <std::size_t N>
    struct argument
    {
        static_assert(N < arity, "error: invalid parameter index.");
        using type = typename std::tuple_element<N,std::tuple<Args...>>::type;
    };
};
#endif


template <class M>
struct response_traits;

template <class M>
struct response_traits<void(*)(ConnectionContext&, DynamicPointer<M> &, ErrorConditionT const&)>{
	typedef M message_type;
};

template <class M>
struct response_traits<void(ConnectionContext&, DynamicPointer<M> &, ErrorConditionT const&)>{
	typedef M message_type;
};

template <class C, class M>
struct response_traits<void(C::*)(ConnectionContext&, DynamicPointer<M> &, ErrorConditionT const&)>{
	typedef M message_type;
};



struct Message;
class Configuration;
class Connection;

//! Inter Process Communication service
/*!
	Allows exchanging ipc::Messages between processes.
	Synchronous vs Asynchronous messages
	A synchronous message is one sent with Message::SynchronousFlagE flag set:
	sendMessage(..., Message::SynchronousFlagE)
	Messages with Message::SynchronousFlagE flag unset are asynchronous.
	
	Synchronous messages
		* are always sent one after another, so they reach destination
			in the same order they were sent.
		* if multiple connections through peers are posible, only one is used for
			synchronous messages.
	
	Asynchronous messages
		* Because the messages are multiplexed, although the messages start being
			serialized on the socket stream in the same order sendMessage was called
			they will reach destination in a different order deppending on the
			message serialization size.
			
	
	Example:
		sendMessage("peer_name", m1_500MB, Message::SynchronousFlagE);
		sendMessage("peer_name", m2_100MB, Message::SynchronousFlagE);
		sendMessage("peer_name", m3_10MB, 0);
		sendMessage("peer_name", m4_1MB, 0);
		
		The order they will reach the peer side is:
		m4_1MB, m3_10MB, m1_500MB, m2_100MB
	
*/
class Service: public Dynamic<Service, frame::Service>{
	typedef FUNCTION<std::pair<MessagePointerT, uint32>(ErrorConditionT const &)>			ActivateConnectionMessageFactoryFunctionT;
	typedef FUNCTION<void(ConnectionContext&, MessagePointerT &, ErrorConditionT const &)>	ResponseHandlerFunctionT;
	
	template <class F, class M>
	struct ResponseHandler{
		F		f;
		
		ResponseHandler(F _f):f(_f){}
		
		void operator()(ConnectionContext&, MessagePointerT &, ErrorConditionT const &){
			//TODO
		}
	};
	
public:
	typedef Dynamic<Service, frame::Service> BaseT;
	
	Service(
		frame::Manager &_rm
	);
	
	//! Destructor
	~Service();

	ErrorConditionT reconfigure(Configuration const& _rcfg);
	
	Configuration const & configuration()const;
	
	template <class T>
	ErrorConditionT sendMessage(
		const char *_recipient_name,
		DynamicPointer<T> const &_rmsgptr,
		ulong _flags = 0
	){
		MessagePointerT		msgptr(_rmsgptr);
		ConnectionUid		conuid;
		return doSendMessage(_recipient_name, conuid, msgptr, nullptr, _flags);
	}
	
	template <class T>
	ErrorConditionT sendMessage(
		const char *_recipient_name,
		DynamicPointer<T> const &_rmsgptr,
		ConnectionPoolUid &_rconpool_uid,
		ulong _flags = 0
	){
		MessagePointerT		msgptr(_rmsgptr);
		ConnectionUid		conuid;
		return doSendMessage(_recipient_name, conuid, msgptr, &_rconpool_uid, _flags);
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
		ConnectionPoolUid const &_rsession_uid,
		DynamicPointer<T> const &_rmsgptr,
		ulong _flags = 0
	){
		MessagePointerT		msgptr(_rmsgptr);
		ConnectionUid		conuid(_rsession_uid);
		return doSendMessage(nullptr, conuid, msgptr, nullptr, _flags);
	}
	
	template <class T, class Fnc>
	ErrorConditionT sendRequest(
		const char *_recipient_name,
		DynamicPointer<T> const &_rmsgptr,
		Fnc _fnc,
		ulong _flags = 0
	){
		typedef ResponseHandler<Fnc, typename response_traits<decltype(_fnc)>::message_type>		ResponseHandlerT;
		
		MessagePointerT		msgptr(_rmsgptr);
		ConnectionUid		conuid;
		ResponseHandlerT	response_handler(_fnc);
		
		return doSendMessage(_recipient_name, conuid, msgptr, nullptr, _flags);
	}
	
	ErrorConditionT scheduleConnectionClose(
		ConnectionUid const &_rconnection_uid
	);
	
	ErrorConditionT activateConnection(
		ConnectionUid const &_rconnection_uid
	){
		ActivateConnectionMessageFactoryFunctionT	msgfactory;
		return doActivateConnection(_rconnection_uid, nullptr, msgfactory, false);
	}
	
	template <class MF>
	ErrorConditionT activateConnection(
		ConnectionUid const &_rconnection_uid,
		const char *_recipient_name,
		MF _msgfactory,
		const bool _may_quit
	){
		ActivateConnectionMessageFactoryFunctionT	msgfactory(_msgfactory);
		return doActivateConnection(_rconnection_uid, _recipient_name, msgfactory, _may_quit);
	}
	
private:
	friend struct ServiceProxy;
	friend class Listener;
	friend class Connection;
	
	void acceptIncomingConnection(SocketDevice &_rsd);
	
	ErrorConditionT doActivateConnection(
		ConnectionUid const &_rconnection_uid,
		const char *_recipient_name,
		ActivateConnectionMessageFactoryFunctionT const &_rmsgfactory,
		const bool _may_quit
	);
	
	void activateConnectionComplete(Connection &_rcon);
	
	void onConnectionClose(Connection &_rcon, aio::ReactorContext &_rctx, ObjectUidT const &_robjuid);
	
	void onIncomingConnectionStart(ConnectionContext &_rconctx);
	void onOutgoingConnectionStart(ConnectionContext &_rconctx);
	void onConnectionStop(ConnectionContext &_rconctx, ErrorConditionT const &_err);
	
	void tryFetchNewMessage(Connection &_rcon, aio::ReactorContext &_rctx, const bool _has_no_message_to_send);
	
	
	void forwardResolveMessage(ConnectionPoolUid const &_rconpoolid, Event const&_revent);
	
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
		
		void operator()(ConnectionContext &_rctx, Message const &_rmsg){
			M const	&rmsg = static_cast<M const &>(_rmsg);
			f(_rctx, rmsg);
		}
	};
	
	template <class T, class FactoryFnc>
	size_t registerType(FactoryFnc _facf, size_t _idx = 0){
		TypeStub ts;
		size_t rv = tm.registerType<T>(_facf, ts, _idx);
		registerCast<T, ipc::Message>();
		return rv;
	}
	
	template <class T, class FactoryFnc, class ReceiveFnc/*, class PrepareFnc*/, class CompleteFnc>
	size_t registerType(FactoryFnc _facf, ReceiveFnc _rcvf/*, PrepareFnc _prepf*/, CompleteFnc _cmpltf, size_t _idx = 0){
		TypeStub ts;
		ts.complete_fnc = MessageCompleteFunctionT(CompleteProxy<CompleteFnc, T>(_cmpltf));
		//ts.prepare_fnc = MessagePrepareFunctionT(PrepareProxy<PrepareFnc, T>(_prepf));
		ts.receive_fnc = MessageReceiveFunctionT(ReceiveProxy<ReceiveFnc, T>(_rcvf));
		size_t rv = tm.registerType<T>(ts, Message::serialize<SerializerT, T>, Message::serialize<DeserializerT, T>, _facf, _idx);
		registerCast<T, ipc::Message>();
		return rv;
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
		ConnectionPoolUid *_pconpoolid_out,
		ulong _flags
	);
	
	ErrorConditionT doSendMessage(
		ObjectUidT const &_robjuid,
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ConnectionPoolUid const &_rconpooluid,
		ConnectionPoolUid *_pconpoolid_out,
		ulong _flags
	);
	size_t doPushNewConnectionPool();
	
	void pushBackMessageToConnectionPool(
		ConnectionPoolUid &_rconpoolid,
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		const ulong _flags,
		const bool _sent
	);
private:
	struct	Data;
	Data			&d;
	TypeIdMapT		tm;
};

struct ServiceProxy{
	template <class T, class FactoryFnc, class ReceiveFnc/*, class PrepareFnc*/, class CompleteFnc>
	size_t registerType(FactoryFnc _facf, ReceiveFnc _rcvf/*, PrepareFnc _prepf*/, CompleteFnc _cmpltf, size_t _idx = 0){
		return rservice.registerType<T>(_facf, _rcvf/*, _prepf*/, _cmpltf, _idx);
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
