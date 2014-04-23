// system/common.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
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

namespace solid{

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

#if UWORDSIZE == 64

#if defined(U_WIN)
typedef __int64				int64;
typedef unsigned __int64	uint64;
#else
typedef unsigned long		uint64;
typedef signed long 		int64;
#endif

#define MAX_ULONG			0xffffffffffffffffULL

#else

#if defined(ON_WINDOWS)
typedef __int64				int64;
typedef unsigned __int64	uint64;
#else
typedef unsigned long long	uint64;
typedef signed long long	int64;
#endif

#define MAX_ULONG			0xffffffffUL
#endif

enum SeekRef {
	SeekBeg=0,
	SeekCur=1,
	SeekEnd=2
};

enum AsyncE{
	AsyncError = -1,
	AsyncSuccess = 0,
	AsyncWait = 1,
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

}//namespace solid

#endif
