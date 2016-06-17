// system/condition_boost.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_CONDITION_BOOST_HPP
#define SYSTEM_CONDITION_BOOST_HPP

#if	defined(UBOOSTMUTEX)

#include "boost/thread/condition_variable.hpp"
#include "boost/chrono.hpp"
#include "system/mutex.hpp"
#include "system/timespec.hpp"

namespace solid{

struct Condition: boost::condition_variable{
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
		boost::condition_variable::wait(_lock);
	}
	void wait(Locker<Mutex> &_lock, ERROR_NS::system_error &_rerr){
		boost::condition_variable::wait(_lock);
	}
	//! Wait for a signal a certain amount of time
	bool wait(Locker<Mutex> &_lock, const TimeSpec &_ts){
		boost::system_time time_limit(
			boost::posix_time::from_time_t(0)
		);
		time_limit += boost::posix_time::seconds(static_cast<long>(_ts.seconds()));
		time_limit += boost::posix_time::microseconds(_ts.nanoSeconds()/1000);
		return timed_wait(_lock, time_limit);
	}
	bool wait(Locker<Mutex> &_lock, const TimeSpec &_ts, ERROR_NS::system_error &_rerr){
		boost::system_time time_limit(
			boost::posix_time::from_time_t(0)
		);
		time_limit += boost::posix_time::seconds(static_cast<long>(_ts.seconds()));
		time_limit += boost::posix_time::microseconds(_ts.nanoSeconds()/1000);
		return timed_wait(_lock, time_limit);
	}
};

}//namespace solid

#endif

#endif
