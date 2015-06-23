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

enum States{
	StateInactiveE = 0,
	StateActiveE = 1,
	StateStoppingE = 2
};
namespace{
struct EventCategory: public Dynamic<EventCategory, EventCategoryBase>{
	enum EventId{
		ActivateE,
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
	
	
	static Event create(EventId _evid){
		return the().doCreate(static_cast<size_t>(_evid));
	}
	
	static Event createActivate(){
		return create(ActivateE);
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
//-----------------------------------------------------------------------------
inline void Connection::doOptimizeRecvBuffer(){
	const size_t cnssz = receivebufoff - consumebufoff;
	if(cnssz <= consumebufoff){
		idbgx(Debug::proto_bin, "memcopy "<<cnssz<<" rcvoff = "<<receivebufoff<<" cnsoff = "<<consumebufoff);
		memcpy(recvbuf, recvbuf + consumebufoff, cnssz);
		consumebufoff = 0;
		receivebufoff = cnssz;
	}
}
//-----------------------------------------------------------------------------
Connection::Connection(
	SocketDevice &_rsd
):	sock(this->proxy(), std::move(_rsd)), timer(this->proxy()),
	crtpushvecidx(0), state(0)
{
	idbgx(Debug::ipc, this);
}
//-----------------------------------------------------------------------------
Connection::Connection(
	ConnectionPoolUid const &_rconpoolid
):	conpoolid(_rconpoolid), sock(this->proxy()), timer(this->proxy()),
	crtpushvecidx(0), state(0)
{
	idbgx(Debug::ipc, this);
}
//-----------------------------------------------------------------------------
Connection::~Connection(){
	idbgx(Debug::ipc, this);
}
//-----------------------------------------------------------------------------
bool Connection::pushMessage(
	MessagePointerT &_rmsgptr,
	const size_t _msg_type_idx,
	ulong _flags
){
	idbgx(Debug::ipc, this->id()<<" crtpushvecidx = "<<crtpushvecidx<<" msg_type_idx = "<<_msg_type_idx<<" flags = "<<_flags<<" msgptr = "<<_rmsgptr.get());
	//Under lock
	sendmsgvec[crtpushvecidx].push_back(PendingSendMessageStub(_rmsgptr, _msg_type_idx, _flags));
	return sendmsgvec[crtpushvecidx].size() == 1;
}
void Connection::directPushMessage(
	frame::aio::ReactorContext &_rctx,
	MessagePointerT &_rmsgptr,
	const size_t _msg_type_idx,
	ulong _flags
){
	ConnectionContext	conctx(service(_rctx), *this);
	const TypeIdMapT	&rtypemap = service(_rctx).typeMap();
	const Configuration &rconfig  = service(_rctx).configuration();
	msgwriter.enqueue(_rmsgptr, _msg_type_idx, _flags, rconfig, rtypemap, conctx);
}
//-----------------------------------------------------------------------------
bool Connection::isActive()const{
	return state & StateActiveE;
}
//-----------------------------------------------------------------------------
bool Connection::isStopping()const{
	return state & StateStoppingE;
}
//-----------------------------------------------------------------------------
void Connection::doPrepare(frame::aio::ReactorContext &_rctx){
	recvbuf = service(_rctx).configuration().allocateRecvBuffer();
	sendbuf = service(_rctx).configuration().allocateSendBuffer();
}
//-----------------------------------------------------------------------------
void Connection::doUnprepare(frame::aio::ReactorContext &_rctx){
	service(_rctx).configuration().freeRecvBuffer(recvbuf);
	service(_rctx).configuration().freeSendBuffer(sendbuf);
}
//-----------------------------------------------------------------------------
void Connection::doStart(frame::aio::ReactorContext &_rctx, const bool _is_incomming){
	ConnectionContext conctx(service(_rctx), *this);
	doPrepare(_rctx);
	if(_is_incomming){
		service(_rctx).onIncomingConnectionStart(conctx);
	}else{
		service(_rctx).onOutgoingConnectionStart(conctx);
	}
	//start sending messages.
	this->post(
		_rctx,
		[this](frame::aio::ReactorContext &_rctx, frame::Event const &/*_revent*/){
			doSend(_rctx);
		}
	);
	
	//start receiving messages
	sock.postRecvSome(_rctx, recvbuf, service(_rctx).configuration().recv_buffer_capacity, Connection::onRecv);
}
//-----------------------------------------------------------------------------
void Connection::doStop(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr){
	ConnectionContext	conctx(service(_rctx), *this);
	ObjectUidT			objuid(uid(_rctx));
	
	postStop(_rctx);//there might be events pending which will be delivered, but after this call
					//no event get posted
	
	service(_rctx).onConnectionClose(*this, _rctx, objuid);
	
	doCompleteAllMessages(_rctx);
	
	service(_rctx).onConnectionStop(conctx, _rerr);
	state |= StateStoppingE;
	
	doUnprepare(_rctx);
}
//-----------------------------------------------------------------------------
/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	if(frame::EventCategory::isStart(_revent)){
		idbgx(Debug::ipc, this->id()<<" Session start: "<<sock.device().ok() ? " connected " : "not connected");
		if(sock.device().ok()){
			doStart(_rctx, true);
		}
	}else if(frame::EventCategory::isKill(_revent)){
		idbgx(Debug::ipc, this->id()<<" Session postStop");
		postStop(_rctx);
	}else if(_revent.msgptr.get()){
		ResolveMessage *presolvemsg = ResolveMessage::cast(_revent.msgptr.get());
		if(presolvemsg){
			idbgx(Debug::ipc, this->id()<<" Session receive resolve event message of size: "<<presolvemsg->addrvec.size()<<" crtidx = "<<presolvemsg->crtidx);
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
			
			idbgx(Debug::ipc, this->id()<<" Session message");
			
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
			
			for(auto it = rsendmsgvec.begin(); it != rsendmsgvec.end(); ++it){
				msgwriter.enqueue(it->msgptr, it->msg_type_idx, it->flags, rconfig, rtypemap, conctx);
			}
			
			rsendmsgvec.clear();
		}
	}
}
//-----------------------------------------------------------------------------
void Connection::doActivate(
	frame::aio::ReactorContext &_rctx,
	frame::Event const &_revent
){
	ActivateMessage *pactivatemsg = ActivateMessage::cast(_revent.msgptr.get());
	
	cassert(pactivatemsg == nullptr and _revent.msgptr.get() == nullptr);
	
	if(pactivatemsg){
		this->conpoolid = pactivatemsg->poolid;
	}
	
	cassert(!isActive());
	state |= StateActiveE;
	
	if(not isStopping()){
		service(_rctx).activateConnectionComplete(*this);
	}else{
		ObjectUidT			objuid;
		
		service(_rctx).onConnectionClose(*this, _rctx, objuid);
		
		doCompleteAllMessages(_rctx);
	}
}
//-----------------------------------------------------------------------------
void Connection::doCompleteAllMessages(
	frame::aio::ReactorContext &_rctx
){
	ConnectionContext	conctx(service(_rctx), *this);
	//TODO:
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
	
	const uint16		recvbufcp = rthis.service(_rctx).configuration().recv_buffer_capacity;
	
	do{
		if(!_rctx.error()){
			rthis.receivebufoff += _sz;
			pbuf = rthis.recvbuf + rthis.consumebufoff;
			bufsz = rthis.receivebufoff - rthis.consumebufoff;
			ErrorConditionT		error;
			rthis.consumebufoff += rthis.msgreader.read(pbuf, bufsz, rconfig, rtypemap, conctx, error);
			
			if(error){
				edbgx(Debug::ipc, rthis.id()<<" parsing "<<error.message());
				rthis.doStop(_rctx, error);
				break;
			}
		}else{
			edbgx(Debug::ipc, rthis.id()<<" receiving "<<_rctx.error().message());
			rthis.doStop(_rctx, _rctx.error());
			break;
		}
		--repeatcnt;
		rthis.doOptimizeRecvBuffer();
		pbuf = rthis.recvbuf + rthis.receivebufoff;
		bufsz =  recvbufcp - rthis.receivebufoff;
	}while(repeatcnt && rthis.sock.recvSome(_rctx, pbuf, bufsz, Connection::onRecv, _sz));
	
	if(repeatcnt == 0){
		bool rv = rthis.sock.postRecvSome(_rctx, pbuf, bufsz, Connection::onRecv);//fully asynchronous call
		cassert(!rv);
		(void)rv;
	}
}
//-----------------------------------------------------------------------------
void Connection::doSend(frame::aio::ReactorContext &_rctx){
	if(!sock.hasPendingSend()){
		ConnectionContext	conctx(service(_rctx), *this);
		unsigned 			repeatcnt = 4;
		ErrorConditionT		error;
		const uint16		sendbufcp = service(_rctx).configuration().send_buffer_capacity;
		const TypeIdMapT	&rtypemap = service(_rctx).typeMap();
		const Configuration &rconfig  = service(_rctx).configuration();
		
		while(repeatcnt){
			
			uint16 sz = msgwriter.write(sendbuf, sendbufcp, rconfig, rtypemap, conctx, error);
			
			if(!error){
				if(sock.sendAll(_rctx, sendbuf, sz, Connection::onSend)){
					if(_rctx.error()){
						edbgx(Debug::ipc, id()<<" sending "<<_rctx.error().message());
						doStop(_rctx, _rctx.error());
					}
				}else{
					break;
				}
			}else{
				edbgx(Debug::ipc, id()<<" storring "<<error.message());
				doStop(_rctx, error);
				break;
			}
			--repeatcnt;
		}
		if(repeatcnt == 0){
			this->post(_rctx, [this](frame::aio::ReactorContext &_rctx, Event const &/*_revent*/){this->doSend(_rctx);});
		}
	}
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onSend(frame::aio::ReactorContext &_rctx){
	Connection	&rthis = static_cast<Connection&>(_rctx.object());
	if(!_rctx.error()){
		rthis.doSend(_rctx);
	}else{
		edbgx(Debug::ipc, rthis.id()<<" sending "<<_rctx.error().message());
		rthis.doStop(_rctx, _rctx.error());
	}
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onConnect(frame::aio::ReactorContext &_rctx){
	Connection	&rthis = static_cast<Connection&>(_rctx.object());
	if(!_rctx.error()){
		idbgx(Debug::ipc, rthis.id());
		rthis.doStart(_rctx, false);
	}else{
		edbgx(Debug::ipc, rthis.id()<<" connecting "<<_rctx.error().message());
		rthis.doStop(_rctx, _rctx.error());
	}
}
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
}//namespace ipc
}//namespace frame
}//namespace solid
