// frame/aio/src/aiolistener.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "frame/aio2/aiolistener.hpp"
#include "frame/aio2/aiocommon.hpp"
#include "frame/aio2/aioreactor.hpp"


namespace solid{
namespace frame{
namespace aio{

/*static*/ void Listener::on_completion(CompletionHandler& _rch, ReactorContext &_rctx){
	Listener &rthis = static_cast<Listener&>(_rch);
	if(rthis.reactorEvent(_rctx) == ReactorEventRecv){
		cassert(!rthis.f.empty());
		SocketDevice sd;
		rthis.doAccept(_rctx, sd);
		FunctionT	tmpf(std::move(rthis.f));
		tmpf(_rctx, sd);
	}else if(rthis.reactorEvent(_rctx) == ReactorEventError){
		SocketDevice	sd;
		FunctionT		tmpf(std::move(rthis.f));
		tmpf(_rctx, sd);
	}else if(rthis.reactorEvent(_rctx) == ReactorEventClear){
		rthis.f.clear();
	}
}

/*static*/ void Listener::on_posted_accept(CompletionHandler& _rch, ReactorContext &_rctx){
	Listener		&rthis = static_cast<Listener&>(_rch);
	SocketDevice	sd;
	if(rthis.doTryAccept(_rctx, sd)){
		FunctionT	tmpf(std::move(rthis.f));
		tmpf(_rctx, sd);
	}
}

/*static*/ void Listener::on_init_completion(CompletionHandler& _rch, ReactorContext &_rctx){
	
}

void Listener::doPostAccept(ReactorContext &_rctx){
	//The post queue will keep [function, object_uid, completion_handler_uid, Event]
	
	reactor(_rctx).post(_rctx, &Listener::on_posted_accept, Event(), this);
}


bool Listener::doTryAccept(ReactorContext &_rctx, SocketDevice &_rsd){
	switch(sd.acceptNonBlocking(_rsd)){
		case AsyncError:
			systemError(_rctx, specific_error_back());
			//TODO: set proper error
			error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
		case AsyncSuccess:
			return true;
		case AsyncWait:
			reactor(_rctx).wait(_rctx, this, ReactorWaitRead);
			break;
	}
	return false;
}

void Listener::doAccept(ReactorContext &_rctx, SocketDevice &_rsd){
	if(sd.accept(_rsd)){
	}else{
		systemError(_rctx, specific_error_back());
		//TODO: set proper error
		error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
	}
}

}//namespace aio
}//namespace frame
}//namespace solid
