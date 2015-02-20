// frame/aio2/src/aioobject.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "system/cassert.hpp"
#include "system/mutex.hpp"
#include "system/thread.hpp"
#include "system/debug.hpp"

#include "frame/service.hpp"
#include "frame/aio2/aioobject.hpp"
#include "frame/aio2/aioreactor.hpp"
#include "frame/aio2/aiocompletion.hpp"

#include "utility/memory.hpp"
#include "utility/dynamicpointer.hpp"

namespace solid{
namespace frame{
namespace aio{
//---------------------------------------------------------------------
//----	Object	----
//---------------------------------------------------------------------

Object::Object():pchfirst(nullptr){}

/*virtual*/ void Object::onEvent(ReactorContext &_rctx, Event const &_revent){
	
}

void Object::postStop(ReactorContext &_rctx){
	
}

bool Object::isRunning()const{
	return runId().isValid();
}

bool Object::registerCompletionHandler(CompletionHandler &_rch){
	CompletionHandler *poldfirst = pchfirst;
	pchfirst = &_rch;
	pchfirst->pprev = poldfirst;
	pchfirst->pnext = NULL;
	if(poldfirst){
		poldfirst->pnext = pchfirst;
	}
	return isRunning();
}

bool Object::unregisterCompletionHandler(CompletionHandler &_rch){
	if(_rch.pprev){
		_rch.pprev->pnext = _rch.pnext;
	}
	if(_rch.pnext){
		_rch.pnext->pprev = _rch.pprev;
	}
	if(&_rch == pchfirst){
		pchfirst = _rch.pprev;
	}

	_rch.pprev = NULL;
	_rch.pnext = NULL;
	return isRunning();
}

}//namespace aio
}//namespace frame
}//namespace solid

