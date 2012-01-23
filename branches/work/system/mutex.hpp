/* Declarations file mutex.hpp
	
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

#ifndef SYSTEM_MUTEX_HPP
#define SYSTEM_MUTEX_HPP

#include "common.hpp"

#if defined(USTLMUTEX)
#include <mutex>

#include "system/timespec.hpp"

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
struct Locker: std::unique_lock<typename M::base>{
	Locker(M &_m):std::unique_lock<typename M::base>(_m){}
};

#elif defined(UBOOSTMUTEX)


#else

#include <pthread.h>

class Condition;
struct TimeSpec;

//! A simple wrapper for POSIX mutex synchronizatin objects.
class Mutex{
public:
	Mutex();
	~Mutex();
	//! Lock for current thread
	void lock();
	//! Unlock for current thread
	void unlock();
	//! Try lock for current thread
	bool tryLock();
protected:
	enum Type{
		FAST = PTHREAD_MUTEX_NORMAL, 
		RECURSIVE = PTHREAD_MUTEX_RECURSIVE,
		ERRORCHECK = PTHREAD_MUTEX_ERRORCHECK
	};
	Mutex(Type _type);
	//! Timed lock for current thread
	int timedLock(const TimeSpec &_rts);
	int reinit(Type _type = FAST);
private:
	friend class Condition;
	pthread_mutex_t mut;
};

struct RecursiveMutex: public Mutex{
    RecursiveMutex():Mutex(RECURSIVE){}
};

struct TimedMutex: public Mutex{
	int timedLock(const TimeSpec &_rts){
		return Mutex::timedLock(_rts);
	}
};

struct RecursiveTimedMutex: public Mutex{
	RecursiveTimedMutex():Mutex(RECURSIVE){}
	
	int timedLock(const TimeSpec &_rts){
		return Mutex::timedLock(_rts);
	}
};

template <class M>
struct Locker{
	Locker(M &_m):m(_m){
		m.lock();
	}
	~Locker(){
		m.unlock();
	}
	M &m;
};

#ifndef NINLINES
#include "system/mutex.ipp"
#endif

#endif

#endif

