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
#include <cstdio>

namespace solid{
namespace frame{
namespace ipc{

namespace{

enum class Flags:size_t{
	Active						= 0x0001,
	Server						= 0x0002,
	Keepalive					= 0x0004,
	WaitKeepAliveTimer			= 0x0008,
	StopPeer					= 0x0010,
	HasActivity 				= 0x0020,
	PollPool					= 0x0040,
	Stopping					= 0x0080,
	DelayedStopping				= 0x0100,
	Secure						= 0x0200,
	Raw							= 0x0400,
	InPoolWaitQueue				= 0x0800,
	Connected					= 0x1000,//once set - the flag should not be reset. Is used by pool for restarting
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
	Stopping,
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
			case ConnectionEvents::Stopping:
				return "Stopping";
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
/*static*/ Event Connection::eventStopping(){
	return connection_event_category.event(ConnectionEvents::Stopping);
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
	):complete_fnc(std::move(_ucomplete_fnc)), data(std::move(_udata)), offset(0){}
	
	ConnectionSendRawDataCompleteFunctionT	complete_fnc;
	std::string								data;
	size_t									offset;
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
	idbgx(Debug::ipc, this);
	//TODO: use _rconfiguration.connection_start_state and _rconfiguration.connection_start_secure
}
//-----------------------------------------------------------------------------
Connection::~Connection(){
	idbgx(Debug::ipc, this);
}
//-----------------------------------------------------------------------------
bool Connection::isFull(Configuration const& _rconfiguration)const{
	return msg_writer.full(_rconfiguration.writer);
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
	const bool success = msg_writer.enqueue(_rconfiguration.writer, _rmsgbundle, _rpool_msg_id, _rconn_msg_id);
	vdbgx(Debug::ipc, this<<" enqueue message "<<_rpool_msg_id<<" to connection "<<this<<" retval = "<<success);
	return success;
}
//-----------------------------------------------------------------------------
bool Connection::isActiveState()const{
	return flags & static_cast<size_t>(Flags::Active);
}
//-----------------------------------------------------------------------------
bool Connection::isRawState()const{
	return flags & static_cast<size_t>(Flags::Raw);
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
bool Connection::isConnected()const{
	return flags & static_cast<size_t>(Flags::Connected);
}
//-----------------------------------------------------------------------------
bool Connection::isSecured()const{
	return false;
}
//-----------------------------------------------------------------------------
bool Connection::shouldSendKeepalive()const{
	return flags & static_cast<size_t>(Flags::Keepalive);
}
//-----------------------------------------------------------------------------
inline bool Connection::shouldPollPool()const{
	return (flags & static_cast<size_t>(Flags::PollPool));
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
	Configuration const &config = service(_rctx).configuration();
	
	doPrepare(_rctx);
	prepareSocket(_rctx);
	
	if(_is_incomming){
		flags |= static_cast<size_t>(Flags::Server);
		service(_rctx).onIncomingConnectionStart(conctx);
	}else{
		flags |= static_cast<size_t>(Flags::Connected);
		service(_rctx).onOutgoingConnectionStart(conctx);
	}
	
	switch(config.connection_start_state){
		case ConnectionState::Raw:
			flags |= static_cast<size_t>(Flags::Raw);
			break;
		case ConnectionState::Active:
			if(config.connection_start_secure){
				this->post(
					_rctx,
					[this](frame::aio::ReactorContext &_rctx, Event &&_revent){this->onEvent(_rctx, std::move(_revent));},
					connection_event_category.event(ConnectionEvents::StartSecure)
				);
			}
			this->post(
				_rctx,
				[this](frame::aio::ReactorContext &_rctx, Event &&_revent){this->onEvent(_rctx, std::move(_revent));}, connection_event_category.event(ConnectionEvents::EnterActive)
			);
			break;
		case ConnectionState::Passive:
		default:
			flags |= static_cast<size_t>(Flags::Raw);
			if(config.connection_start_secure){
				this->post(
					_rctx,
					[this](frame::aio::ReactorContext &_rctx, Event &&_revent){this->onEvent(_rctx, std::move(_revent));},
					connection_event_category.event(ConnectionEvents::StartSecure)
				);
			}
			this->post(
				_rctx,
				[this](frame::aio::ReactorContext &_rctx, Event &&_revent){this->onEvent(_rctx, std::move(_revent));},
				connection_event_category.event(ConnectionEvents::EnterPassive)
			);
			break;
	}
}
//-----------------------------------------------------------------------------
void Connection::doStop(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr){
	
	if(not isStopping()){
		
		idbgx(Debug::ipc, this<<' '<<this->id()<<"");
		
		flags |= static_cast<size_t>(Flags::Stopping);
		
		ErrorConditionT		error;
		ObjectIdT			objuid(uid(_rctx));
		ulong				seconds_to_wait = 0;
		
		MessageBundle		msg_bundle;
		MessageId			pool_msg_id;
		
		Event				event;
		
		bool				can_stop = service(_rctx).connectionStopping(*this, objuid, seconds_to_wait, pool_msg_id, msg_bundle, event, error);
		
		
		if(not msg_bundle.message_ptr.empty()){
			if(not error){
				error = error_connection_message_fail_send;
			}
			doCompleteMessage(_rctx, pool_msg_id, msg_bundle, error);
		}
		
		error = _rerr;
		//at this point we need to start completing all connection's remaining messages
		
		bool				has_no_message = pending_message_vec.empty() and msg_writer.empty();
		
		
		if(can_stop and has_no_message){
			idbgx(Debug::ipc, this<<' '<<this->id()<<" postStop");
			//can stop rightaway
			postStop(_rctx, 
				[error](frame::aio::ReactorContext &_rctx, Event &&/*_revent*/){
					Connection	&rthis = static_cast<Connection&>(_rctx.object());
					rthis.onStopped(_rctx, error);
				}
			);	//there might be events pending which will be delivered, but after this call
				//no event get posted
		}else if(has_no_message){
			idbgx(Debug::ipc, this<<' '<<this->id()<<" wait "<<seconds_to_wait<<" seconds");
			if(seconds_to_wait){
				timer.waitFor(_rctx,
					TimeSpec(seconds_to_wait),
					[error, event](frame::aio::ReactorContext &_rctx){
						Connection	&rthis = static_cast<Connection&>(_rctx.object());
						rthis.doContinueStopping(_rctx, error, event);
					}
				);
			}else{
				post(_rctx,
					[error](frame::aio::ReactorContext &_rctx, Event &&_revent){
						Connection	&rthis = static_cast<Connection&>(_rctx.object());
						rthis.doContinueStopping(_rctx, error, _revent);
					},
					std::move(event)
				);
			}
		}else{
			idbgx(Debug::ipc, this<<' '<<this->id()<<" post message cleanup");
			size_t		offset = 0;
			post(_rctx,
				 [error, seconds_to_wait, can_stop, offset](frame::aio::ReactorContext &_rctx, Event &&_revent){
					Connection	&rthis = static_cast<Connection&>(_rctx.object());
					rthis.doCompleteAllMessages(_rctx, offset, can_stop, seconds_to_wait, error, _revent);
				},
				std::move(event)
			);
		}
	}
}
//-----------------------------------------------------------------------------
void Connection::doCompleteAllMessages(
	frame::aio::ReactorContext &_rctx,
	size_t _offset,
	const bool _can_stop,
	const ulong _seconds_to_wait,
	ErrorConditionT const &_rerr,
	Event &_revent
){
	
	bool	has_any_message = not msg_writer.empty();
	
	if(has_any_message){
		//complete msg_writer messages
		MessageBundle	msg_bundle;
		MessageId		pool_msg_id;
		
		if(msg_writer.cancelOldest(msg_bundle, pool_msg_id)){
			doCompleteMessage(_rctx, pool_msg_id, msg_bundle, error_connection_message_fail_send);
		}
		
		has_any_message = (not msg_writer.empty()) or (not pending_message_vec.empty());
		
	}else if(_offset < pending_message_vec.size()){
		//complete pending messages
		MessageBundle	msg_bundle;
				
		if(service(_rctx).fetchCanceledMessage(*this, pending_message_vec[_offset], msg_bundle)){
			doCompleteMessage(_rctx, pending_message_vec[_offset], msg_bundle, error_connection_message_fail_send);
		}
		++_offset;
		if(_offset == pending_message_vec.size()){
			pending_message_vec.clear();
			has_any_message = false;
		}else{
			has_any_message = true;
		}
	}
	
	if(has_any_message){
		post(_rctx,
				[_rerr, _seconds_to_wait, _can_stop, _offset](frame::aio::ReactorContext &_rctx, Event &&_revent){
				Connection	&rthis = static_cast<Connection&>(_rctx.object());
				rthis.doCompleteAllMessages(_rctx, _offset, _can_stop, _seconds_to_wait, _rerr, _revent);
			},
			std::move(_revent)
		);
	}else if(_can_stop){
		//can stop rightaway
		postStop(_rctx, 
			[_rerr](frame::aio::ReactorContext &_rctx, Event &&/*_revent*/){
				Connection	&rthis = static_cast<Connection&>(_rctx.object());
				rthis.onStopped(_rctx, _rerr);
			}
		);	//there might be events pending which will be delivered, but after this call
			//no event get posted
	}else if(_seconds_to_wait){
		timer.waitFor(_rctx,
			TimeSpec(_seconds_to_wait),
			[_rerr, _revent](frame::aio::ReactorContext &_rctx){
				Connection	&rthis = static_cast<Connection&>(_rctx.object());
				rthis.doContinueStopping(_rctx, _rerr, _revent);
			}
		);
	}else{
		post(_rctx,
				[_rerr](frame::aio::ReactorContext &_rctx, Event &&_revent){
				Connection	&rthis = static_cast<Connection&>(_rctx.object());
				rthis.doContinueStopping(_rctx, _rerr, _revent);
			},
			std::move(_revent)
		);
	}
}
//-----------------------------------------------------------------------------
void Connection::doContinueStopping(
	frame::aio::ReactorContext &_rctx,
	ErrorConditionT const &_rerr,
	const Event &_revent
){
	
	idbgx(Debug::ipc, this<<' '<<this->id()<<"");
	
	ErrorConditionT		error;
	ObjectIdT			objuid(uid(_rctx));
	ulong				seconds_to_wait = 0;
	
	MessageBundle		msg_bundle;
	MessageId			pool_msg_id;
	
	Event				event(_revent);
	
	bool				can_stop = service(_rctx).connectionStopping(*this, objuid, seconds_to_wait, pool_msg_id, msg_bundle, event, error);
	
	if(not msg_bundle.message_ptr.empty()){
		if(not error){
			error = error_connection_message_fail_send;
		}
		doCompleteMessage(_rctx, pool_msg_id, msg_bundle, error);
	}
	
	if(can_stop){
		//can stop rightaway
		postStop(_rctx, 
			[_rerr](frame::aio::ReactorContext &_rctx, Event &&/*_revent*/){
				Connection	&rthis = static_cast<Connection&>(_rctx.object());
				rthis.onStopped(_rctx, _rerr);
			}
		);	//there might be events pending which will be delivered, but after this call
			//no event get posted
	}else if(seconds_to_wait){
		idbgx(Debug::ipc, this<<' '<<this->id()<<" wait for "<<seconds_to_wait<<" seconds");
		timer.waitFor(_rctx,
			TimeSpec(seconds_to_wait),
			[_rerr, _revent](frame::aio::ReactorContext &_rctx){
				Connection	&rthis = static_cast<Connection&>(_rctx.object());
				rthis.doContinueStopping(_rctx, _rerr, _revent);
			}
		);
	}else{
		post(_rctx,
			[_rerr](frame::aio::ReactorContext &_rctx, Event &&_revent){
				Connection	&rthis = static_cast<Connection&>(_rctx.object());
				rthis.doContinueStopping(_rctx, _rerr, _revent);
			},
			std::move(event)
		);
	}
	
}
//-----------------------------------------------------------------------------
void Connection::onStopped(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr){
	
	ObjectIdT			objuid(uid(_rctx));
	ConnectionContext	conctx(service(_rctx), *this);
	
	service(_rctx).connectionStop(*this);
	
	service(_rctx).onConnectionStop(conctx, _rerr);
	
	doUnprepare(_rctx);
}
//-----------------------------------------------------------------------------
/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext &_rctx, Event &&_uevent){
	
	static const EventHandler<
		void,
		Connection&,
		frame::aio::ReactorContext &
	> event_handler = {
		[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
			Connection			&rthis = static_cast<Connection&>(_rctx.object());
			ConnectionContext	conctx(rthis.service(_rctx), rthis);
			rthis.service(_rctx).configuration().connection_on_event_fnc(conctx, _revt);
		},
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
			{
				connection_event_category.event(ConnectionEvents::Stopping),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					idbgx(Debug::ipc, &_rcon<<' '<<_rcon.id()<<" cancel timer");
					//NOTE: we're trying this way to preserve the
					//error condition (for which the connection is stopping)
					//held by the timer callback
					_rcon.timer.cancel(_rctx);
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
			edbgx(Debug::ipc, this<<' '<<this->id()<<" Empty resolve message");
			SOLID_ASSERT(false);
			doStop(_rctx, error_connection_logic);
		}
	}else{
		SOLID_ASSERT(false);
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
	SOLID_ASSERT(pmsgid);
	
	if(pmsgid){
		
		if(not this->isStopping()){
			pending_message_vec.push_back(*pmsgid);
			if(not this->isRawState()){
				flags |= static_cast<size_t>(Flags::PollPool);
				doSend(_rctx);
			}
		}else{
			MessageBundle	msg_bundle;
			
			if(service(_rctx).fetchCanceledMessage(*this, *pmsgid, msg_bundle)){
				doCompleteMessage(_rctx, *pmsgid, msg_bundle, error_connection_stopping);
			}
		}
	}
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventCancelConnMessage(frame::aio::ReactorContext &_rctx, Event &_revent){
	
	MessageId	*pmsgid = _revent.any().cast<MessageId>();
	SOLID_ASSERT(pmsgid);
	
	if(pmsgid){
		MessageBundle	msg_bundle;
		MessageId		pool_msg_id;
		if(msg_writer.cancel(*pmsgid, msg_bundle, pool_msg_id)){
			doCompleteMessage(_rctx, pool_msg_id, msg_bundle, error_connection_message_canceled);
		}
	}
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventCancelPoolMessage(frame::aio::ReactorContext &_rctx, Event &_revent){
	
	MessageId	*pmsgid = _revent.any().cast<MessageId>();
	SOLID_ASSERT(pmsgid);
	
	if(pmsgid){
		MessageBundle	msg_bundle;
				
		if(service(_rctx).fetchCanceledMessage(*this, *pmsgid, msg_bundle)){
			doCompleteMessage(_rctx, *pmsgid, msg_bundle, error_connection_message_canceled);
		}
	}
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventEnterActive(frame::aio::ReactorContext &_rctx, Event &_revent){
	
	//idbgx(Debug::ipc, this);
	
	ConnectionContext	conctx(service(_rctx), *this);
	EnterActive			*pdata = _revent.any().cast<EnterActive>();
	
	if(not isStopping()){

		ErrorConditionT		error = service(_rctx).activateConnection(*this, uid(_rctx));
		
		if(not error){
			flags |= static_cast<size_t>(Flags::Active);
			
			if(pdata){
			
				MessagePointerT msg_ptr = pdata->complete_fnc(conctx, error);
			
				if(not msg_ptr.empty()){
					//TODO: push message to writer
				}
			}
			
			if(not isServer()){
				//poll pool only for clients
				flags |= static_cast<size_t>(Flags::PollPool);
			}
			
			this->post(
				_rctx,
				[this](frame::aio::ReactorContext &_rctx, Event &&/*_revent*/){
					doSend(_rctx);
				}
			);
			
			//start receiving messages
			this->postRecvSome(_rctx, recv_buf, recvBufferCapacity());
			
			doResetTimerStart(_rctx);
			
		}else{
			
			if(pdata){
				
				MessagePointerT msg_ptr = pdata->complete_fnc(conctx, error);
				
				if(not msg_ptr.empty()){
					//TODO: push message to writer
					//then push nullptr message - delayed closing
				}
			}
			
			doStop(_rctx, error);
		}
	}else if(pdata){
		pdata->complete_fnc(conctx, error_connection_stopping);
	}
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventEnterPassive(frame::aio::ReactorContext &_rctx, Event &_revent){
	
	EnterPassive		*pdata = _revent.any().cast<EnterPassive>();
	ConnectionContext	conctx(service(_rctx), *this);
	
	if(this->isRawState()){
		flags &= ~static_cast<size_t>(Flags::Raw);
		
		if(pdata){
			pdata->complete_fnc(conctx, ErrorConditionT());
		}
		
		if(pending_message_vec.size()){
			flags |= static_cast<size_t>(Flags::PollPool);
			doSend(_rctx);
		}
	}else if(pdata){
		pdata->complete_fnc(conctx, error_connection_invalid_state);
	}
	
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
void Connection::doHandleEventStartSecure(frame::aio::ReactorContext &_rctx, Event &_revent){
	//TODO:
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventSendRaw(frame::aio::ReactorContext &_rctx, Event &_revent){
	
	SendRaw				*pdata = _revent.any().cast<SendRaw>();
	ConnectionContext	conctx(service(_rctx), *this);
	
	SOLID_ASSERT(pdata);
	
	if(this->isRawState() and pdata){
		
		size_t	tocopy = this->sendBufferCapacity();
		
		if(tocopy > pdata->data.size()){
			tocopy = pdata->data.size();
		}
		
		memcpy(send_buf, pdata->data.data(), tocopy);
		
		if(tocopy){
			pdata->offset = tocopy;
			if(not this->postSendAll(_rctx, send_buf, tocopy, _revent)){
				pdata->complete_fnc(conctx, _rctx.error());
			}
		}else{
			pdata->complete_fnc(conctx, ErrorConditionT());
		}
		
	}else if(pdata){
		pdata->complete_fnc(conctx, error_connection_invalid_state);
	}
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventRecvRaw(frame::aio::ReactorContext &_rctx, Event &_revent){
	
	RecvRaw				*pdata = _revent.any().cast<RecvRaw>();
	ConnectionContext	conctx(service(_rctx), *this);
	size_t				used_size = 0;
	
	SOLID_ASSERT(pdata);
	
	if(this->isRawState() and pdata){
		if(recv_buf_off == cons_buf_off){
			if(not this->postRecvSome(_rctx, recv_buf, this->recvBufferCapacity(), _revent)){
				
				pdata->complete_fnc(conctx, nullptr, used_size, _rctx.error());
			}
		}else{
			used_size = recv_buf_off - cons_buf_off;
			
			pdata->complete_fnc(conctx, recv_buf + cons_buf_off, used_size, _rctx.error());
			
			if(used_size > (recv_buf_off - cons_buf_off)){
				used_size = (recv_buf_off - cons_buf_off);
			}
			
			cons_buf_off += used_size;
			
			if(cons_buf_off == recv_buf_off){
				cons_buf_off = recv_buf_off = 0;
			}else{
				SOLID_ASSERT(cons_buf_off < recv_buf_off);
			}
		}
	}else if(pdata){
		
		pdata->complete_fnc(conctx, nullptr, used_size, error_connection_invalid_state);
	}
}
//-----------------------------------------------------------------------------
void Connection::doResetTimerStart(frame::aio::ReactorContext &_rctx){
	
	Configuration const &config = service(_rctx).configuration();
	
	if(isServer()){
		if(config.connection_inactivity_timeout_seconds){
			recv_keepalive_count = 0;
			flags &= (~static_cast<size_t>(Flags::HasActivity));
			
			idbgx(Debug::ipc, this<<' '<<this->id()<<" wait for "<<config.connection_inactivity_timeout_seconds<<" seconds");
			
			timer.waitFor(_rctx, TimeSpec(config.connection_inactivity_timeout_seconds), onTimerInactivity);
		}
	}else{//client
		if(config.connection_keepalive_timeout_seconds){
			flags |= static_cast<size_t>(Flags::WaitKeepAliveTimer);
			
			idbgx(Debug::ipc, this<<' '<<this->id()<<" wait for "<<config.connection_keepalive_timeout_seconds<<" seconds");
			
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
			
			idbgx(Debug::ipc, this<<' '<<this->id()<<" wait for "<<config.connection_keepalive_timeout_seconds<<" seconds");
			
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
			
			idbgx(Debug::ipc, this<<' '<<this->id()<<" wait for "<<config.connection_keepalive_timeout_seconds<<" seconds");
			
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
		
		idbgx(Debug::ipc, &rthis<<' '<<rthis.id()<<" wait for "<<config.connection_inactivity_timeout_seconds<<" seconds");
		
		rthis.timer.waitFor(_rctx, TimeSpec(config.connection_inactivity_timeout_seconds), onTimerInactivity);
	}else{
		rthis.doStop(_rctx, error_connection_inactivity_timeout);
	}
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onTimerKeepalive(frame::aio::ReactorContext &_rctx){
	
	Connection	&rthis = static_cast<Connection&>(_rctx.object());
	
	SOLID_ASSERT(not rthis.isServer());
	rthis.flags |= static_cast<size_t>(Flags::Keepalive);
	rthis.flags &= (~static_cast<size_t>(Flags::WaitKeepAliveTimer));
	idbgx(Debug::ipc, &rthis<<" post send");
	rthis.post(_rctx, [&rthis](frame::aio::ReactorContext &_rctx, Event const &/*_revent*/){rthis.doSend(_rctx);});
	
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onSendAllRaw(frame::aio::ReactorContext &_rctx, Event &_revent){
	
	Connection			&rthis = static_cast<Connection&>(_rctx.object());
	SendRaw				*pdata = _revent.any().cast<SendRaw>();
	ConnectionContext	conctx(rthis.service(_rctx), rthis);
	
	SOLID_ASSERT(pdata);
	
	if(pdata){
		
		if(not _rctx.error()){
		
			size_t	tocopy = pdata->data.size() - pdata->offset;
			
			if(tocopy > rthis.sendBufferCapacity()){
				tocopy = rthis.sendBufferCapacity();
			}
			
			memcpy(rthis.send_buf, pdata->data.data() + pdata->offset, tocopy);
			
			if(tocopy){
				pdata->offset += tocopy;
				if(not rthis.postSendAll(_rctx, rthis.send_buf, tocopy, _revent)){
					pdata->complete_fnc(conctx, _rctx.error());
				}
			}else{
				pdata->complete_fnc(conctx, ErrorConditionT());
			}
			
		}else{
			pdata->complete_fnc(conctx, _rctx.error());
		}
	}
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onRecvSomeRaw(frame::aio::ReactorContext &_rctx, const size_t _sz, Event &_revent){
	
	Connection			&rthis = static_cast<Connection&>(_rctx.object());
	RecvRaw				*pdata = _revent.any().cast<RecvRaw>();
	ConnectionContext	conctx(rthis.service(_rctx), rthis);
	
	SOLID_ASSERT(pdata);
	
	if(pdata){
		
		size_t		used_size = _sz;
		
		pdata->complete_fnc(conctx, rthis.recv_buf, used_size, _rctx.error());
		
		if(used_size > _sz){
			used_size = _sz;
		}
		
		if(used_size < _sz){
			rthis.recv_buf_off = _sz;
			rthis.cons_buf_off = used_size;
		}
	}
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onRecv(frame::aio::ReactorContext &_rctx, size_t _sz){
	
	Connection			&rthis = static_cast<Connection&>(_rctx.object());
	ConnectionContext	conctx(rthis.service(_rctx), rthis);
	const Configuration &rconfig  = rthis.service(_rctx).configuration();
	
	unsigned			repeatcnt = 4;
	char				*pbuf;
	size_t				bufsz;
	const uint32		recvbufcp = rthis.recvBufferCapacity();
	bool				recv_something = false;
	
	auto				complete_lambda(
		[&rthis, &_rctx](const MessageReader::Events _event, MessagePointerT & _rmsg_ptr, const size_t _msg_type_id){
			switch(_event){
				case MessageReader::MessageCompleteE:
					rthis.doCompleteMessage(_rctx, _rmsg_ptr, _msg_type_id);
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
			
			rthis.cons_buf_off += rthis.msg_reader.read(
				pbuf, bufsz, completefnc, rconfig.reader, rconfig.protocol(), conctx, error
			);
			
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
	}
	
	if(repeatcnt == 0){
		bool rv = rthis.postRecvSome(_rctx, pbuf, bufsz);//fully asynchronous call
		SOLID_ASSERT(!rv);
		(void)rv;
	}
}
//-----------------------------------------------------------------------------
void Connection::doSend(frame::aio::ReactorContext &_rctx){
	
	idbgx(Debug::ipc, this<<"");
	
	if(not this->hasPendingSend() and not this->isStopping()){
		ConnectionContext	conctx(service(_rctx), *this);
		unsigned 			repeatcnt = 4;
		ErrorConditionT		error;
		const uint32		sendbufcp = sendBufferCapacity();
		const Configuration &rconfig  = service(_rctx).configuration();
		bool				sent_something = false;
		
		auto				complete_lambda(
			[this, &_rctx](MessageBundle &_rmsg_bundle, MessageId const &_rpool_msg_id){
				this->doCompleteMessage(_rctx, _rpool_msg_id, _rmsg_bundle, ErrorConditionT());
				return service(_rctx).pollPoolForUpdates(*this, uid(_rctx), _rpool_msg_id);
			}
		);
		
		//doResetTimerSend(_rctx);
		
		while(repeatcnt){
			
			if(
				shouldPollPool()
			){
				flags &= ~static_cast<size_t>(Flags::PollPool);//reset flag
				if((error = service(_rctx).pollPoolForUpdates(*this, uid(_rctx), MessageId()))){
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
			MessageWriter::CompleteFunctionT	completefnc(std::cref(complete_lambda));

			uint32								sz = msg_writer.write(
				send_buf, sendbufcp, shouldSendKeepalive(), completefnc, rconfig.writer, rconfig.protocol(), conctx, error
			);
			
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
void Connection::doCompleteMessage(frame::aio::ReactorContext &_rctx, MessagePointerT &_rresponse_ptr, const size_t _response_type_id){
	
	ConnectionContext	conctx(service(_rctx), *this);
	const Configuration &rconfig  = service(_rctx).configuration();
	const Protocol		&rproto = rconfig.protocol();
	ErrorConditionT		error;
	MessageBundle		msg_bundle;//request message
	
	if(_rresponse_ptr->isBackOnSender()){
		idbgx(Debug::ipc, this<<' '<<"Completing back on sender message: "<<_rresponse_ptr->requid);
		
		MessageId		pool_msg_id;
		
		msg_writer.cancel(_rresponse_ptr->requid, msg_bundle, pool_msg_id);
		idbgx(Debug::ipc, this);
		conctx.message_flags = msg_bundle.message_flags;
		conctx.request_id = _rresponse_ptr->requid;
		conctx.message_id = pool_msg_id;
		
		if(not FUNCTION_EMPTY(msg_bundle.complete_fnc)){
			idbgx(Debug::ipc, this);
			msg_bundle.complete_fnc(conctx, msg_bundle.message_ptr, _rresponse_ptr, error);
		}else{
			idbgx(Debug::ipc, this<<" "<<_response_type_id);
			rproto[_response_type_id].complete_fnc(conctx, msg_bundle.message_ptr, _rresponse_ptr, error);
		}
		
	}else{
		idbgx(Debug::ipc, this<<" "<<_response_type_id);
		rproto[_response_type_id].complete_fnc(conctx, msg_bundle.message_ptr, _rresponse_ptr, error);
	}
}
//-----------------------------------------------------------------------------
void Connection::doCompleteMessage(
	solid::frame::aio::ReactorContext& _rctx,
	MessageId const &_rpool_msg_id,
	MessageBundle &_rmsg_bundle,
	ErrorConditionT const &_rerror
){
	
	ConnectionContext	conctx(service(_rctx), *this);
	const Configuration &rconfig  = service(_rctx).configuration();
	const Protocol		&rproto = rconfig.protocol();
	//const Configuration &rconfig  = service(_rctx).configuration();
	
	MessagePointerT		dummy_recv_msg_ptr;
	
	conctx.message_flags = _rmsg_bundle.message_flags;
	conctx.message_id = _rpool_msg_id;
	
	if(not FUNCTION_EMPTY(_rmsg_bundle.complete_fnc)){
		idbgx(Debug::ipc, this);
		_rmsg_bundle.complete_fnc(conctx, _rmsg_bundle.message_ptr, dummy_recv_msg_ptr, _rerror);
	}else{
		idbgx(Debug::ipc, this<<" "<<_rmsg_bundle.message_type_id);
		rproto[_rmsg_bundle.message_type_id].complete_fnc(conctx, _rmsg_bundle.message_ptr, dummy_recv_msg_ptr, _rerror);
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
					this->doStop(_rctx, error_connection_too_many_keepalive_packets_received);
				}
			);
		}
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
/*virtual*/ bool PlainConnection::postSendAll(frame::aio::ReactorContext &_rctx, const char *_pbuf, size_t _bufcp, Event &_revent){
	struct Closure{
		Event event;
		
		Closure(Event const &_revent):event(_revent){
		}
		
		void operator()(frame::aio::ReactorContext &_rctx){
			Connection::onSendAllRaw(_rctx, event);
		}
		
	} lambda(_revent);
	
	//TODO: find solution for costly event copy
	
	return sock.postSendAll(_rctx, _pbuf, _bufcp, lambda);
}
//-----------------------------------------------------------------------------
/*virtual*/ bool PlainConnection::postRecvSome(frame::aio::ReactorContext &_rctx, char *_pbuf, size_t _bufcp, Event &_revent){
	struct Closure{
		Event event;
		
		Closure(Event const &_revent):event(_revent){
		}
		
		void operator()(frame::aio::ReactorContext &_rctx, size_t _sz){
			Connection::onRecvSomeRaw(_rctx, _sz, event);
		}
		
	} lambda(_revent);
	
	//TODO: find solution for costly event copy
	
	return sock.postRecvSome(_rctx, _pbuf, _bufcp, lambda);
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
/*virtual*/ void PlainConnection::prepareSocket(frame::aio::ReactorContext &_rctx){
	ErrorCodeT error = sock.device().enableNoSignal();
	idbgx(Debug::ipc, this<<" enableNoSignal error: "<<error.message());
	(void)error;
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
/*virtual*/ bool SecureConnection::postSendAll(frame::aio::ReactorContext &_rctx, const char *_pbuf, size_t _bufcp, Event &_revent){
	struct Closure{
		Event event;
		
		Closure(Event const &_revent):event(_revent){
		}
		
		void operator()(frame::aio::ReactorContext &_rctx){
			Connection::onSendAllRaw(_rctx, event);
		}
		
	} lambda(_revent);
	
	//TODO: find solution for costly event copy
	
	return sock.postSendAll(_rctx, _pbuf, _bufcp, lambda);
}
//-----------------------------------------------------------------------------
/*virtual*/ bool SecureConnection::postRecvSome(frame::aio::ReactorContext &_rctx, char *_pbuf, size_t _bufcp, Event &_revent){
	struct Closure{
		Event event;
		
		Closure(Event const &_revent):event(_revent){
		}
		
		void operator()(frame::aio::ReactorContext &_rctx, size_t _sz){
			Connection::onRecvSomeRaw(_rctx, _sz, event);
		}
		
	} lambda(_revent);
	
	//TODO: find solution for costly event copy
	
	return sock.postRecvSome(_rctx, _pbuf, _bufcp, lambda);
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
/*virtual*/ void SecureConnection::prepareSocket(frame::aio::ReactorContext &_rctx){
	sock.device().enableNoSignal();
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
ObjectIdT ConnectionContext::connectionId()const{
	return rservice.manager().id(rconnection);
}
//-----------------------------------------------------------------------------
const std::string& ConnectionContext::recipientName()const{
	return rconnection.poolName();
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
