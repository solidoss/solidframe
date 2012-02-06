/* Implementation file thread.cpp
	
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


#include <cerrno>
#include <cstring>
#include <limits.h>

#include "system/timespec.hpp"
#include "system/thread.hpp"
#include "system/debug.hpp"
#include "system/condition.hpp"
#include "system/exception.hpp"
#include "system/cassert.hpp"

#include "mutexpool.hpp"

#if defined(ON_FREEBSD)
#include <pmc.h>
#elif defined(ON_MACOS)
#elif defined(ON_WINDOWS)
#else
#include <sys/sysinfo.h>
#endif


#if defined(ON_WINDOWS)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <windows.h>
#include <Process.h>
//#include <io.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

#if defined(ON_MACOS)
#include <mach/mach_time.h>
#endif


#include <limits.h>

struct Cleaner{
	~Cleaner(){
		Thread::cleanup();
	}
};

/*static*/ const char* src_file_name(char const *_fname){
#ifdef ON_WINDOWS
	static const unsigned fileoff = (strlen(__FILE__) - strlen(strstr(__FILE__, "system\\src")));
#else
	static const unsigned fileoff = (strlen(__FILE__) - strlen(strstr(__FILE__, "system/src")));
#endif
	return _fname + fileoff;
}

void throw_exception(const char* const _pt, const char * const _file, const int _line, const char * const _func){
	throw Exception<const char*>(_pt, _file, _line, _func);
}

#ifndef ON_WINDOWS
static const pthread_once_t	oncek = PTHREAD_ONCE_INIT;
#endif

struct ThreadData{
	enum {
		MutexPoolSize = 4,
		FirstSpecificId = 0
	};
	//ThreadData();
	//ThreadData():crtthread_key(0), thcnt(0), once_key(PTHREAD_ONCE_INIT){}
	uint32    						thcnt;
#ifndef ON_WINDOWS
	pthread_key_t					crtthread_key;
	pthread_once_t					once_key;
#else
	DWORD							crtthread_key;
#endif
	Condition						gcon;
	Mutex							gmut;
	FastMutexPool<MutexPoolSize>	mutexpool;
	ThreadData():thcnt(0)
#ifndef ON_WINDOWS
		,crtthread_key(0)
		,once_key(oncek){
	}
#else
	{
		crtthread_key = TlsAlloc();
	}
#endif
};

#ifdef HAVE_SAFE_STATIC
static ThreadData& threadData(){
	static ThreadData td;
	return td;
}
#else
ThreadData& threadDataStub(){
	static ThreadData td;
	return td;
}

void once_cbk_thread_data(){
	threadDataStub();
}
static ThreadData& threadData(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_cbk_thread_data, once);
	return threadDataStub();
}


#endif

Cleaner             			cleaner;
//static unsigned 				crtspecid = 0;
//*************************************************************************
/*static*/ const TimeSpec TimeSpec::maximum(0xffffffff, 0xffffffff);
#ifdef NINLINES
#include "system/timespec.ipp"
#endif

/*static*/ TimeSpec TimeSpec::createRealTime(){
	TimeSpec ct;
	return ct.currentRealTime();
}
/*static*/ TimeSpec TimeSpec::createMonotonic(){
	TimeSpec ct;
	return ct.currentMonotonic();
}

#if		defined(ON_WINDOWS)

struct TimeStartData{
#ifdef NWINDOWSQPC
	TimeStartData():start_time(time(NULL)), start_msec(::GetTickCount64()){
	}
#else
	TimeStartData():start_time(time(NULL)){
		QueryPerformanceCounter(&start_msec);
		QueryPerformanceFrequency(&start_freq);
	}
#endif
	static TimeStartData& instance();
	const time_t	start_time;
#ifndef NWINDOWSQPC
	LARGE_INTEGER	start_msec;
	LARGE_INTEGER	start_freq;
#else
	const uint64	start_msec;
#endif
};
 
TimeStartData& tsd_instance_stub(){
	static TimeStartData tsd;
	return tsd;
}

void once_cbk_tsd(){
 	tsd_instance_stub();
}
 
TimeStartData& TimeStartData::instance(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_cbk_tsd, once);
	return tsd_instance_stub();
}

const TimeSpec& TimeSpec::currentRealTime(){
#ifdef NWINDOWSQPC
	const TimeStartData	&tsd = TimeStartData::instance();
	const ULONGLONG	msecs = ::GetTickCount64() - tsd.start_msec;

	const ulong		secs = static_cast<ulong>(msecs / 1000);
	const ulong		nsecs = (msecs % 1000) * 1000 * 1000;
	this->seconds(tsd.start_time + secs);
	this->nanoSeconds(nsecs);
#else
	const TimeStartData	&tsd = TimeStartData::instance();
	LARGE_INTEGER		ms;
	
	QueryPerformanceCounter(&ms);
	const uint64 qpc = ms.QuadPart - tsd.start_msec.QuadPart;
	const uint32 secs = static_cast<uint32>(qpc / tsd.start_freq.QuadPart);
	const uint32 nsecs = static_cast<uint32>(((1000 * (qpc % tsd.start_freq.QuadPart))/tsd.start_freq.QuadPart) * 1000 * 1000);
	this->seconds(tsd.start_time + secs);
	this->nanoSeconds(nsecs);
#endif
	return *this;
}

const TimeSpec& TimeSpec::currentMonotonic(){
#ifdef NWINDOWSQPC
	const ULONGLONG	msecs = ::GetTickCount64();
	const ulong		secs = msecs / 1000;
	const ulong		nsecs = (msecs % 1000) * 1000 * 1000;
	this->seconds(secs);
	this->nanoSeconds(nsecs);
#else
	const TimeStartData	&tsd = TimeStartData::instance();
	LARGE_INTEGER		ms;
	
	QueryPerformanceCounter(&ms);
	const uint64 qpc = ms.QuadPart;
	const uint32 secs = static_cast<uint32>(qpc / tsd.start_freq.QuadPart);
	const uint32 nsecs = static_cast<uint32>(((1000 * (qpc % tsd.start_freq.QuadPart))/tsd.start_freq.QuadPart) * 1000 * 1000);
	this->seconds(secs);
	this->nanoSeconds(nsecs);
#endif
	return *this;
}

#elif	defined(ON_MACOS)

struct TimeStartData{
	TimeStartData(){
		st = time(NULL);
		stns = mach_absolute_time();
		stns -= (stns % 1000000000);
	}
	uint64	stns;
	time_t	st;
};
struct HelperMatchTimeBase: mach_timebase_info_data_t{
    HelperMatchTimeBase(){
		this->denom = 0;
		this->numer = 0;
		::mach_timebase_info((mach_timebase_info_data_t*)this);
	}
};
const TimeSpec& TimeSpec::currentRealTime(){
	static TimeStartData		tsd;
	static HelperMatchTimeBase	info;
	uint64				difference = mach_absolute_time() - tsd.stns;

	uint64 elapsednano = difference * (info.numer / info.denom);

	this->seconds(tsd.st + elapsednano / 1000000000);
	this->nanoSeconds(elapsednano % 1000000000);
	return *this;
}

const TimeSpec& TimeSpec::currentMonotonic(){
	static uint64			tsd(mach_absolute_time());
	static HelperMatchTimeBase	info;
	uint64				difference = mach_absolute_time() - tsd;

	uint64 elapsednano = difference * (info.numer / info.denom);

	this->seconds(elapsednano / 1000000000);
	this->nanoSeconds(elapsednano % 1000000000);
	return *this;
}

#else

const TimeSpec& TimeSpec::currentRealTime(){
	clock_gettime(CLOCK_REALTIME, this);
	return *this;
}

const TimeSpec& TimeSpec::currentMonotonic(){
	clock_gettime(CLOCK_MONOTONIC, this);
	return *this;
}

#endif

#if 	defined(USTLMUTEX)
#elif	defined(UBOOSTMUTEX)
#else
//*************************************************************************
#ifdef NINLINES
#include "system/mutex.ipp"
#endif
//*************************************************************************
#ifdef NINLINES
#include "system/condition.ipp"
#endif
//*************************************************************************
#ifdef NINLINES
#include "system/synchronization.ipp"
#endif
//-------------------------------------------------------------------------
int Condition::wait(Locker<Mutex> &_lock, const TimeSpec &_ts){
	return pthread_cond_timedwait(&cond,&_lock.m.mut, &_ts);
}
//-------------------------------------------------------------------------
int Mutex::timedLock(const TimeSpec &_rts){
#if defined (ON_MACOS)
    return -1;
#else
	return pthread_mutex_timedlock(&mut,&_rts);
#endif
}
//-------------------------------------------------------------------------
int Mutex::reinit(Type _type){
#ifdef UPOSIXMUTEX
	pthread_mutex_destroy(&mut);
	pthread_mutexattr_t att;
	pthread_mutexattr_init(&att);
	pthread_mutexattr_settype(&att, (int)_type);
	return pthread_mutex_init(&mut,&att);
#else
	return -1;
#endif
}
#endif
//*************************************************************************
struct MainThread: Thread{
#ifdef ON_WINDOWS
	MainThread(bool _detached = true, HANDLE _th = NULL):Thread(_detached, _th){}
#else
	MainThread(bool _detached = true, pthread_t _th = 0):Thread(_detached, _th){}
#endif
	void run(){}
};
/*static*/ void Thread::init(){
#ifdef ON_WINDOWS
	TlsSetValue(threadData().crtthread_key, NULL);
	static MainThread	t(false, GetCurrentThread());
	Thread::current(&t);
#else
	if(pthread_key_create(&threadData().crtthread_key, NULL)) throw -1;
	static MainThread	t(false, pthread_self());
	Thread::current(&t);
#endif
}
//-------------------------------------------------------------------------
void Thread::cleanup(){
#ifdef ON_WINDOWS
	TlsFree(threadData().crtthread_key);
#else
	pthread_key_delete(threadData().crtthread_key);
#endif
}
//-------------------------------------------------------------------------
void Thread::sleep(ulong _msec){
#ifdef ON_WINDOWS
	Sleep(_msec);
#else
	usleep(_msec*1000);
#endif
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
#ifdef ON_WINDOWS
	return NULL;
#else
	return reinterpret_cast<Thread*>(pthread_getspecific(threadData().crtthread_key));
#endif
}
//-------------------------------------------------------------------------
long Thread::processId(){
#ifdef ON_WINDOWS
	return _getpid();
#else
	return getpid();
#endif
}
//-------------------------------------------------------------------------
#ifdef ON_WINDOWS
Thread::Thread(bool _detached, void* _th):th(_th), dtchd(_detached), pthrstub(NULL){
}
#else
Thread::Thread(bool _detached, pthread_t _th):th(_th), dtchd(_detached), pthrstub(NULL){
}
#endif
//-------------------------------------------------------------------------
Thread::~Thread(){
	for(SpecVecT::iterator it(specvec.begin()); it != specvec.end(); ++it){
		if(it->first){
			cassert(it->second);
			(*it->second)(it->first);
		}
	}
}
//-------------------------------------------------------------------------
#ifdef ON_WINDOWS
long Thread::currentId(){
	return (long)GetCurrentThreadId();
}
void Thread::yield(){
	SwitchToThread();
}
#endif

//-------------------------------------------------------------------------
void Thread::dummySpecificDestroy(void*){
}
//-------------------------------------------------------------------------
/*static*/ unsigned Thread::processorCount(){
#if		defined(ON_SOLARIS)
	return 1;
#elif	defined(ON_FREEBSD)
	return 1;//pmc_ncpu();//sysconf(_SC_NPROCESSORS_ONLN)
#elif	defined(ON_MACOS)
    return 1;
#elif	defined(ON_WINDOWS)
	return 1;
#else
	return get_nprocs();
#endif
}
//-------------------------------------------------------------------------
int Thread::join(){
#ifdef ON_WINDOWS
	if(detached()) return NOK;
	WaitForSingleObject(th, INFINITE);
	return 0;
#else
	if(pthread_equal(th, pthread_self())) return NOK;
	if(detached()) return NOK;
	int rcode =  pthread_join(this->th, NULL);
	return rcode;
#endif
}
//-------------------------------------------------------------------------
int Thread::detached() const{
	//Locker<Mutex> lock(mutex());
	return dtchd;
}
//-------------------------------------------------------------------------
int Thread::detach(){
#ifdef ON_WINDOWS
	Locker<Mutex> lock(mutex());
	if(detached()) return OK;
	dtchd = 1;
	return 0;
#else
	Locker<Mutex> lock(mutex());
	if(detached()) return OK;
	int rcode = pthread_detach(this->th);
	if(rcode == OK)	dtchd = 1;
	return rcode;
#endif
}
//-------------------------------------------------------------------------
#ifdef HAVE_SAFE_STATIC
unsigned Thread::specificId(){
	static unsigned sid = ThreadData::FirstSpecificId - 1;
	Locker<Mutex> lock(gmutex());
	return ++sid;
}
#else

unsigned specificIdStub(){
	static unsigned sid = ThreadData::FirstSpecificId - 2;
	Locker<Mutex> lock(Thread::gmutex());
	return ++sid;
}

void once_cbk_specific_id(){
	specificIdStub();
}

unsigned Thread::specificId(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_cbk_specific_id, once);
	return specificIdStub();
}

#endif
//-------------------------------------------------------------------------
void Thread::specific(unsigned _pos, void *_psd, SpecificFncT _pf){
	Thread *pct = current();
	cassert(pct);
	if(_pos >= pct->specvec.size()) pct->specvec.resize(_pos + 4);
	//This is safe because pair will initialize with NULL on resize
	if(pct->specvec[_pos].first){
		(*pct->specvec[_pos].second)(pct->specvec[_pos].first);
	}
	pct->specvec[_pos] = SpecPairT(_psd, _pf);
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
#ifdef ON_WINDOWS
	TlsSetValue(threadData().crtthread_key, _ptb);
	return OK;
#else
	pthread_setspecific(threadData().crtthread_key, _ptb);
	return OK;
#endif
}
//-------------------------------------------------------------------------
Mutex& Thread::mutex()const{
	return threadData().mutexpool.getr(this);
}
//-------------------------------------------------------------------------
#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN 4096
#endif
struct Thread::ThreadStub{
	ThreadStub(
		Condition *_pcnd = NULL,
		int *_pval = NULL
	):pcnd(_pcnd), pval(_pval){}
	Condition	*pcnd;
	int			*pval;
};
int Thread::start(bool _wait, bool _detached, ulong _stacksz){
#ifdef ON_WINDOWS
	if(_wait){
		Locker<Mutex>	lock(mutex());
		Condition		cnd;
		int				val(1);
		ThreadStub		thrstub(&cnd, &val);
		if(th){
			return BAD;
		}
		pthrstub = &thrstub;
		{
			Locker<Mutex>	lock2(gmutex());
			Thread::enter();
		}
		if(_detached){
			dtchd = 1;
		}
		th = CreateThread(NULL, _stacksz, (LPTHREAD_START_ROUTINE)&Thread::th_run, this, 0, NULL);
		if(th == NULL){
			{
				Locker<Mutex>	lock2(gmutex());
				Thread::exit();
			}
			pthrstub = NULL;
			return BAD;
		}
		while(val){
			cnd.wait(lock);
		}
	}else{
		Locker<Mutex>	lock(mutex());
		if(th){
			return BAD;
		}
		{
			Locker<Mutex>	lock2(gmutex());
			Thread::enter();
		}
		if(_detached){
			dtchd = 1;
		}
		th = CreateThread(NULL, _stacksz, (LPTHREAD_START_ROUTINE)&Thread::th_run, this, 0, NULL);
		if(th == NULL){
			{
				Locker<Mutex>	lock2(gmutex());
				Thread::exit();
			}
			return BAD;
		}
	}
	return OK;
#else
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	if(_detached){
		if(pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED)){
			pthread_attr_destroy(&attr);
			edbgx(Dbg::system, "pthread_attr_setdetachstate: "<<strerror(errno));
			return BAD;
		}
	}
	if(_stacksz){
		if(_stacksz < PTHREAD_STACK_MIN){
			_stacksz = PTHREAD_STACK_MIN;
		}
		int rv = pthread_attr_setstacksize(&attr, _stacksz);
		if(rv){
			edbgx(Dbg::system, "pthread_attr_setstacksize "<<_stacksz<<": "<<strerror(errno));
			pthread_attr_destroy(&attr);
			return BAD;
		}
	}

	if(_wait){
		Locker<Mutex>	lock(mutex());
		Condition		cnd;
		int				val(1);
		ThreadStub		thrstub(&cnd, &val);
		if(th){
			pthread_attr_destroy(&attr);
			return BAD;
		}
		pthrstub = &thrstub;
		{
			Locker<Mutex>	lock2(gmutex());
			Thread::enter();
		}
		if(_detached){
			dtchd = 1;
		}
		if(pthread_create(&th,&attr,&Thread::th_run,this)){
			edbgx(Dbg::system, "pthread_create: "<<strerror(errno));
			pthread_attr_destroy(&attr);
			th = 0;
			pthrstub = NULL;
			{
				Locker<Mutex>	lock2(gmutex());
				Thread::exit();
			}
			return BAD;
		}
		idbgx(Dbg::system, "started thread "<<th);
		while(val){
			cnd.wait(lock);
		}
	}else{
		Locker<Mutex>	lock(mutex());
		if(th){
			pthread_attr_destroy(&attr);
			return BAD;
		}
		{
			Locker<Mutex>	lock2(gmutex());
			Thread::enter();
		}
		if(_detached){
			dtchd = 1;
		}
		if(pthread_create(&th,&attr,&Thread::th_run,this)){
			edbgx(Dbg::system, "pthread_create: "<<strerror(errno));
			pthread_attr_destroy(&attr);
			th = 0;
			{
				Locker<Mutex>	lock2(gmutex());
				Thread::exit();
			}
			return BAD;
		}
		idbgx(Dbg::system, "started thread "<<th);
	}

	pthread_attr_destroy(&attr);
	vdbgx(Dbg::system, "");
	return OK;
#endif
}
//-------------------------------------------------------------------------
void Thread::signalWaiter(){
	*(pthrstub->pval) = 0;
	pthrstub->pcnd->signal();
	pthrstub = NULL;
}
//-------------------------------------------------------------------------
int Thread::waited(){
	return pthrstub != NULL;
}
//-------------------------------------------------------------------------
void Thread::waitAll(){
	ThreadData &td(threadData());
    Locker<Mutex> lock(td.gmut);
    while(td.thcnt != 0) td.gcon.wait(lock);
}
//-------------------------------------------------------------------------
#ifdef ON_WINDOWS
unsigned long Thread::th_run(void *pv){
	vdbgx(Dbg::system, "thrun enter "<<pv);
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
	vdbgx(Dbg::system, "thrun exit "<<pv);
	Thread::exit();
	return NULL;
}

#else
void* Thread::th_run(void *pv){
	vdbgx(Dbg::system, "thrun enter "<<pv);
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
	vdbgx(Dbg::system, "thrun exit "<<pv);
	Thread::exit();
	return NULL;
}
#endif
//-------------------------------------------------------------------------
