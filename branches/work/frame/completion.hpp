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

class Object;
struct ObjectProxy;
class CompletionHandler;
struct ExecuteContext;

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
	
	CompletionHandler(
		ObjectProxy const &_rid
	);
	
	CompletionHandler();
	
	~CompletionHandler();
	
	bool isActive()const{
		return  pobj != NULL && selidx != static_cast<size_t>(-1);
	}
	bool activate(ObjectProxy const &_rd);
	void deactivate(ObjectProxy const &_rd);
private:
	
	void handleCompletion(ActionContext &_rctx){
		cassert(pact);
		pact->call(this, _rctx);
	}
private:
	friend class Object;
	static void doInitComplete(CompletionHandler *_ph, ActionContext &){
		_ph->pact = NULL;
	}
private:
	Object					*pobj;
	CompletionHandler		*pprev;
	CompletionHandler		*pnext;//double linked list within the object
	size_t					selidx;//index within selector
	Action					*pact;
};



}//namespace frame
}//namespace solid


#endif
