/* Declarations file convertors.hpp
	
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
#define BitsToCount(v) (1 << (v))

inline uint32 bitsToMask32(unsigned v){
	return (1 << v) - 1;
}
inline uint32 bitsToCount32(unsigned v){
	return (1 << v);
}

inline uint64 bitsToMask64(unsigned v){
	return (1 << v) - 1;
}
inline uint64 bitsToCount64(unsigned v){
	return (1 << v);
}

inline size_t bitsToMask(unsigned v){
	return (static_cast<size_t>(1) << v) - 1;
}
inline size_t bitsToCount(unsigned v){
	return (1 << v);
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
