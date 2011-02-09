/* Declarations file schedulerbase.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FOUNDATION_SCHEDULER_BASE_HPP
#define FOUNDATION_SCHEDULER_BASE_HPP

#include "foundation/common.hpp"

namespace foundation{

class Manager;
class SelectorBase;

//! A base class for all schedulers
class SchedulerBase{
public:
	virtual void start(uint16 _startwkrcnt = 0) = 0;
	
	virtual void stop(bool _wait = true) = 0;
	virtual ~SchedulerBase();
protected:
	
	SchedulerBase(uint16 _startwkrcnt, uint16 _maxwkrcnt, const IndexT &_selcap);
	SchedulerBase(Manager &_rm, uint16 _startwkrcnt, uint16 _maxwkrcnt, const IndexT &_selcap);
	
	void prepareThread(SelectorBase *_ps = NULL);
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
	uint16	startwkrcnt;
	uint16	maxwkrcnt;
	uint16	crtwkrcnt;
	IndexT	selcap;
};

}//namespace

#endif

