// frame/aio/src/aiolistener.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "frame/aio/aiolistener.hpp"
#include "frame/aio/aiocommon.hpp"
#include "frame/aio/aioreactor.hpp"


namespace solid{
namespace frame{
namespace aio{

/*static*/ void Listener::on_completion(CompletionHandler& _rch, ReactorContext &_rctx){
	Listener &rthis = static_cast<Listener&>(_rch);
	
	switch(rthis.reactorEvent(_rctx)){
		case ReactorEventRecv:
			if(!FUNCTION_EMPTY(rthis.f)){
				SocketDevice	sd;
				FunctionT		tmpf(std::move(rthis.f));
				rthis.doAccept(_rctx, sd);
				
				tmpf(_rctx, sd);
			}break;
		case ReactorEventError:
			if(!FUNCTION_EMPTY(rthis.f)){
				SocketDevice	sd;
				FunctionT		tmpf(std::move(rthis.f));
				tmpf(_rctx, sd);
			}break;
		case ReactorEventClear:
			FUNCTION_CLEAR(rthis.f);
			break;
		default:
			cassert(false);
	}
}

/*static*/ void Listener::on_posted_accept(ReactorContext &_rctx, Event const&){
	Listener		*pthis = static_cast<Listener*>(completion_handler(_rctx));
	Listener		&rthis = *pthis;
	SocketDevice	sd;
	if(!FUNCTION_EMPTY(rthis.f) && rthis.doTryAccept(_rctx, sd)){
		FunctionT	tmpf(std::move(rthis.f));
		tmpf(_rctx, sd);
	}
}

/*static*/ void Listener::on_init_completion(CompletionHandler& _rch, ReactorContext &_rctx){
	Listener		&rthis = static_cast<Listener&>(_rch);
	//rthis.completionCallback(on_dummy_completion);
	rthis.completionCallback(&on_completion);
	rthis.addDevice(_rctx, rthis.sd, ReactorWaitRead);
}

void Listener::doPostAccept(ReactorContext &_rctx){
	EventFunctionT	evfn(&Listener::on_posted_accept);
	reactor(_rctx).post(_rctx, evfn, Event(), *this);
}


bool Listener::doTryAccept(ReactorContext &_rctx, SocketDevice &_rsd){
	switch(sd.acceptNonBlocking(_rsd)){
		case AsyncError:
			systemError(_rctx, specific_error_back());
			//TODO: set proper error
			error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
		case AsyncSuccess:
			//make sure we're not receiving any io event
			return true;
		case AsyncWait:
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
