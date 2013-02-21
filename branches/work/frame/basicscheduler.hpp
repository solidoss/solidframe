/* Declarations file basicscheduler.hpp
	
	Copyright 2007, 2008, 2013 Valentin Palade 
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
	
	BasicScheduler(uint16 _startthrcnt = 0,uint32 _maxthrcnt = 1);
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
