// system/src/system.cpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "system/exception.hpp"
#include "system/nanotime.hpp"
#include <cstring>


#if defined(SOLID_ON_FREEBSD)
#include <pmc.h>
#elif defined(SOLID_ON_DARWIN)
#elif defined(SOLID_ON_WINDOWS)
#else
#include <sys/sysinfo.h>
#endif


#if defined(SOLID_ON_WINDOWS)
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

#if defined(SOLID_ON_DARWIN)
#include <mach/mach_time.h>
#endif


namespace solid{

/*static*/ const char* src_file_name(char const *_fname){
#ifdef SOLID_ON_WINDOWS
	static const unsigned fileoff = (strlen(__FILE__) - strlen(strstr(__FILE__, "system\\src")));
#else
	static const unsigned fileoff = (strlen(__FILE__) - strlen(strstr(__FILE__, "system/src")));
#endif
	return _fname + fileoff;
}

//=============================================================================
//	NanoTime
//=============================================================================
/*static*/ const NanoTime NanoTime::maximum(-1, -1);

#ifdef SOLID_HAS_NO_INLINES
#include "system/nanotime.ipp"
#endif

/*static*/ NanoTime NanoTime::createRealTime(){
	NanoTime ct;
	return ct.currentRealTime();
}
/*static*/ NanoTime NanoTime::createMonotonic(){
	NanoTime ct;
	return ct.currentMonotonic();
}

#if		defined(SOLID_ON_WINDOWS)

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
	const uint64_t	start_msec;
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

const NanoTime& NanoTime::currentRealTime(){
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
	const uint64_t qpc = ms.QuadPart - tsd.start_msec.QuadPart;
	const uint32_t secs = static_cast<uint32_t>(qpc / tsd.start_freq.QuadPart);
	const uint32_t nsecs = static_cast<uint32_t>(((1000 * (qpc % tsd.start_freq.QuadPart))/tsd.start_freq.QuadPart) * 1000 * 1000);
	this->seconds(tsd.start_time + secs);
	this->nanoSeconds(nsecs);
#endif
	return *this;
}

const NanoTime& NanoTime::currentMonotonic(){
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
	const uint64_t qpc = ms.QuadPart;
	const uint32_t secs = static_cast<uint32_t>(qpc / tsd.start_freq.QuadPart);
	const uint32_t nsecs = static_cast<uint32_t>(((1000 * (qpc % tsd.start_freq.QuadPart))/tsd.start_freq.QuadPart) * 1000 * 1000);
	this->seconds(secs);
	this->nanoSeconds(nsecs);
#endif
	return *this;
}

#elif	defined(SOLID_ON_DARWIN)

struct TimeStartData{
	TimeStartData(){
		st = time(NULL);
		stns = mach_absolute_time();
		stns -= (stns % 1000000000);
	}
	uint64_t	stns;
	time_t	st;
};
struct HelperMatchTimeBase: mach_timebase_info_data_t{
    HelperMatchTimeBase(){
		this->denom = 0;
		this->numer = 0;
		::mach_timebase_info((mach_timebase_info_data_t*)this);
	}
};
const NanoTime& NanoTime::currentRealTime(){
	static TimeStartData		tsd;
	static HelperMatchTimeBase	info;
	uint64_t				difference = mach_absolute_time() - tsd.stns;

	uint64_t elapsednano = difference * (info.numer / info.denom);

	this->seconds(tsd.st + elapsednano / 1000000000);
	this->nanoSeconds(elapsednano % 1000000000);
	return *this;
}

const NanoTime& NanoTime::currentMonotonic(){
	static uint64_t			tsd(mach_absolute_time());
	static HelperMatchTimeBase	info;
	uint64_t				difference = mach_absolute_time() - tsd;

	uint64_t elapsednano = difference * (info.numer / info.denom);

	this->seconds(elapsednano / 1000000000);
	this->nanoSeconds(elapsednano % 1000000000);
	return *this;
}

#else

const NanoTime& NanoTime::currentRealTime(){
	clock_gettime(CLOCK_REALTIME, this);
	return *this;
}

const NanoTime& NanoTime::currentMonotonic(){
	clock_gettime(CLOCK_MONOTONIC, this);
	return *this;
}

#endif


}//namespace solid
