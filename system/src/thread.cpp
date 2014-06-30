// system/src/thread.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <cerrno>
#include <cstring>
#include <limits.h>

#include "system/timespec.hpp"
#include "system/thread.hpp"
#include "system/debug.hpp"
#include "system/condition.hpp"
#include "system/exception.hpp"
#include "system/cassert.hpp"
#include "system/synchronization.hpp"
#include "system/atomic.hpp"

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

namespace solid{

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
#else
#endif//ON_WINDOWS

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

#ifdef HAS_SAFE_STATIC
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
bool Condition::wait(Locker<Mutex> &_lock, const TimeSpec &_ts){
	const int rv = pthread_cond_timedwait(&cond,&_lock.m.mut, &_ts);
	if(rv == 0){
		return true;
	}else if(rv == ETIMEDOUT){
		return false;
	}else{
		cassert(false);
		return false;
	}
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
namespace{
struct DummyThread: Thread{
#ifdef ON_WINDOWS
	DummyThread(HANDLE _th = NULL):Thread(true, _th){}
#else
	DummyThread(pthread_t _th = 0):Thread(true, _th){}
#endif
	void run(){}
};

}//namespace

#ifdef ON_WINDOWS
#else
void Thread::free_thread(void *_pth){
	delete static_cast<Thread*>(_pth);
}
#endif

/*static*/ void Thread::init(){
#ifdef ON_WINDOWS
	TlsSetValue(threadData().crtthread_key, NULL);
#else
	cverify(!pthread_key_create(&threadData().crtthread_key, &Thread::free_thread));
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
Thread* Thread::associateToCurrent(){
#ifdef ON_WINDOWS
	Thread *pth = new DummyThread(GetCurrentThread());
#else
	Thread *pth = new DummyThread(pthread_self());
#endif
	Thread::current(pth);
	return pth;
}
//-------------------------------------------------------------------------
Thread& Thread::current(){
#ifdef ON_WINDOWS
	Thread * pth = reinterpret_cast<Thread*>(TlsGetValue(threadData().crtthread_key));
	return pth ? *pth : *associate_to_current_thread();
#else
	Thread * pth = reinterpret_cast<Thread*>(pthread_getspecific(threadData().crtthread_key));
	return pth ? *pth : *associateToCurrent();
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
#elif	defined(ON_FREEBSD) || defined(ON_MACOS)
	int count;
    size_t size=sizeof(count);
    return sysctlbyname("hw.ncpu",&count,&size,NULL,0)?0:count;
#elif	defined(ON_WINDOWS)
	SYSTEM_INFO info={{0}};
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
#else
	return get_nprocs();
#endif
}
//-------------------------------------------------------------------------
bool Thread::join(){
	specific_error_clear();
#ifdef ON_WINDOWS
	if(detached()) return false;
	WaitForSingleObject(th, INFINITE);
	return true;
#else
	if(pthread_equal(th, pthread_self())) return false;
	if(detached()) return false;
	int rv =  pthread_join(this->th, NULL);
	if(rv < 0){
		SPECIFIC_ERROR_PUSH1(last_system_error());
		return false;
	}
	return true;
#endif
}
//-------------------------------------------------------------------------
bool Thread::detached() const{
	//Locker<Mutex> lock(mutex());
	return dtchd;
}
//-------------------------------------------------------------------------
bool Thread::detach(){
#ifdef ON_WINDOWS
	Locker<Mutex> lock(mutex());
	if(detached()) return true;
	dtchd = true;
	return false;
#else
	Locker<Mutex> lock(mutex());
	if(detached()) return true;
	int rcode = pthread_detach(this->th);
	if(rcode == 0){
		dtchd = true;
		return true;
	}
	return false;
#endif
}
//-------------------------------------------------------------------------
typedef ATOMIC_NS::atomic<size_t>			AtomicSizeT;

#ifdef HAS_SAFE_STATIC
size_t Thread::specificId(){
	static AtomicSizeT sid((size_t)ThreadData::FirstSpecificId);
	return sid.fetch_add(1/*, ATOMIC_NS::memory_order_seq_cst*/);
}
#else

size_t specificIdStub(){
	static AtomicSizeT sid(ATOMIC_VAR_INIT((size_t)ThreadData::FirstSpecificId));
	return sid.fetch_add(1/*, ATOMIC_NS::memory_order_seq_cst*/);
}

void once_cbk_specific_id(){
	specificIdStub();
}

size_t Thread::specificId(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_cbk_specific_id, once);
	return specificIdStub();
}

#endif
//-------------------------------------------------------------------------
void Thread::specific(unsigned _pos, void *_psd, SpecificFncT _pf){
	Thread &rct = current();
	if(_pos >= rct.specvec.size()) rct.specvec.resize(_pos + 4);
	//This is safe because pair will initialize with NULL on resize
	if(rct.specvec[_pos].first){
		(*rct.specvec[_pos].second)(rct.specvec[_pos].first);
	}
	rct.specvec[_pos] = SpecPairT(_psd, _pf);
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
	cassert(_pos < current().specvec.size());
	return current().specvec[_pos].first;
}
Mutex& Thread::gmutex(){
	return threadData().gmut;
}
//-------------------------------------------------------------------------
void Thread::current(Thread *_ptb){
#ifdef ON_WINDOWS
	TlsSetValue(threadData().crtthread_key, _ptb);
#else
	pthread_setspecific(threadData().crtthread_key, _ptb);
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
bool Thread::start(bool _wait, bool _detached, ulong _stacksz){
	specific_error_clear();
#ifdef ON_WINDOWS
	if(_wait){
		Locker<Mutex>	lock(mutex());
		Condition		cnd;
		int				val(1);
		ThreadStub		thrstub(&cnd, &val);
		if(th){
			
			return false;
		}
		pthrstub = &thrstub;
		{
			Locker<Mutex>	lock2(gmutex());
			Thread::enter();
		}
		if(_detached){
			dtchd = true;
		}
		th = CreateThread(NULL, _stacksz, (LPTHREAD_START_ROUTINE)&Thread::th_run, this, 0, NULL);
		if(th == NULL){
			{
				Locker<Mutex>	lock2(gmutex());
				Thread::exit();
			}
			pthrstub = NULL;
			return false;
		}
		while(val){
			cnd.wait(lock);
		}
	}else{
		Locker<Mutex>	lock(mutex());
		if(th){
			return false;
		}
		{
			Locker<Mutex>	lock2(gmutex());
			Thread::enter();
		}
		if(_detached){
			dtchd = true;
		}
		th = CreateThread(NULL, _stacksz, (LPTHREAD_START_ROUTINE)&Thread::th_run, this, 0, NULL);
		if(th == NULL){
			{
				Locker<Mutex>	lock2(gmutex());
				Thread::exit();
			}
			return false;
		}
	}
	return true;
#else
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	if(_detached){
		if(pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED)){
			pthread_attr_destroy(&attr);
			edbgx(Debug::system, "pthread_attr_setdetachstate: "<<strerror(errno));
			return false;
		}
	}
	if(_stacksz){
		if(_stacksz < PTHREAD_STACK_MIN){
			_stacksz = PTHREAD_STACK_MIN;
		}
		int rv = pthread_attr_setstacksize(&attr, _stacksz);
		if(rv){
			edbgx(Debug::system, "pthread_attr_setstacksize "<<_stacksz<<": "<<strerror(errno));
			pthread_attr_destroy(&attr);
			return false;
		}
	}

	if(_wait){
		Locker<Mutex>	lock(mutex());
		Condition		cnd;
		int				val(1);
		ThreadStub		thrstub(&cnd, &val);
		if(th){
			pthread_attr_destroy(&attr);
			return false;
		}
		pthrstub = &thrstub;
		{
			Locker<Mutex>	lock2(gmutex());
			Thread::enter();
		}
		if(_detached){
			dtchd = true;
		}
		if(pthread_create(&th,&attr,&Thread::th_run,this)){
			edbgx(Debug::system, "pthread_create: "<<strerror(errno));
			pthread_attr_destroy(&attr);
			th = 0;
			pthrstub = NULL;
			{
				Locker<Mutex>	lock2(gmutex());
				Thread::exit();
			}
			return false;
		}
		idbgx(Debug::system, "started thread "<<th);
		while(val){
			cnd.wait(lock);
		}
	}else{
		Locker<Mutex>	lock(mutex());
		if(th){
			pthread_attr_destroy(&attr);
			return false;
		}
		{
			Locker<Mutex>	lock2(gmutex());
			Thread::enter();
		}
		if(_detached){
			dtchd = true;
		}
		if(pthread_create(&th,&attr,&Thread::th_run,this)){
			edbgx(Debug::system, "pthread_create: "<<strerror(errno));
			pthread_attr_destroy(&attr);
			th = 0;
			{
				Locker<Mutex>	lock2(gmutex());
				Thread::exit();
			}
			return false;
		}
		idbgx(Debug::system, "started thread "<<th);
	}
	pthread_attr_destroy(&attr);
	vdbgx(Debug::system, "");
	return true;
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
	vdbgx(Debug::system, "thrun enter "<<pv);
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
	if(!pth->detached()){
		Thread::current(NULL);
	}
	vdbgx(Debug::system, "thrun exit "<<pv);
	Thread::exit();
	return NULL;
}

#else
void* Thread::th_run(void *pv){
	vdbgx(Debug::system, "thrun enter "<<pv);
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
	if(pth->detached()){
		delete pth;
	}
	Thread::current(NULL);
	vdbgx(Debug::system, "thrun exit "<<pv);
	Thread::exit();
	return NULL;
}
#endif
//-------------------------------------------------------------------------
}//namespace solid
