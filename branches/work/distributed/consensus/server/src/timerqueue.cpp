/* Implementation file timerqueue.cpp
	
	Copyright 2011, 2012 Valentin Palade 
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

#include "timerqueue.hpp"

bool TimerQueue::TimerDataCmp::operator()(
	const TimerData &_rtd1, const TimerData &_rtd2
)const{
	return _rtd1.timepos > _rtd2.timepos;
}


TimerQueue::TimerQueue(){}

void TimerQueue::push(const TimeSpec &_rts, uint32 _idx, uint16 _uid, uint16 _v){
	tq.push(TimerData(_rts, _idx, _uid, _v));
}
