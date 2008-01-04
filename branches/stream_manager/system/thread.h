/* Declarations file thread.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef THREADPP_H
#define THREADPP_H

#include "synchronization.h"
#include "src/mutexpool.h"
#include <pthread.h>
#include <vector>


class Thread{
public:
    typedef void RunParamTp;
    typedef void (*SpecificFncTp)(void *_ptr);
    
    static void dummySpecificDestroy(void*);
    
    static void init(); 
    static void cleanup();
    static void sleep(ulong _msec);
    
    static void waitAll();
    static void yield();
    static Thread * current();
    static int currentid();
    
    static unsigned specificId();
    static void* specific(unsigned _pos);
    static unsigned specific(unsigned _pos, void *_psd, SpecificFncTp _pfnc = &dummySpecificDestroy);
    //static unsigned specific(void *_psd);
    static Mutex& gmutex();
    
    int start(int _wait = false,int _detached = true, ulong _stacksz = 0);
    int join();
    int detached() const;
    int detach();
    
    Mutex& mutex()const;
    ulong id()const { return (ulong)th;}
protected:
	Thread();
	virtual ~Thread();
	virtual void prepare(){}
	virtual void unprepare(){}
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
    
    static ulong getNextId();
    static void threadEnter();
    static void threadExit();
    //a dummy function
    static int current(Thread *_ptb);
    
    typedef std::pair<void*, SpecificFncTp>	SpecPairTp;	
    typedef std::vector<SpecPairTp>			SpecVecTp;
    
    pthread_t       th;
    int				dtchd;
    unsigned        thcrtstatid;
    SpecVecTp       specvec;
    Semaphore		*psem;
    void signalWaiter();
    int waited();
private:
	Thread(const Thread&){}
	static void* th_run(void*);
    static ulong    nextid;
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
inline int Thread::currentid(){
	return pthread_self();
}

//non static
inline Mutex& Thread::mutex()const{
    return mutexpool.getr(this);
}

#endif


