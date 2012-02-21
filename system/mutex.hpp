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

#if		defined(USTLMUTEX)

#include "system/mutex_stl.hpp"

#elif	defined(UBOOSTMUTEX)

#include "system/mutex_boost.hpp"

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

