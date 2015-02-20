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

#include "frame/aio2/aiocommon.hpp"
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
	static void on_init_completion(CompletionHandler&, ReactorContext &);
protected:
	typedef void (*CallbackT)(CompletionHandler&, ReactorContext &);
public:
	CompletionHandler(
		ObjectProxy const &_rop,
		CallbackT _pcall = &on_init_completion
	);
	
	~CompletionHandler();
	
	bool isActive()const{
		return  pobj != NULL && idxreactor != static_cast<size_t>(-1);
	}
	bool activate(ReactorContext &_rctx);
	void deactivate();
protected:
	CompletionHandler(CallbackT _pcall = &on_init_completion);
	
	void completionCallback(CallbackT *_pcbk);
	ReactorEventsE reactorEvent(ReactorContext &_rctx)const;
	Reactor& reactor(ReactorContext &_rctx)const;
	void error(ReactorContext &_rctx, ERROR_NS::error_condition const& _err)const;
	void systemError(ReactorContext &_rctx, ERROR_NS::error_code const& _err)const;
private:
	friend class Reactor;
	
	void handleCompletion(ReactorContext &_rctx){
		(*call)(*this, _rctx);
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
