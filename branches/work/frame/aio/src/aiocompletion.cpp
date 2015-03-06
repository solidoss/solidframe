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
#include "frame/aio/aioobject.hpp"
#include "frame/aio/aiocompletion.hpp"
#include "frame/aio/aioreactorcontext.hpp"
#include "frame/aio/aioreactor.hpp"

#include "system/exception.hpp"


namespace solid{
namespace frame{
namespace aio{
//---------------------------------------------------------------------
//----	Completion	----
//---------------------------------------------------------------------


CompletionHandler::CompletionHandler(
	ObjectProxy const &_rop,
	CallbackT _pcall/* = &on_init_completion*/
):pprev(nullptr), idxreactor(-1), call(_pcall)
{
	if(_rop.object().registerCompletionHandler(*this)){
		this->activate(_rop.object());
	}
}

CompletionHandler::CompletionHandler(
	CallbackT _pcall/* = &on_init_completion*/
):pprev(nullptr), idxreactor(-1), call(_pcall){
	
}


CompletionHandler::~CompletionHandler(){
	unregister();
	deactivate();
}

ReactorEventsE CompletionHandler::reactorEvent(ReactorContext &_rctx)const{
	return _rctx.reactorEvent();
}

/*static*/ CompletionHandler* CompletionHandler::completion_handler(ReactorContext &_rctx){
	return _rctx.completionHandler();
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

bool CompletionHandler::activate(Object const &_robj){
	Reactor *preactor = nullptr;
	if(!isActive() && (preactor = Reactor::safeSpecific())){
		preactor->registerCompletionHandler(*this, _robj);
	}
	return isActive();
}


void CompletionHandler::unregister(){
	this->pprev->pnext = this->pnext;
	this->pprev = this->pnext = nullptr;
}

void CompletionHandler::deactivate(){
	Reactor *preactor = nullptr;
	if(isActive() && (preactor = Reactor::safeSpecific())){
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
	_rch.call = nullptr;
}

void CompletionHandler::addDevice(ReactorContext &_rctx, Device const &_rsd, const ReactorWaitRequestsE _req){
	_rctx.reactor().addDevice(_rctx, _rsd, _req);
}


}//namespace aio
}//namespace frame
}//namespace solid
