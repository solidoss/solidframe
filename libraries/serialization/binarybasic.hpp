// serialization/binarybasic.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_SERIALIZATION_BINARY_BASIC_HPP
#define SOLID_SERIALIZATION_BINARY_BASIC_HPP

#include "utility/algorithm.hpp"
#include "system/binary.hpp"
#include <cstring>

namespace solid{
namespace serialization{
namespace binary{

#define BASIC_DECL(tp) \
template <class S>\
void serialize(S &_s, tp &_t){\
	_s.push(_t, "basic");\
}\
template <class S, class Ctx>\
void serialize(S &_s, tp &_t, Ctx &){\
	_s.push(_t, "basic");\
}

inline char *store(char *_pd, const uint8 _val){
	uint8 *pd = reinterpret_cast<uint8*>(_pd);
	*pd = _val;
	return _pd + 1;
}

inline char *store(char *_pd, const uint16 _val){
	uint8 *pd = reinterpret_cast<uint8*>(_pd);
	*(pd) 		= ((_val >> 8) & 0xff);
	*(pd + 1)	= (_val & 0xff);
	return _pd + 2;
}

inline char *store(char *_pd, const uint32 _val){
	
	_pd = store(_pd, static_cast<uint16>(_val >> 16));
	
	return store(_pd, static_cast<uint16>(_val & 0xffff));;
}

inline char *store(char *_pd, const uint64 _val){
	
	_pd = store(_pd, static_cast<uint32>(_val >> 32));
	
	return store(_pd, static_cast<uint32>(_val & 0xffffffffULL));;
}

template <size_t S>
inline char *store(char *_pd, const Binary<S> _val){
	memcpy(_pd, _val.data, S);
	return _pd + S;
}


inline const char* load(const char *_ps, uint8 &_val){
	const uint8 *ps = reinterpret_cast<const uint8*>(_ps);
	_val = *ps;
	return _ps + 1;
}

inline const char* load(const char *_ps, uint16 &_val){
	const uint8 *ps = reinterpret_cast<const uint8*>(_ps);
	_val = *ps;
	_val <<= 8;
	_val |= *(ps + 1);
	return _ps + 2;
}

inline const char* load(const char *_ps, uint32 &_val){
	uint16	upper;
	uint16	lower;
	_ps = load(_ps, upper);
	_ps = load(_ps, lower);
	_val = upper;
	_val <<= 16;
	_val |= lower;
	return _ps;
}

inline const char* load(const char *_ps, uint64 &_val){
	uint32	upper;
	uint32	lower;
	_ps = load(_ps, upper);
	_ps = load(_ps, lower);
	_val = upper;
	_val <<= 32;
	_val |= lower;
	return _ps;
}
template <size_t S>
inline const char *load(const char *_ps, Binary<S> &_val){
	memcpy(_val.data, _ps, S);
	return _ps + S;
}

//cross integer serialization

inline size_t crossSize(const char *_ps){
	const uint8 *ps = reinterpret_cast<const uint8*>(_ps);
	uint8 		v = *ps;
	const bool	ok = check_value_with_crc(v, v);
	if(ok){
		return v + 1;
	}
	return -1;
}

inline size_t crossSize(uint8 _v){
	return max_padded_byte_cout(_v) + 1;
}

inline size_t crossSize(uint16 _v){
	return max_padded_byte_cout(_v) + 1;
}

inline size_t crossSize(uint32 _v){
	return max_padded_byte_cout(_v) + 1;
}
inline size_t crossSize(uint64 _v){
	return max_padded_byte_cout(_v) + 1;
}

char* crossStore(char *_pd, uint8 _v);
char* crossStore(char *_pd, uint16 _v);
char* crossStore(char *_pd, uint32 _v);
char* crossStore(char *_pd, uint64 _v);

const char* crossLoad(const char *_ps, uint8 &_val);
const char* crossLoad(const char *_ps, uint16 &_val);
const char* crossLoad(const char *_ps, uint32 &_val);
const char* crossLoad(const char *_ps, uint64 &_val);


}//namespace binary
}//namespace serialization
}//namespace solid

#endif
