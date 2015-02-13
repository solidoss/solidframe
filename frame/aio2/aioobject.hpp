// frame/aio/aioobject.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_OBJECT_HPP
#define SOLID_FRAME_AIO_OBJECT_HPP

#include "system/timespec.hpp"

#include "frame/objectbase.hpp"
#include "frame/aio2/aiocommon.hpp"

namespace solid{

namespace frame{
namespace aio{

class Message;
class Reactor;
struct ReactorContext;
class CompletionHandler;

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
	typedef DynamicPointer<Message>	MessagePointerT;
	
// 	static Object& specific(){
// 		return static_cast<ThisT&>(ObjectBase::specific());
// 	}
protected:
	friend class CompletionHandler;
	
	//! Constructor
	Object();
	
	ObjectProxy proxy(){
		return ObjectProxy(*this);
	}
	
	bool registerCompletionHandler(CompletionHandler &_rch);
	bool unregisterCompletionHandler(CompletionHandler &_rch);
	
	bool isRunning()const;
	void enterRunning();
	
	Reactor* safeSpecificReactor()const;
	
	void postStop(ReactorContext &_rctx);
	
	template <class F>
	void post(ReactorContext &_rctx, F _f, Event const &_revent = Event()){
		
	}
	
private:
	virtual void onEvent(ReactorContext &_rctx, Event const &_revent);
private:
	CompletionHandler	*pchfirst;//A linked list of completion handlers
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
