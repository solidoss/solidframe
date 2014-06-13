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

namespace solid{
namespace frame{

class ObjectBase;

struct ActionContext{
	error_code	error;
};

struct Action{
	bool		done;
	CbkPtrT		pcbk;
};

class CompletionHandler{
private:
	void handleCompletion(ActionContext &_rctx){
		cassert(pact);
		(*pact->pcbk)(this, _rctx);
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
