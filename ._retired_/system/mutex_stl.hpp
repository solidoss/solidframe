// system/mutex_stl.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_MUTEX_STL_HPP
#define SYSTEM_MUTEX_STL_HPP


#include <mutex>

#include "system/timespec.hpp"

namespace solid{

struct Mutex: std::mutex{
	typedef std::mutex base;
	bool tryLock(){
		return try_lock();
	}
};

struct RecursiveMutex: std::recursive_mutex{
};

struct TimedMutex: std::timed_mutex{
	int timedLock(const TimeSpec &_rts){
		std::chrono::milliseconds ms(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::seconds(_rts.seconds()) +
			std::chrono::nanoseconds(_rts.nanoSeconds())
		));
		std::chrono::system_clock::time_point time_limit(
			ms
		);
		if(try_lock_until(time_limit)){
			return 0;
		}else{
			return -1;
		}
	}
};

struct RecursiveTimedMutex: std::recursive_timed_mutex{
	int timedLock(const TimeSpec &_rts){
		std::chrono::milliseconds ms(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::seconds(_rts.seconds()) +
			std::chrono::nanoseconds(_rts.nanoSeconds())
		));
		std::chrono::system_clock::time_point time_limit(
			ms
		);
		if(try_lock_until(time_limit)){
			return 0;
		}else{
			return -1;
		}
	}
};

template <class M>
struct Locker;

template <>
struct Locker<Mutex>: std::unique_lock<Mutex::base>{
	Locker(Mutex &_m):std::unique_lock<Mutex::base>(_m){}
};


template <class M>
struct Locker: std::unique_lock<typename M::base>{
	Locker(M &_m):std::unique_lock<typename M::base>(_m){}
};

}//namespace solid

#endif
