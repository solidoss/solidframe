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
public:
	virtual void start(uint16 _startwkrcnt = 0) = 0;
	
	virtual void stop(bool _wait = true) = 0;
	virtual ~SchedulerBase();
protected:
	
	SchedulerBase(
		Manager &_rm,
		uint16 _maxwkrcnt,
		const IndexT &_selcap
	);
	
	bool prepareThread(SelectorBase *_ps = NULL);
	void unprepareThread(SelectorBase *_ps = NULL);
	
	bool tryRaiseOneSelector()const;
	void raiseOneSelector();
	
	void markSelectorFull(SelectorBase &_rs);
	void markSelectorNotFull(SelectorBase &_rs);
	void doStop();
protected:
	struct Data;
	Manager	&rm;
	Data	&d;
	IndexT	cap;//the total count of objects already in pool
	uint16	maxwkrcnt;
	uint16	crtwkrcnt;
	IndexT	selcap;
};

}//namespace frame
}//namespace solid

#endif

