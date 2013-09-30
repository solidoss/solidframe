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

#include "system/common.hpp"
#include "system/binary.hpp"
#include <cstring>

namespace solid{
namespace serialization{
namespace binary{


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

inline unsigned crossSize(const char *_ps){
	const uint8 *ps = reinterpret_cast<const uint8*>(_ps);
	uint8 		v = *ps;
	return v & 0xf;//4 bits
}

inline unsigned crossSize(uint8 _v){
	_v >>= 4;
	if(!_v){
		return 1;
	}
	return 2;
}
inline unsigned crossSize(uint16 _v){
	_v >>= 4;
	if(!_v){
		return 1;
	}
	_v >>= 8;
	if(!_v){
		return 2;
	}
	return 3;
}
inline unsigned crossSize(uint32 _v){
	_v >>= 4;
	if(!_v){
		return 1;
	}
	_v >>= 8;
	if(!_v){
		return 2;
	}
	_v >>= 8;
	if(!_v){
		return 3;
	}
	_v >>= 8;
	if(!_v){
		return 4;
	}
	return 5;
}
inline unsigned crossSize(uint64 _v){
	_v >>= 4;
	if(!_v){
		return 1;
	}
	_v >>= 8;
	if(!_v){
		return 2;
	}
	_v >>= 8;
	if(!_v){
		return 3;
	}
	_v >>= 8;
	if(!_v){
		return 4;
	}
	//
	_v >>= 8;
	if(!_v){
		return 5;
	}
	_v >>= 8;
	if(!_v){
		return 6;
	}
	_v >>= 8;
	if(!_v){
		return 7;
	}
	_v >>= 8;
	if(!_v){
		return 8;
	}
	return 9;
}

inline char* storeCross(char *_pd, uint8 _v){
	uint8	*pd = reinterpret_cast<uint8*>(_pd);
	*pd = ((_v & 0xf) << 4);
	_v >>= 4;
	if(!_v){
		return _pd + 1;
	}
	*(pd + 1) = _v;
	*pd |= 1;
	return _pd + 2;
}
inline char* storeCross(char *_pd, uint16 _v){
	uint8	*pd = reinterpret_cast<uint8*>(_pd);
	*pd = ((_v & 0xf) << 4);
	_v >>= 4;
	if(!_v){
		*pd |= 0;
		return _pd + 1;
	}
	*(pd + 1) = (_v & 0xff);
	_v >>= 8;
	if(!_v){
		*pd |= 1;
		return _pd + 2;
	}
	*(pd + 2) = (_v & 0xf);
	*pd |= 2;
	return _pd + 3;
}
inline char* storeCross(char *_pd, uint32 _v){
	uint8	*pd = reinterpret_cast<uint8*>(_pd);
	*pd = ((_v & 0xf) << 4);
	_v >>= 4;
	if(!_v){
		*pd |= 0;
		return _pd + 1;
	}
	*(pd + 1) = (_v & 0xff);
	_v >>= 8;
	if(!_v){
		*pd |= 1;
		return _pd + 2;
	}
	*(pd + 2) = (_v & 0xff);
	_v >>= 8;
	if(!_v){
		*pd |= 2;
		return _pd + 3;
	}
	*(pd + 3) = (_v & 0xff);
	_v >>= 8;
	if(!_v){
		*pd |= 3;
		return _pd + 4;
	}
	*(pd + 4) = (_v & 0xf);
	*pd |= 4;
	return _pd + 5;
}
inline char* storeCross(char *_pd, uint64 _v){
	uint8	*pd = reinterpret_cast<uint8*>(_pd);
	*pd = ((_v & 0xf) << 4);
	_v >>= 4;
	if(!_v){
		*pd |= 0;
		return _pd + 1;
	}
	*(pd + 1) = (_v & 0xff);
	_v >>= 8;
	if(!_v){
		*pd |= 1;
		return _pd + 2;
	}
	*(pd + 2) = (_v & 0xff);
	_v >>= 8;
	if(!_v){
		*pd |= 2;
		return _pd + 3;
	}
	*(pd + 3) = (_v & 0xff);
	_v >>= 8;
	if(!_v){
		*pd |= 3;
		return _pd + 4;
	}
	//
	*(pd + 4) = (_v & 0xff);
	_v >>= 8;
	if(!_v){
		*pd |= 4;
		return _pd + 5;
	}
	*(pd + 5) = (_v & 0xff);
	_v >>= 8;
	if(!_v){
		*pd |= 5;
		return _pd + 6;
	}
	*(pd + 6) = (_v & 0xff);
	_v >>= 8;
	if(!_v){
		*pd |= 6;
		return _pd + 7;
	}
	*(pd + 7) = (_v & 0xff);
	_v >>= 8;
	if(!_v){
		*pd |= 7;
		return _pd + 8;
	}
	*(pd + 8) = (_v & 0xf);
	*pd |= 8;
	return _pd + 9;
}

inline const char* loadCross(const char *_ps, uint8 &_val){
	const uint8		*ps = reinterpret_cast<const uint8*>(_ps);
	const unsigned	sz = crossSize(_ps);
	if(sz == 0){
		_val = (*ps >> 4);
		return _ps + 1;
	}
	_val = (*ps >> 4) | (*(ps + 1) << 4);
	return _ps + 2;
}
inline const char* loadCross(const char *_ps, uint16 &_val){
	const uint8		*ps = reinterpret_cast<const uint8*>(_ps);
	const unsigned	sz = crossSize(_ps);
	switch(sz){
		case 0:
			_val = (*ps >> 4);
			return _ps + 1;
		case 1:
			_val = (*ps >> 4) | (static_cast<uint16>(*(ps + 1)) << 4);
			return _ps + 2;
		default:
			_val = ((*ps >> 4) | (static_cast<uint16>(*(ps + 1)) << 4) | (static_cast<uint16>(*(ps + 2)) << 12));
			return _ps + 3;
	}
}
inline const char* loadCross(const char *_ps, uint32 &_val){
	const uint8		*ps = reinterpret_cast<const uint8*>(_ps);
	const unsigned	sz = crossSize(_ps);
	switch(sz){
		case 0:
			_val = (*ps >> 4);
			return _ps + 1;
		case 1:
			_val = (*ps >> 4) | (static_cast<uint32>(*(ps + 1)) << 4);
			return _ps + 2;
		case 2:
			_val = (*ps >> 4) | (static_cast<uint32>(*(ps + 1)) << 4) | (static_cast<uint32>(*(ps + 2)) << 12);
			return _ps + 3;
		case 3:
			_val = (*ps >> 4) | (static_cast<uint32>(*(ps + 1)) << 4) | (static_cast<uint32>(*(ps + 2)) << 12) | (static_cast<uint32>(*(ps + 3)) << 20);
			return _ps + 4;
		default:
			_val = (*ps >> 4) | (static_cast<uint32>(*(ps + 1)) << 4) | (static_cast<uint32>(*(ps + 2)) << 12) | (static_cast<uint32>(*(ps + 3)) << 20);
			_val |= (static_cast<uint32>(*(ps + 4)) << 28);
			return _ps + 5;
	}
}
inline const char* loadCross(const char *_ps, uint64 &_val){
	const uint8		*ps = reinterpret_cast<const uint8*>(_ps);
	const unsigned	sz = crossSize(_ps);
	switch(sz){
		case 0:
			_val = (*ps >> 4);
			return _ps + 1;
		case 1:
			_val = (*ps >> 4) | (static_cast<uint64>(*(ps + 1)) << 4);
			return _ps + 2;
		case 2:
			_val = (*ps >> 4) | (static_cast<uint64>(*(ps + 1)) << 4) | (static_cast<uint64>(*(ps + 2)) << 12);
			return _ps + 3;
		case 3:
			_val = (*ps >> 4) | (static_cast<uint64>(*(ps + 1)) << 4) | (static_cast<uint64>(*(ps + 2)) << 12) | (static_cast<uint64>(*(ps + 3)) << 20);
			return _ps + 4;
		case 4:
			_val = (*ps >> 4) | (static_cast<uint64>(*(ps + 1)) << 4) | (static_cast<uint64>(*(ps + 2)) << 12) | (static_cast<uint64>(*(ps + 3)) << 20);
			_val |= (static_cast<uint64>(*(ps + 4)) << 28);
			return _ps + 5;
		case 5:
			_val = (*ps >> 4) | (static_cast<uint64>(*(ps + 1)) << 4) | (static_cast<uint64>(*(ps + 2)) << 12) | (static_cast<uint64>(*(ps + 3)) << 20);
			_val |= (static_cast<uint64>(*(ps + 4)) << 28) | (static_cast<uint64>(*(ps + 5)) << 36);
			return _ps + 6;
		case 6:
			_val = (*ps >> 4) | (static_cast<uint64>(*(ps + 1)) << 4) | (static_cast<uint64>(*(ps + 2)) << 12) | (static_cast<uint64>(*(ps + 3)) << 20);
			_val |= (static_cast<uint64>(*(ps + 4)) << 28) | (static_cast<uint64>(*(ps + 5)) << 36) | (static_cast<uint64>(*(ps + 6)) << 44);
			return _ps + 7;
		case 7:
			_val = (*ps >> 4) | (static_cast<uint64>(*(ps + 1)) << 4) | (static_cast<uint64>(*(ps + 2)) << 12) | (static_cast<uint64>(*(ps + 3)) << 20);
			_val |= (static_cast<uint64>(*(ps + 4)) << 28) | (static_cast<uint64>(*(ps + 5)) << 36) | (static_cast<uint64>(*(ps + 6)) << 44);
			_val |= (static_cast<uint64>(*(ps + 7)) << 52);
			return _ps + 8;
		default:
			_val = (*ps >> 4) | (static_cast<uint64>(*(ps + 1)) << 4) | (static_cast<uint64>(*(ps + 2)) << 12) | (static_cast<uint64>(*(ps + 3)) << 20);
			_val |= (static_cast<uint64>(*(ps + 4)) << 28) | (static_cast<uint64>(*(ps + 5)) << 36) | (static_cast<uint64>(*(ps + 6)) << 44);
			_val |= (static_cast<uint64>(*(ps + 7)) << 52) | (static_cast<uint64>(*(ps + 8)) << 60);
			return _ps + 9;
	}
}
}//namespace binary
}//namespace serialization
}//namespace solid

#endif
