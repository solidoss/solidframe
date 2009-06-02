/* Declarations file convertors.hpp
	
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

#ifndef SYSTEM_CONVERTORS_HPP
#define SYSTEM_CONVERTORS_HPP

#include "common.hpp"
#include <arpa/inet.h>

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
struct UnsignedConvertor<long>{
	typedef ulong UnsignedType;
};

template <>
struct UnsignedConvertor<ulong>{
	typedef ulong UnsignedType;
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

#define BitsToMsk(v) ((1 << (v)) - 1)
#define BitsToCnt(v) (1 << (v))

inline unsigned bitsToMsk(unsigned v){
	return (1 << v) - 1;
}
inline unsigned bitsToCnt(unsigned v){
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


#endif
