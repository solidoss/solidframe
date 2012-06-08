/* Declarations file binarybasic.hpp
	
	Copyright 2012 Valentin Palade 
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

#ifndef ALGORITHM_SERIALIZATION_BINARY_BASIC_HPP
#define ALGORITHM_SERIALIZATION_BINARY_BASIC_HPP

#include "system/common.hpp"

namespace serialization{
namespace binary{

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

}//namespace binary
}//namespace serialization
#endif
