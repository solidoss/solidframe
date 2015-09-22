// frame/ipc/src/ipcconnection.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/manager.hpp"
#include "ipcconnection.hpp"
#include "frame/ipc/ipcservice.hpp"
#include "frame/aio/aioreactorcontext.hpp"
#include "frame/aio/openssl/aiosecuresocket.hpp"

namespace solid{
namespace frame{
namespace ipc{

enum Flags{
	FlagInactiveE = 0,
	FlagActiveE = 1,
	FlagStoppingE = 2,
	FlagServerE = 4,
	FlagKeepaliveE = 8,
	FlagWaitKeepAliveTimerE = 16,
	FlagStopForcedE = 32,
};
namespace{
struct EventCategory: public Dynamic<EventCategory, EventCategoryBase>{
	enum EventId{
		ActivateE,
		ResolveE,
		InvalidE
	};
	
	static EventId id(Event const&_re){
		if(the().check(_re)){
			return static_cast<EventId>(the().eventId(_re));
		}else{
			return InvalidE;
	}
	}
	
	static bool isActivate(Event const&_re){
		return id(_re) == ActivateE;
	}
	
	static bool isResolve(Event const&_re){
		return id(_re) == ResolveE;
	}
	
	static Event create(EventId _evid){
		return the().doCreate(static_cast<size_t>(_evid));
	}
	
	static Event createActivate(){
		return create(ActivateE);
	}
	
	static Event createResolve(){
		return create(ResolveE);
	}
	
	static EventCategory const& the(){
		static const EventCategory evc;
		return evc;
	}

private:
	/*virtual*/ void print(std::ostream &_ros, Event const &_re)const{
		const char *pstr;
		switch(eventId(_re)){
			case ActivateE:
				pstr = "frame::ipc::ActivateE";break;
			case ResolveE:
				pstr = "frame::ipc::ResolveE";break;
			default:
				_ros<<"frame::ipc::Unkonwn::"<<eventId(_re);
				return;
		}
		_ros<<pstr;
	}
};
}//namespace
//-----------------------------------------------------------------------------
inline Service& Connection::service(frame::aio::ReactorContext &_rctx)const{
	return static_cast<Service&>(_rctx.service());
}
//-----------------------------------------------------------------------------
inline ObjectUidT Connection::uid(frame::aio::ReactorContext &_rctx)const{
	return service(_rctx).manager().id(*this);
}
//-----------------------------------------------------------------------------

struct ActivateMessage: Dynamic<ActivateMessage>{
	ConnectionPoolUid	poolid;
	ActivateMessage(){}
	ActivateMessage(ConnectionPoolUid const& _rconpoolid):poolid(_rconpoolid){}
};

/*static*/ Event Connection::activateEvent(ConnectionPoolUid const& _rconpoolid){
	Event ev = activateEvent();
	ev.msgptr = new ActivateMessage(_rconpoolid);
	return ev;
}
/*static*/ Event Connection::activateEvent(){
	return EventCategory::createActivate();
}

/*static*/ Event Connection::resolveEvent(){
	return EventCategory::createResolve();
}

//-----------------------------------------------------------------------------
inline void Connection::doOptimizeRecvBuffer(){
	const size_t cnssz = receivebufoff - consumebufoff;
	if(cnssz <= consumebufoff){
		idbgx(Debug::proto_bin, this<<' '<<"memcopy "<<cnssz<<" rcvoff = "<<receivebufoff<<" cnsoff = "<<consumebufoff);
		memcpy(recvbuf, recvbuf + consumebufoff, cnssz);
		consumebufoff = 0;
		receivebufoff = cnssz;
	}
}
inline void Connection::doOptimizeRecvBufferForced(){
	const size_t cnssz = receivebufoff - consumebufoff;
	idbgx(Debug::proto_bin, this<<' '<<"memcopy "<<cnssz<<" rcvoff = "<<receivebufoff<<" cnsoff = "<<consumebufoff);
	memmove(recvbuf, recvbuf + consumebufoff, cnssz);
	consumebufoff = 0;
	receivebufoff = cnssz;
}
//-----------------------------------------------------------------------------
Connection::Connection(
	SocketDevice &_rsd
):	sock(this->proxy(), std::move(_rsd)), timer(this->proxy()),
	crtpushvecidx(0), flags(0), receivebufoff(0), consumebufoff(0),
	recvbuf(nullptr), sendbuf(nullptr)
{
	idbgx(Debug::ipc, this<<' '<<timer.isActive()<<' '<<sock.isActive());
}
//-----------------------------------------------------------------------------
Connection::Connection(
	ConnectionPoolUid const &_rconpoolid
):	conpoolid(_rconpoolid), sock(this->proxy()), timer(this->proxy()),
	crtpushvecidx(0), flags(0), receivebufoff(0), consumebufoff(0),
	recvbuf(nullptr), sendbuf(nullptr)
{
	idbgx(Debug::ipc, this<<' '<<timer.isActive()<<' '<<sock.isActive());
}
//-----------------------------------------------------------------------------
Connection::~Connection(){
	idbgx(Debug::ipc, this);
}
//-----------------------------------------------------------------------------
bool Connection::pushMessage(
	MessagePointerT &_rmsgptr,
	const size_t _msg_type_idx,
	ResponseHandlerFunctionT &_rresponse_fnc,
	ulong _flags
){
	idbgx(Debug::ipc, this<<' '<<this->id()<<" crtpushvecidx = "<<(int)crtpushvecidx<<" msg_type_idx = "<<_msg_type_idx<<" flags = "<<_flags<<" msgptr = "<<_rmsgptr.get());
	//Under lock
	sendmsgvec[crtpushvecidx].push_back(PendingSendMessageStub(_rmsgptr, _msg_type_idx, _rresponse_fnc, _flags));
	return sendmsgvec[crtpushvecidx].size() == 1;
}
void Connection::directPushMessage(
	frame::aio::ReactorContext &_rctx,
	MessagePointerT &_rmsgptr,
	const size_t _msg_type_idx,
	ResponseHandlerFunctionT &_rresponse_fnc,
	ulong _flags
){
	ConnectionContext	conctx(service(_rctx), *this);
	const TypeIdMapT	&rtypemap = service(_rctx).typeMap();
	const Configuration &rconfig  = service(_rctx).configuration();
	msgwriter.enqueue(_rmsgptr, _msg_type_idx, _rresponse_fnc, _flags, rconfig, rtypemap, conctx);
}
//-----------------------------------------------------------------------------
bool Connection::isActive()const{
	return flags & FlagActiveE;
}
//-----------------------------------------------------------------------------
bool Connection::isStopping()const{
	return flags & FlagStoppingE;
}
//-----------------------------------------------------------------------------
bool Connection::isServer()const{
	return flags & FlagServerE;
}
//-----------------------------------------------------------------------------
bool Connection::shouldSendKeepalive()const{
	return flags & FlagKeepaliveE;
}
//-----------------------------------------------------------------------------
bool Connection::isWaitingKeepAliveTimer()const{
	return flags & FlagWaitKeepAliveTimerE;
}
//-----------------------------------------------------------------------------
bool Connection::isStopForced()const{
	return flags & FlagStopForcedE;
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
		flags |= FlagServerE;
		service(_rctx).onIncomingConnectionStart(conctx);
	}else{
		service(_rctx).onOutgoingConnectionStart(conctx);
	}
	
	idbgx(Debug::ipc, this<<" post send");
	//start sending messages.
	this->post(
		_rctx,
		[this](frame::aio::ReactorContext &_rctx, frame::Event const &/*_revent*/){
			doSend(_rctx);
		}
	);
	
	//start receiving messages
	sock.postRecvSome(_rctx, recvbuf, config.recv_buffer_capacity, Connection::onRecv);
	doResetTimerStart(_rctx);
}
//-----------------------------------------------------------------------------
void Connection::doStop(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr){
	if(not isStopping()){
		ConnectionContext	conctx(service(_rctx), *this);
		ObjectUidT			objuid(uid(_rctx));
		
		postStop(_rctx);//there might be events pending which will be delivered, but after this call
						//no event get posted
		
		service(_rctx).onConnectionClose(*this, _rctx, objuid);//must be called after postStop!!
		
		doCompleteAllMessages(_rctx, _rerr);
		
		service(_rctx).onConnectionStop(conctx, _rerr);
		flags |= FlagStoppingE;
		
		doUnprepare(_rctx);
	}
}
//-----------------------------------------------------------------------------
/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	if(frame::EventCategory::isStart(_revent)){
		idbgx(Debug::ipc, this<<' '<<timer.isActive()<<' '<<sock.isActive());
		idbgx(Debug::ipc, this<<' '<<this->id()<<" Session start: "<<sock.device().ok() ? " connected " : "not connected");
		if(sock.device().ok()){
			doStart(_rctx, true);
		}
	}else if(frame::EventCategory::isKill(_revent)){
		idbgx(Debug::ipc, this<<' '<<this->id()<<" Session postStop");
		flags |= FlagStopForcedE;
		ErrorConditionT err;
		err.assign(-1, err.category());//TODO:
		doStop(_rctx, err);
	}else if(EventCategory::isResolve(_revent)){
		ResolveMessage *presolvemsg = ResolveMessage::cast(_revent.msgptr.get());
		if(presolvemsg){
			idbgx(Debug::ipc, this<<' '<<this->id()<<" Session receive resolve event message of size: "<<presolvemsg->addrvec.size()<<" crtidx = "<<presolvemsg->crtidx);
			if(presolvemsg->crtidx < presolvemsg->addrvec.size()){
				//initiate connect:
				if(sock.connect(_rctx, presolvemsg->currentAddress(), Connection::onConnect)){
					onConnect(_rctx);
				}
				
				service(_rctx).forwardResolveMessage(conpoolid, _revent);
			}else{
				ErrorConditionT err;
				err.assign(-1, err.category());//TODO:
				doStop(_rctx, err);
			}
		}
	}else if(EventCategory::isActivate(_revent)){
		doActivate(_rctx, _revent);
	}else{
		size_t		vecidx = -1;
		{
			Locker<Mutex>	lock(service(_rctx).mutex(*this));
			
			idbgx(Debug::ipc, this<<' '<<this->id()<<" Session message");
			
			if(sendmsgvec[crtpushvecidx].size()){
				vecidx = crtpushvecidx;
				crtpushvecidx += 1;
				crtpushvecidx %= 2;
			}
		}
		
		if(vecidx < 2){
			ConnectionContext				conctx(service(_rctx), *this);
			const TypeIdMapT				&rtypemap = service(_rctx).typeMap();
			const Configuration 			&rconfig  = service(_rctx).configuration();
			
			PendingSendMessageVectorT 		&rsendmsgvec = sendmsgvec[vecidx];
			
			bool was_empty_msgwriter = msgwriter.empty();
			
			for(auto it = rsendmsgvec.begin(); it != rsendmsgvec.end(); ++it){
				msgwriter.enqueue(it->msgptr, it->msg_type_idx, it->response_fnc, it->flags, rconfig, rtypemap, conctx);
			}
			rsendmsgvec.clear();
			
			if(was_empty_msgwriter and not msgwriter.empty()){
				idbgx(Debug::ipc, this<<" post send");
				this->post(_rctx, [this](frame::aio::ReactorContext &_rctx, Event const &/*_revent*/){this->doSend(_rctx);});
			}
		}
	}
}
//-----------------------------------------------------------------------------
void Connection::doActivate(
	frame::aio::ReactorContext &_rctx,
	frame::Event const &_revent
){
	ActivateMessage *pactivatemsg = ActivateMessage::cast(_revent.msgptr.get());
	
	idbgx(Debug::ipc, this<<" "<<pactivatemsg);
	
	//if(pactivatemsg == nullptr and _revent.msgptr.get() != nullptr){
	//	cassert(false);
	//}
	
	cassert(!(pactivatemsg == nullptr and _revent.msgptr.get() != nullptr));
	
	if(pactivatemsg){
		this->conpoolid = pactivatemsg->poolid;
	}
	
	cassert(!isActive());
	flags |= FlagActiveE;
	
	if(not isStopping()){
		service(_rctx).activateConnectionComplete(*this);
		this->post(_rctx, [this](frame::aio::ReactorContext &_rctx, Event const &/*_revent*/){this->doSend(_rctx);});
	}else{
		ObjectUidT			objuid;
		ErrorConditionT 	err;
	
		service(_rctx).onConnectionClose(*this, _rctx, objuid);
		
		err.assign(-1, err.category());//TODO:
		
		doCompleteAllMessages(_rctx, err);
	}
}
//-----------------------------------------------------------------------------
void Connection::doResetTimerStart(frame::aio::ReactorContext &_rctx){
	Configuration const &config = service(_rctx).configuration();
	if(isServer()){
		if(config.inactivity_timeout_seconds){
			timer.waitFor(_rctx, TimeSpec(config.inactivity_timeout_seconds), onTimerInactivity);
		}
	}else{//client
		if(config.keepalive_timeout_seconds){
			flags |= FlagWaitKeepAliveTimerE;
			timer.waitFor(_rctx, TimeSpec(config.keepalive_timeout_seconds), onTimerKeepalive);
		}
	}
}
//-----------------------------------------------------------------------------
void Connection::doResetTimerSend(frame::aio::ReactorContext &_rctx){
	Configuration const &config = service(_rctx).configuration();
	if(isServer()){
		if(config.inactivity_timeout_seconds){
			timer.waitFor(_rctx, TimeSpec(config.inactivity_timeout_seconds), onTimerInactivity);
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
			timer.waitFor(_rctx, TimeSpec(config.inactivity_timeout_seconds), onTimerInactivity);
		}
	}else{//client
		if(config.keepalive_timeout_seconds and not isWaitingKeepAliveTimer()){
			flags |= FlagWaitKeepAliveTimerE;
			timer.waitFor(_rctx, TimeSpec(config.keepalive_timeout_seconds), onTimerKeepalive);
		}
	}
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
		idbgx(Debug::ipc, &rthis<<" buffer size "<<bufsz);
	}while(repeatcnt && rthis.sock.recvSome(_rctx, pbuf, bufsz, Connection::onRecv, _sz));
	
	if(recv_something){
		rthis.doResetTimerRecv(_rctx);
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
				service(_rctx).tryFetchNewMessage(*this, _rctx, msgwriter.empty());
			}
			
#if 0
			if(msgwriter.empty()){
				//nothing to do but wait
				break;
			}
#endif

			uint32 sz = msgwriter.write(sendbuf, sendbufcp, shouldSendKeepalive(), rconfig, rtypemap, conctx, error);
			
			flags &= (~FlagKeepaliveE);
			
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
				flags |= FlagStopForcedE;//TODO: maybe you should not set this all the time
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
			idbgx(Debug::ipc, this<<" post send");
			this->post(_rctx, [this](frame::aio::ReactorContext &_rctx, Event const &/*_revent*/){this->doSend(_rctx);});
		}
		
	}
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onTimerInactivity(frame::aio::ReactorContext &_rctx){
	Connection		&rthis = static_cast<Connection&>(_rctx.object());
	ErrorConditionT	err;
	
	idbgx(Debug::ipc, &rthis<<"");
	
	err.assign(-1, err.category());//TODO:
 	rthis.doStop(_rctx, err);
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onTimerKeepalive(frame::aio::ReactorContext &_rctx){
	Connection	&rthis = static_cast<Connection&>(_rctx.object());
	cassert(not rthis.isServer());
	rthis.flags |= FlagKeepaliveE;
	rthis.flags &= (~FlagWaitKeepAliveTimerE);
	idbgx(Debug::ipc, &rthis<<" post send");
	rthis.post(_rctx, [&rthis](frame::aio::ReactorContext &_rctx, Event const &/*_revent*/){rthis.doSend(_rctx);});
	
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
		idbgx(Debug::ipc, this<<' '<<"Completing back on sender message: "<<_rmsgptr->msguid);
		msgwriter.completeMessage(_rmsgptr, _rmsgptr->msguid, rconfig, rtypemap, conctx, error);
	}
}
//-----------------------------------------------------------------------------
void Connection::doCompleteKeepalive(frame::aio::ReactorContext &_rctx){
	if(isServer()){
		flags |= FlagKeepaliveE;
		idbgx(Debug::ipc, this<<" post send");
		this->post(_rctx, [this](frame::aio::ReactorContext &_rctx, Event const &/*_revent*/){this->doSend(_rctx);});
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
HolderT& ConnectionContext::holder(){
	return rconnection.holder();
}
//-----------------------------------------------------------------------------
ConnectionUid	ConnectionContext::connectionId()const{
	return ConnectionUid(rconnection.poolUid(), rservice.manager().id(rconnection));
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
