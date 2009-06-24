/* Implementation file thread.cpp
	
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


#include <cerrno>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include "system/timespec.hpp"
#include "system/thread.hpp"
#include "system/debug.hpp"
#include "system/condition.hpp"
#include "mutexpool.hpp"
#ifdef ON_FBSD
#include <pmc.h>
#else
#include <sys/sysinfo.h>
#endif

#include <unistd.h>
#include <limits.h>

struct Cleaner{
	~Cleaner(){
		Thread::cleanup();
	}
};

static const pthread_once_t	oncek = PTHREAD_ONCE_INIT;

struct ThreadData{
	enum {
		MutexPoolSize = 4,
		FirstSpecificId = 0
	};
	//ThreadData();
	//ThreadData():crtthread_key(0), thcnt(0), once_key(PTHREAD_ONCE_INIT){}
	pthread_key_t					crtthread_key;
	uint32    						thcnt;
	pthread_once_t					once_key;
	Condition						gcon;
	Mutex							gmut;
	FastMutexPool<MutexPoolSize>	mutexpool;
	ThreadData():crtthread_key(0), thcnt(0), once_key(oncek){
	}
};

//ThreadData::ThreadData(){
//	crtthread_key = 0;
//	thcnt = 0;
//	once_key = PTHREAD_ONCE_INIT;
//}
static ThreadData& threadData(){
	static ThreadData td;
	return td;
}

Cleaner             			cleaner;
//static unsigned 				crtspecid = 0;
//*************************************************************************
int Condition::wait(Mutex &_mut, const TimeSpec &_ts){
	return pthread_cond_timedwait(&cond,&_mut.mut, &_ts);
}
//*************************************************************************
#ifndef UINLINES
#include "timespec.ipp"
#endif
//*************************************************************************
#ifndef UINLINES
#include "mutex.ipp"
#endif
//*************************************************************************
#ifndef UINLINES
#include "condition.ipp"
#endif
//*************************************************************************
#ifndef UINLINES
#include "synchronization.ipp"
#endif
//-------------------------------------------------------------------------
int Mutex::timedLock(const TimeSpec &_rts){
	return pthread_mutex_timedlock(&mut,&_rts);
}
//-------------------------------------------------------------------------
int Mutex::reinit(Type _type){
	pthread_mutex_destroy(&mut);
	pthread_mutexattr_t att;
	pthread_mutexattr_init(&att);
	pthread_mutexattr_settype(&att, (int)_type);
	return pthread_mutex_init(&mut,&att);
}
//*************************************************************************
void Thread::init(){
	if(pthread_key_create(&threadData().crtthread_key, NULL)) throw -1;
	Thread::current(NULL);
}
//-------------------------------------------------------------------------
void Thread::cleanup(){
	pthread_key_delete(threadData().crtthread_key);
}
//-------------------------------------------------------------------------
void Thread::sleep(ulong _msec){
	usleep(_msec*1000);
}
//-------------------------------------------------------------------------
inline void Thread::enter(){
	ThreadData &td(threadData());
    //td.gmut.lock();
    ++td.thcnt; td.gcon.broadcast();
    //td.gmut.unlock();
}
//-------------------------------------------------------------------------
inline void Thread::exit(){
	ThreadData &td(threadData());
    td.gmut.lock();
    --td.thcnt; td.gcon.broadcast();
    td.gmut.unlock();
}
//-------------------------------------------------------------------------
Thread * Thread::current(){
	return reinterpret_cast<Thread*>(pthread_getspecific(threadData().crtthread_key));
}
//-------------------------------------------------------------------------
long Thread::processId(){
	return getpid();
}
//-------------------------------------------------------------------------
Thread::Thread():th(0),dtchd(true),pcndpair(NULL){
}
//-------------------------------------------------------------------------
Thread::~Thread(){
	for(SpecVecTp::iterator it(specvec.begin()); it != specvec.end(); ++it){
		if(it->first){
			cassert(it->second);
			(*it->second)(it->first);
		}
	}
}
//-------------------------------------------------------------------------
void Thread::dummySpecificDestroy(void*){
}
//-------------------------------------------------------------------------
/*static*/ unsigned Thread::processorCount(){
#ifdef ON_SUN
	return 1;
#else
#ifdef ON_FBSD
	return 1;//pmc_ncpu();
#else
	return get_nprocs();
#endif
#endif
}
//-------------------------------------------------------------------------
int Thread::join(){
	if(pthread_equal(th, pthread_self())) return NOK;
	if(detached()) return NOK;
	int rcode =  pthread_join(this->th, NULL);
	return rcode;
}
//-------------------------------------------------------------------------
int Thread::detached() const{
	//Mutex::Locker lock(mutex());
	return dtchd;
}
//-------------------------------------------------------------------------
int Thread::detach(){
	Mutex::Locker lock(mutex());
	if(detached()) return OK;
	int rcode = pthread_detach(this->th);
	if(rcode == OK)	dtchd = 1;
	return rcode;
}
//-------------------------------------------------------------------------
unsigned Thread::specificId(){
	static unsigned sid = ThreadData::FirstSpecificId - 1;
	Mutex::Locker lock(gmutex());
	return ++sid;
}
//-------------------------------------------------------------------------
void Thread::specific(unsigned _pos, void *_psd, SpecificFncTp _pf){
	Thread *pct = current();
	cassert(pct);
	if(_pos >= pct->specvec.size()) pct->specvec.resize(_pos + 4);
	//This is safe because pair will initialize with NULL on resize
	if(pct->specvec[_pos].first){
		(*pct->specvec[_pos].second)(pct->specvec[_pos].first);
	}
	pct->specvec[_pos] = SpecPairTp(_psd, _pf);
	//return _pos;
}
//-------------------------------------------------------------------------
// unsigned Thread::specific(void *_psd){
// 	cassert(current());
// 	current()->specvec.push_back(_psd);
// 	return current()->specvec.size() - 1;
// }
//-------------------------------------------------------------------------
void* Thread::specific(unsigned _pos){
	cassert(current() && _pos < current()->specvec.size());
	return current()->specvec[_pos].first;
}
Mutex& Thread::gmutex(){
	return threadData().gmut;
}
//-------------------------------------------------------------------------
int Thread::current(Thread *_ptb){
	pthread_setspecific(threadData().crtthread_key, _ptb);
	return OK;
}
//-------------------------------------------------------------------------
Mutex& Thread::mutex()const{
	return threadData().mutexpool.getr(this);
}
//-------------------------------------------------------------------------
#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN 4096
#endif
int Thread::start(int _wait, int _detached, ulong _stacksz){	
	Mutex::Locker locker(mutex());
	idbg("");
	if(th) return BAD;
	idbg("");
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	if(_detached){
		if(pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED)){
			pthread_attr_destroy(&attr);
			idbg("could not made thread detached");
			return BAD;
		}
	}
	if(_stacksz){
		if(_stacksz < PTHREAD_STACK_MIN){
			_stacksz = PTHREAD_STACK_MIN;
		}
		int rv = pthread_attr_setstacksize(&attr, _stacksz);
		if(rv){
			edbg("pthread_attr_setstacksize " <<strerror(rv)<<" "<<strerror(errno));
			pthread_attr_destroy(&attr);
			idbg("could not set staksize");
			return BAD;
		}
	}
	ConditionPairTp cndpair;
	cndpair.second = 1;
	if(_wait){
		gmutex().lock();
		pcndpair = &cndpair;
		Thread::enter();
	}else{
		gmutex().lock();
		Thread::enter();
		gmutex().unlock();
	}
	if(pthread_create(&th,&attr,&Thread::th_run,this)){
		pthread_attr_destroy(&attr);
		idbg("could not create thread");
		Thread::exit();
		return BAD;
	}
	//NOTE: DO not access any thread member from now - the thread may be detached
	if(_wait){
		while(cndpair.second){
			cndpair.first.wait(gmutex());
		}
		gmutex().unlock();
	}
	pthread_attr_destroy(&attr);
	idbg("");
	return OK;
}
//-------------------------------------------------------------------------
void Thread::signalWaiter(){
	pcndpair->second = 0;
	pcndpair->first.signal();
	pcndpair = NULL;
}
//-------------------------------------------------------------------------
int Thread::waited(){
	return pcndpair != NULL;
}
//-------------------------------------------------------------------------
void Thread::waitAll(){
	ThreadData &td(threadData());
    td.gmut.lock();
    while(td.thcnt != 0) td.gcon.wait(td.gmut);
    td.gmut.unlock();
}
//-------------------------------------------------------------------------
void* Thread::th_run(void *pv){
	idbg("thrun enter"<<pv);
	Thread	*pth(reinterpret_cast<Thread*>(pv));
	//Thread::enter();
	Thread::current(pth);
	if(pth->waited()){
		pth->signalWaiter();
		Thread::yield();
	}
	pth->prepare();
	pth->run();
	pth->unprepare();
	if(pth->detached()) delete pth;
	idbg("thrun exit");
	Thread::exit();
	return NULL;
}

//-------------------------------------------------------------------------
