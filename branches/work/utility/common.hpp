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

#ifndef UTILITY_COMMON_HPP
#define UTILITY_COMMON_HPP

#include "system/common.hpp"

template <typename T>
inline T tmax(const T &v1, const T &v2){
    return (v1 < v2) ? v2 : v1;
}
template <typename T>
inline T tmin(const T &v1, const T &v2){
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

#if 0
bool overflow_safe_great(const uint32 _u1, const uint32 _u2){
	if(_u1 > _u2){
		return (_u1 - _u2) <= (uint32)(0xffffffff/2);
	}else{
		return (_u2 - _u1) > (uint32)(0xffffffff/2);
	}
}
#endif

inline bool overflow_safe_less(const uint32 &_u1, const uint32 &_u2){
	if(_u1 < _u2){
		return (_u2 - _u1) <= (uint32)(0xffffffff/2);
	}else{
		return (_u1 - _u2) > (uint32)(0xffffffff/2);
	}

}

inline bool overflow_safe_less(const uint64 &_u1, const uint64 &_u2){
	if(_u1 < _u2){
		return (_u2 - _u1) <= ((uint64)-1)/2;
	}else{
		return (_u1 - _u2) > ((uint64)-1)/2;
	}

}

inline uint32 overflow_safe_max(const uint32 &_u1, const uint32 &_u2){
	if(overflow_safe_less(_u1, _u2)){
		return _u2;
	}else{
		return _u1;
	}
}

inline uint64 overflow_safe_max(const uint64 &_u1, const uint64 &_u2){
	if(overflow_safe_less(_u1, _u2)){
		return _u2;
	}else{
		return _u1;
	}
}


template <typename T>
inline T circular_distance(const T &_v, const T &_piv, const T& _max){
	if(_v >= _piv){
		return _v - _piv;
	}else{
		return _max - _piv + _v;
	}
}

inline size_t padding_size(const size_t _sz, const size_t _pad){
	return ((_sz / _pad) + 1) * _pad;
}

inline size_t fast_padding_size(const size_t _sz, const size_t _bitpad){
	return ((_sz >> _bitpad) + 1) << _bitpad;
}

#endif
