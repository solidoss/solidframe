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

#include "frame/objectbase.hpp"
#include "frame/common.hpp"
#include "frame/forwardcompletion.hpp"


namespace solid{
namespace frame{

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

class Object: public Dynamic<Object, ObjectBase>, ForwardCompletionHandler{
protected:
	friend class CompletionHandler;
	friend class Reactor;
	
	//! Constructor
	Object();
	
	ObjectProxy proxy(){
		return ObjectProxy(*this);
	}
	
	bool registerCompletionHandler(CompletionHandler &_rch);
	
	void registerCompletionHandlers();
	
	bool isRunning()const;
	
	void postStop(ReactorContext &_rctx);
	
	template <class F>
	void post(ReactorContext &_rctx, F _f, Event const &_revent = Event()){
		EventFunctionT	evfn(_f);
		doPost(_rctx, evfn, _revent);
	}
private:
	virtual void onEvent(ReactorContext &_rctx, Event const &_revent);
	void doPost(ReactorContext &_rctx, EventFunctionT &_revfn, Event const &_revent);
};

}//namespace frame
}//namespace solid


#endif
