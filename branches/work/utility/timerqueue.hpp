/* Declarations file timerqueue.hpp
	
	Copyright 2011, 2012, 2013 Valentin Palade 
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

namespace solid{

template <class V>
class TimerQueue{
	typedef std::pair<TimeSpec, V>	TimeSpecValuePairT;
public:
	typedef V ValueT; 
	
	TimerQueue(){}
	
	void push(const TimeSpec &_rts, const ValueT &_rv){
		tq.push(TimeSpecValuePairT(_rts, _rv));
	}
	
	const TimeSpec& frontTime()const{
		return tq.top().first;
	}
	const ValueT&	frontValue()const{
		return tq.top().second;
	}

	
	bool isHit(const TimeSpec &_rts)const{
		if(tq.empty()) return false;
		return _rts >=  frontTime();
	}
	

	void pop(){
		tq.pop();
	}
	size_t size()const{
		return tq.size();
	}
	bool empty()const{
		return tq.empty();
	}
private:
	
	struct TimerDataCmp{
		bool operator()(const TimeSpecValuePairT &_rtd1, const TimeSpecValuePairT &_rtd2)const{
			return _rtd1.first > _rtd2.first;
		}
	};
	
	typedef std::priority_queue<
		TimeSpecValuePairT,
		std::vector<TimeSpecValuePairT>,
		TimerDataCmp
	>	TimerPriorityQueueT;

private:	
	TimerQueue(const TimerQueue&);
	TimerQueue& operator=(const TimerQueue&);
private:
	TimerPriorityQueueT		tq;
};

}//namespace solid
#endif
