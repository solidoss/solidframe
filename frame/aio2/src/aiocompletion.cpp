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
#include "frame/aio2/aiocompletion.hpp"
#include "system/exception.hpp"

namespace solid{
namespace frame{
namespace aio{
//---------------------------------------------------------------------
//----	Completion	----
//---------------------------------------------------------------------
/*static*/ Action	CompletionHandler::dummy_init_action(&CompletionHandler::doInitComplete);

CompletionHandler::CompletionHandler(
	ObjectProxy const &_rop
):pobj(&_rop.object()), pprev(NULL), pnext(NULL), selidx(-1), pact(&dummy_init_action){
	
	pobj->registerCompletionHandler(*this);
	activate(_rop);
}

CompletionHandler::CompletionHandler():pobj(NULL), pprev(NULL), pnext(NULL), selidx(-1), pact(&dummy_init_action){
	
}


CompletionHandler::~CompletionHandler(){
	if(pobj){
		pobj->unregisterCompletionHandler(*this);
		if(isActive()){
			deactivate(pobj->proxy());
		}
	}
}

bool CompletionHandler::activate(ObjectProxy const &_rop){
	Reactor *preactor = NULL;
	if(!isActive() && pobj->isRunning() && (preactor = pobj->reactor())){
		//the object has entered the reactor
		preactor->registerCompletionHandler(*this);
	}
	return isActive();
}

void CompletionHandler::deactivate(ObjectProxy const &_rop){
	Reactor *preactor = NULL;
	if(isActive() && pobj->isRunning() && (preactor = pobj->reactor())){
		//the object has entered the reactor
		preactor->unregisterCompletionHandler(*this);
		selidx = -1;
	}
	if(isActive()){
		THROW_EXCEPTION("FATAL: CompletionHandler deleted/deactivated outside object's reactor!");
		std::terminate();
	}
}

/*static*/ void CompletionHandler::doInitComplete(CompletionHandler *_ph, ActionContext &){
	_ph->pact = NULL;
}

}//namespace aio
}//namespace frame
}//namespace solid
