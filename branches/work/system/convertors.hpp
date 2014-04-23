// system/convertors.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_CONVERTORS_HPP
#define SYSTEM_CONVERTORS_HPP

#include "common.hpp"
#ifdef ON_WINDOWS
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif
namespace solid{
//!A template convertor to an unsigned
/*!
	it convert a basic type to it unsigned equivalent:<br>
	int -> unsigned int<br>
	long -> unsigned long<br>
	usigned long -> unsigned long<br>
	etc.<br>
*/
template <class T>
struct UnsignedConvertor;

template <>
struct UnsignedConvertor<int16>{
	typedef uint16 UnsignedType;
};

template <>
struct UnsignedConvertor<int32>{
	typedef uint32 UnsignedType;
};

template <>
struct UnsignedConvertor<int64>{
	typedef uint64 UnsignedType;
};

template <>
struct UnsignedConvertor<uint16>{
	typedef uint16 UnsignedType;
};

template <>
struct UnsignedConvertor<uint32>{
	typedef uint32 UnsignedType;
};

template <>
struct UnsignedConvertor<uint64>{
	typedef uint64 UnsignedType;
};

#if UWORDSIZE == 32
template <>
struct UnsignedConvertor<long>{
	typedef ulong UnsignedType;
};

template <>
struct UnsignedConvertor<ulong>{
	typedef ulong UnsignedType;
};

#endif

#define BitsToMask(v) ((1 << (v)) - 1)
#define BitsToCount(v) ((1 << (v)) & (~1))

inline uint32 bitsToMask32(unsigned v){
	return (1 << v) - 1;
}
inline uint32 bitsToCount32(unsigned v){
	return (1 << v)  & (~static_cast<uint32>(1));
}

inline uint64 bitsToMask64(unsigned v){
	return (1 << v) - 1;
}
inline uint64 bitsToCount64(unsigned v){
	return (1 << v) & (~static_cast<uint64>(1));
}

inline size_t bitsToMask(unsigned v){
	return (static_cast<size_t>(1) << v) - 1;
}
inline size_t bitsToCount(unsigned v){
	return (1 << v) & (~static_cast<size_t>(1));
}


inline uint32 toNetwork(uint32 _v){
	return htonl(_v);
}

inline uint32 toHost(uint32 _v){
	return ntohl(_v);
}

inline uint16 toNetwork(uint16 _v){
	return htons(_v);
}

inline uint16 toHost(uint16 _v){
	return ntohs(_v);
}

}//namespace solid

#endif
