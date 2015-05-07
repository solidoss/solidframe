// frame/ipc/src/ipcsession.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/manager.hpp"
#include "ipcsession.hpp"
#include "frame/ipc/ipcservice.hpp"
#include "frame/aio/aioreactorcontext.hpp"
#include "frame/aio/openssl/aiosecuresocket.hpp"

namespace solid{
namespace frame{
namespace ipc{


Session::Session(
	const size_t _idx,
	const SocketDevice &_rsd
): idx(_idx), crtpushvecidx(0){}

Session::Session(
	const size_t _idx
): idx(_idx), crtpushvecidx(0){
	
}

Session::~Session(){
	
}

inline Service& Session::service(frame::aio::ReactorContext &_rctx){
	return static_cast<Service&>(_rctx.service());
}

bool Session::pushMessage(
	MessagePointerT &_rmsgptr,
	const ConnectionUid	&_rconuid_in,
	ulong _flags
){
	idbgx(Debug::ipc, idx<<" crtpushvecidx = "<<crtpushvecidx<<" conidx = "<<_rconuid_in.conidx<<" conuid = "<<_rconuid_in.conuid<<" flags = "<<_flags<<" msgptr = "<<_rmsgptr.get());
	//We're under lock
	incommingmsgvec[crtpushvecidx].push_back(IncommingMessageStub(_rmsgptr, UidT(_rconuid_in.conidx, _rconuid_in.conuid), _flags));
	return incommingmsgvec[crtpushvecidx].size() == 1;
}

/*virtual*/ void Session::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	if(service(_rctx).isEventStart(_revent)){
		idbgx(Debug::ipc, idx<<" Session start");
	}else if(service(_rctx).isEventStop(_revent)){
		idbgx(Debug::ipc, idx<<" Session postStop");
		postStop(_rctx);
	}else if(_revent.msgptr.get()){
		ResolveMessage *presolvemsg = ResolveMessage::cast(_revent.msgptr.get());
		if(presolvemsg){
			idbgx(Debug::ipc, idx<<" Session receive resolve event message of size: "<<presolvemsg->addrvec.size());
		}
	}else{
		size_t		vecidx = -1;
		{
			Locker<Mutex>	lock(service(_rctx).mutex(*this));
			
			idbgx(Debug::ipc, idx<<" Session message");
			if(incommingmsgvec[crtpushvecidx].size()){
				vecidx = crtpushvecidx;
				crtpushvecidx += 1;
				crtpushvecidx %= 2;
			}
		}
		if(vecidx < 2){
			IncommingMessageVectorT &riv(incommingmsgvec[vecidx]);
			
			for(auto it = riv.begin(); it != riv.end(); ++it){
				if(it->conuid.index == 0xffff){
					msgq.push(MessageStub(it->msgptr, it->flags));
				}else if(it->conuid.index < convec.size() && convec[it->conuid.index].unique == it->conuid.unique){
					convec[it->conuid.index].msgq.push(MessageStub(it->msgptr, it->flags));
				}else{
					//TODO: terminate message calling the callback with appropriate error
				}
			}
		}
	}
}

/*static*/ void Session::onRecv(frame::aio::ReactorContext &_rctx, size_t _sz){
	
}
/*static*/ void Session::onSend(frame::aio::ReactorContext &_rctx){
	
}

}//namespace ipc
}//namespace frame
}//namespace solid
