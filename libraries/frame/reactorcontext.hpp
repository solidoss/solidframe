// frame/aio/reactorcontext.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_REACTORCONTEXT_HPP
#define SOLID_FRAME_REACTORCONTEXT_HPP

#include "system/common.hpp"
#include "system/error.hpp"
#include "system/socketdevice.hpp"
#include "system/nanotime.hpp"

#include "frame/aio/aiocommon.hpp"

namespace solid{
namespace frame{

class Service;
class Object;
class Reactor;
class CompletionHandler;

struct ReactorContext{
	~ReactorContext(){
		
	}
	
	const NanoTime& time()const{
		return rcrttm;
	}
	
	ERROR_NS::error_code const& systemError()const{
		return syserr;
	}
	
	ErrorConditionT const& error()const{
		return err;
	}
	
	Object& object()const;
	Service& service()const;
	
	UniqueId objectUid()const;
	
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
	
	
	void error(ErrorConditionT const& _err){
		err = _err;
	}
	
	void systemError(ERROR_NS::error_code const& _err){
		syserr = _err;
	}
	
	ReactorContext(
		Reactor	&_rreactor,
		const NanoTime &_rcrttm
	):	rreactor(_rreactor),
		rcrttm(_rcrttm), chnidx(-1), objidx(-1), reactevn(ReactorEventNone){}
	
	Reactor						&rreactor;
	const NanoTime				&rcrttm;
	size_t						chnidx;
	size_t						objidx;
	ReactorEventsE				reactevn;
	ERROR_NS::error_code		syserr;
	ErrorConditionT				err;
};

}//namespace frame
}//namespace solid


#endif
