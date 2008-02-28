/* Inline implementation file timespec.ipp
	
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

#ifndef UINLINES
#define inline
#endif


inline void TimeSpec::set(const TimeTp &_s, long _ns){
	tv_sec = _s;tv_nsec = _ns;
}

inline TimeSpec operator-(const TimeSpec &_ts1, const TimeSpec &_ts2){
	//cassert(_ts1 > _ts2);
	TimeSpec ts = _ts1;
	ts.tv_sec -= _ts2.tv_sec;
	ts.tv_nsec -= _ts2.tv_nsec;
	if(ts.tv_nsec < 0){ 
		--ts.tv_sec; ts.tv_nsec = 1000000000 + (ts.tv_nsec % 1000000000);
	}
	return ts;
}

inline void TimeSpec::add(const TimeTp &_s, long _ns){
	tv_sec += _s;
	tv_nsec += _ns;
	tv_sec += tv_nsec/1000000000;
	tv_nsec %= 1000000000;
}

inline bool TimeSpec::operator >=(const TimeSpec &_ts)const{
	return (seconds() > _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec >= _ts.tv_nsec));
}

inline bool TimeSpec::operator >(const TimeSpec &_ts)const{
	return (seconds() > _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec > _ts.tv_nsec));
}

inline bool TimeSpec::operator <=(const TimeSpec &_ts)const{
	return (seconds() < _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec <= _ts.tv_nsec));
}

inline bool TimeSpec::operator <(const TimeSpec &_ts)const{
	return (seconds() < _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec < _ts.tv_nsec));
}

inline TimeSpec& TimeSpec::operator += (unsigned _msec){
	tv_sec += _msec / 1000;
	tv_nsec += (_msec % 1000) * 1000000;
	tv_sec += tv_nsec/1000000000;
	tv_nsec %= 1000000000;
	return *this;
}

#ifndef UINLINES
#undef inline
#endif

