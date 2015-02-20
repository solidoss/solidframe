// frame/src/aiocompletion.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "system/cassert.hpp"
#include "frame/aio2/aioobject.hpp"
#include "frame/aio2/aiocompletion.hpp"
#include "frame/aio2/aioreactorcontext.hpp"
#include "system/exception.hpp"
#include "frame/aio2/aioreactor.hpp"

namespace solid{
namespace frame{
namespace aio{
//---------------------------------------------------------------------
//----	Completion	----
//---------------------------------------------------------------------


CompletionHandler::CompletionHandler(
	ObjectProxy const &_rop,
	CallbackT _pcall/* = &on_init_completion*/
):pobj(&_rop.object()), pprev(NULL), pnext(NULL), idxreactor(-1), call(_pcall)
{
	pobj->registerCompletionHandler(*this);
}

CompletionHandler::CompletionHandler(
	CallbackT _pcall/* = &on_init_completion*/
):pobj(NULL), pprev(NULL), pnext(NULL), idxreactor(-1), call(_pcall){
	
}


CompletionHandler::~CompletionHandler(){
	if(pobj){
		pobj->unregisterCompletionHandler(*this);
		deactivate();
	}
}

ReactorEventsE CompletionHandler::reactorEvent(ReactorContext &_rctx)const{
	return _rctx.reactorEvent();
}

Reactor& CompletionHandler::reactor(ReactorContext &_rctx)const{
	return _rctx.reactor();
}

void CompletionHandler::error(ReactorContext &_rctx, ERROR_NS::error_condition const& _err)const{
	_rctx.error(_err);
}

void CompletionHandler::systemError(ReactorContext &_rctx, ERROR_NS::error_code const& _err)const{
	_rctx.systemError(_err);
}

bool CompletionHandler::activate(ReactorContext &_rctx){
	if(!isActive() && pobj->isRunning()){
		//the object has entered the reactor
		_rctx.reactor().registerCompletionHandler(*this);
	}
	return isActive();
}

void CompletionHandler::deactivate(){
	Reactor *preactor = NULL;
	if(isActive() && pobj->isRunning() && (preactor = Reactor::safeSpecific())){
		//the object has entered the reactor
		preactor->unregisterCompletionHandler(*this);
		idxreactor = -1;
	}
	if(isActive()){
		THROW_EXCEPTION("FATAL: CompletionHandler deleted/deactivated outside object's reactor!");
		std::terminate();
	}
}

/*static*/ void CompletionHandler::on_init_completion(CompletionHandler& _rch, ReactorContext &_rctx){
	_rch.call = NULL;
}

}//namespace aio
}//namespace frame
}//namespace solid
