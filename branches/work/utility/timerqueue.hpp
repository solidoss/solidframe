// utility/timerqueue.hpp
//
// Copyright (c) 2011, 2012, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
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
