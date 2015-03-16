// frame/object.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "system/cassert.hpp"

#include "frame/object.hpp"
#include "frame/reactor.hpp"
#include "frame/completion.hpp"
#include "frame/reactorcontext.hpp"
#include "frame/manager.hpp"

namespace solid{
namespace frame{
//---------------------------------------------------------------------
//----	Object	----
//---------------------------------------------------------------------

Object::Object(){}

/*virtual*/ void Object::onEvent(ReactorContext &_rctx, Event const &_revent){
	
}

void Object::postStop(ReactorContext &_rctx){
	CompletionHandler *pch = this->pnext;
	
	while(pch != nullptr){
		pch->pprev = nullptr;//unregister
		pch->deactivate();
		
		pch = pch->pnext;
	}
	this->pnext = nullptr;
	_rctx.reactor().postObjectStop(_rctx);
}

bool Object::isRunning()const{
	return runId().isValid();
}

bool Object::registerCompletionHandler(CompletionHandler &_rch){
	_rch.pnext = this->pnext;
	if(_rch.pnext){
		_rch.pnext->pprev = &_rch;
	}
	this->pnext = &_rch;
	_rch.pprev = this;
	return isRunning();
}

void Object::registerCompletionHandlers(){
	CompletionHandler *pch = this->pnext;
	
	while(pch != nullptr){
		pch->activate(*this);
		pch = pch->pnext;
	}
}

void Object::doPost(ReactorContext &_rctx, EventFunctionT &_revfn, Event const &_revent){
	_rctx.reactor().post(_rctx, _revfn, _revent);
}

//---------------------------------------------------------------------
//----  ObjectBase      ----
//---------------------------------------------------------------------

ObjectBase::ObjectBase():
	fullid(-1), smask(0){
}

void ObjectBase::unregister(Manager &_rm){
	if(isRegistered()){
		_rm.unregisterObject(*this);
		fullid = -1;
	}
}

ObjectBase::~ObjectBase(){
}

/*virtual*/void ObjectBase::doStop(Manager &_rm){
}
void ObjectBase::stop(Manager &_rm){
	doStop(_rm);
	unregister(_rm);
}


}//namespace frame
}//namespace solid

