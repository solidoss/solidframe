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
		FAST = PTHREAD_MUTEX_FAST_NP, 
		RECURSIVE = PTHREAD_MUTEX_RECURSIVE_NP,
		ERRORCHECK = PTHREAD_MUTEX_ERRORCHECK_NP
	};
#ifdef UDEBUG	
	Mutex(Type _type = ERRORCHECK);
#else
	Mutex(Type _type = FAST);
#endif
	~Mutex();
	
	void lock();
	void unlock();
	int timedLock(const TimeSpec &_rts);
	bool tryLock();
	int reinit(Type _type = FAST);
private:
	friend class Condition;
	pthread_mutex_t mut;
};

#ifdef UINLINES
#include "src/mutex.ipp"
#endif

#endif

