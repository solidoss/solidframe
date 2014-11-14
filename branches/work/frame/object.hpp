// frame/object.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_OBJECT_HPP
#define SOLID_FRAME_OBJECT_HPP

#include "system/timespec.hpp"

#include "frame/objectbase.hpp"
#include "frame/event.hpp"

#include "utility/dynamictype.hpp"
#include "utility/dynamicpointer.hpp"
#include "system/atomic.hpp"
#include "system/error.hpp"

namespace solid{
namespace frame{

class Reactor;
class CompletionHandler;

struct ExecuteContext{
	~ExecuteContext();
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
	
	Reactor& reactor(){
		return rreactor;
	}
	
	
	ExecuteContext(
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

class Object;

struct ObjectProxy{
	Object& object()const{
		return robj;
	}
private:
	friend class Object;
	ObjectProxy(Object &_robj): robj(_robj){}
	ObjectProxy(ObjectProxy const &_rd):robj(_rd.robj){}
	ObjectProxy& operator=(ObjectProxy const&_rd);
private:
	Object &robj;
};

class Object: public Dynamic<Object, ObjectBase>{
public:	
	static Object& specific(){
		return static_cast<Object&>(ObjectBase::specific());
	}
protected:
	//! Constructor
	Object();
	
	ObjectProxy proxy(){
		return ObjectProxy(*this);
	}
private:
	friend class CompletionHandler;
	virtual void execute(ExecuteContext &_rexectx) = 0;
	
	bool registerCompletionHandler(CompletionHandler &_rch);
	bool unregisterCompletionHandler(CompletionHandler &_rch);
	
	Reactor* reactor()const;
	
	bool isRunning()const;
	void enterRunning();
private:
	CompletionHandler	*pchfirst;//A linked list of completion handlers
};

}//namespace frame
}//namespace solid


#endif
