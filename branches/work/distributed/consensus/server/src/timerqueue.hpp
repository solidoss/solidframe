/* Declarations file timerqueue.hpp
	
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

#ifndef TIMERQUEUE_HPP
#define TIMERQUEUE_HPP

#include <queue>
#include "system/common.hpp"
#include "system/timespec.hpp"

class TimerQueue{
public:
	TimerQueue();
	void push(const TimeSpec &_rts, uint32 _idx, uint16 _uid, uint16 _v = 0);
	bool hitted(const TimeSpec &_rts)const;
	const TimeSpec& frontTime()const;
	const uint32&	frontIndex()const;
	const uint16&	frontValue()const;
	const uint16&	frontUid()const;
	void pop();
	size_t size()const;
	bool empty()const;
private:
	struct TimerData{
		TimerData(
			const TimeSpec&_rts,
			uint32 _idx = -1,
			uint16 _timerid = 0,
			uint16 _val = 0
		): timepos(_rts), idx(_idx), timerid(_timerid), val(_val){}
		TimeSpec	timepos;
		uint32		idx;
		uint16		timerid;
		uint16		val;
	};
	struct TimerDataCmp{
		bool operator()(const TimerData &_rtd1, const TimerData &_rtd2)const;
	};
	
	typedef std::priority_queue<
		TimerData,
		std::vector<TimerData>,
		TimerDataCmp
	>	TimerPriorityQueueT;

private:	
	TimerQueue(const TimerQueue&);
	TimerQueue& operator=(const TimerQueue&);
private:
	TimerPriorityQueueT		tq;
};

inline bool TimerQueue::hitted(const TimeSpec &_rts)const{
	if(tq.empty()) return false;
	return _rts >=  frontTime();
}
inline const TimeSpec& TimerQueue::frontTime()const{
	return tq.top().timepos;
}
inline const uint32&	TimerQueue::frontIndex()const{
	return tq.top().idx;
}
inline const uint16&	TimerQueue::frontValue()const{
	return tq.top().val;
}
inline const uint16&	TimerQueue::frontUid()const{
	return tq.top().timerid;
}
inline void TimerQueue::pop(){
	tq.pop();
}
inline size_t TimerQueue::size()const{
	return tq.size();
}
inline bool TimerQueue::empty()const{
	return tq.empty();
}

#endif
