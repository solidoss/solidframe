/* Declarations file common.hpp
	
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

#ifndef SYSTEM_COMMON_HPP
#define SYSTEM_COMMON_HPP

#include <cstdlib>
#include "config.h"


typedef unsigned char		uchar;
typedef unsigned int		uint;

typedef unsigned long       ulong;
typedef unsigned short      ushort;

typedef long long			longlong;
typedef unsigned long long	ulonglong;

typedef signed char			int8;
typedef unsigned char		uint8;

typedef short				int16;
typedef unsigned short		uint16;

typedef unsigned int		uint32;
typedef int					int32;
// #if defined(U_WIN) && !defined(U_GCC)
#if defined(U_WIN)
typedef __int64				int64;
typedef unsigned __int64	uint64;
#else
typedef unsigned long long	uint64;
typedef long long 			int64;
#endif

#ifdef _LP64
#define MAX_ULONG			0xffffffffffffffffULL
#else
#define MAX_ULONG			0xffffffffUL
#endif

enum SeekRef {
	SeekBeg=0,
	SeekCur=1,
	SeekEnd=2
};

//! Some project wide used return values
enum RetVal{
	BAD = -1,
	OK = 0,
	NOK,
	YIELD,
	CONTINUE
};

template <typename T>
inline T tmax(T v1, T v2){
    return (v1 < v2) ? v2 : v1;
}
template <typename T>
inline T tmin(T v1, T v2){
    return (v1 > v2) ? v2 : v1;
}
//! A fast template inline function for exchanging values
template <typename T>
void exchange(T &a, T &b, T &tmp){
	tmp = a;
	a = b;
	b = tmp;
}
//! A fast template inline function for exchanging values
template <typename T>
void exchange(T &a, T &b){
	T tmp(a);
	a = b;
	b = tmp;
}

struct EmptyType{};
class NullType{};

#endif
