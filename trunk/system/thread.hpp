/* Declarations file thread.hpp
	
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

#ifndef THREADPP_HPP
#define THREADPP_HPP

#include <sys/sysinfo.h>
#include <pthread.h>

#include <vector>

#include "condition.hpp"
#include "src/mutexpool.hpp"

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
	typedef void RunParamTp;
	typedef void (*SpecificFncTp)(void *_ptr);
	
	static void dummySpecificDestroy(void*);
	
	static void init(); 
	static void cleanup();
	//! Make the current thread to sleep fo _msec milliseconds
	static void sleep(ulong _msec);
	//! Returns the number of processors on the running machine
	static unsigned processorCount();
	
	static void waitAll();
	//! Releases the processor for another thread
	static void yield();
	//! Returns a pointer to the current thread
	static Thread * current();
	//! Returns the id of the current thread
	static int currentId();
	//! Returns a new id for use with specific objects
	static unsigned specificId();
	//! Returns the data for a specific id
	static void* specific(unsigned _pos);
	//! Sets the data for a specific id, allong with a pointer to a destructor function
	static unsigned specific(unsigned _pos, void *_psd, SpecificFncTp _pfnc = &dummySpecificDestroy);
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
	Thread();
	virtual ~Thread();
	//! Method called right before run
	virtual void prepare(){}
	//! Method called right after run
	virtual void unprepare(){}
	//! Implement this method to make the thread usefull.
	virtual void run() = 0;
protected:
	enum {MutexPoolSize = 4};
	typedef void* (*ThFncTp)(void *);
	
	static Condition                        gcon;
	static Mutex                            gmut;
	static FastMutexPool<MutexPoolSize>     mutexpool;
	static pthread_key_t                    crtthread_key;  
	static pthread_key_t                    **static_keys;
	static pthread_once_t                  	once_key;
	
	static void enter();
	static void exit();
	//a dummy function
	static int current(Thread *_ptb);
	
	typedef std::pair<void*, SpecificFncTp>	SpecPairTp;
	typedef std::pair<Condition, int>		ConditionPairTp;
	typedef std::vector<SpecPairTp>			SpecVecTp;
	
	pthread_t       th;
	int				dtchd;
	unsigned        thcrtstatid;
	SpecVecTp       specvec;
	ConditionPairTp	*pcndpair;
	void signalWaiter();
	int waited();
private:
	Thread(const Thread&){}
	static void* th_run(void*);
	static ulong    thcnt;
};

inline void Thread::sleep(ulong _msec){
	usleep(_msec*1000);
}

inline void Thread::yield(){
	pthread_yield();
}

inline Thread * Thread::current(){
	return reinterpret_cast<Thread*>(pthread_getspecific(crtthread_key));
}

inline int Thread::currentId(){
	return pthread_self();
}

//non static
inline Mutex& Thread::mutex()const{
	return mutexpool.getr(this);
}

#endif


