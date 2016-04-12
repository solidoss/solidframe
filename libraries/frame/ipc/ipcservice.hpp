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
#include "frame/ipc/ipcconfiguration.hpp"
#include "system/debug.hpp"

namespace solid{
namespace frame{

namespace aio{
struct ReactorContext;
}//namespace aio

namespace ipc{

template <class Req>
struct message_complete_traits;

template <class Req, class Res>
struct message_complete_traits<void(*)(ConnectionContext&, DynamicPointer<Req> &, DynamicPointer<Res>&, ErrorConditionT const&)>{
	typedef Req request_type;
	typedef Res response_type;
};

template <class Req, class Res>
struct message_complete_traits<void(ConnectionContext&, DynamicPointer<Req> &, DynamicPointer<Res>&, ErrorConditionT const&)>{
	typedef Req request_type;
	typedef Res response_type;
};

template <class C, class Req, class Res>
struct message_complete_traits<void(C::*)(ConnectionContext&, DynamicPointer<Req> &, DynamicPointer<Res>&, ErrorConditionT const&)>{
	typedef Req request_type;
	typedef Res response_type;
};

template <class C, class Req, class Res>
struct message_complete_traits<void(C::*)(ConnectionContext&, DynamicPointer<Req> &, DynamicPointer<Res>&, ErrorConditionT const&) const>{
	typedef Req request_type;
	typedef Res response_type;
};

template <class C, class R>
struct message_complete_traits<R(C::*)>: public message_complete_traits<R(C&)>{
};

template <class F>
struct message_complete_traits{
private:
	using call_type = message_complete_traits<decltype(&F::operator())>;
public:
	using request_type  = typename call_type::request_type;
	using response_type = typename call_type::response_type;
};

struct Message;
class Configuration;
class Connection;
struct MessageBundle;


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
	
	template <class F, class Req, class Res>
	struct CompleteHandler{
		F		f;
		
		CompleteHandler(F _f):f(_f){}
		
		void operator()(
			ConnectionContext &_rctx,
			MessagePointerT &_rreq_msg_ptr,
			MessagePointerT &_rres_msg_ptr,
			ErrorConditionT const &_err
		){
			Req					*prequest = dynamic_cast<Req*>(_rreq_msg_ptr.get());
			DynamicPointer<Res>	req_msg_ptr(prequest);
			
			Res					*presponse = dynamic_cast<Res*>(_rres_msg_ptr.get());
			DynamicPointer<Res>	res_msg_ptr(presponse);
			
			ErrorConditionT		error(_err);

			if(not error and req_msg_ptr.get() and not prequest){
				error.assign(-1, error.category());//TODO: bad cast request
			}
			
			if(not error and res_msg_ptr.get() and not presponse){
				error.assign(-1, error.category());//TODO: bad cast response
			}
			
			f(_rctx, req_msg_ptr, res_msg_ptr, error);
		}
	};
	
public:
	typedef Dynamic<Service, frame::Service> BaseT;
	
	Service(
		frame::Manager &_rm
	);
	
	//! Destructor
	~Service();

	ErrorConditionT reconfigure(Configuration && _ucfg);
	
	Configuration const & configuration()const;
	
	// send message using recipient name --------------------------------------
	template <class T>
	ErrorConditionT sendMessage(
		const char *_recipient_name,
		DynamicPointer<T> const &_rmsgptr,
		ulong _flags = 0
	);
	
	template <class T>
	ErrorConditionT sendMessage(
		const char *_recipient_name,
		DynamicPointer<T> const &_rmsgptr,
		RecipientId &_rrecipient_id,
		ulong _flags = 0
	);
	
	template <class T>
	ErrorConditionT sendMessage(
		const char *_recipient_name,
		DynamicPointer<T> const &_rmsgptr,
		RecipientId &_rrecipient_id,
		MessageId &_rmsg_id,
		ulong _flags = 0
	);
	
	// send message using connection uid  -------------------------------------
	
	template <class T>
	ErrorConditionT sendMessage(
		RecipientId const &_rrecipient_id,
		DynamicPointer<T> const &_rmsgptr,
		ulong _flags = 0
	);
	
	template <class T>
	ErrorConditionT sendMessage(
		RecipientId const &_rrecipient_id,
		DynamicPointer<T> const &_rmsgptr,
		MessageId &_rmsg_id,
		ulong _flags = 0
	);
	
	// send request using recipient name --------------------------------------
	
	template <class T, class Fnc>
	ErrorConditionT sendRequest(
		const char *_recipient_name,
		DynamicPointer<T> const &_rmsgptr,
		Fnc _complete_fnc,
		ulong _flags = 0
	);
	
	template <class T, class Fnc>
	ErrorConditionT sendRequest(
		const char *_recipient_name,
		DynamicPointer<T> const &_rmsgptr,
		Fnc _complete_fnc,
		RecipientId &_rrecipient_id,
		ulong _flags = 0
	);
	
	template <class T, class Fnc>
	ErrorConditionT sendRequest(
		const char *_recipient_name,
		DynamicPointer<T> const &_rmsgptr,
		Fnc _complete_fnc,
		RecipientId &_rrecipient_id,
		MessageId &_rmsguid,
		ulong _flags = 0
	);
	
	// send request using connection uid --------------------------------------
	
	template <class T, class Fnc>
	ErrorConditionT sendRequest(
		RecipientId const &_rrecipient_id,
		DynamicPointer<T> const &_rmsgptr,
		Fnc _complete_fnc,
		ulong _flags = 0
	);
	
	template <class T, class Fnc>
	ErrorConditionT sendRequest(
		RecipientId const &_rrecipient_id,
		DynamicPointer<T> const &_rmsgptr,
		Fnc _complete_fnc,
		MessageId &_rmsguid,
		ulong _flags = 0
	);
	// send message with complete using recipient name -------------------------------
	
	template <class T, class Fnc>
	ErrorConditionT sendMessage(
		const char *_recipient_name,
		DynamicPointer<T> const &_rmsgptr,
		Fnc _complete_fnc,
		ulong _flags = 0
	);
	
	template <class T, class Fnc>
	ErrorConditionT sendMessage(
		const char *_recipient_name,
		DynamicPointer<T> const &_rmsgptr,
		Fnc _complete_fnc,
		RecipientId &_rrecipient_id,
		ulong _flags = 0
	);
	
	template <class T, class Fnc>
	ErrorConditionT sendMessage(
		const char *_recipient_name,
		DynamicPointer<T> const &_rmsgptr,
		Fnc _complete_fnc,
		RecipientId &_rrecipient_id,
		MessageId &_rmsguid,
		ulong _flags = 0
	);
	
	// send message with complete using connection uid --------------------------------------
	
	template <class T, class Fnc>
	ErrorConditionT sendMessage(
		RecipientId const &_rrecipient_id,
		DynamicPointer<T> const &_rmsgptr,
		Fnc _complete_fnc,
		ulong _flags = 0
	);
	
	template <class T, class Fnc>
	ErrorConditionT sendMessage(
		RecipientId const &_rrecipient_id,
		DynamicPointer<T> const &_rmsgptr,
		Fnc _complete_fnc,
		MessageId &_rmsguid,
		ulong _flags = 0
	);
	
	//----------------------
	template <typename F>
	ErrorConditionT forceCloseConnectionPool(
		RecipientId const &_rrecipient_id,
		F _f
	);
	
	template <typename F>
	ErrorConditionT delayCloseConnectionPool(
		RecipientId const &_rrecipient_id,
		F _f
	);
	
	
	template <class CompleteFnc>
	ErrorConditionT connectionNotifyEnterActiveState(
		RecipientId const &_rrecipient_id,
		CompleteFnc _complete_fnc,
		const size_t _send_buffer_capacity = 0//0 means: leave as it is
	);
	
	template <class CompleteFnc>
	ErrorConditionT connectionNotifyStartSecureHandshake(
		RecipientId const &_rrecipient_id,
		CompleteFnc _complete_fnc
	);
	
	template <class CompleteFnc>
	ErrorConditionT connectionNotifyEnterPassiveState(
		RecipientId const &_rrecipient_id,
		CompleteFnc _complete_fnc
	);
	
	template <class CompleteFnc>
	ErrorConditionT connectionNotifySendAllRawData(
		RecipientId const &_rrecipient_id,
		CompleteFnc _complete_fnc,
		std::string &&_rdata
	);
	
	template <class CompleteFnc>
	ErrorConditionT connectionNotifyRecvSomeRawData(
		RecipientId const &_rrecipient_id,
		CompleteFnc _complete_fnc
	);
	
	ErrorConditionT cancelMessage(RecipientId const &_rrecipient_id, MessageId const &_rmsg_id);
private:
	ErrorConditionT doConnectionNotifyEnterActiveState(
		RecipientId const &_rrecipient_id,
		ConnectionEnterActiveCompleteFunctionT &&_ucomplete_fnc,
		const size_t _send_buffer_capacity
	);
	ErrorConditionT doConnectionNotifyStartSecureHandshake(
		RecipientId const &_rrecipient_id,
		ConnectionSecureHandhakeCompleteFunctionT &&_ucomplete_fnc
	);
	
	ErrorConditionT doConnectionNotifyEnterPassiveState(
		RecipientId const &_rrecipient_id,
		ConnectionEnterPassiveCompleteFunctionT &&_ucomplete_fnc
	);
	ErrorConditionT doConnectionNotifySendRawData(
		RecipientId const &_rrecipient_id,
		ConnectionSendRawDataCompleteFunctionT &&_ucomplete_fnc,
		std::string &&_rdata
	);
	ErrorConditionT doConnectionNotifyRecvRawData(
		RecipientId const &_rrecipient_id,
		ConnectionRecvRawDataCompleteFunctionT &&_ucomplete_fnc
	);
private:
	friend struct ServiceProxy;
	friend class Listener;
	friend class Connection;
	
	void acceptIncomingConnection(SocketDevice &_rsd);
	
	ErrorConditionT activateConnection(Connection &_rcon, ObjectIdT const &_robjuid);
	
	void connectionStop(Connection const &_rcon);
	
	bool connectionStopping(
		Connection &_rcon, ObjectIdT const &_robjuid,
		ulong &_rseconds_to_wait,
		MessageId &_rmsg_id,
		MessageBundle &_rmsg_bundle,
		Event &_revent_context
	);
	
	bool doNonMainConnectionStopping(
		Connection &_rcon, ObjectIdT const &_robjuid,
		ulong &_rseconds_to_wait,
		MessageId &_rmsg_id,
		MessageBundle &_rmsg_bundle,
		Event &_revent_context
	);
	
	bool doMainConnectionStoppingNotLast(
		Connection &_rcon, ObjectIdT const &/*_robjuid*/,
		ulong &_rseconds_to_wait,
		MessageId &/*_rmsg_id*/,
		MessageBundle &/*_rmsg_bundle*/,
		Event &_revent_context
	);
	
	bool doMainConnectionStoppingCleanOneShot(
		Connection &_rcon, ObjectIdT const &_robjuid,
		ulong &_rseconds_to_wait,
		MessageId &_rmsg_id,
		MessageBundle &_rmsg_bundle,
		Event &_revent_context
	);
	
	bool doMainConnectionStoppingCleanAll(
		Connection &_rcon, ObjectIdT const &_robjuid,
		ulong &_rseconds_to_wait,
		MessageId &_rmsg_id,
		MessageBundle &_rmsg_bundle,
		Event &_revent_context
	);
	
	bool doMainConnectionStoppingPrepareCleanOneShot(
		Connection &_rcon, ObjectIdT const &/*_robjuid*/,
		ulong &/*_rseconds_to_wait*/,
		MessageId &/*_rmsg_id*/,
		MessageBundle &/*_rmsg_bundle*/,
		Event &_revent_context
	);
	
	bool doMainConnectionStoppingPrepareCleanAll(
		Connection &_rcon, ObjectIdT const &/*_robjuid*/,
		ulong &/*_rseconds_to_wait*/,
		MessageId &/*_rmsg_id*/,
		MessageBundle &/*_rmsg_bundle*/,
		Event &_revent_context
	);
	
	bool doMainConnectionRestarting(
		Connection &_rcon, ObjectIdT const &/*_robjuid*/,
		ulong &/*_rseconds_to_wait*/,
		MessageId &/*_rmsg_id*/,
		MessageBundle &/*_rmsg_bundle*/,
		Event &_revent_context
	);
	
	void onIncomingConnectionStart(ConnectionContext &_rconctx);
	void onOutgoingConnectionStart(ConnectionContext &_rconctx);
	void onConnectionStop(ConnectionContext &_rconctx, ErrorConditionT const &_err);
	
	ErrorConditionT pollPoolForUpdates(
		Connection &_rcon,
		ObjectIdT const &_robjuid,
		MessageId const &_rmsgid
	);
	
	void rejectNewPoolMessage(Connection const &_rcon);
	
	bool fetchMessage(Connection &_rcon, ObjectIdT const &_robjuid, MessageId const &_rmsg_id);
	
	bool fetchCanceledMessage(Connection const &_rcon, MessageId const &_rmsg_id, MessageBundle &_rmsg_bundle);
	
	bool doTryPushMessageToConnection(
		Connection &_rcon,
		ObjectIdT const &_robjuid,
		const size_t _pool_idx,
		const size_t msg_idx
	);
	
	bool doTryPushMessageToConnection(
		Connection &_rcon,
		ObjectIdT const &_robjuid,
		const size_t _pool_idx,
		const MessageId & _rmsg_id
	);
	
	void forwardResolveMessage(ConnectionPoolId const &_rconpoolid, Event &_revent);
	
	void doPushFrontMessageToPool(
		const ConnectionPoolId &_rpool_id,
		MessageBundle &_rmsgbundle,
		MessageId const &_rmsgid
	);
	
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
	size_t registerType(
		FactoryFnc _facf,
		const size_t _protocol_id,
		const size_t _idx = 0
	){
		TypeStub ts;
		size_t rv = tm.registerType<T>(_facf, ts, _protocol_id, _idx);
		registerCast<T, ipc::Message>();
		return rv;
	}
	
	template <class T, class FactoryFnc, class ReceiveFnc/*, class PrepareFnc*/, class CompleteFnc>
	size_t registerType(
		FactoryFnc _facf, ReceiveFnc _rcvf/*, PrepareFnc _prepf*/,
		CompleteFnc _cmpltf,
		const size_t _protocol_id,
		const size_t _idx = 0
	){
		TypeStub ts;
		ts.complete_fnc = MessageCompleteFunctionT(CompleteProxy<CompleteFnc, T>(_cmpltf));
		//ts.prepare_fnc = MessagePrepareFunctionT(PrepareProxy<PrepareFnc, T>(_prepf));
		ts.complete_fnc = MessageReceiveFunctionT(ReceiveProxy<ReceiveFnc, T>(_rcvf));
		size_t rv = tm.registerType<T>(
			ts, Message::serialize<SerializerT, T>, Message::serialize<DeserializerT, T>, _facf,
			_protocol_id, _idx
		);
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
		const char *_recipient_name,
		const RecipientId	&_rrecipient_id_in,
		MessagePointerT &_rmsgptr,
		MessageCompleteFunctionT &_rcomplete_fnc,
		RecipientId *_precipient_id_out,
		MessageId *_pmsg_id_out,
		ulong _flags
	);
	
	ErrorConditionT doSendMessageToNewPool(
		const char *_recipient_name,
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		MessageCompleteFunctionT &_rcomplete_fnc,
		RecipientId *_precipient_id_out,
		MessageId *_pmsguid_out,
		ulong _flags
	);
	
	ErrorConditionT doSendMessageToConnection(
		const RecipientId	&_rrecipient_id_in,
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		MessageCompleteFunctionT &_rcomplete_fnc,
		MessageId *_pmsg_id_out,
		ulong _flags
	);
	
	bool doTryCreateNewConnectionForPool(const size_t _pool_index, ErrorConditionT &_rerror);
	
	void doFetchResendableMessagesFromConnection(
		Connection &_rcon
	);
	
	size_t doPushNewConnectionPool();
	
	void pushFrontMessageToConnectionPool(
		ConnectionPoolId &_rconpoolid,
		MessageBundle &_rmsgbundle,
		MessageId const &_rmsgid
	);
	
	bool doTryNotifyPoolWaitingConnection(const size_t _conpoolindex);
	
	ErrorConditionT doForceCloseConnectionPool(
		RecipientId const &_rrecipient_id, 
		MessageCompleteFunctionT &_rcomplete_fnc
	);
	
	ErrorConditionT doDelayCloseConnectionPool(
		RecipientId const &_rrecipient_id, 
		MessageCompleteFunctionT &_rcomplete_fnc
	);
	
private:
	struct	Data;
	Data			&d;
	TypeIdMapT		tm;
};

//-----------------------------------------------------------------------------
template <class T>
ErrorConditionT Service::sendMessage(
	const char *_recipient_name,
	DynamicPointer<T> const &_rmsgptr,
	ulong _flags
){
	MessagePointerT				msgptr(_rmsgptr);
	RecipientId					recipient_id;
	MessageCompleteFunctionT	complete_handler;
	return doSendMessage(_recipient_name, recipient_id, msgptr, complete_handler, nullptr, nullptr, _flags);
}
//-----------------------------------------------------------------------------
template <class T>
ErrorConditionT Service::sendMessage(
	const char *_recipient_name,
	DynamicPointer<T> const &_rmsgptr,
	RecipientId &_rrecipient_id,
	ulong _flags
){
	MessagePointerT				msgptr(_rmsgptr);
	RecipientId					recipient_id;
	MessageCompleteFunctionT	complete_handler;
	return doSendMessage(_recipient_name, recipient_id, msgptr, complete_handler, &_rrecipient_id, nullptr, _flags);
}
//-----------------------------------------------------------------------------
template <class T>
ErrorConditionT Service::sendMessage(
	const char *_recipient_name,
	DynamicPointer<T> const &_rmsgptr,
	RecipientId &_rrecipient_id,
	MessageId &_rmsg_id,
	ulong _flags
){
	MessagePointerT				msgptr(_rmsgptr);
	RecipientId					recipient_id;
	MessageCompleteFunctionT	complete_handler;
	return doSendMessage(_recipient_name, recipient_id, msgptr, complete_handler, &_rrecipient_id, &_rmsg_id, _flags);
}
//-----------------------------------------------------------------------------
// send message using connection uid  -------------------------------------
template <class T>
ErrorConditionT Service::sendMessage(
	RecipientId const &_rrecipient_id,
	DynamicPointer<T> const &_rmsgptr,
	ulong _flags
){
	MessagePointerT				msgptr(_rmsgptr);
	MessageCompleteFunctionT	complete_handler;
	return doSendMessage(nullptr, _rrecipient_id, msgptr, complete_handler, nullptr, nullptr, _flags);
}
//-----------------------------------------------------------------------------
template <class T>
ErrorConditionT Service::sendMessage(
	RecipientId const &_rrecipient_id,
	DynamicPointer<T> const &_rmsgptr,
	MessageId &_rmsg_id,
	ulong _flags
){
	MessagePointerT				msgptr(_rmsgptr);
	MessageCompleteFunctionT	complete_handler;
	return doSendMessage(nullptr, _rrecipient_id, msgptr, complete_handler, nullptr, &_rmsg_id, _flags);
}
//-----------------------------------------------------------------------------
// send request using recipient name ------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendRequest(
	const char *_recipient_name,
	DynamicPointer<T> const &_rmsgptr,
	Fnc _complete_fnc,
	ulong _flags
){
	using CompleteHandlerT = CompleteHandler<
		Fnc,
		typename message_complete_traits<decltype(_complete_fnc)>::request_type,
		typename message_complete_traits<decltype(_complete_fnc)>::response_type
	>;
	
	MessagePointerT				msgptr(_rmsgptr);
	RecipientId					recipient_id;
	CompleteHandlerT			fnc(_complete_fnc);
	MessageCompleteFunctionT	complete_handler(fnc);
	
	return doSendMessage(_recipient_name, recipient_id, msgptr, complete_handler, nullptr, nullptr, _flags | Message::WaitResponseFlagE);
}
//-----------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendRequest(
	const char *_recipient_name,
	DynamicPointer<T> const &_rmsgptr,
	Fnc _complete_fnc,
	RecipientId &_rrecipient_id,
	ulong _flags
){
	using CompleteHandlerT = CompleteHandler<
		Fnc,
		typename message_complete_traits<decltype(_complete_fnc)>::request_type,
		typename message_complete_traits<decltype(_complete_fnc)>::response_type
	>;
	
	MessagePointerT				msgptr(_rmsgptr);
	RecipientId					recipient_id;
	CompleteHandlerT			fnc(_complete_fnc);
	MessageCompleteFunctionT	complete_handler(fnc);
	
	return doSendMessage(_recipient_name, recipient_id, msgptr, complete_handler, &_rrecipient_id, nullptr, _flags | Message::WaitResponseFlagE);
}
//-----------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendRequest(
	const char *_recipient_name,
	DynamicPointer<T> const &_rmsgptr,
	Fnc _complete_fnc,
	RecipientId &_rrecipient_id,
	MessageId &_rmsguid,
	ulong _flags
){
	using CompleteHandlerT = CompleteHandler<
		Fnc,
		typename message_complete_traits<decltype(_complete_fnc)>::request_type,
		typename message_complete_traits<decltype(_complete_fnc)>::response_type
	>;
	
	MessagePointerT				msgptr(_rmsgptr);
	RecipientId					recipient_id;
	CompleteHandlerT			fnc(_complete_fnc);
	MessageCompleteFunctionT	complete_handler(fnc);
	
	return doSendMessage(_recipient_name, recipient_id, msgptr, complete_handler, &_rrecipient_id, &_rmsguid, _flags | Message::WaitResponseFlagE);
}

//-----------------------------------------------------------------------------
// send request using connection uid ------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendRequest(
	RecipientId const &_rrecipient_id,
	DynamicPointer<T> const &_rmsgptr,
	Fnc _complete_fnc,
	ulong _flags
){
	using CompleteHandlerT = CompleteHandler<
		Fnc,
		typename message_complete_traits<decltype(_complete_fnc)>::request_type,
		typename message_complete_traits<decltype(_complete_fnc)>::response_type
	>;
	
	MessagePointerT				msgptr(_rmsgptr);
	CompleteHandlerT			fnc(_complete_fnc);
	MessageCompleteFunctionT	complete_handler(fnc);
	
	return doSendMessage(nullptr, _rrecipient_id, msgptr, complete_handler, nullptr, nullptr, _flags | Message::WaitResponseFlagE);
}
//-----------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendRequest(
	RecipientId const &_rrecipient_id,
	DynamicPointer<T> const &_rmsgptr,
	Fnc _complete_fnc,
	MessageId &_rmsguid,
	ulong _flags
){
	using CompleteHandlerT = CompleteHandler<
		Fnc,
		typename message_complete_traits<decltype(_complete_fnc)>::request_type,
		typename message_complete_traits<decltype(_complete_fnc)>::response_type
	>;
	
	MessagePointerT				msgptr(_rmsgptr);
	CompleteHandlerT			fnc(_complete_fnc);
	MessageCompleteFunctionT	complete_handler(fnc);
	
	return doSendMessage(nullptr, _rrecipient_id, msgptr, complete_handler, nullptr, &_rmsguid, _flags | Message::WaitResponseFlagE);
}

//-----------------------------------------------------------------------------
// send message with complete using recipient name ----------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendMessage(
	const char *_recipient_name,
	DynamicPointer<T> const &_rmsgptr,
	Fnc _complete_fnc,
	ulong _flags
){
	using CompleteHandlerT = CompleteHandler<
		Fnc,
		typename message_complete_traits<decltype(_complete_fnc)>::request_type,
		typename message_complete_traits<decltype(_complete_fnc)>::response_type
	>;
	
	MessagePointerT				msgptr(_rmsgptr);
	RecipientId					recipient_id;
	CompleteHandlerT			fnc(_complete_fnc);
	MessageCompleteFunctionT	complete_handler(fnc);
	
	return doSendMessage(_recipient_name, recipient_id, msgptr, complete_handler, nullptr, nullptr, _flags);
}
//-----------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendMessage(
	const char *_recipient_name,
	DynamicPointer<T> const &_rmsgptr,
	Fnc _complete_fnc,
	RecipientId &_rrecipient_id,
	ulong _flags
){
	using CompleteHandlerT = CompleteHandler<
		Fnc,
		typename message_complete_traits<decltype(_complete_fnc)>::request_type,
		typename message_complete_traits<decltype(_complete_fnc)>::response_type
	>;
	
	MessagePointerT				msgptr(_rmsgptr);
	RecipientId					recipient_id;
	CompleteHandlerT			fnc(_complete_fnc);
	MessageCompleteFunctionT	complete_handler(fnc);
	
	return doSendMessage(_recipient_name, recipient_id, msgptr, complete_handler, &_rrecipient_id, nullptr, _flags);
}
//-----------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendMessage(
	const char *_recipient_name,
	DynamicPointer<T> const &_rmsgptr,
	Fnc _complete_fnc,
	RecipientId &_rrecipient_id,
	MessageId &_rmsguid,
	ulong _flags
){
	using CompleteHandlerT = CompleteHandler<
		Fnc,
		typename message_complete_traits<decltype(_complete_fnc)>::request_type,
		typename message_complete_traits<decltype(_complete_fnc)>::response_type
	>;
	
	MessagePointerT				msgptr(_rmsgptr);
	RecipientId					recipient_id;
	CompleteHandlerT			fnc(_complete_fnc);
	MessageCompleteFunctionT	complete_handler(fnc);
	
	return doSendMessage(_recipient_name, recipient_id, msgptr, complete_handler, &_rrecipient_id, &_rmsguid, _flags);
}
//-----------------------------------------------------------------------------
// send message with complete using connection uid ----------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendMessage(
	RecipientId const &_rrecipient_id,
	DynamicPointer<T> const &_rmsgptr,
	Fnc _complete_fnc,
	ulong _flags
){
	using CompleteHandlerT = CompleteHandler<
		Fnc,
		typename message_complete_traits<decltype(_complete_fnc)>::request_type,
		typename message_complete_traits<decltype(_complete_fnc)>::response_type
	>;
	
	MessagePointerT				msgptr(_rmsgptr);
	CompleteHandlerT			fnc(_complete_fnc);
	MessageCompleteFunctionT	complete_handler(fnc);
	
	return doSendMessage(nullptr, _rrecipient_id, msgptr, complete_handler, nullptr, nullptr, _flags);
}
//-----------------------------------------------------------------------------
template <class T, class Fnc>
ErrorConditionT Service::sendMessage(
	RecipientId const &_rrecipient_id,
	DynamicPointer<T> const &_rmsgptr,
	Fnc _complete_fnc,
	MessageId &_rmsguid,
	ulong _flags
){
	using CompleteHandlerT = CompleteHandler<
		Fnc,
		typename message_complete_traits<decltype(_complete_fnc)>::request_type,
		typename message_complete_traits<decltype(_complete_fnc)>::response_type
	>;
	
	MessagePointerT				msgptr(_rmsgptr);
	CompleteHandlerT			fnc(_complete_fnc);
	MessageCompleteFunctionT	complete_handler(fnc);
	
	return doSendMessage(nullptr, _rrecipient_id, msgptr, complete_handler, nullptr, &_rmsguid, _flags);
}

//-----------------------------------------------------------------------------
template <typename F>
ErrorConditionT Service::forceCloseConnectionPool(
	RecipientId const &_rrecipient_id,
	F _f
){
	auto fnc = [_f](ConnectionContext &_rctx, MessagePointerT &/*_rmsgptr*/, MessagePointerT &, ErrorConditionT const &/*_err*/){
		_f(_rctx);
	};
	
	MessageCompleteFunctionT	complete_handler(fnc);
	return doForceCloseConnectionPool(_rrecipient_id, complete_handler);
}
//-----------------------------------------------------------------------------
template <typename F>
ErrorConditionT Service::delayCloseConnectionPool(
	RecipientId const &_rrecipient_id,
	F _f
){
	auto fnc = [_f](ConnectionContext &_rctx, MessagePointerT &/*_rmsgptr*/, MessagePointerT &, ErrorConditionT const &/*_err*/){
		_f(_rctx);
	};
	
	MessageCompleteFunctionT	complete_handler(fnc);
	return doDelayCloseConnectionPool(_rrecipient_id, complete_handler);
}

//-----------------------------------------------------------------------------
template <class CompleteFnc>
ErrorConditionT Service::connectionNotifyEnterActiveState(
	RecipientId const &_rrecipient_id,
	CompleteFnc _complete_fnc,
	const size_t _send_buffer_capacity/* = 0*///0 means: leave as it is
){
	ConnectionEnterActiveCompleteFunctionT	complete_fnc(_complete_fnc);
	return doConnectionNotifyEnterActiveState(_rrecipient_id, std::move(complete_fnc), _send_buffer_capacity);
}
//-----------------------------------------------------------------------------
template <class CompleteFnc>
ErrorConditionT Service::connectionNotifyStartSecureHandshake(
	RecipientId const &_rrecipient_id,
	CompleteFnc _complete_fnc
){
	ConnectionSecureHandhakeCompleteFunctionT	complete_fnc(_complete_fnc);
	return doConnectionNotifyStartSecureHandshake(_rrecipient_id, std::move(complete_fnc));
}
//-----------------------------------------------------------------------------
template <class CompleteFnc>
ErrorConditionT Service::connectionNotifyEnterPassiveState(
	RecipientId const &_rrecipient_id,
	CompleteFnc _complete_fnc
){
	ConnectionEnterPassiveCompleteFunctionT	complete_fnc(_complete_fnc);
	return doConnectionNotifyEnterPassiveState(_rrecipient_id, std::move(complete_fnc));
}
//-----------------------------------------------------------------------------
template <class CompleteFnc>
ErrorConditionT Service::connectionNotifySendAllRawData(
	RecipientId const &_rrecipient_id,
	CompleteFnc _complete_fnc,
	std::string &&_rdata
){
	ConnectionSendRawDataCompleteFunctionT	complete_fnc(_complete_fnc);
	return doConnectionNotifySendRawData(_rrecipient_id, std::move(complete_fnc), std::move(_rdata));
}
//-----------------------------------------------------------------------------
template <class CompleteFnc>
ErrorConditionT Service::connectionNotifyRecvSomeRawData(
	RecipientId const &_rrecipient_id,
	CompleteFnc _complete_fnc
){
	ConnectionRecvRawDataCompleteFunctionT	complete_fnc(_complete_fnc);
	return doConnectionNotifyRecvRawData(_rrecipient_id, std::move(complete_fnc));
}

//-----------------------------------------------------------------------------
// ServiceProxy
//-----------------------------------------------------------------------------
struct ServiceProxy{
	template <class T, class FactoryFnc, class CompleteFnc>
	size_t registerType(
		FactoryFnc _facf,
		CompleteFnc _cmpltf,
		const size_t _protocol_id = 0,
		const size_t _idx = 0
	){
		return rservice.registerType<T>(_facf, _cmpltf, _protocol_id, _idx);
	}
	template <class T, class FactoryFnc>
	size_t registerType(FactoryFnc _facf,
		const size_t _protocol_id = 0,
		const size_t _idx = 0
	){
		return rservice.registerType<T>(_facf, _protocol_id, _idx);
	}
private:
	friend class Service;
	friend class Configuration;
	ServiceProxy(Service &_rservice):rservice(_rservice){}
	
	ServiceProxy(ServiceProxy const &) = delete;
	ServiceProxy& operator=(ServiceProxy const &) = delete;
	
	Service &rservice;
};

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
