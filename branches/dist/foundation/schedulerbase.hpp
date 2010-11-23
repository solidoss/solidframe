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

namespace foundation{

class Manager;

//! This is the base class for all active containers/sets (i.e. WorkPools)
/*!
	It is used by foundation::Manager for signaling a workpool
	currently holding an object.
*/
class SchedulerBase{
protected:
	SchedulerBase();
	SchedulerBase(Manager &_rm);
	virtual ~SchedulerBase();
	//! Wake a theread given by its index
	virtual void raise(uint _thridx) = 0;
	//! Wake an object
	/*!
		Wake an object given the thread on which it resides and an index in the 
		thread.
	*/
	virtual void raise(uint _thridx, uint _objid) = 0;
	
	void prepareThread();
	void unprepareThread();
protected:
	Manager	&rm;
};

}//namespace

#endif

