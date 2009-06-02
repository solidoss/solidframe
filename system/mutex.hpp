/* Declarations file mutex.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MUTEXPP_HPP
#define MUTEXPP_HPP

#include <pthread.h>

class Condition;
struct TimeSpec;

//! A simple wrapper for POSIX mutex synchronizatin objects.
class Mutex{
public:
	struct Locker{
		Locker(Mutex &_m);
		~Locker();
		Mutex &m;
	};
	enum Type{
		FAST = PTHREAD_MUTEX_NORMAL, 
		RECURSIVE = PTHREAD_MUTEX_RECURSIVE,
		ERRORCHECK = PTHREAD_MUTEX_ERRORCHECK
	};
#ifdef UDEBUG	
	Mutex(Type _type = ERRORCHECK);
#else
	Mutex(Type _type = FAST);
#endif
	~Mutex();
	//! Lock for current thread
	void lock();
	//! Unlock for current thread
	void unlock();
	//! Timed lock for current thread
	int timedLock(const TimeSpec &_rts);
	//! Try lock for current thread
	bool tryLock();
	//! Reinit with a new tipe
	int reinit(Type _type = FAST);
private:
	friend class Condition;
	pthread_mutex_t mut;
};

#ifdef UINLINES
#include "src/mutex.ipp"
#endif

#endif

