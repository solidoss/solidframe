// system/mutex.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_MUTEX_HPP
#define SYSTEM_MUTEX_HPP

#include "common.hpp"

#if		defined(USTLMUTEX)

#include "system/mutex_stl.hpp"

#elif	defined(UBOOSTMUTEX)

#include "system/mutex_boost.hpp"

#else

#include <pthread.h>

namespace solid{

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

struct SharedMutex{
	SharedMutex();
	~SharedMutex();
	//! Lock for current thread
	void lock();
	//! Unlock for current thread
	void unlock();
	//! Try lock for current thread
	bool tryLock();
	void sharedLock();
	void sharedUnlock();
	bool sharedTryLock();
private:
	pthread_rwlock_t	mut;
};

template <class M>
struct Locker{
	typedef Locker<M> ThisT;
	
	Locker(M &_m):m(_m){
		m.lock();
	}
	~Locker(){
		m.unlock();
	}
	M &m;
private:
	Locker(ThisT const &);
	ThisT& operator=(ThisT const&);
};

template <class M>
struct SmartLocker{
	typedef SmartLocker<M> ThisT;
	
	SmartLocker(M &_rm): pm(&_rm){
		pm->lock();
	}
	
	SmartLocker():pm(nullptr){}
	
	ThisT& operator=(ThisT && _rl){
		if(pm){
			pm->unlock();
		}
		pm = _rl.pm;
		_rl.pm = nullptr;
		return *this;
	}
	
	~SmartLocker(){
		if(pm) pm->unlock();
	}
private:
	SmartLocker(ThisT const&);
	ThisT& operator=(ThisT const&);
private:
	M	*pm;
};

template <class M>
struct SharedLocker{
	SharedLocker(M &_m):m(_m){
		m.sharedLock();
	}
	~SharedLocker(){
		m.sharedUnlock();
	}
	M &m;
};


#ifndef SOLID_HAS_NO_INLINES
#include "system/mutex.ipp"
#endif

}//namespace solid

#endif

#endif

