// system/mutex_boost.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_MUTEX_BOOST_HPP
#define SYSTEM_MUTEX_BOOST_HPP


#include "boost/thread/mutex.hpp"
#include "boost/thread/recursive_mutex.hpp"
#include "boost/chrono.hpp"

#include "system/timespec.hpp"

namespace solid{

struct Mutex: boost::mutex{
	typedef boost::mutex base;
	bool tryLock(){
		return try_lock();
	}
};

struct RecursiveMutex: boost::recursive_mutex{
};

struct TimedMutex: boost::timed_mutex{
	int timedLock(const TimeSpec &_rts){
				boost::system_time time_limit(
			boost::posix_time::from_time_t(0)
		);

		time_limit += boost::posix_time::seconds(static_cast<long>(_rts.seconds()));
		time_limit += boost::posix_time::microseconds(_rts.nanoSeconds()/1000);
		if(timed_lock(time_limit)){
			return 0;
		}else{
			return -1;
		}
	}
};

struct RecursiveTimedMutex: boost::recursive_timed_mutex{
	int timedLock(const TimeSpec &_rts){
		boost::system_time time_limit(
			boost::posix_time::from_time_t(0)
		);
		time_limit += boost::posix_time::seconds(static_cast<long>(_rts.seconds()));
		time_limit += boost::posix_time::microseconds(_rts.nanoSeconds()/1000);
		if(timed_lock(time_limit)){
			return 0;
		}else{
			return -1;
		}
	}
};

template <class M>
struct Locker;

template <>
struct Locker<Mutex>: boost::unique_lock<Mutex::base>{
	Locker(Mutex &_m):boost::unique_lock<Mutex::base>(_m){}
};


template <class M>
struct Locker: boost::unique_lock<M>{
	Locker(M &_m):boost::unique_lock<M>(_m){}
};

}//namespace solid

#endif
