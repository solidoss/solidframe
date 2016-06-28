// solid/system/convertors.hpp
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
#ifdef SOLID_ON_WINDOWS
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
struct UnsignedConvertor<int16_t>{
	typedef uint16_t UnsignedType;
};

template <>
struct UnsignedConvertor<int>{
	typedef unsigned int UnsignedType;
};

template <>
struct UnsignedConvertor<long>{
	typedef ulong UnsignedType;
};

template <>
struct UnsignedConvertor<long long>{
	typedef unsigned long long UnsignedType;
};


template <>
struct UnsignedConvertor<uint16_t>{
	typedef uint16_t UnsignedType;
};

template <>
struct UnsignedConvertor<unsigned int>{
	typedef unsigned int UnsignedType;
};

template <>
struct UnsignedConvertor<unsigned long>{
	typedef unsigned long UnsignedType;
};

template <>
struct UnsignedConvertor<unsigned long long>{
	typedef unsigned long long UnsignedType;
};


#define BitsToMask(v) ((1 << (v)) - 1)
#define BitsToCount(v) ((1 << (v)) & (~1))

inline uint32_t bitsToMask32(unsigned v){
	return (1 << v) - 1;
}
inline uint32_t bitsToCount32(unsigned v){
	return (1 << v)  & (~static_cast<uint32_t>(1));
}

inline uint64_t bitsToMask64(unsigned v){
	return (1 << v) - 1;
}
inline uint64_t bitsToCount64(unsigned v){
	return (1 << v) & (~static_cast<uint64_t>(1));
}

inline size_t bitsToMask(unsigned v){
	return (static_cast<size_t>(1) << v) - 1;
}
inline size_t bitsToCount(unsigned v){
	return (1 << v) & (~static_cast<size_t>(1));
}


inline uint32_t toNetwork(uint32_t _v){
	return htonl(_v);
}

inline uint32_t toHost(uint32_t _v){
	return ntohl(_v);
}

inline uint16_t toNetwork(uint16_t _v){
	return htons(_v);
}

inline uint16_t toHost(uint16_t _v){
	return ntohs(_v);
}

}//namespace solid

#endif
