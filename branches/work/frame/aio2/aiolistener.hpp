// frame/aio/aiolistener.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_LISTENER_HPP
#define SOLID_FRAME_AIO_LISTENER_HPP

#include "boost/function.hpp"
#include "system/common.hpp"
#include "system/socketdevice.hpp"

#include "aioreactorcontext.hpp"

namespace solid{
namespace frame{
namespace aio{

struct	ObjectProxy;
struct	ReactorContext;

template <class F>
struct AcceptCommand{
	F	f;
	AcceptCommand(F	_f):f(_f){}
	
	void operator()(ReactorContext &_rctx){
		
	}
};

class Listener{
static void react_cbk(CompletionHandler *_ph, ReactorContext &_rctx);
public:
	Listener(ObjectProxy &_robj, SocketDevice &_rsd){
	}
	Listener(ObjectProxy &_robj){}
	
	//Returns false when the operation is scheduled for completion. On completion _f(...) will be called.
	//Returns true when operation could not be scheduled for completion - e.g. operation already in progress.
	template <typename F>
	bool postAccept(ReactorContext &_rctx, F _f){
		if(f.empty()){
			f = _f;
			doPostAccept(_rctx);
		}else{
			_rctx.error(-1, _rctx.error().category());
			return true;
		}
	}
	
	//Returns true when the operation completed. Check _rctx.error() for success or fail
	//Returns false when operation is scheduled for completion. On completion _f(...) will be called.
	template <typename F>
	bool accept(ReactorContext &_rctx, F _f, SocketDevice &_rsd){
		if(f.empty()){
			if(this->tryAccept(_rctx, _rsd)){
				return true;
			}
			f = _f;
			return false;
		}else{
			_rctx.error(-1, _rctx.error().category());
			return true;
		}
	}
private:
	void doPostAccept(ReactorContext &_rctx);
private:
	typedef boost::function<void(ReactorContext&, SocketDevice&)>		FunctionT;
	FunctionT		f;
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
