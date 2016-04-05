// frame/ipc/src/ipcconnection.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "utility/event.hpp"
#include "frame/manager.hpp"
#include "ipcconnection.hpp"
#include "frame/ipc/ipcerror.hpp"
#include "frame/ipc/ipcservice.hpp"
#include "frame/aio/aioreactorcontext.hpp"
#include "frame/aio/openssl/aiosecuresocket.hpp"

namespace solid{
namespace frame{
namespace ipc{

namespace{

enum class Flags:size_t{
	Active						= 1,
	Server						= 4,
	Keepalive					= 8,
	WaitKeepAliveTimer			= 16,
	StopPeer					= 32,
	HasActivity 				= 64,
	PollPool					= 128,
	Stopping					= 256,
	DelayedStopping				= 512,
	Secure						= 1024,
	Raw							= 2048,
	InPoolWaitQueue				= 4096,
};

enum class ConnectionEvents{
	Resolve,
	NewPoolMessage,
	NewConnMessage,
	CancelConnMessage,
	CancelPoolMessage,
	EnterActive,
	EnterPassive,
	StartSecure,
	SendRaw,
	RecvRaw,
	Invalid,
};

const EventCategory<ConnectionEvents>	connection_event_category{
	"solid::frame::ipc::connection_event_category",
	[](const ConnectionEvents _evt){
		switch(_evt){
			case ConnectionEvents::Resolve:
				return "Resolve";
			case ConnectionEvents::NewPoolMessage:
				return "NewPoolMessage";
			case ConnectionEvents::NewConnMessage:
				return "NewConnMessage";
			case ConnectionEvents::CancelConnMessage:
				return "CancelConnMessage";
			case ConnectionEvents::CancelPoolMessage:
				return "CancelPoolMessage";
			case ConnectionEvents::EnterActive:
				return "EnterActive";
			case ConnectionEvents::EnterPassive:
				return "EnterPassive";
			case ConnectionEvents::StartSecure:
				return "StartSecure";
			case ConnectionEvents::SendRaw:
				return "SendRaw";
			case ConnectionEvents::RecvRaw:
				return "RecvRaw";
			case ConnectionEvents::Invalid:
				return "Invalid";
			default:
				return "unknown";
		}
	}
};

}//namespace


//-----------------------------------------------------------------------------
inline Service& Connection::service(frame::aio::ReactorContext &_rctx)const{
	return static_cast<Service&>(_rctx.service());
}
//-----------------------------------------------------------------------------
inline ObjectIdT Connection::uid(frame::aio::ReactorContext &_rctx)const{
	return service(_rctx).manager().id(*this);
}
//-----------------------------------------------------------------------------

/*static*/ Event Connection::eventResolve(){
	return connection_event_category.event(ConnectionEvents::Resolve);
}
//-----------------------------------------------------------------------------
/*static*/ Event Connection::eventNewMessage(){
	return connection_event_category.event(ConnectionEvents::NewPoolMessage);
}
//-----------------------------------------------------------------------------
/*static*/ Event Connection::eventNewMessage(const MessageId &_rmsgid){
	Event event = connection_event_category.event(ConnectionEvents::NewConnMessage);
	event.any().reset(_rmsgid);
	return event;
}
//-----------------------------------------------------------------------------
/*static*/ Event Connection::eventCancelConnMessage(const MessageId &_rmsgid){
	Event event = connection_event_category.event(ConnectionEvents::CancelConnMessage);
	event.any().reset(_rmsgid);
	return event;
}
//-----------------------------------------------------------------------------
/*static*/ Event Connection::eventCancelPoolMessage(const MessageId &_rmsgid){
	Event event = connection_event_category.event(ConnectionEvents::CancelPoolMessage);
	event.any().reset(_rmsgid);
	return event;
}
//-----------------------------------------------------------------------------
struct EnterActive{
	EnterActive(
		ConnectionEnterActiveCompleteFunctionT &&_ucomplete_fnc,
		const size_t _send_buffer_capacity
	):complete_fnc(std::move(_ucomplete_fnc)), send_buffer_capacity(_send_buffer_capacity){}
	
	ConnectionEnterActiveCompleteFunctionT	complete_fnc;
	const size_t							send_buffer_capacity;
};
/*static*/ Event Connection::eventEnterActive(ConnectionEnterActiveCompleteFunctionT &&_ucomplete_fnc, const size_t _send_buffer_capacity){
	Event event = connection_event_category.event(ConnectionEvents::EnterActive);
	event.any().reset(EnterActive(std::move(_ucomplete_fnc), _send_buffer_capacity));
	return event;
}
//-----------------------------------------------------------------------------
struct EnterPassive{
	EnterPassive(
		ConnectionEnterPassiveCompleteFunctionT &&_ucomplete_fnc
	):complete_fnc(std::move(_ucomplete_fnc)){}
	
	ConnectionEnterPassiveCompleteFunctionT	complete_fnc;
};
/*static*/ Event Connection::eventEnterPassive(ConnectionEnterPassiveCompleteFunctionT &&_ucomplete_fnc){
	Event event = connection_event_category.event(ConnectionEvents::EnterPassive);
	event.any().reset(EnterPassive(std::move(_ucomplete_fnc)));
	return event;
}
//-----------------------------------------------------------------------------
struct StartSecure{
	StartSecure(
		ConnectionSecureHandhakeCompleteFunctionT &&_ucomplete_fnc
	):complete_fnc(std::move(_ucomplete_fnc)){}
	
	ConnectionSecureHandhakeCompleteFunctionT	complete_fnc;
};
/*static*/ Event Connection::eventStartSecure(ConnectionSecureHandhakeCompleteFunctionT &&_ucomplete_fnc){
	Event event = connection_event_category.event(ConnectionEvents::StartSecure);
	event.any().reset(StartSecure(std::move(_ucomplete_fnc)));
	return event;
}
//-----------------------------------------------------------------------------
struct SendRaw{
	SendRaw(
		ConnectionSendRawDataCompleteFunctionT &&_ucomplete_fnc,
		std::string &&_udata
	):complete_fnc(std::move(_ucomplete_fnc)), data(std::move(_udata)){}
	
	ConnectionSendRawDataCompleteFunctionT	complete_fnc;
	std::string								data;
};
/*static*/ Event Connection::eventSendRaw(ConnectionSendRawDataCompleteFunctionT &&_ucomplete_fnc, std::string &&_udata){
	Event event = connection_event_category.event(ConnectionEvents::SendRaw);
	event.any().reset(SendRaw(std::move(_ucomplete_fnc), std::move(_udata)));
	return event;
}
//-----------------------------------------------------------------------------
struct RecvRaw{
	RecvRaw(
		ConnectionRecvRawDataCompleteFunctionT &&_ucomplete_fnc
	):complete_fnc(std::move(_ucomplete_fnc)){}
	
	ConnectionRecvRawDataCompleteFunctionT	complete_fnc;
};
/*static*/ Event Connection::eventRecvRaw(ConnectionRecvRawDataCompleteFunctionT &&_ucomplete_fnc){
	Event event = connection_event_category.event(ConnectionEvents::RecvRaw);
	event.any().reset(RecvRaw(std::move(_ucomplete_fnc)));
	return event;
}
//-----------------------------------------------------------------------------
inline void Connection::doOptimizeRecvBuffer(){
	const size_t cnssz = recv_buf_off - cons_buf_off;
	if(cnssz <= cons_buf_off){
		//idbgx(Debug::proto_bin, this<<' '<<"memcopy "<<cnssz<<" rcvoff = "<<receivebufoff<<" cnsoff = "<<consumebufoff);
		
		memcpy(recv_buf, recv_buf + cons_buf_off, cnssz);
		cons_buf_off = 0;
		recv_buf_off = cnssz;
	}
}
//-----------------------------------------------------------------------------
inline void Connection::doOptimizeRecvBufferForced(){
	const size_t cnssz = recv_buf_off - cons_buf_off;
	
	//idbgx(Debug::proto_bin, this<<' '<<"memcopy "<<cnssz<<" rcvoff = "<<receivebufoff<<" cnsoff = "<<consumebufoff);
	
	memmove(recv_buf, recv_buf + cons_buf_off, cnssz);
	cons_buf_off = 0;
	recv_buf_off = cnssz;
}
//-----------------------------------------------------------------------------
Connection::Connection(
	Configuration const& _rconfiguration,
	ConnectionPoolId const &_rpool_id,
	const std::string &_rpool_name
):	pool_id(_rpool_id), rpool_name(_rpool_name), timer(this->proxy()),
	flags(0), recv_buf_off(0), cons_buf_off(0),
	recv_keepalive_count(0),
	recv_buf(nullptr), send_buf(nullptr),
	recv_buf_cp_kb(0), send_buf_cp_kb(0)
{
	//TODO: use _rconfiguration.connection_start_state and _rconfiguration.connection_start_secure
}
//-----------------------------------------------------------------------------
Connection::~Connection(){
	idbgx(Debug::ipc, this);
}
//-----------------------------------------------------------------------------
bool Connection::isFull(Configuration const& _rconfiguration)const{
	return true;
}
//-----------------------------------------------------------------------------
bool Connection::isInPoolWaitingQueue() const{
	return flags & static_cast<size_t>(Flags::InPoolWaitQueue);
}
//-----------------------------------------------------------------------------
void Connection::setInPoolWaitingQueue(){
	flags |= static_cast<size_t>(Flags::InPoolWaitQueue);
}
//-----------------------------------------------------------------------------
// - called under pool's lock
bool Connection::tryPushMessage(
	Configuration const& _rconfiguration,
	MessageBundle &_rmsgbundle,
	MessageId &_rconn_msg_id,
	const MessageId &_rpool_msg_id
){
	return msg_writer.enqueue(_rconfiguration.writer, _rmsgbundle, _rpool_msg_id, _rconn_msg_id);
}
//-----------------------------------------------------------------------------
bool Connection::isActive()const{
	return flags & static_cast<size_t>(Flags::Active);
}
//-----------------------------------------------------------------------------
bool Connection::isStopping()const{
	return flags & static_cast<size_t>(Flags::Stopping);
}
//-----------------------------------------------------------------------------
bool Connection::isDelayedStopping()const{
	return flags & static_cast<size_t>(Flags::DelayedStopping);
}
//-----------------------------------------------------------------------------
bool Connection::isServer()const{
	return flags & static_cast<size_t>(Flags::Server);
}
//-----------------------------------------------------------------------------
bool Connection::shouldSendKeepalive()const{
	return flags & static_cast<size_t>(Flags::Keepalive);
}
//-----------------------------------------------------------------------------
bool Connection::shouldPollPool()const{
	return flags & static_cast<size_t>(Flags::PollPool);
}
//-----------------------------------------------------------------------------
bool Connection::isWaitingKeepAliveTimer()const{
	return flags & static_cast<size_t>(Flags::WaitKeepAliveTimer);
}
//-----------------------------------------------------------------------------
bool Connection::isStopPeer()const{
	return flags & static_cast<size_t>(Flags::StopPeer);
}
//-----------------------------------------------------------------------------
void Connection::doPrepare(frame::aio::ReactorContext &_rctx){
	recv_buf = service(_rctx).configuration().allocateRecvBuffer(recv_buf_cp_kb);
	send_buf = service(_rctx).configuration().allocateSendBuffer(send_buf_cp_kb);
	msg_reader.prepare(service(_rctx).configuration().reader);
	msg_writer.prepare(service(_rctx).configuration().writer);
}
//-----------------------------------------------------------------------------
void Connection::doUnprepare(frame::aio::ReactorContext &_rctx){
	service(_rctx).configuration().freeRecvBuffer(recv_buf);
	service(_rctx).configuration().freeSendBuffer(send_buf);
	msg_reader.unprepare();
	msg_writer.unprepare();
}
//-----------------------------------------------------------------------------
void Connection::doStart(frame::aio::ReactorContext &_rctx, const bool _is_incomming){
	ConnectionContext 	conctx(service(_rctx), *this);
	//Configuration const &config = service(_rctx).configuration();
	
	doPrepare(_rctx);
	
	if(_is_incomming){
		flags |= static_cast<size_t>(Flags::Server);
		service(_rctx).onIncomingConnectionStart(conctx);
	}else{
		service(_rctx).onOutgoingConnectionStart(conctx);
	}
	
	idbgx(Debug::ipc, this<<" post send");
	//start sending messages.
	this->post(
		_rctx,
		[this](frame::aio::ReactorContext &_rctx, Event &&/*_revent*/){
			doSend(_rctx);
		}
	);
	
	//start receiving messages
	this->postRecvSome(_rctx, recv_buf, recvBufferCapacity());
	doResetTimerStart(_rctx);
}
//-----------------------------------------------------------------------------
void Connection::onStopped(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr){
	ObjectIdT			objuid(uid(_rctx));
	ConnectionContext	conctx(service(_rctx), *this);
	
	//TODO: service(_rctx).connectionClose(*this, _rctx, objuid);//must be called after postStop!!
	
	doCompleteAllMessages(_rctx, _rerr);
	
	//TODO: service(_rctx).onConnectionStop(conctx, _rerr);
	
	doUnprepare(_rctx);
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onTimerWaitStopping(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr){
	Connection			&rthis = static_cast<Connection&>(_rctx.object());
	ObjectIdT			objuid(rthis.uid(_rctx));
	ErrorConditionT		error(_rerr);
	ulong				seconds_to_wait = 0;//rthis.service(_rctx).onConnectionWantClose(rthis, _rctx, objuid);
	
	if(!seconds_to_wait){
		//can stop rightaway
		rthis.postStop(_rctx, 
			[error](frame::aio::ReactorContext &_rctx, Event &&/*_revent*/){
				Connection	&rthis = static_cast<Connection&>(_rctx.object());
				rthis.onStopped(_rctx, error);
			}
		);	//there might be events pending which will be delivered, but after this call
			//no event get posted
	}else{
		//wait for seconds_to_wait and then retry
		rthis.timer.waitFor(_rctx,
			TimeSpec(seconds_to_wait),
			[error](frame::aio::ReactorContext &_rctx){
				onTimerWaitStopping(_rctx, error);
			}
		);
	}
}
//-----------------------------------------------------------------------------
void Connection::doStop(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr){
	if(not isStopping()){
		flags |= static_cast<size_t>(Flags::Stopping);
		
		ErrorConditionT		error(_rerr);
		ObjectIdT			objuid(uid(_rctx));
		const ulong			seconds_to_wait = 0;// = service(_rctx).onConnectionWantClose(*this, _rctx, objuid);
		
		//onConnectionWantStop may fetch some of the
		//pending messages so call doCompleteAllMessages after calling onConnectionWantStop
		doCompleteAllMessages(_rctx, _rerr);
		
		if(!seconds_to_wait){
			//can stop rightaway
			postStop(_rctx, 
				[error](frame::aio::ReactorContext &_rctx, Event &&/*_revent*/){
					Connection	&rthis = static_cast<Connection&>(_rctx.object());
					rthis.onStopped(_rctx, error);
				}
			);	//there might be events pending which will be delivered, but after this call
				//no event get posted
		}else{
			//wait for seconds_to_wait and then retry
			timer.waitFor(_rctx,
				TimeSpec(seconds_to_wait),
				[error](frame::aio::ReactorContext &_rctx){
					onTimerWaitStopping(_rctx, error);
				}
			);
		}
	}
}
//-----------------------------------------------------------------------------
/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext &_rctx, Event &&_uevent){
	static const EventHandler<
		void,
		Connection&,
		frame::aio::ReactorContext &
	> event_handler = {
		[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){idbgx(Debug::ipc, &_rcon<<" Unhandled event "<<_revt);},
		{
			
			{
				generic_event_category.event(GenericEvents::Start),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventStart(_rctx, _revt);
				}
			},
			{
				generic_event_category.event(GenericEvents::Kill),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventKill(_rctx, _revt);
				}
			},
			{
				connection_event_category.event(ConnectionEvents::Resolve),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventResolve(_rctx, _revt);
				}
			},
			{
				connection_event_category.event(ConnectionEvents::NewPoolMessage),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventNewPoolMessage(_rctx, _revt);
				}
			},
			{
				connection_event_category.event(ConnectionEvents::NewConnMessage),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventNewConnMessage(_rctx, _revt);
				}
			},
			{
				connection_event_category.event(ConnectionEvents::CancelConnMessage),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventCancelConnMessage(_rctx, _revt);
				}
			},
			{
				connection_event_category.event(ConnectionEvents::CancelPoolMessage),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventCancelPoolMessage(_rctx, _revt);
				}
			},
			{
				connection_event_category.event(ConnectionEvents::EnterActive),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventEnterActive(_rctx, _revt);
				}
			},
			{
				connection_event_category.event(ConnectionEvents::EnterPassive),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventEnterPassive(_rctx, _revt);
				}
			},
			{
				connection_event_category.event(ConnectionEvents::StartSecure),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventStartSecure(_rctx, _revt);
				}
			},
			{
				connection_event_category.event(ConnectionEvents::SendRaw),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventSendRaw(_rctx, _revt);
				}
			},
			{
				connection_event_category.event(ConnectionEvents::RecvRaw),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventRecvRaw(_rctx, _revt);
				}
			},
		}
	};
	
	event_handler.handle(_uevent, *this, _rctx);
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventStart(
	frame::aio::ReactorContext &_rctx,
	Event &_revent
){
	bool		has_valid_socket = this->hasValidSocket();
	idbgx(Debug::ipc, this<<' '<<this->id()<<" Session start: "<<has_valid_socket ? " connected " : "not connected");
	if(has_valid_socket){
		doStart(_rctx, true);
	}
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventKill(
	frame::aio::ReactorContext &_rctx,
	Event &_revent
){
	idbgx(Debug::ipc, this<<' '<<this->id()<<" Session postStop");
	doStop(_rctx, error_connection_killed);
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventResolve(
	frame::aio::ReactorContext &_rctx,
	Event &_revent
){

	ResolveMessage *presolvemsg = _revent.any().cast<ResolveMessage>();
	
	if(presolvemsg){
		idbgx(Debug::ipc, this<<' '<<this->id()<<" Session receive resolve event message of size: "<<presolvemsg->addrvec.size());
		if(not presolvemsg->empty()){
			idbgx(Debug::ipc, this<<' '<<this->id()<<" Connect to "<<presolvemsg->currentAddress());
			
			//initiate connect:
			
			if(this->connect(_rctx, presolvemsg->currentAddress())){
				onConnect(_rctx);
			}
			
			service(_rctx).forwardResolveMessage(poolId(), _revent);
		}else{
			cassert(true);
			doStop(_rctx, error_library_logic);
		}
	}else{
		cassert(false);
	}
}
//-----------------------------------------------------------------------------
bool Connection::willAcceptNewMessage(frame::aio::ReactorContext &_rctx)const{
	return not isStopping()/* and waiting_message_vec.empty() and msg_writer.willAcceptNewMessage(service(_rctx).configuration().writer)*/;
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventNewPoolMessage(frame::aio::ReactorContext &_rctx, Event &_revent){
	flags &= ~static_cast<size_t>(Flags::InPoolWaitQueue);//reset flag

	if(willAcceptNewMessage(_rctx)){
		flags |= static_cast<size_t>(Flags::PollPool);
		doSend(_rctx);
	}else{
		service(_rctx).rejectNewPoolMessage(*this);
	}
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventNewConnMessage(frame::aio::ReactorContext &_rctx, Event &_revent){
	MessageId	*pmsgid = _revent.any().cast<MessageId>();
	cassert(pmsgid);
	if(pmsgid){
		
		if(this->isStopping()){
			pending_message_vec.push_back(*pmsgid);
		
			flags |= static_cast<size_t>(Flags::PollPool);
			doSend(_rctx);
		}else{
			//TODO: take the message and complete it
		}
	}
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventCancelConnMessage(frame::aio::ReactorContext &_rctx, Event &_revent){
	
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventCancelPoolMessage(frame::aio::ReactorContext &_rctx, Event &_revent){
	
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventEnterActive(frame::aio::ReactorContext &_rctx, Event &_revent){
	idbgx(Debug::ipc, this);
	
	ConnectionContext	conctx(service(_rctx), *this);
	
	if(not isStopping()){
		ErrorConditionT		error = service(_rctx).activateConnection(*this);
		
		if(not error){
			flags |= static_cast<size_t>(Flags::Active);
			MessagePointerT msg_ptr = _revent.any().cast<EnterActive>()->complete_fnc(conctx, error);
			if(not msg_ptr.empty()){
				//TODO: push message to writer
			}
			this->post(_rctx, [this](frame::aio::ReactorContext &_rctx, Event &&/*_revent*/){this->doSend(_rctx);});
		}else{
			MessagePointerT msg_ptr = _revent.any().cast<EnterActive>()->complete_fnc(conctx, error);
			if(msg_ptr.empty()){
				doStop(_rctx, error);
			}else{
				//TODO: push message to writer
				//then push nullptr message - delayed closing
			}
		}
	}else{
		MessagePointerT msg_ptr = _revent.any().cast<EnterActive>()->complete_fnc(conctx, error_connection_stopping);
		//ignore the message
	}
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventEnterPassive(frame::aio::ReactorContext &_rctx, Event &_revent){
	
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventStartSecure(frame::aio::ReactorContext &_rctx, Event &_revent){
	
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventSendRaw(frame::aio::ReactorContext &_rctx, Event &_revent){
	
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventRecvRaw(frame::aio::ReactorContext &_rctx, Event &_revent){
	
}
//-----------------------------------------------------------------------------
void Connection::doResetTimerStart(frame::aio::ReactorContext &_rctx){
	Configuration const &config = service(_rctx).configuration();
	if(isServer()){
		if(config.connection_inactivity_timeout_seconds){
			recv_keepalive_count = 0;
			flags &= (~static_cast<size_t>(Flags::HasActivity));
			timer.waitFor(_rctx, TimeSpec(config.connection_inactivity_timeout_seconds), onTimerInactivity);
		}
	}else{//client
		if(config.connection_keepalive_timeout_seconds){
			flags |= static_cast<size_t>(Flags::WaitKeepAliveTimer);
			timer.waitFor(_rctx, TimeSpec(config.connection_keepalive_timeout_seconds), onTimerKeepalive);
		}
	}
}
//-----------------------------------------------------------------------------
void Connection::doResetTimerSend(frame::aio::ReactorContext &_rctx){
	Configuration const &config = service(_rctx).configuration();
	if(isServer()){
		if(config.connection_inactivity_timeout_seconds){
			flags |= static_cast<size_t>(Flags::HasActivity);
		}
	}else{//client
		if(config.connection_keepalive_timeout_seconds and isWaitingKeepAliveTimer()){
			timer.waitFor(_rctx, TimeSpec(config.connection_keepalive_timeout_seconds), onTimerKeepalive);
		}
	}
}
//-----------------------------------------------------------------------------
void Connection::doResetTimerRecv(frame::aio::ReactorContext &_rctx){
	Configuration const &config = service(_rctx).configuration();
	if(isServer()){
		if(config.connection_inactivity_timeout_seconds){
			flags |= static_cast<size_t>(Flags::HasActivity);
		}
	}else{//client
		if(config.connection_keepalive_timeout_seconds and not isWaitingKeepAliveTimer()){
			flags |= static_cast<size_t>(Flags::WaitKeepAliveTimer);
			timer.waitFor(_rctx, TimeSpec(config.connection_keepalive_timeout_seconds), onTimerKeepalive);
		}
	}
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onTimerInactivity(frame::aio::ReactorContext &_rctx){
	Connection			&rthis = static_cast<Connection&>(_rctx.object());
	
	idbgx(Debug::ipc, &rthis<<" "<<(int)rthis.flags<<" "<<rthis.recv_keepalive_count);
	
	if(rthis.flags & static_cast<size_t>(Flags::HasActivity)){
		
		rthis.flags &= (~static_cast<size_t>(Flags::HasActivity));
		rthis.recv_keepalive_count = 0;
		
		Configuration const &config = rthis.service(_rctx).configuration();
		
		rthis.timer.waitFor(_rctx, TimeSpec(config.connection_inactivity_timeout_seconds), onTimerInactivity);
	}else{
		rthis.doStop(_rctx, error_inactivity_timeout);
	}
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onTimerKeepalive(frame::aio::ReactorContext &_rctx){
	Connection	&rthis = static_cast<Connection&>(_rctx.object());
	cassert(not rthis.isServer());
	rthis.flags |= static_cast<size_t>(Flags::Keepalive);
	rthis.flags &= (~static_cast<size_t>(Flags::WaitKeepAliveTimer));
	idbgx(Debug::ipc, &rthis<<" post send");
	rthis.post(_rctx, [&rthis](frame::aio::ReactorContext &_rctx, Event const &/*_revent*/){rthis.doSend(_rctx);});
	
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onRecv(frame::aio::ReactorContext &_rctx, size_t _sz){
	Connection			&rthis = static_cast<Connection&>(_rctx.object());
	ConnectionContext	conctx(rthis.service(_rctx), rthis);
	const TypeIdMapT	&rtypemap = rthis.service(_rctx).typeMap();
	const Configuration &rconfig  = rthis.service(_rctx).configuration();
	
	unsigned			repeatcnt = 4;
	char				*pbuf;
	size_t				bufsz;
	const uint32		recvbufcp = rthis.recvBufferCapacity();
	bool				recv_something = false;
	
	auto				complete_lambda(
		[&rthis, &_rctx](const MessageReader::Events _event, MessagePointerT /*const*/& _rmsgptr){
			switch(_event){
				case MessageReader::MessageCompleteE:
					rthis.doCompleteMessage(_rctx, _rmsgptr);
					break;
				case MessageReader::KeepaliveCompleteE:
					rthis.doCompleteKeepalive(_rctx);
					break;
			}
		}
	);
	
	rthis.doResetTimerRecv(_rctx);
	
	do{
		idbgx(Debug::ipc, &rthis<<" received size "<<_sz);
		
		if(!_rctx.error()){
			recv_something = true;
			rthis.recv_buf_off += _sz;
			pbuf = rthis.recv_buf + rthis.cons_buf_off;
			bufsz = rthis.recv_buf_off - rthis.cons_buf_off;
			ErrorConditionT						error;
			MessageReader::CompleteFunctionT	completefnc(std::cref(complete_lambda));
			
			rthis.cons_buf_off += rthis.msg_reader.read(pbuf, bufsz, completefnc, rconfig.reader, rtypemap, conctx, error);
			
			idbgx(Debug::ipc, &rthis<<" consumed size "<<rthis.cons_buf_off<<" of "<<bufsz);
			
			if(error){
				edbgx(Debug::ipc, &rthis<<' '<<rthis.id()<<" parsing "<<error.message());
				rthis.doStop(_rctx, error);
				recv_something = false;//prevent calling doResetTimerRecv after doStop
				break;
			}else if(rthis.cons_buf_off < bufsz){
				rthis.doOptimizeRecvBufferForced();
			}
		}else{
			edbgx(Debug::ipc, &rthis<<' '<<rthis.id()<<" receiving "<<_rctx.error().message());
			rthis.flags |= static_cast<size_t>(Flags::StopPeer);
			rthis.doStop(_rctx, _rctx.error());
			recv_something = false;//prevent calling doResetTimerRecv after doStop
			break;
		}
		--repeatcnt;
		rthis.doOptimizeRecvBuffer();
		pbuf = rthis.recv_buf + rthis.recv_buf_off;
		bufsz =  recvbufcp - rthis.recv_buf_off;
		//idbgx(Debug::ipc, &rthis<<" buffer size "<<bufsz);
	}while(repeatcnt && rthis.recvSome(_rctx, pbuf, bufsz, _sz));
	
	if(recv_something){
		rthis.doResetTimerRecv(_rctx);
		//TODO:
		//if(not rthis.msg_writer.isNonSafeCacheEmpty()){
		//	Locker<Mutex>	lock(rthis.service(_rctx).mutex(rthis));
		//	rthis.msg_writer.safeMoveCacheToSafety();
		//}
	}
	
	if(repeatcnt == 0){
		//bool rv = rthis.sock.postRecvSome(_rctx, pbuf, bufsz, Connection::onRecv);//fully asynchronous call
		bool rv = rthis.postRecvSome(_rctx, pbuf, bufsz);
		cassert(!rv);
		(void)rv;
	}
}
//-----------------------------------------------------------------------------
void Connection::doSend(frame::aio::ReactorContext &_rctx){
	idbgx(Debug::ipc, this<<"");
	if(not this->hasPendingSend()){
		ConnectionContext	conctx(service(_rctx), *this);
		unsigned 			repeatcnt = 4;
		ErrorConditionT		error;
		const uint32		sendbufcp = sendBufferCapacity();
		const TypeIdMapT	&rtypemap = service(_rctx).typeMap();
		const Configuration &rconfig  = service(_rctx).configuration();
		bool				sent_something = false;
		
		//doResetTimerSend(_rctx);
		
		while(repeatcnt){
			
			if(
				isActive() and
				msg_writer.hasFreeSeats(rconfig)
			){
				if(not service(_rctx).pollPoolForUpdates(*this, uid(_rctx), MessageId())){
					flags |= static_cast<size_t>(Flags::StopPeer);
					error.assign(-1, error.category());//TODO: Forced stop
					doStop(_rctx, error);
					sent_something = false;//prevent calling doResetTimerSend after doStop
					break;
				}
			}
			
#if 0
			if(msgwriter.empty()){
				//nothing to do but wait
				break;
			}
#endif

			uint32 sz = msg_writer.write(send_buf, sendbufcp, shouldSendKeepalive(), rconfig, rtypemap, conctx, error);
			
			flags &= (~static_cast<size_t>(Flags::Keepalive));
			
			if(!error){
				if(sz && this->sendAll(_rctx, send_buf, sz)){
					if(_rctx.error()){
						edbgx(Debug::ipc, this<<' '<<id()<<" sending "<<sz<<": "<<_rctx.error().message());
						flags |= static_cast<size_t>(Flags::StopPeer);
						doStop(_rctx, _rctx.error());
						sent_something = false;//prevent calling doResetTimerSend after doStop
						break;
					}else{
						sent_something = true;
					}
				}else{
					break;
				}
			}else{
				edbgx(Debug::ipc, this<<' '<<id()<<" storring "<<error.message());
				//flags |= static_cast<size_t>(Flags::StopForced);//TODO: maybe you should not set this all the time
				doStop(_rctx, error);
				sent_something = false;//prevent calling doResetTimerSend after doStop
				break;
			}
			--repeatcnt;
		}
		
		if(sent_something){
			doResetTimerSend(_rctx);
			//TODO:
			//if(not msg_writer.isNonSafeCacheEmpty()){
			//	Locker<Mutex>	lock(service(_rctx).mutex(*this));
			//	msg_writer.safeMoveCacheToSafety();
			//}
		}
		
		if(repeatcnt == 0){
			//idbgx(Debug::ipc, this<<" post send");
			this->post(_rctx, [this](frame::aio::ReactorContext &_rctx, Event const &/*_revent*/){this->doSend(_rctx);});
		}
		//idbgx(Debug::ipc, this<<" done-doSend "<<this->sendmsgvec[0].size()<<" "<<this->sendmsgvec[1].size());
	}
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onSend(frame::aio::ReactorContext &_rctx){
	Connection	&rthis = static_cast<Connection&>(_rctx.object());
	if(!_rctx.error()){
		rthis.doResetTimerSend(_rctx);
		rthis.doSend(_rctx);
	}else{
		edbgx(Debug::ipc, &rthis<<' '<<rthis.id()<<" sending "<<_rctx.error().message());
		rthis.doStop(_rctx, _rctx.error());
	}
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onConnect(frame::aio::ReactorContext &_rctx){
	Connection	&rthis = static_cast<Connection&>(_rctx.object());
	if(!_rctx.error()){
		idbgx(Debug::ipc, &rthis<<' '<<rthis.id());
		rthis.doStart(_rctx, false);
	}else{
		edbgx(Debug::ipc, &rthis<<' '<<rthis.id()<<" connecting "<<_rctx.error().message());
		rthis.doStop(_rctx, _rctx.error());
	}
}
//-----------------------------------------------------------------------------
void Connection::doCompleteMessage(frame::aio::ReactorContext &_rctx, MessagePointerT /*const*/ &_rmsgptr){
	//_rmsgptr is the received message
	ConnectionContext	conctx(service(_rctx), *this);
	const TypeIdMapT	&rtypemap = service(_rctx).typeMap();
	const Configuration &rconfig  = service(_rctx).configuration();
	ErrorConditionT		error;
	if(_rmsgptr->isBackOnSender()){
		idbgx(Debug::ipc, this<<' '<<"Completing back on sender message: "<<_rmsgptr->requid);
		msg_writer.completeMessage(_rmsgptr, _rmsgptr->requid, rconfig, rtypemap, conctx, error);
	}
}
//-----------------------------------------------------------------------------
void Connection::doCompleteKeepalive(frame::aio::ReactorContext &_rctx){
	if(isServer()){
		Configuration const &config = service(_rctx).configuration();
		
		++recv_keepalive_count;
		
		if(recv_keepalive_count < config.connection_inactivity_keepalive_count){
			flags |= static_cast<size_t>(Flags::Keepalive);
			idbgx(Debug::ipc, this<<" post send");
			this->post(_rctx, [this](frame::aio::ReactorContext &_rctx, Event const &/*_revent*/){this->doSend(_rctx);});
		}else{
			idbgx(Debug::ipc, this<<" post stop because of too many keep alive messages");
			recv_keepalive_count = 0;//prevent other posting
			this->post(
				_rctx,
				[this](frame::aio::ReactorContext &_rctx, Event const &/*_revent*/){
					this->doStop(_rctx, error_too_many_keepalive_packets_received);
				}
			);
		}
	}
}
//-----------------------------------------------------------------------------
void Connection::doCompleteAllMessages(
	frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr
){
	ConnectionContext	conctx(service(_rctx), *this);
	const TypeIdMapT	&rtypemap = service(_rctx).typeMap();
	const Configuration &rconfig  = service(_rctx).configuration();
	
	if(/*isStopForced() or */poolId().isInvalid()){
		//really complete
		//msgwriter.completeAllMessages(rconfig, rtypemap, conctx, _rerr);
	}else{
		//connection lost - try reschedule whatever messages we can
		//TODO:
	}
}

//-----------------------------------------------------------------------------
//		PlainConnection
//-----------------------------------------------------------------------------
PlainConnection::PlainConnection(
	Configuration const& _rconfiguration,
	SocketDevice &_rsd, ConnectionPoolId const &_rpool_id,
	std::string const & _rpool_name
): Connection(_rconfiguration, _rpool_id, _rpool_name), sock(this->proxy(), std::move(_rsd))
{
	idbgx(Debug::ipc, this<<' '<<timer.isActive()<<' '<<sock.isActive());
}
//-----------------------------------------------------------------------------
PlainConnection::PlainConnection(
	Configuration const& _rconfiguration,
	ConnectionPoolId const &_rpool_id,
	std::string const & _rpool_name
): Connection(_rconfiguration, _rpool_id, _rpool_name), sock(this->proxy())
{
	idbgx(Debug::ipc, this<<' '<<timer.isActive()<<' '<<sock.isActive());
}
//-----------------------------------------------------------------------------
/*virtual*/ bool PlainConnection::postRecvSome(frame::aio::ReactorContext &_rctx, char *_pbuf, size_t _bufcp){
	return sock.postRecvSome(_rctx, _pbuf, _bufcp, Connection::onRecv);
}
//-----------------------------------------------------------------------------
/*virtual*/ bool PlainConnection::hasValidSocket()const{
	return sock.device().ok();
}
//-----------------------------------------------------------------------------
/*virtual*/ bool PlainConnection::connect(frame::aio::ReactorContext &_rctx, const SocketAddressInet&_raddr){
	return sock.connect(_rctx, _raddr, Connection::onConnect);
}
//-----------------------------------------------------------------------------
/*virtual*/ bool PlainConnection::recvSome(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp, size_t &_sz){
	return sock.recvSome(_rctx, _buf, _bufcp, Connection::onRecv, _sz);
}
//-----------------------------------------------------------------------------
/*virtual*/ bool PlainConnection::hasPendingSend()const{
	return sock.hasPendingSend();
}
//-----------------------------------------------------------------------------
/*virtual*/ bool PlainConnection::sendAll(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp){
	return sock.sendAll(_rctx, _buf, _bufcp, Connection::onSend);
}

//-----------------------------------------------------------------------------
//		SecureConnection
//-----------------------------------------------------------------------------
SecureConnection::SecureConnection(
	Configuration const& _rconfiguration,
	SocketDevice &_rsd,
	ConnectionPoolId const &_rpool_id,
	std::string const & _rpool_name
): Connection(_rconfiguration, _rpool_id, _rpool_name), sock(this->proxy(), std::move(_rsd), _rconfiguration.secure_context)
{
	idbgx(Debug::ipc, this<<' '<<timer.isActive()<<' '<<sock.isActive());
}
//-----------------------------------------------------------------------------
SecureConnection::SecureConnection(
	Configuration const& _rconfiguration,
	ConnectionPoolId const &_rpool_id,
	std::string const & _rpool_name
): Connection(_rconfiguration, _rpool_id, _rpool_name), sock(this->proxy(), _rconfiguration.secure_context)
{
	idbgx(Debug::ipc, this<<' '<<timer.isActive()<<' '<<sock.isActive());
}
//-----------------------------------------------------------------------------
/*virtual*/ bool SecureConnection::postRecvSome(frame::aio::ReactorContext &_rctx, char *_pbuf, size_t _bufcp){
	return sock.postRecvSome(_rctx, _pbuf, _bufcp, Connection::onRecv);
}
//-----------------------------------------------------------------------------
/*virtual*/ bool SecureConnection::hasValidSocket()const{
	return sock.device().ok();
}
//-----------------------------------------------------------------------------
/*virtual*/ bool SecureConnection::connect(frame::aio::ReactorContext &_rctx, const SocketAddressInet&_raddr){
	return sock.connect(_rctx, _raddr, Connection::onConnect);
}
//-----------------------------------------------------------------------------
/*virtual*/ bool SecureConnection::recvSome(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp, size_t &_sz){
	return sock.recvSome(_rctx, _buf, _bufcp, Connection::onRecv, _sz);
}
//-----------------------------------------------------------------------------
/*virtual*/ bool SecureConnection::hasPendingSend()const{
	return sock.hasPendingSend();
}
//-----------------------------------------------------------------------------
/*virtual*/ bool SecureConnection::sendAll(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp){
	return sock.sendAll(_rctx, _buf, _bufcp, Connection::onSend);
}

//-----------------------------------------------------------------------------
//	ConnectionContext
//-----------------------------------------------------------------------------
Any<>& ConnectionContext::any(){
	return rconnection.any();
}
//-----------------------------------------------------------------------------
RecipientId	ConnectionContext::recipientId()const{
	return RecipientId(rconnection.poolId(), rservice.manager().id(rconnection));
}
//-----------------------------------------------------------------------------
const std::string& ConnectionContext::recipientName()const{
	return rconnection.poolName();
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
