/* Declarations file common.hpp
	
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

#ifndef SYSTEM_COMMON_HPP
#define SYSTEM_COMMON_HPP

#include <cstdlib>
#include "config.h"

#ifndef HAS_SAFE_STATIC
#include <boost/thread/once.hpp>
#endif

#ifdef ON_WINDOWS
//#ifdef HAS_CPP11
//	#define USTLMUTEX
//#else
	#define UBOOSTMUTEX
	#define UBOOSTSHAREDPTR
//#endif
#endif

typedef unsigned char		uchar;
typedef unsigned int		uint;

typedef unsigned long       ulong;
typedef unsigned short      ushort;

typedef long long			longlong;
typedef unsigned long long	ulonglong;

typedef signed char			int8;
typedef unsigned char		uint8;

typedef signed short		int16;
typedef unsigned short		uint16;

typedef unsigned int		uint32;
typedef signed int			int32;
// #if defined(U_WIN) && !defined(U_GCC)
#if defined(U_WIN)
typedef __int64				int64;
typedef unsigned __int64	uint64;
#else
typedef unsigned long long	uint64;
typedef signed long long 	int64;
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


struct EmptyType{};
class NullType{};

template <class T>
struct UnsignedType;

template <>
struct UnsignedType<int8>{
    typedef uint8 Type;
};

template <>
struct UnsignedType<int16>{
    typedef uint16 Type;
};

template <>
struct UnsignedType<int32>{
    typedef uint32 Type;
};

template <>
struct UnsignedType<long>{
    typedef ulong Type;
};

template <>
struct UnsignedType<ulong>{
    typedef ulong Type;
};


template <>
struct UnsignedType<int64>{
    typedef uint64 Type;
};

template <>
struct UnsignedType<uint8>{
    typedef uint8 Type;
};

template <>
struct UnsignedType<uint16>{
    typedef uint16 Type;
};

template <>
struct UnsignedType<uint32>{
    typedef uint32 Type;
};

template <>
struct UnsignedType<uint64>{
    typedef uint64 Type;
};

const char* src_file_name(char const *_fname);


#endif
