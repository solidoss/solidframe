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
#include "frame/aio/aiocommon.hpp"

namespace solid{
namespace frame{

class Service;

namespace aio{

class Object;
class Reactor;
struct ReactorContext;
class CompletionHandler;

struct ReactorContext{
	~ReactorContext(){
		
	}
	
	const TimeSpec& time()const{
		return rcrttm;
	}
	
	ERROR_NS::error_code const& systemError()const{
		return syserr;
	}
	
	ERROR_NS::error_condition const& error()const{
		return err;
	}
	
	Object& object()const;
	Service& service()const;
	
	UidT objectUid()const;
	
	void clearError(){
		err.clear();
		syserr.clear();
	}
private:
	friend class CompletionHandler;
	friend class Reactor;
	friend class Object;
	
	Reactor& reactor(){
		return rreactor;
	}
	
	Reactor const& reactor()const{
		return rreactor;
	}
	ReactorEventsE reactorEvent()const{
		return reactevn;
	}
	CompletionHandler* completionHandler()const;
	
	
	void error(ERROR_NS::error_condition const& _err){
		err = _err;
	}
	
	void systemError(ERROR_NS::error_code const& _err){
		syserr = _err;
	}
	
	ReactorContext(
		Reactor	&_rreactor,
		const TimeSpec &_rcrttm
	):	rreactor(_rreactor),
		rcrttm(_rcrttm), chnidx(-1), objidx(-1), reactevn(ReactorEventNone){}
	
	Reactor						&rreactor;
	const TimeSpec				&rcrttm;
	size_t						chnidx;
	size_t						objidx;
	ReactorEventsE				reactevn;
	ERROR_NS::error_code		syserr;
	ERROR_NS::error_condition	err;
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
