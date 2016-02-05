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
	StopForced					= 32,
	HasActivity 				= 64,
};

enum class AtomicFlags{
	Active				= 1,
	Stopping			= 2,
	DelayedClosing		= 4,
};


enum class ConnectionEvents{
	Activate,
	Resolve,
	Push,
	DelayedClose,
	Invalid,
};

const EventCategory<ConnectionEvents>	connection_event_category{
	"solid::frame::ipc::connection_event_category",
	[](const ConnectionEvents _evt){
		switch(_evt){
			case ConnectionEvents::Activate:
				return "activate";
			case ConnectionEvents::Resolve:
				return "resolve";
			case ConnectionEvents::Push:
				return "push";
			case ConnectionEvents::DelayedClose:
				return "delayed_close";
			case ConnectionEvents::Invalid:
				return "invalid";
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

/*static*/ Event Connection::resolveEvent(){
	return connection_event_category.event(ConnectionEvents::Resolve);
}

//-----------------------------------------------------------------------------
inline void Connection::doOptimizeRecvBuffer(){
	const size_t cnssz = receivebufoff - consumebufoff;
	if(cnssz <= consumebufoff){
		//idbgx(Debug::proto_bin, this<<' '<<"memcopy "<<cnssz<<" rcvoff = "<<receivebufoff<<" cnsoff = "<<consumebufoff);
		memcpy(recvbuf, recvbuf + consumebufoff, cnssz);
		consumebufoff = 0;
		receivebufoff = cnssz;
	}
}
//-----------------------------------------------------------------------------
inline void Connection::doOptimizeRecvBufferForced(){
	const size_t cnssz = receivebufoff - consumebufoff;
	//idbgx(Debug::proto_bin, this<<' '<<"memcopy "<<cnssz<<" rcvoff = "<<receivebufoff<<" cnsoff = "<<consumebufoff);
	memmove(recvbuf, recvbuf + consumebufoff, cnssz);
	consumebufoff = 0;
	receivebufoff = cnssz;
}
//-----------------------------------------------------------------------------
Connection::Connection(
	SocketDevice &_rsd
):	sock(this->proxy(), std::move(_rsd)), timer(this->proxy()),
	crtpushvecidx(0), flags(0), atomic_flags(0), receivebufoff(0), consumebufoff(0),
	receive_keepalive_count(0),
	recvbuf(nullptr), sendbuf(nullptr)
{
	idbgx(Debug::ipc, this<<' '<<timer.isActive()<<' '<<sock.isActive());
}
//-----------------------------------------------------------------------------
Connection::Connection(
	ConnectionPoolId const &_rconpoolid
):	conpoolid(_rconpoolid), sock(this->proxy()), timer(this->proxy()),
	crtpushvecidx(0), flags(0), atomic_flags(0), receivebufoff(0), consumebufoff(0),
	receive_keepalive_count(0),
	recvbuf(nullptr), sendbuf(nullptr)
{
	idbgx(Debug::ipc, this<<' '<<timer.isActive()<<' '<<sock.isActive());
}
//-----------------------------------------------------------------------------
Connection::~Connection(){
	idbgx(Debug::ipc, this);
}
//-----------------------------------------------------------------------------
// NOTE
// Use two sendmsgvecs for locking granularity.
// We do not relay only on the service's connection pool queue, because
// of server-side connections which do not/cannot have an associated connection pool.
// In this case - server-side - the messages must pass directly to the connection.
//
bool Connection::pushMessage(
	Service &_rservice,
	MessagePointerT &_rmsgptr,
	const size_t _msg_type_idx,
	ResponseHandlerFunctionT &_rresponse_fnc,
	ulong _flags,
	MessageId *_pmsguid,
	Event &_revent,
	ErrorConditionT &_rerror,
	const MessageId &_rpool_msg_id
){
	idbgx(Debug::ipc, this<<' '<<this->id()<<" crtpushvecidx = "<<(int)crtpushvecidx<<" msg_type_idx = "<<_msg_type_idx<<" flags = "<<_flags<<" msgptr = "<<_rmsgptr.get());
	//Under lock
	if(not isAtomicStopping()){
		if(not isAtomicDelayedClosing()){
			MessageId	msguid = msgwriter.safeNewMessageId(_rservice.configuration());
			
			if(msguid.isValid()){
				
				sendmsgvec[crtpushvecidx].push_back(
					PendingSendMessageStub(
						MessageBundle(_rmsgptr, _msg_type_idx, _flags, _rresponse_fnc),
						msguid
					)
				);
				
				if(_pmsguid){
					*_pmsguid = msguid;
				}
				
				if(sendmsgvec[crtpushvecidx].size() == 1){
					_revent = connection_event_category.event(ConnectionEvents::Push);
				}
				return true;
			}else{
				//TODO: TooManyMessages
				_rerror.assign(-1, _rerror.category());
			}
		}else{
			//TODO: ConnectionIsDelayedClosing
			_rerror.assign(-1, _rerror.category());
		}
	}else{
		//TODO: ConnectionIsStopping
		_rerror.assign(-1, _rerror.category());
	}
	
	return false;
}
//-----------------------------------------------------------------------------
bool Connection::pushCanceledMessage(
	Service &_rservice,
	MessagePointerT &_rmsgptr,
	const size_t _msg_type_idx,
	ResponseHandlerFunctionT &_rresponse_fnc,
	ulong _flags,
	Event &_revent,
	ErrorConditionT &_rerror,
	const MessageId &_rpool_msg_id
){
	cassert(Message::is_canceled(_flags));
	//under lock
	sendmsgvec[crtpushvecidx].push_back(
		PendingSendMessageStub(
			MessageBundle(_rmsgptr, _msg_type_idx, _flags, _rresponse_fnc),
			_rpool_msg_id
		)
	);
	
	if(sendmsgvec[crtpushvecidx].size() == 1){
		_revent = connection_event_category.event(ConnectionEvents::Push);
	}
	return true;
}
//-----------------------------------------------------------------------------
void Connection::directPushMessage(
	frame::aio::ReactorContext &_rctx,
	MessageBundle &_rmsgbundle,
	MessageId *_pmsguid,
	const MessageId &_rpool_msg_id
){
	ConnectionContext	conctx(service(_rctx), *this);
	const TypeIdMapT	&rtypemap = service(_rctx).typeMap();
	const Configuration &rconfig  = service(_rctx).configuration();
	MessageId			msguid;
	if(not Message::is_canceled(_rmsgbundle.message_flags)){
		{
			Locker<Mutex>		lock(service(_rctx).mutex(*this));
			msguid = msgwriter.safeNewMessageId(rconfig);
		}
		
		msgwriter.enqueue(_rmsgbundle, msguid, rconfig, rtypemap, conctx);
		
		if(_pmsguid){
			*_pmsguid = msguid;
		}
	}else{
		//canceled message
		msgwriter.cancel(_rmsgbundle, _rpool_msg_id, rconfig, rtypemap, conctx);
	}
}
//-----------------------------------------------------------------------------
bool Connection::pushMessageCancel(
	Service &_rservice,
	MessageId const &_rmsguid,
	Event &_revent,
	ErrorConditionT &_rerror
){
	idbgx(Debug::ipc, this<<' '<<this->id()<<' '<<_rmsguid);
	//Under lock
	if(not isAtomicStopping()){
		
		sendmsgvec[crtpushvecidx].push_back(
			PendingSendMessageStub(
				_rmsguid
			)
		);
		
		if(sendmsgvec[crtpushvecidx].size() == 1){
			_revent = connection_event_category.event(ConnectionEvents::Push);
		}
		return true;
	}else{
		//TODO: ConnectionIsStopping
		_rerror.assign(-1, _rerror.category());
	}
	return false;
}
//-----------------------------------------------------------------------------
bool Connection::pushDelayedClose(
	Service &_rservice,
	Event &_revent,
	ErrorConditionT &_rerror
){
	idbgx(Debug::ipc, this<<' '<<this->id());
				
	//Under lock
	if(not isAtomicStopping()){
		if(not isAtomicDelayedClosing()){
			atomic_flags |= static_cast<size_t>(AtomicFlags::DelayedClosing);
			_revent = connection_event_category.event(ConnectionEvents::DelayedClose);
			return true;
		}else{
			//TODO: ConnectionIsDelayedClosing
			_rerror.assign(-1, _rerror.category());
		}
	}else{
		//TODO: ConnectionIsStopping
		_rerror.assign(-1, _rerror.category());
	}
	return false;
}
//-----------------------------------------------------------------------------
bool Connection::prepareActivate(
	Service &_rservice,
	ConnectionPoolId const &_rconpoolid, Event &_revent, ErrorConditionT &_rerror
){
	//Under lock
	if(not isAtomicStopping()){
		if(_rconpoolid.isValid()){
			if(conpoolid.isInvalid()){
				conpoolid = _rconpoolid;//conpoolid should be used only if isActive
			}else{
				cassert(conpoolid == _rconpoolid);
			}
		}
		
		atomic_flags |= static_cast<size_t>(AtomicFlags::Active);
		
		_revent = connection_event_category.event(ConnectionEvents::Activate);
		return true;
	}else{
		//TODO: ConnectionIsStopping
		_rerror.assign(-1, _rerror.category());
		return false;
	}
}
//-----------------------------------------------------------------------------
bool Connection::isActive()const{
	return flags & static_cast<size_t>(Flags::Active);
}
//-----------------------------------------------------------------------------
bool Connection::isAtomicActive()const{
	return atomic_flags & static_cast<size_t>(AtomicFlags::Active);
}
//-----------------------------------------------------------------------------
bool Connection::isAtomicStopping()const{
	return atomic_flags & static_cast<size_t>(AtomicFlags::Stopping);
}
//-----------------------------------------------------------------------------
bool Connection::isAtomicDelayedClosing()const{
	return atomic_flags & static_cast<size_t>(AtomicFlags::DelayedClosing);
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
bool Connection::isWaitingKeepAliveTimer()const{
	return flags & static_cast<size_t>(Flags::WaitKeepAliveTimer);
}
//-----------------------------------------------------------------------------
bool Connection::isStopForced()const{
	return flags & static_cast<size_t>(Flags::StopForced);
}
//-----------------------------------------------------------------------------
void Connection::doPrepare(frame::aio::ReactorContext &_rctx){
	recvbuf = service(_rctx).configuration().allocateRecvBuffer();
	sendbuf = service(_rctx).configuration().allocateSendBuffer();
	msgreader.prepare(service(_rctx).configuration());
	msgwriter.prepare(service(_rctx).configuration());
}
//-----------------------------------------------------------------------------
void Connection::doUnprepare(frame::aio::ReactorContext &_rctx){
	service(_rctx).configuration().freeRecvBuffer(recvbuf);
	service(_rctx).configuration().freeSendBuffer(sendbuf);
	msgreader.unprepare();
	msgwriter.unprepare();
}
//-----------------------------------------------------------------------------
void Connection::doStart(frame::aio::ReactorContext &_rctx, const bool _is_incomming){
	ConnectionContext 	conctx(service(_rctx), *this);
	Configuration const &config = service(_rctx).configuration();
	
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
	sock.postRecvSome(_rctx, recvbuf, config.recv_buffer_capacity, Connection::onRecv);
	doResetTimerStart(_rctx);
}
//-----------------------------------------------------------------------------
void Connection::onStopped(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr){
	ObjectIdT			objuid(uid(_rctx));
	ConnectionContext	conctx(service(_rctx), *this);
	
	service(_rctx).onConnectionClose(*this, _rctx, objuid);//must be called after postStop!!
	
	doCompleteAllMessages(_rctx, _rerr);
	
	service(_rctx).onConnectionStop(conctx, _rerr);
	
	doUnprepare(_rctx);
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onTimerWaitStopping(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr){
	Connection			&rthis = static_cast<Connection&>(_rctx.object());
	ObjectIdT			objuid(rthis.uid(_rctx));
	ErrorConditionT		error(_rerr);
	ulong				seconds_to_wait = rthis.service(_rctx).onConnectionWantStop(rthis, _rctx, objuid);
	
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
	if(not isAtomicStopping()){
		{
			Locker<Mutex>	lock(service(_rctx).mutex(*this));
			atomic_flags |= static_cast<size_t>(AtomicFlags::Stopping);
		}
		
		//after this poin we know for sure that
		//either the connection was already active or will never be active
		
		ErrorConditionT		error(_rerr);
		ObjectIdT			objuid(uid(_rctx));
		const ulong			seconds_to_wait = service(_rctx).onConnectionWantStop(*this, _rctx, objuid);
		
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
				connection_event_category.event(ConnectionEvents::Push),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventPush(_rctx, _revt);
				}
			},
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
				connection_event_category.event(ConnectionEvents::Activate),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventActivate(_rctx, _revt);
				}
			},
			{
				connection_event_category.event(ConnectionEvents::DelayedClose),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventDelayedClose(_rctx, _revt);
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
	idbgx(Debug::ipc, this<<' '<<this->id()<<" Session start: "<<sock.device().ok() ? " connected " : "not connected");
	if(sock.device().ok()){
		doStart(_rctx, true);
	}
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventKill(
	frame::aio::ReactorContext &_rctx,
	Event &_revent
){
	idbgx(Debug::ipc, this<<' '<<this->id()<<" Session postStop");
	flags |= static_cast<size_t>(Flags::StopForced);
	doStop(_rctx, error_connection_killed);
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventActivate(
	frame::aio::ReactorContext &_rctx,
	Event &_revent
){
	idbgx(Debug::ipc, this);

	if(not isAtomicStopping()){
		flags |= static_cast<size_t>(Flags::Active);
		service(_rctx).activateConnectionComplete(*this);
		this->post(_rctx, [this](frame::aio::ReactorContext &_rctx, Event &&/*_revent*/){this->doSend(_rctx);});
	}
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventPush(
	frame::aio::ReactorContext &_rctx,
	Event &_revent
){
	size_t		vecidx = InvalidIndex();
	
	{
		Locker<Mutex>	lock(service(_rctx).mutex(*this));
		
		vecidx = crtpushvecidx;
		crtpushvecidx += 1;
		crtpushvecidx %= 2;
	}
	
	ConnectionContext			conctx(service(_rctx), *this);
	const TypeIdMapT			&rtypemap = service(_rctx).typeMap();
	const Configuration 		&rconfig  = service(_rctx).configuration();
	bool						was_empty_msgwriter = msgwriter.empty();
	PendingSendMessageVectorT 	&rsendmsgvec = sendmsgvec[vecidx];
	
	
	for(auto it = rsendmsgvec.begin(); it != rsendmsgvec.end(); ++it){
		if(not it->msgbundle.message_ptr.empty()){
			if(not Message::is_canceled(it->msgbundle.message_flags)){
				msgwriter.enqueue(
					it->msgbundle, it->msguid, rconfig, rtypemap, conctx
				);
			}else{
				msgwriter.cancel(
					it->msgbundle, it->msguid, rconfig, rtypemap, conctx
				);
			}
		}else{
			msgwriter.cancel(it->msguid, rconfig, rtypemap, conctx);
		}
	}
	
	rsendmsgvec.clear();
	
	if(not msgwriter.isNonSafeCacheEmpty()){
		Locker<Mutex>	lock(service(_rctx).mutex(*this));
		msgwriter.safeMoveCacheToSafety();
	}
	if(was_empty_msgwriter and not msgwriter.empty()){
		idbgx(Debug::ipc, this<<" post send");
		this->post(_rctx, [this](frame::aio::ReactorContext &_rctx, Event const &/*_revent*/){this->doSend(_rctx);});
	}
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventResolve(
	frame::aio::ReactorContext &_rctx,
	Event &_revent
){

	ResolveMessage *presolvemsg = _revent.any().cast<ResolveMessage>();
	if(presolvemsg){
		idbgx(Debug::ipc, this<<' '<<this->id()<<" Session receive resolve event message of size: "<<presolvemsg->addrvec.size()<<" crtidx = "<<presolvemsg->crtidx);
		if(presolvemsg->crtidx < presolvemsg->addrvec.size()){
			std::string addr;
			
			idbgx(Debug::ipc, this<<' '<<this->id()<<" Connect to "<<presolvemsg->currentAddress());
			//initiate connect:
			if(sock.connect(_rctx, presolvemsg->currentAddress(), Connection::onConnect)){
				onConnect(_rctx);
			}
			
			service(_rctx).forwardResolveMessage(conpoolid, _revent);
		}else{
			cassert(true);
			doStop(_rctx, error_library_logic);
		}
	}else{
		cassert(false);
	}
}
//-----------------------------------------------------------------------------
void Connection::doHandleEventDelayedClose(frame::aio::ReactorContext &_rctx, Event &/*_revent*/){
	cassert(isAtomicDelayedClosing());
	bool				was_empty_msgwriter = msgwriter.empty();
	MessageId			msguid;
	//Configuration const &rconfig = service(_rctx).configuration();
	
	{
		Locker<Mutex>		lock(service(_rctx).mutex(*this));
		msguid = msgwriter.safeForcedNewMessageId();//enqueueing close cannot fail
	}
	
	msgwriter.enqueueClose(msguid);
	
	cassert(not msgwriter.empty());
	
	if(was_empty_msgwriter and not msgwriter.empty()){
		idbgx(Debug::ipc, this<<" post send");
		this->post(_rctx, [this](frame::aio::ReactorContext &_rctx, Event const &/*_revent*/){this->doSend(_rctx);});
	}
}
//-----------------------------------------------------------------------------
void Connection::doResetTimerStart(frame::aio::ReactorContext &_rctx){
	Configuration const &config = service(_rctx).configuration();
	if(isServer()){
		if(config.inactivity_timeout_seconds){
			receive_keepalive_count = 0;
			flags &= (~static_cast<size_t>(Flags::HasActivity));
			timer.waitFor(_rctx, TimeSpec(config.inactivity_timeout_seconds), onTimerInactivity);
		}
	}else{//client
		if(config.keepalive_timeout_seconds){
			flags |= static_cast<size_t>(Flags::WaitKeepAliveTimer);
			timer.waitFor(_rctx, TimeSpec(config.keepalive_timeout_seconds), onTimerKeepalive);
		}
	}
}
//-----------------------------------------------------------------------------
void Connection::doResetTimerSend(frame::aio::ReactorContext &_rctx){
	Configuration const &config = service(_rctx).configuration();
	if(isServer()){
		if(config.inactivity_timeout_seconds){
			flags |= static_cast<size_t>(Flags::HasActivity);
		}
	}else{//client
		if(config.keepalive_timeout_seconds and isWaitingKeepAliveTimer()){
			timer.waitFor(_rctx, TimeSpec(config.keepalive_timeout_seconds), onTimerKeepalive);
		}
	}
}
//-----------------------------------------------------------------------------
void Connection::doResetTimerRecv(frame::aio::ReactorContext &_rctx){
	Configuration const &config = service(_rctx).configuration();
	if(isServer()){
		if(config.inactivity_timeout_seconds){
			flags |= static_cast<size_t>(Flags::HasActivity);
		}
	}else{//client
		if(config.keepalive_timeout_seconds and not isWaitingKeepAliveTimer()){
			flags |= static_cast<size_t>(Flags::WaitKeepAliveTimer);
			timer.waitFor(_rctx, TimeSpec(config.keepalive_timeout_seconds), onTimerKeepalive);
		}
	}
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onTimerInactivity(frame::aio::ReactorContext &_rctx){
	Connection			&rthis = static_cast<Connection&>(_rctx.object());
	
	idbgx(Debug::ipc, &rthis<<" "<<(int)rthis.flags<<" "<<rthis.receive_keepalive_count);
	
	if(rthis.flags & static_cast<size_t>(Flags::HasActivity)){
		
		rthis.flags &= (~static_cast<size_t>(Flags::HasActivity));
		rthis.receive_keepalive_count = 0;
		
		Configuration const &config = rthis.service(_rctx).configuration();
		
		rthis.timer.waitFor(_rctx, TimeSpec(config.inactivity_timeout_seconds), onTimerInactivity);
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
	const uint32		recvbufcp = rthis.service(_rctx).configuration().recv_buffer_capacity;
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
			rthis.receivebufoff += _sz;
			pbuf = rthis.recvbuf + rthis.consumebufoff;
			bufsz = rthis.receivebufoff - rthis.consumebufoff;
			ErrorConditionT						error;
			MessageReader::CompleteFunctionT	completefnc(std::cref(complete_lambda));
			
			rthis.consumebufoff += rthis.msgreader.read(pbuf, bufsz, completefnc, rconfig, rtypemap, conctx, error);
			
			idbgx(Debug::ipc, &rthis<<" consumed size "<<rthis.consumebufoff<<" of "<<bufsz);
			
			if(error){
				edbgx(Debug::ipc, &rthis<<' '<<rthis.id()<<" parsing "<<error.message());
				rthis.doStop(_rctx, error);
				recv_something = false;//prevent calling doResetTimerRecv after doStop
				break;
			}else if(rthis.consumebufoff < bufsz){
				rthis.doOptimizeRecvBufferForced();
			}
		}else{
			edbgx(Debug::ipc, &rthis<<' '<<rthis.id()<<" receiving "<<_rctx.error().message());
			rthis.doStop(_rctx, _rctx.error());
			recv_something = false;//prevent calling doResetTimerRecv after doStop
			break;
		}
		--repeatcnt;
		rthis.doOptimizeRecvBuffer();
		pbuf = rthis.recvbuf + rthis.receivebufoff;
		bufsz =  recvbufcp - rthis.receivebufoff;
		//idbgx(Debug::ipc, &rthis<<" buffer size "<<bufsz);
	}while(repeatcnt && rthis.sock.recvSome(_rctx, pbuf, bufsz, Connection::onRecv, _sz));
	
	if(recv_something){
		rthis.doResetTimerRecv(_rctx);
		if(not rthis.msgwriter.isNonSafeCacheEmpty()){
			Locker<Mutex>	lock(rthis.service(_rctx).mutex(rthis));
			rthis.msgwriter.safeMoveCacheToSafety();
		}
	}
	
	if(repeatcnt == 0){
		bool rv = rthis.sock.postRecvSome(_rctx, pbuf, bufsz, Connection::onRecv);//fully asynchronous call
		cassert(!rv);
		(void)rv;
	}
}
//-----------------------------------------------------------------------------
void Connection::doSend(frame::aio::ReactorContext &_rctx, const bool _sent_something/* = false*/){
	idbgx(Debug::ipc, this<<"");
	if(!sock.hasPendingSend()){
		ConnectionContext	conctx(service(_rctx), *this);
		unsigned 			repeatcnt = 4;
		ErrorConditionT		error;
		const uint32		sendbufcp = service(_rctx).configuration().send_buffer_capacity;
		const TypeIdMapT	&rtypemap = service(_rctx).typeMap();
		const Configuration &rconfig  = service(_rctx).configuration();
		bool				sent_something = _sent_something;
		
		doResetTimerSend(_rctx);
		
		while(repeatcnt){
			
			if(
				conpoolid.isValid() and
				isActive() and
				msgwriter.shouldTryFetchNewMessage(rconfig)
			){
				service(_rctx).tryFetchNewMessage(*this, _rctx, uid(_rctx), msgwriter.empty());
			}
			
#if 0
			if(msgwriter.empty()){
				//nothing to do but wait
				break;
			}
#endif

			uint32 sz = msgwriter.write(sendbuf, sendbufcp, shouldSendKeepalive(), rconfig, rtypemap, conctx, error);
			
			flags &= (~static_cast<size_t>(Flags::Keepalive));
			
			if(!error){
				if(sz && sock.sendAll(_rctx, sendbuf, sz, Connection::onSend)){
					if(_rctx.error()){
						edbgx(Debug::ipc, this<<' '<<id()<<" sending "<<sz<<": "<<_rctx.error().message());
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
				flags |= static_cast<size_t>(Flags::StopForced);//TODO: maybe you should not set this all the time
				doStop(_rctx, error);
				sent_something = false;//prevent calling doResetTimerSend after doStop
				break;
			}
			--repeatcnt;
		}
		
		if(sent_something){
			doResetTimerSend(_rctx);
			if(not msgwriter.isNonSafeCacheEmpty()){
				Locker<Mutex>	lock(service(_rctx).mutex(*this));
				msgwriter.safeMoveCacheToSafety();
			}
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
		msgwriter.completeMessage(_rmsgptr, _rmsgptr->requid, rconfig, rtypemap, conctx, error);
	}
}
//-----------------------------------------------------------------------------
void Connection::doCompleteKeepalive(frame::aio::ReactorContext &_rctx){
	if(isServer()){
		Configuration const &config = service(_rctx).configuration();
		
		++receive_keepalive_count;
		
		if(receive_keepalive_count < config.inactivity_keepalive_count){
			flags |= static_cast<size_t>(Flags::Keepalive);
			idbgx(Debug::ipc, this<<" post send");
			this->post(_rctx, [this](frame::aio::ReactorContext &_rctx, Event const &/*_revent*/){this->doSend(_rctx);});
		}else{
			idbgx(Debug::ipc, this<<" post stop because of too many keep alive messages");
			receive_keepalive_count = 0;//prevent other posting
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
	
	if(isStopForced() or conpoolid.isInvalid()){
		//really complete
		msgwriter.completeAllMessages(rconfig, rtypemap, conctx, _rerr);
	}else{
		//connection lost - try reschedule whatever messages we can
		//TODO:
	}
}
//-----------------------------------------------------------------------------
//=============================================================================
//-----------------------------------------------------------------------------
SocketDevice const & ConnectionContext::device()const{
	return rconnection.device();
}
//-----------------------------------------------------------------------------
Any<>& ConnectionContext::any(){
	return rconnection.any();
}
//-----------------------------------------------------------------------------
RecipientId	ConnectionContext::recipientId()const{
	return RecipientId(rconnection.poolId(), rservice.manager().id(rconnection));
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
