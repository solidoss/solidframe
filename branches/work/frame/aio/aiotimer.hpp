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
	typedef Timer	ThisT;
	
	static void on_completion(CompletionHandler& _rch, ReactorContext &_rctx){
		ThisT &rthis = static_cast<ThisT&>(_rch);
		
		switch(rthis.reactorEvent(_rctx)){
			case ReactorEventTimer:
				rthis.doExec(_rctx);
				break;
			case ReactorEventClear:
				rthis.doClear();
				break;
			default:
				cassert(false);
		}
	}
	static void on_init_completion(CompletionHandler& _rch, ReactorContext &_rctx){
		ThisT &rthis = static_cast<ThisT&>(_rch);
		rthis.completionCallback(Timer::on_completion);
	}
public:
	Timer(
		ObjectProxy const &_robj
	):CompletionHandler(_robj, Timer::on_init_completion), storeidx(-1)
	{
	}
	
	//Returns false when the operation is scheduled for completion. On completion _f(...) will be called.
	//Returns true when operation could not be scheduled for completion - e.g. operation already in progress.
	template <typename F>
	bool waitFor(ReactorContext &_rctx, TimeSpec const& _tm, F _f){
		TimeSpec t = _rctx.time();
		t += _tm;
		return waitUntil(_rctx, _tm, _f);
	}
	
	//Returns true when the operation completed. Check _rctx.error() for success or fail
	//Returns false when operation is scheduled for completion. On completion _f(...) will be called.
	template <typename F>
	bool waitUntil(ReactorContext &_rctx, TimeSpec const& _tm, F _f){
		if(FUNCTION_EMPTY(f)){
			f = _f;
			this->addTimer(_rctx, _tm, storeidx);
			return false;
		}else{
			//TODO: set proper error
			error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			return true;
		}
	}
	void cancel(ReactorContext &_rctx){
		this->remTimer(_rctx, storeidx);
		doClear();
	}
private:
	friend class Reactor;
	void doExec(ReactorContext &_rctx){
		FunctionT	tmpf(std::move(f));
		storeidx = -1;
		tmpf(_rctx);
	}
	void doClear(){
		FUNCTION_CLEAR(f);
		storeidx = -1;
	}
private:
	typedef FUNCTION<void(ReactorContext&)>		FunctionT;
	
	FunctionT				f;
	size_t					storeidx;
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
