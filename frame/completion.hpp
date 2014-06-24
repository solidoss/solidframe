// frame/completion.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_COMPLETION_HPP
#define SOLID_FRAME_COMPLETION_HPP

#include "frame/common.hpp"
#include "system/error.hpp"
#include "system/cassert.hpp"

namespace solid{
namespace frame{

class ObjectBase;
class CompletionHandler;

struct ActionContext{
};

struct Action{
	typedef void (*CallbackT)(CompletionHandler *, ActionContext &);
	
	Action(CallbackT _pcbk):pcbk(_pcbk){}
	
	void call(CompletionHandler *_ph, ActionContext &_rctx){
		(*pcbk)(_ph, _rctx);
	}
	
	ERROR_NS::error_code	error;
	CallbackT				pcbk;
};

class CompletionHandler{
	static Action	dummy_init_action;
public:
	CompletionHandler(ObjectBase &_robj):robj(_robj), pact(&dummy_init_action){
		doRegister();
	}
private:
	
	void handleCompletion(ActionContext &_rctx){
		cassert(pact);
		pact->call(this, _rctx);
	}
	void doRegister();
private:
	static void doInitComplete(CompletionHandler *_ph, ActionContext &){
		_ph->pact = NULL;
	}
protected:
	ObjectBase				&robj;
	CompletionHandler		*pobjprev;
	CompletionHandler		*pobjnext;//double linked list within the object
	size_t					selidx;//index within selector
	Action					*pact;
};



}//namespace frame
}//namespace solid


#endif
