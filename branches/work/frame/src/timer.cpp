// frame/src/timer.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "frame/timer.hpp"
#include "frame/object.hpp"
#include "frame/reactor.hpp"

namespace solid{
namespace frame{



Timer::Timer(ObjectProxy const &_rop):CompletionHandler(_rop){}

Timer::~Timer(){}

void Timer::cancel(ExecuteContext &_rexectx){
	_rexectx.reactor().cancelCompletion(*this);
}

bool Timer::waitUntil(
	ExecuteContext &_rexectx,
	const TimeSpec &_rtimepos,
	size_t _event,
	size_t _index
){
	_rexectx.reactor();
	return false;
}

bool Timer::doneWait(ExecuteContext &_rexectx){
	if(_rexectx.time() >= ts){
		ts.set();
		return true;
	}
	return false;
}

}//namespace frame
}//namespace solid
