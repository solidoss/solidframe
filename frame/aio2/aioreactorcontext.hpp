// frame/aio/aioreactorcontext.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_REACTORCONTEXT_HPP
#define SOLID_FRAME_AIO_REACTORCONTEXT_HPP

#include "system/common.hpp"
#include "system/error.hpp"
#include "system/socketdevice.hpp"
#include "system/timespec.hpp"
#include "frame/event.hpp"

namespace solid{
namespace frame{
namespace aio{

class Reactor;
struct ReactorContext;

template <typename F>
void post(ReactorContext &_rctx, F _f);

struct ReactorContext{
	~ReactorContext();
	Event const& event()const{
		return evn;
	}
	const TimeSpec& time()const{
		return rcrttm;
	}
	void reschedule(Event const &_revn);
	void die(){
		
	}
	
	ERROR_NS::error_code const& systemError()const{
		return syserr;
	}
	
	ERROR_NS::error_condition const& error()const{
		return err;
	}
protected:
	friend class CompletionHandler;
	
	template <typename F>
	friend void post(ReactorContext &_rctx, F _f);

	
	Reactor& reactor(){
		return rreactor;
	}
	
	
	ReactorContext(
		Reactor	&_rreactor,
		const Event &_evn,
		const TimeSpec &_rcrttm
	):	rreactor(_rreactor),
		evn(_evn), rcrttm(_rcrttm){}
	
	Reactor						&rreactor;
	Event						evn;
	const TimeSpec				&rcrttm;
	ERROR_NS::error_code		syserr;
	ERROR_NS::error_condition	err;
};

template <typename F>
void post(ReactorContext &_rctx, F _f){
	_rctx.reactor().post(_rctx, _f, /*CompletionHandler**/NULL);
}

}//namespace aio
}//namespace frame
}//namespace solid


#endif
