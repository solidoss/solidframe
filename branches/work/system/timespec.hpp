// system/timespec.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_TIMESPEC_HPP
#define SYSTEM_TIMESPEC_HPP

#include <time.h>
#include "convertors.hpp"

#ifdef ON_WINDOWS
struct timespec{
	time_t	tv_sec;
	long	tv_nsec;
};
#endif

namespace solid{

//! A timespec wrapper
/*!
	Basicaly it is a pair of seconds and nanoseconds.
*/
struct TimeSpec: public timespec{
	typedef UnsignedConvertor<time_t>::UnsignedType TimeT;
	
	static const TimeSpec maximum;
	
	static TimeSpec createRealTime();
	static TimeSpec createMonotonic();
	
	//! Current of the wall time
	const TimeSpec& currentRealTime();
	//! Current time lapsed from a fixed moment - e.g. computer power-up
	const TimeSpec& currentMonotonic();
	
	TimeSpec(const TimeT &_s = 0, long _ns = 0){set(_s, _ns);}
	TimeT	seconds()const{return tv_sec;}
	bool isMax()const;
	long nanoSeconds()const{return tv_nsec;}
	void seconds(const TimeT &_s){tv_sec = _s;}
	void nanoSeconds(long _ns){tv_nsec = _ns;}
	void set(const TimeT &_s, long _ns = 0);
	void add(const TimeT &_s, long _ns = 0);
	void sub(const TimeT &_s, long _ns = 0);
	bool operator !=(const TimeSpec &_ts)const;
	bool operator ==(const TimeSpec &_ts)const;
	bool operator >=(const TimeSpec &_ts)const;
	bool operator >(const TimeSpec &_ts)const;
	bool operator <=(const TimeSpec &_ts)const;
	bool operator <(const TimeSpec &_ts)const;
	TimeSpec& operator += (const TimeSpec &_ts);
	TimeSpec& operator -= (const TimeSpec &_ts);
	operator bool () const{	return (tv_sec | tv_nsec) != 0;}
	TimeSpec& operator += (unsigned _msec);
};

TimeSpec operator-(const TimeSpec &_ts1, const TimeSpec &_ts2);

#ifndef NINLINES
#include "system/timespec.ipp"
#endif

}//namespace solid

#endif
