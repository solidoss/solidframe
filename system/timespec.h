/* Declarations file timespec.h
	
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

#ifndef SYSTEM_TIMESPEC_H
#define SYSTEM_TIMESPEC_H

#include <time.h>
#include "convertors.h"
//#include <sys/times.h>

struct TimeSpec: public timespec{
	typedef UnsignedConvertor<time_t>::UnsignedType TimeTp;
	
	TimeSpec(const TimeTp &_s = 0, long _ns = 0){set(_s, _ns);}
	TimeTp	seconds()const{return tv_sec;}
	long	nanoSeconds()const{return tv_nsec;}
	void seconds(const TimeTp &_s){tv_sec = _s;}
	void nanoSeconds(long _ns){tv_nsec = _ns;}
	void set(const TimeTp &_s, long _ns = 0){tv_sec = _s;tv_nsec = _ns;}
	bool operator >=(const TimeSpec &_ts)const{
		return (seconds() > _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec >= _ts.tv_nsec));
	}
	bool operator >(const TimeSpec &_ts)const{
		return (seconds() > _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec > _ts.tv_nsec));
	}
	bool operator <=(const TimeSpec &_ts)const{
		return (seconds() < _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec <= _ts.tv_nsec));
	}
	bool operator <(const TimeSpec &_ts)const{
		return (seconds() < _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec < _ts.tv_nsec));
	}
	operator bool () const{
		return tv_sec | tv_nsec;
	}
	TimeSpec& operator += (unsigned _msec){
		tv_sec += _msec / 1000;
		tv_nsec += (_msec % 1000) * 1000000;
		if(tv_nsec >= 1000000000){++tv_sec; tv_nsec %= 1000000000;}
		return *this;
	}
};

inline TimeSpec operator-(const TimeSpec &_ts1, const TimeSpec &_ts2){
	//assert(_ts1 > _ts2);
	TimeSpec ts = _ts1;
	ts.tv_sec -= _ts2.tv_sec;
	ts.tv_nsec -= _ts2.tv_nsec;
	if(ts.tv_nsec < 0){ 
		--ts.tv_sec; ts.tv_nsec = 1000000000 + (ts.tv_nsec % 1000000000);
	}
	return ts;
}

#endif
