// solid/system/nanotime.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_NANO_TIME_HPP
#define SYSTEM_NANO_TIME_HPP

#include <time.h>
#include "convertors.hpp"

#ifdef SOLID_ON_WINDOWS
struct timespec{
    time_t  tv_sec;
    long    tv_nsec;
};
#endif

namespace solid{

//! A timespec wrapper
/*!
    Basicaly it is a pair of seconds and nanoseconds.
*/
struct NanoTime: public timespec{
    typedef UnsignedConvertor<time_t>::UnsignedType TimeT;
    
    static const NanoTime maximum;
    
    static NanoTime createRealTime();
    static NanoTime createMonotonic();
    
    //! Current of the wall time
    const NanoTime& currentRealTime();
    //! Current time lapsed from a fixed moment - e.g. computer power-up
    const NanoTime& currentMonotonic();
    
    NanoTime(const TimeT &_s = 0, long _ns = 0){set(_s, _ns);}
    TimeT   seconds()const{return tv_sec;}
    bool isMax()const;
    long nanoSeconds()const{return tv_nsec;}
    void seconds(const TimeT &_s){tv_sec = _s;}
    void nanoSeconds(long _ns){tv_nsec = _ns;}
    void set(const TimeT &_s, long _ns = 0);
    void add(const TimeT &_s, long _ns = 0);
    void sub(const TimeT &_s, long _ns = 0);
    bool operator !=(const NanoTime &_ts)const;
    bool operator ==(const NanoTime &_ts)const;
    bool operator >=(const NanoTime &_ts)const;
    bool operator >(const NanoTime &_ts)const;
    bool operator <=(const NanoTime &_ts)const;
    bool operator <(const NanoTime &_ts)const;
    NanoTime& operator += (const NanoTime &_ts);
    NanoTime& operator -= (const NanoTime &_ts);
    //operator bool () const{   return (tv_sec | tv_nsec) != 0;}
    NanoTime& operator += (unsigned _msec);
};

NanoTime operator-(const NanoTime &_ts1, const NanoTime &_ts2);
NanoTime operator+(const NanoTime &_ts1, unsigned _msec);

#ifndef SOLID_HAS_NO_INLINES
#include "solid/system/nanotime.ipp"
#endif

}//namespace solid

#endif
