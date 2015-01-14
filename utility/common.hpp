// utility/common.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_COMMON_HPP
#define UTILITY_COMMON_HPP

#include "system/common.hpp"

namespace solid{

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

uint8 bit_count(const uint8 _v);
uint16 bit_count(const uint16 _v);
uint32 bit_count(const uint32 _v);
uint64 bit_count(const uint64 _v);


inline void pack(uint32 &_v, const uint16 _v1, const uint16 _v2){
	_v = _v2;
	_v <<= 16;
	_v |= _v1;
}

inline uint32 pack(const uint16 _v1, const uint16 _v2){
	uint32 v;
	pack(v, _v1, _v2);
	return v;
}


inline void unpack(uint16 &_v1, uint16 &_v2, const uint32 _v){
	_v1 = _v & 0xffffUL;
	_v2 = (_v >> 16) & 0xffffUL;
}

extern const uint8 reverted_chars[];

inline uint32 bit_revert(const uint32 _v){
	uint32 r = (((uint32)reverted_chars[_v   & 0xff]) << 24);
	r |= (((uint32)reverted_chars[(_v >>  8) & 0xff]) << 16);
	r |= (((uint32)reverted_chars[(_v >> 16) & 0xff]) << 8);
	r |= (((uint32)reverted_chars[(_v >> 24) & 0xff]) << 0);
	return r;
}

inline uint64 bit_revert(const uint64 _v){
	uint64 r = (((uint64)reverted_chars[_v   & 0xff]) << 56);
	r |= (((uint64)reverted_chars[(_v >>  8) & 0xff]) << 48);
	r |= (((uint64)reverted_chars[(_v >> 16) & 0xff]) << 40);
	r |= (((uint64)reverted_chars[(_v >> 24) & 0xff]) << 32);
	
	r |= (((uint64)reverted_chars[(_v >> 32) & 0xff]) << 24);
	r |= (((uint64)reverted_chars[(_v >> 40) & 0xff]) << 16);
	r |= (((uint64)reverted_chars[(_v >> 48) & 0xff]) << 8);
	r |= (((uint64)reverted_chars[(_v >> 56) & 0xff]) << 0);
	return r;
}

}//namespace solid

#endif
