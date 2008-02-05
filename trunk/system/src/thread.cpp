/* Implementation file thread.cpp
	
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

#include "timespec.h"

#include "thread.h"
#include "debug.h"
#include "common.h"
#include "mutexpool.h"
#include <cerrno>

struct Cleaner{
	~Cleaner(){
		Thread::cleanup();
	}
};

enum ThreadInfo{
	FirstSpecificId = 0
};

pthread_key_t                   Thread::crtthread_key = 0;
Condition                       Thread::gcon;
Mutex                           Thread::gmut;
ulong                           Thread::nextid = 1;
ulong                           Thread::thcnt = 0;
FastMutexPool<Thread::MutexPoolSize>    Thread::mutexpool;
pthread_once_t                  Thread::once_key = PTHREAD_ONCE_INIT;
Cleaner             			cleaner;
//static unsigned 				crtspecid = 0;
//*************************************************************************
int Condition::wait(Mutex &_mut, const TimeSpec &_ts){
	return pthread_cond_timedwait(&cond,&_mut.mut, &_ts);
}
//*************************************************************************
int Mutex::lock(){
#ifdef UDEBUG
	int rv = pthread_mutex_lock(&mut);
	if(rv){
		idbg("lock error "<<strerror(errno));
	}
	cassert(!rv);
	return rv;
#else
	return pthread_mutex_lock(&mut);
#endif
}
//-------------------------------------------------------------------------
int Mutex::unlock(){
#ifdef UDEBUG
	int rv = pthread_mutex_unlock(&mut);
	if(rv){
		idbg("lock error "<<strerror(errno));
	}
	cassert(!rv);
	return rv;
#else
	return pthread_mutex_unlock(&mut);
#endif
}
//-------------------------------------------------------------------------
bool Mutex::timedLock(unsigned long _ms){
	struct timespec lts;
	lts.tv_sec=_ms/1000;
	lts.tv_nsec=(_ms%1000)*1000000;
	return pthread_mutex_timedlock(&mut,&lts)==0;  
}
//*************************************************************************
void Thread::init(){
	if(pthread_key_create(&Thread::crtthread_key, NULL)) throw -1;
	Thread::current(NULL);
}
//-------------------------------------------------------------------------
void Thread::cleanup(){
	idbg("");
	pthread_key_delete(Thread::crtthread_key);
}
//-------------------------------------------------------------------------
inline void Thread::threadEnter(){
    gmut.lock();
    ++thcnt; gcon.broadcast();
    gmut.unlock();
}
//-------------------------------------------------------------------------
inline void Thread::threadExit(){
    gmut.lock();
    --thcnt; gcon.broadcast();
    gmut.unlock();
}
//-------------------------------------------------------------------------
Thread::Thread():dtchd(true),psem(NULL){
	th = 0;
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
	static unsigned sid = FirstSpecificId - 1;
	return ++sid;
}
//-------------------------------------------------------------------------
unsigned Thread::specific(unsigned _pos, void *_psd, SpecificFncTp _pf){
	cassert(current());
	//cassert(_pos < current()->specvec.size());
	if(_pos >= current()->specvec.size()) current()->specvec.resize(_pos + 4);
	current()->specvec[_pos] = SpecPairTp(_psd, _pf);
	return _pos;
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
	return gmut;
}
//-------------------------------------------------------------------------
int Thread::current(Thread *_ptb){
	pthread_setspecific(Thread::crtthread_key, _ptb);
	return OK;
}

//-------------------------------------------------------------------------
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
		if(pthread_attr_setstacksize(&attr, _stacksz)){
			pthread_attr_destroy(&attr);
			idbg("could not set staksize");
			return BAD;
		}
	}
	Semaphore sem;
	if(_wait) psem = &sem;
	if(pthread_create(&th,&attr,&Thread::th_run,this)){
		pthread_attr_destroy(&attr);
		idbg("could not create thread");
		return BAD;
	}
	if(_wait) sem.wait();
	pthread_attr_destroy(&attr);
	idbg("");
	return OK;
}
//-------------------------------------------------------------------------
void Thread::signalWaiter(){
	++(*psem);
	psem = NULL;
}
//-------------------------------------------------------------------------
int Thread::waited(){
	return psem != NULL;
}
//-------------------------------------------------------------------------
void Thread::waitAll(){
    gmut.lock();
    while(thcnt != 0) gcon.wait(gmut);
    gmut.unlock();
}
//-------------------------------------------------------------------------
void* Thread::th_run(void *pv){
	idbg("thrun enter"<<pv);
	Thread	*pth(reinterpret_cast<Thread*>(pv));
	Thread::threadEnter();
	Thread::current(pth);
	if(pth->waited()){
		pth->signalWaiter();
		Thread::yield();
	}
	pth->prepare();
	pth->run();
	pth->unprepare();
	Thread::threadExit();
	if(pth->detached()) delete pth;
	idbg("thrun exit");
	return NULL;
}

//-------------------------------------------------------------------------
