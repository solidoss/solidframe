// system/nanotime.ipp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifdef SOLID_HAS_NO_INLINES
#define inline
#endif


inline void NanoTime::set(const TimeT &_s, long _ns){
	tv_sec = _s;tv_nsec = _ns;
}

inline NanoTime operator-(const NanoTime &_ts1, const NanoTime &_ts2){
	//SOLID_ASSERT(_ts1 > _ts2);
	NanoTime ts = _ts1;
	ts.tv_sec -= _ts2.tv_sec;
	ts.tv_nsec -= _ts2.tv_nsec;
	if(ts.tv_nsec < 0){ 
		--ts.tv_sec; ts.tv_nsec = 1000000000 + (ts.tv_nsec % 1000000000);
	}
	return ts;
}
inline NanoTime operator+(const NanoTime &_ts1, unsigned _msec){
	NanoTime rv = _ts1;
	rv += _msec;
	return rv;
}
inline void NanoTime::add(const TimeT &_s, long _ns){
	tv_sec += _s;
	tv_nsec += _ns;
	tv_sec += tv_nsec/1000000000;
	tv_nsec %= 1000000000;
}

inline void NanoTime::sub(const TimeT &_s, long _ns){
	tv_sec -= _s;
	tv_nsec -= _ns;
	tv_sec -= tv_nsec/1000000000;
	tv_nsec %= 1000000000;
}

inline bool NanoTime::operator >=(const NanoTime &_ts)const{
	return (seconds() > _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec >= _ts.tv_nsec));
}

inline bool NanoTime::operator >(const NanoTime &_ts)const{
	return (seconds() > _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec > _ts.tv_nsec));
}

inline bool NanoTime::operator <=(const NanoTime &_ts)const{
	return (seconds() < _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec <= _ts.tv_nsec));
}

inline bool NanoTime::operator <(const NanoTime &_ts)const{
	return (seconds() < _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec < _ts.tv_nsec));
}

inline NanoTime& NanoTime::operator += (unsigned _msec){
	tv_sec += _msec / 1000;
	tv_nsec += (_msec % 1000) * 1000000;
	tv_sec += tv_nsec/1000000000;
	tv_nsec %= 1000000000;
	return *this;
}

inline bool NanoTime::operator !=(const NanoTime &_ts)const{
	return tv_sec != _ts.tv_sec || tv_nsec != _ts.tv_nsec;
}

inline bool NanoTime::operator ==(const NanoTime &_ts)const{
	return tv_sec == _ts.tv_sec && tv_nsec == _ts.tv_nsec;
}

inline NanoTime& NanoTime::operator += (const NanoTime &_ts){
	tv_sec += _ts.seconds();
	tv_nsec += _ts.nanoSeconds();
	tv_sec += tv_nsec/1000000000;
	tv_nsec %= 1000000000;
	return *this;
}
inline NanoTime& NanoTime::operator -= (const NanoTime &_ts){
	tv_sec -= _ts.seconds();
	--tv_sec;
	tv_nsec = (tv_nsec + 1000000000) - _ts.nanoSeconds();
	tv_sec += tv_nsec/1000000000;
	tv_nsec %= 1000000000;
	return *this;
}

inline bool NanoTime::isMax()const{
	return tv_sec == static_cast<decltype(tv_sec)>(-1);
}

#ifdef SOLID_HAS_NO_INLINES
#undef inline
#endif

