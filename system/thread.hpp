/* Declarations file thread.hpp
	
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

#ifndef SYSTEM_THREAD_HPP
#define SYSTEM_THREAD_HPP

#ifdef ON_WINDOWS
#else
#include <pthread.h>
#endif

#ifdef ON_SOLARIS
#include <sched.h>
#endif

#include <vector>
#include "system/common.hpp"

class Mutex;
class Condition;

//! A wrapper for system threads
/*!
	<b>Usage:</b><br>
	In your application, before creating any thread you should call 
	Thread::init and when dying after you signaled all your application threads to 
	stop, you should call Thread::waitAll to blocking wait for all threads to stop.
	Then inherit from Thread, implement Thread::run method (and eventually 
	Thread::prepare && Thread::unprepare).
*/
class Thread{
public:
	typedef void RunParamT;
	typedef void (*SpecificFncT)(void *_ptr);
	
	static void dummySpecificDestroy(void*);
	
	static void init(); 
	static void cleanup();
	//! Make the current thread to sleep fo _msec milliseconds
	static void sleep(ulong _msec);
	//! Returns the number of processors on the running machine
	static unsigned processorCount();
	
	static long processId();
	static void waitAll();
	//! Releases the processor for another thread
	static void yield();
	//! Returns a pointer to the current thread
	static Thread * current();
	//! Returns the id of the current thread
	static long currentId();
	//! Returns a new id for use with specific objects
	static unsigned specificId();
	//! Returns the data for a specific id
	static void* specific(unsigned _pos);
	//! Sets the data for a specific id, allong with a pointer to a destructor function
	static void specific(unsigned _pos, void *_psd, SpecificFncT _pfnc = &dummySpecificDestroy);
	//static unsigned specific(void *_psd);
	//! Returns a reference to a global mutex
	static Mutex& gmutex();
	//! Starts a new thread
	/*!
		\param _wait If true, the function will return after the the thread was started.
		\param _detached If true create the thread in a detached state
		\param _stacksz 
	*/
	int start(int _wait = false,int _detached = true, ulong _stacksz = 0);
	//! Join the calling thread
	int join();
	//! Check if the thread is detached
	int detached() const;
	//! Detach the thread
	int detach();
	
	Mutex& mutex()const;
protected:
#ifdef _WIN32
#else
	Thread(bool _detached = true, pthread_t _th = 0);
#endif
	virtual ~Thread();
	//! Method called right before run
	virtual void prepare(){}
	//! Method called right after run
	virtual void unprepare(){}
	//! Implement this method to make the thread usefull.
	virtual void run() = 0;
private:
	//a dummy function
	static int current(Thread *_ptb);
	Thread(const Thread&){}
	static void* th_run(void*);
	static void enter();
	static void exit();
	
	void signalWaiter();
	int waited();
private:
	typedef std::pair<void*, SpecificFncT>	SpecPairT;
	typedef std::pair<Condition, int>		ConditionPairT;
	typedef std::vector<SpecPairT>			SpecVecT;
	
#if		defined(ON_WINDOWS)
#else
	pthread_t       th;
#endif
	int				dtchd;
	unsigned        thcrtstatid;
	SpecVecT       specvec;
	ConditionPairT	*pcndpair;

};

inline void Thread::yield(){
#if		defined(ON_WINDOWS)
#elseif	defined(ON_SOLARIS)
	sched_yield();
#else
	pthread_yield();
#endif
}

inline long Thread::currentId(){
#ifdef ON_WINDOWS
#else
	return (long)pthread_self();
#endif
}

#endif


