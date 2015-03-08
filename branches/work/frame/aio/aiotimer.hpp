// frame/aio/aiotimer.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_TIMER_HPP
#define SOLID_FRAME_AIO_TIMER_HPP

#include "system/common.hpp"
#include "system/socketdevice.hpp"

#include "aiocompletion.hpp"
#include "aioreactorcontext.hpp"

namespace solid{
namespace frame{
namespace aio{

struct	ObjectProxy;
struct	ReactorContext;

class Timer: public CompletionHandler{
	static void on_completion(CompletionHandler& _rch, ReactorContext &_rctx);
	static void on_init_completion(CompletionHandler& _rch, ReactorContext &_rctx);
	static void on_posted_accept(ReactorContext &_rctx, Event const&);
public:
	Listener(
		ObjectProxy const &_robj
	):CompletionHandler(_robj, Timer::on_init_completion)
	{
	}
	
	//Returns false when the operation is scheduled for completion. On completion _f(...) will be called.
	//Returns true when operation could not be scheduled for completion - e.g. operation already in progress.
	template <typename F>
	bool waitFor(ReactorContext &_rctx, TimeSpec const& _tm, F _f){
		if(FUNCTION_EMPTY(f)){
			f = _f;
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
	bool waitUntil(ReactorContext &_rctx, TimeSpec const& _tm, F _f){
		if(FUNCTION_EMPTY(f)){
			f = _f;
			return false;
		}else{
			//TODO: set proper error
			error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			return true;
		}
	}
private:
private:
	typedef FUNCTION<void(ReactorContext&)>		FunctionT;
	
	FunctionT				f;
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
