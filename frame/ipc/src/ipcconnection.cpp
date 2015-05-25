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
//-----------------------------------------------------------------------------
inline Service& Connection::service(frame::aio::ReactorContext &_rctx)const{
	return static_cast<Service&>(_rctx.service());
}
//-----------------------------------------------------------------------------
inline ObjectUidT Connection::uid(frame::aio::ReactorContext &_rctx)const{
	return service(_rctx).manager().id(*this);
}
//-----------------------------------------------------------------------------
Connection::Connection(
	SocketDevice &_rsd
): sock(this->proxy(), std::move(_rsd)), timer(this->proxy()), crtpushvecidx(0)
{
	idbgx(Debug::ipc, this);
}
//-----------------------------------------------------------------------------
Connection::Connection(
	ConnectionPoolUid const &_rconpoolid
): conpoolid(_rconpoolid), sock(this->proxy()), timer(this->proxy()),  crtpushvecidx(0)
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
	incomingmsgvec[crtpushvecidx].push_back(IncomingMessageStub(_rmsgptr, _msg_type_idx, _flags));
	return incomingmsgvec[crtpushvecidx].size() == 1;
}
//-----------------------------------------------------------------------------
void Connection::doStop(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr){
	ConnectionContext conctx(service(_rctx), *this);
	
	service(_rctx).onConnectionClose(*this);
	//TODO: terminate all messages
	
	service(_rctx).onConnectionStop(conctx, _rerr);
	postStop(_rctx);
}
//-----------------------------------------------------------------------------
/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	if(EventCategory::isStart(_revent)){
		idbgx(Debug::ipc, this->id()<<" Session start: "<<sock.device().ok() ? " connected " : "not connected");
		{
			ConnectionContext conctx(service(_rctx), *this);
			service(_rctx).onIncomingConnectionStart(conctx);
		}
	}else if(EventCategory::isKill(_revent)){
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
	}else{
		size_t		vecidx = -1;
		{
			Locker<Mutex>	lock(service(_rctx).mutex(*this));
			
			idbgx(Debug::ipc, this->id()<<" Session message");
			
			if(incomingmsgvec[crtpushvecidx].size()){
				vecidx = crtpushvecidx;
				crtpushvecidx += 1;
				crtpushvecidx %= 2;
			}
		}
		doMoveIncommingMessagesToQueue(vecidx);
	}
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onRecv(frame::aio::ReactorContext &_rctx, size_t _sz){
	
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onSend(frame::aio::ReactorContext &_rctx){
	
}
//-----------------------------------------------------------------------------
/*static*/ void Connection::onConnect(frame::aio::ReactorContext &_rctx){
	Connection	&rthis = static_cast<Connection&>(_rctx.object());
	if(!_rctx.error()){
		idbgx(Debug::ipc, rthis.id());
		{
			ConnectionContext conctx(rthis.service(_rctx), rthis);
			rthis.service(_rctx).onOutgoingConnectionStart(conctx);
		}
	}else{
		idbgx(Debug::ipc, rthis.id()<<" error: "<<_rctx.error().message());
		rthis.doStop(_rctx, _rctx.error());
	}
}
//-----------------------------------------------------------------------------
void Connection::doMoveIncommingMessagesToQueue(const size_t _vecidx){
	if(_vecidx < 2){
		IncomingMessageVectorT &riv(incomingmsgvec[_vecidx]);
		
		for(auto it = riv.begin(); it != riv.end(); ++it){
			msgq.push(MessageStub(it->msgptr, it->msg_type_idx, it->flags));
		}
		riv.clear();
	}
}

//-----------------------------------------------------------------------------
SocketDevice const & ConnectionContext::device()const{
	return rconnection.device();
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
