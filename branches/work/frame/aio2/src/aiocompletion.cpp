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

CompletionHandler::CompletionHandler():pobj(NULL), pprev(NULL), pnext(NULL), idxreactor(-1), call(CompletionHandler::on_init_completion){
	
}


CompletionHandler::~CompletionHandler(){
	if(pobj){
		pobj->unregisterCompletionHandler(*this);
		deactivate();
	}
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
	if(isActive() && pobj->isRunning() && (preactor = pobj->safeSpecificReactor())){
		//the object has entered the reactor
		preactor->unregisterCompletionHandler(*this);
		idxreactor = -1;
	}
	if(isActive()){
		THROW_EXCEPTION("FATAL: CompletionHandler deleted/deactivated outside object's reactor!");
		std::terminate();
	}
}

/*static*/ void CompletionHandler::on_init_completion(ReactorContext &_rctx){
	_rctx.completionHandler()->call = NULL;
}

}//namespace aio
}//namespace frame
}//namespace solid
