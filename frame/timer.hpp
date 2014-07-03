// frame/timer.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_TIMER_HPP
#define SOLID_FRAME_TIMER_HPP

#include "system/timespec.hpp"
#include "frame/common.hpp"
#include "frame/completion.hpp"

namespace solid{

class TimeSpec;

namespace frame{
class	Object;
struct	ExecuteContext;

class Timer: public CompletionHandler{
public:
	Timer(Object &_robj);
	~Timer();
	void cancel(ExecuteContext &_rexectx);
	bool waitUntil(
		ExecuteContext &_rexectx,
		const TimeSpec &_rtimepos,
		size_t _event,
		size_t _index = 0
	);

};

}//namespace frame
}//namespace solid

#endif
