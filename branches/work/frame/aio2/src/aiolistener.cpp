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


namespace solid{
namespace frame{
namespace aio{

/*static*/ void Listener::on_completion(ReactorContext &_rctx){
	Listener *pthis = static_cast<Listener*>(_rctx.completionHandler());
	if(_rctx.reactorEvent() == ReactorEventRecv){
		cassert(!pthis->f.empty());
		SocketDevice sd;
		pthis->doAccept(_rctx, sd);
		FunctionT	tmpf(std::move(this->f));
		tmpf(_rctx, sd);
	}else if(_rctx.reactorEvent() == ReactorEventError){
		SocketDevice	sd;
		FunctionT		tmpf(std::move(this->f));
		tmpf(_rctx, sd);
	}else if(_rctx.reactorEvent() == ReactorEventClear){
		f.clear();
	}
}

/*static*/ void Listener::on_posted_accept(ReactorContext &_rctx){
	Listener *pthis = static_cast<Listener*>(_rctx.completionHandler());
	SocketDevice sd;
	if(pthis->doTryAccept(_rctx, sd)){
		FunctionT	tmpf(std::move(this->f));
		tmpf(_rctx, sd);
	}
}

void Listener::doPostAccept(ReactorContext &_rctx){
	//The post queue will keep [function, object_uid, completion_handler_uid, Event]
	
	_rctx.reactor().post(_rctx, &Listener::on_posted_accept, this);
}


bool Listener::doTryAccept(ReactorContext &_rctx, SocketDevice &_rsd){
	switch(sd.acceptNonBlocking(_rsd)){
		case AsyncError:
			_rctx.systemError(specific_error_back());
			//TODO: set proper error
			_rctx.error(ERROR_NS::error_condition(-1, _rctx.error().category()));
		case AsyncSuccess:
			return true;
		case AsyncWait:
			_rctx.reactor().wait(_rctx, this, ReactorWaitRead);
			break;
	}
	return false;
}

void Listener::doAccept(ReactorContext &_rctx, SocketDevice &_rsd){
	if(sd.accept(_rsd)){
	}else{
		_rctx.systemError(specific_error_back());
		//TODO: set proper error
		_rctx.error(ERROR_NS::error_condition(-1, _rctx.error().category()));
	}
}

}//namespace aio
}//namespace frame
}//namespace solid
