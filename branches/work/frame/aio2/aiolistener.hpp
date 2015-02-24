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

#include "aiocompletion.hpp"
#include "aioreactorcontext.hpp"

namespace solid{
namespace frame{
namespace aio{

struct	ObjectProxy;
struct	ReactorContext;

class Listener: public CompletionHandler{
static void on_completion(CompletionHandler& _rch, ReactorContext &_rctx);
static void on_dummy_completion(CompletionHandler& _rch, ReactorContext &_rctx);
static void on_posted_accept(ReactorContext &_rctx, Event const&);
static void on_init_completion(CompletionHandler& _rch, ReactorContext &_rctx);
public:
	Listener(
		ObjectProxy const &_robj,
		SocketDevice &_rsd
	):CompletionHandler(_robj, Listener::on_init_completion), sd(_rsd), req(ReactorWaitNone)
	{
		if(sd.ok()){
			sd.makeNonBlocking();
		}
	}
	
	const SocketDevice& device()const;
	
	//Returns false when the operation is scheduled for completion. On completion _f(...) will be called.
	//Returns true when operation could not be scheduled for completion - e.g. operation already in progress.
	template <typename F>
	bool postAccept(ReactorContext &_rctx, F _f){
		if(f.empty()){
			f = _f;
			doPostAccept(_rctx);
			return false;
		}else{
			//TODO: set proper error
			error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			return true;
		}
	}
	
	//Returns true when the operation completed. Check _rctx.error() for success or fail
	//Returns false when operation is scheduled for completion. On completion _f(...) will be called.
	template <typename F>
	bool accept(ReactorContext &_rctx, F _f, SocketDevice &_rsd){
		if(f.empty()){
			if(this->doTryAccept(_rctx, _rsd)){
				return true;
			}
			f = _f;
			return false;
		}else{
			//TODO: set proper error
			error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			return true;
		}
	}
private:
	void doPostAccept(ReactorContext &_rctx);
	bool doTryAccept(ReactorContext &_rctx, SocketDevice &_rsd);
	void doAccept(solid::frame::aio::ReactorContext& _rctx, solid::SocketDevice& _rsd);
private:
	typedef boost::function<void(ReactorContext&, SocketDevice&)>		FunctionT;
	FunctionT				f;
	SocketDevice			sd;
	ReactorWaitRequestsE	req;
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
