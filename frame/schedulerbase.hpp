// frame/schedulerbase.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_SCHEDULER_BASE_HPP
#define SOLID_FRAME_SCHEDULER_BASE_HPP

#include "frame/common.hpp"

namespace solid{
namespace frame{

class Manager;
class SelectorBase;

//! A base class for all schedulers
class SchedulerBase{
protected:
	typedef bool (*CreateWorkerF)(SchedulerBase &_rsch);
	
	typedef FunctorReference<bool, SelectorBase*>	ScheduleFunctorT;
	
	bool doStart(CreateWorkerF _pf, size_t _selcnt = 1, size_t _selchunkcp = 1024);

	void doStop(bool _wait = true);
	
	void doSchedule(ScheduleFunctorT &_rfct);
protected:
	SchedulerBase(
		Manager &_rm
	);
	SchedulerBase();
	bool prepareThread(SelectorBase &_rsel);
	void unprepareThread(SelectorBase &_rsel);
private:
	struct Data;
	Data	&d;
};

}//namespace frame
}//namespace solid

#endif

