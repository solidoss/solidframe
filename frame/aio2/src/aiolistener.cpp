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


namespace solid{
namespace frame{
namespace aio{

/*static*/ void Listener::react_cbk(CompletionHandler *_ph, ReactorContext &_rctx){
	Listener *pthis = static_cast<Listener*>(_ph);
	if(_ev == EventRecvE){
		cassert(!pthis->f.empty());
		SocketDevice sd;
		if(pthis->tryAccept(_rctx, sd)){
			pthis->f(_rctx, sd);
		}
	}else if(_ev == EventErrorE){
		
	}else if(_ev == EventClearE){
		f.clear();
	}
}


void Listener::doPostAccept(ReactorContext &_rctx){
	_rctx.post(
}

}//namespace aio
}//namespace frame
}//namespace solid
