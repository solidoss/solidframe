// frame/aio/aiocompletion.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_COMPLETION_HPP
#define SOLID_FRAME_AIO_COMPLETION_HPP

#include "frame/aio/aiocommon.hpp"
#include "system/error.hpp"
#include "system/cassert.hpp"

namespace solid{
namespace frame{
namespace aio{

class Object;
class Reactor;
struct ObjectProxy;
struct ReactorContext;
struct ReactorEvent;

class CompletionHandler{
	static void on_init_completion(ReactorContext &);
protected:
	typedef void (*CallbackT)(ReactorContext &);
public:
	CompletionHandler(
		ObjectProxy const &_rop,
		CallbackT _pcall = &on_init_completion
	);
	
	CompletionHandler();
	
	~CompletionHandler();
	
	bool isActive()const{
		return  pobj != NULL && idxreactor != static_cast<size_t>(-1);
	}
	bool activate(ReactorContext &_rctx);
	void deactivate();
protected:
	void completionCallback(CallbackT *_pcbk);
private:
	friend class Reactor;
	void handleCompletion(ReactorContext &_rctx){
		(*call)(_rctx);
	}
private:
	friend class Object;
private:
	Object					*pobj;
	CompletionHandler		*pprev;
	CompletionHandler		*pnext;//double linked list within the object
	size_t					idxreactor;//index within reactor
	CallbackT				call;
};


}//namespace aio
}//namespace frame
}//namespace solid


#endif
