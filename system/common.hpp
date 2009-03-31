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

typedef unsigned char		uchar;
typedef unsigned int		uint;
typedef unsigned long		ulong;
typedef unsigned short		ushort;
typedef unsigned long long	uint64;
typedef long long 			int64;
typedef unsigned short		uint16;
typedef short		        int16;
#ifdef _LP64
typedef unsigned long		uint32;
typedef long			    int32;
#define MAX_ULONG			0xffffffffffffffffL
#else
#define MAX_ULONG			0xfffffff
typedef unsigned			uint32;
typedef long int			int32;
#endif
typedef unsigned char		uint8;


enum SeekRef {
	SeekBeg=0,
	SeekCur=1,
	SeekEnd=2
};

enum RetVal{
	BAD = -1,
	OK = 0,
	NOK,
	YIELD,
	CONTINUE
};

template <typename T>
inline T tMax(T v1,T v2){
    return (v1<v2)?v2:v1;
}
template <typename T>
inline T tMin(T v1,T v2){
    return (v1>v2)?v2:v1;
}

template <typename T>
void exchange(T &a, T &b, T &tmp){
	tmp = a;
	a = b;
	b = tmp;
}

#endif
