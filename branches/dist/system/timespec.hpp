/* Declarations file timespec.hpp
	
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

#ifndef SYSTEM_TIMESPEC_HPP
#define SYSTEM_TIMESPEC_HPP

#include <time.h>
#include "convertors.hpp"
//#include <sys/times.h>
//! A timespec wrapper
/*!
	Basicaly it is a pair of seconds and nanoseconds.
*/
struct TimeSpec: public timespec{
	typedef UnsignedConvertor<time_t>::UnsignedType TimeT;
	
	static const TimeSpec max;
	
	TimeSpec(const TimeT &_s = 0, long _ns = 0){set(_s, _ns);}
	TimeT	seconds()const{return tv_sec;}
	bool isMax()const;
	long	nanoSeconds()const{return tv_nsec;}
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
	operator bool () const{	return tv_sec | tv_nsec;}
	TimeSpec& operator += (unsigned _msec);
};

TimeSpec operator-(const TimeSpec &_ts1, const TimeSpec &_ts2);

#ifndef NINLINES
#include "system/timespec.ipp"
#endif


#endif
