// frame/basicscheduler.hpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_BASICSCHEDULER_HPP
#define SOLID_FRAME_BASICSCHEDULER_HPP

#include "frame/schedulerbase.hpp"
#include "utility/dynamicpointer.hpp"

namespace solid{
namespace frame{

class Object;

//! A simple execution pool for one shot object execution
/*!
	It doesn't support object signaling and timeouts.
*/
class BasicScheduler: public SchedulerBase{
public:
	typedef DynamicPointer<Object>		JobT;
	typedef Object						ObjectT;
	
	BasicScheduler(Manager &_rm, uint16 _startthrcnt = 0,uint32 _maxthrcnt = 1);
	~BasicScheduler();
	
	void start(uint16 _startwkrcnt = 0);
	
	void stop(bool _wait = true);
	
private:
	struct Data;
	friend struct Data;
	Data	&d;
};


}//namespace frame
}//namespace solid

#endif
