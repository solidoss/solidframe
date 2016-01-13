// system/condition_stl.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_CONDITION_STL_HPP
#define SYSTEM_CONDITION_STL_HPP


#if	defined(USTLMUTEX)

#include <condition_variable>
#include <chrono>
#include "system/mutex.hpp"
#include "system/timespec.hpp"

namespace solid{

struct Condition: std::condition_variable{
public:
	Condition(){}
	~Condition(){}
	//! Try to wake one waiting thread
	void signal(){
		this->notify_one();
	}
	void signal(ERROR_NS::system_error &_rerr){
		this->notify_one();
	}
	//! Try to wake all waiting threads
	void broadcast(){
		this->notify_all();
	}
	void broadcast(ERROR_NS::system_error &_rerr){
		this->notify_all();
	}
	//! Wait for a signal
	void wait(Locker<Mutex> &_lock){
		std::condition_variable::wait(_lock);
	}
	void wait(Locker<Mutex> &_lock, ERROR_NS::system_error &_rerr){
		std::condition_variable::wait(_lock);
	}
	//! Wait for a signal a certain amount of time
	bool wait(Locker<Mutex> &_lock, const TimeSpec &_ts){
		std::chrono::milliseconds ms(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::seconds(_ts.seconds()) +
			std::chrono::nanoseconds(_ts.nanoSeconds())
		));
		std::chrono::system_clock::time_point time_limit(ms);
		const std::cv_status stat = wait_until(_lock, time_limit);
		return stat == std::cv_status::no_timeout;
	}
	bool wait(Locker<Mutex> &_lock, const TimeSpec &_ts, ERROR_NS::system_error &_rerr){
		std::chrono::milliseconds ms(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::seconds(_ts.seconds()) +
			std::chrono::nanoseconds(_ts.nanoSeconds())
		));
		std::chrono::system_clock::time_point time_limit(ms);
		const std::cv_status stat = wait_until(_lock, time_limit);
		return stat == std::cv_status::no_timeout;
	}
};

}//namespace solid

#endif

#endif
