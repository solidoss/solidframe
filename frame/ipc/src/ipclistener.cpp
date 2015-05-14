// frame/ipc/src/ipclistener.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "frame/aio/aioreactorcontext.hpp"
#include "ipclistener.hpp"
#include "frame/ipc/ipcservice.hpp"

namespace solid{
namespace frame{
namespace ipc{

inline Service& Listener::service(frame::aio::ReactorContext &_rctx){
	return static_cast<Service&>(_rctx.service());
}

/*virtual*/ void Listener::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	idbg("event = "<<_revent);
	if(EventCategory::isStart(_revent)){
		sock.postAccept(
			_rctx,
			[this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){onAccept(_rctx, _rsd);}
		);
	}else if(EventCategory::isKill(_revent)){
		postStop(_rctx);
	}
}

void Listener::onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){
	idbg("");
	unsigned	repeatcnt = 4;
	
	do{
		if(!_rctx.error()){
			service(_rctx).connectionReceive(_rsd);
		}else{
			//TODO:
			//e.g. a limit of open file descriptors was reached - we sleep for 10 seconds
			//timer.waitFor(_rctx, TimeSpec(10), std::bind(&Listener::onEvent, this, _1, frame::Event(EventStartE)));
			break;
		}
		--repeatcnt;
	}while(
		repeatcnt && 
		sock.accept(
			_rctx,
			[this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){onAccept(_rctx, _rsd);},
			_rsd
		)
	);
	
	if(!repeatcnt){
		sock.postAccept(
			_rctx,
			[this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){onAccept(_rctx, _rsd);}
		);//fully asynchronous call
	}
}

}//namespace ipc
}//namespace frame
}//namespace solid
