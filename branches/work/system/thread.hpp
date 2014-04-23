// system/thread.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_THREAD_HPP
#define SYSTEM_THREAD_HPP

#ifdef ON_WINDOWS
#else
#include <pthread.h>
#endif

#if defined(ON_SOLARIS) || defined(ON_MACOS)
#include <sched.h>
#endif

#include <vector>
#include "system/error.hpp"
#include "system/common.hpp"

namespace solid{



struct Mutex;

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
	static Thread& current();
	//! Returns the id of the current thread
	static long currentId();
	//! Returns a new id for use with specific objects
	static size_t specificId();
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
	bool start(bool _wait = false, bool _detached = true, ulong _stacksz = 0);
	//! Join the calling thread
	bool join();
	//! Check if the thread is detached
	bool detached() const;
	//! Detach the thread
	bool detach();
	
	Mutex& mutex()const;
	
	void specificErrorClear();
	void specificErrorPush(const ErrorStub &_rerr);
	ErrorVectorT const& specificErrorGet()const;
	
protected:
#ifdef _WIN32
	Thread(bool _detached = true, void* _th = NULL);
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
	static void free_thread(void *_ptr);
	static Thread* associateToCurrent();
	
	static void current(Thread *_ptb);
	Thread(const Thread&){}
#ifdef ON_WINDOWS
	static unsigned long th_run(void*);
#else
	static void* th_run(void*);
#endif
	static void enter();
	static void exit();
	
	void signalWaiter();
	int waited();
private:
	typedef std::pair<void*, SpecificFncT>	SpecPairT;
	typedef std::vector<SpecPairT>			SpecVecT;
	struct ThreadStub;
#if		defined(ON_WINDOWS)
	void*			th;
#else
	pthread_t       th;
#endif
	bool			dtchd;
	unsigned        thcrtstatid;
	SpecVecT		specvec;
	ErrorVectorT	errvec;
	ThreadStub		*pthrstub;
};

#ifndef ON_WINDOWS
inline void Thread::yield(){
#if	defined(ON_SOLARIS) || defined(ON_MACOS)
	sched_yield();
#else
	pthread_yield();
#endif
}
inline long Thread::currentId(){
	return (long)pthread_self();
}
#endif

inline void Thread::specificErrorClear(){
	errvec.clear();
}
inline void Thread::specificErrorPush(const ErrorStub &_rerr){
	errvec.push_back(_rerr);
}
inline ErrorVectorT const& Thread::specificErrorGet()const{
	return errvec;
}

}//namespace solid

#endif


