// frame/arrayfind.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_ARRAY_FIND_HPP
#define SOLID_FRAME_ARRAY_FIND_HPP

#include <array>
#include "frame/common.hpp"

namespace solid{
namespace frame{

template <class C>
size_t find_min(const std::array<C, 2> &_ra, size_t &_rcrtidx){
	if(_ra[0] < _ra[1]){
		return 0;
	}else if(_ra[0] > _ra[1]){
		return 1;
	}
	const size_t idx = _rcrtidx;
	_rcrtidx = (idx + 1) % 2;
	return idx;
}

template <class C>
size_t find_min(const std::array<C, 3> &_ra, size_t &_rcrtidx){
	size_t	crtidx;
	C		crtmin;
	
	if(_ra[0] < _ra[1]){
		crtidx = 0;
		crtmin = _ra[0];
	}else if(_ra[0] > _ra[1]){
		crtidx = 1;
		crtmin = _ra[1];
	}else if(_ra[1] == _ra[2]){
		const size_t idx = _rcrtidx;
		_rcrtidx = (idx + 1) % 3;
		return idx;
	}else{
		crtidx = 0;
		crtmin = _ra[0];
	}
	
	if(crtmin < _ra[2]){
		return crtidx;
	}else{
		return 2;
	}
	
	
}
template <class C>
size_t find_min(const std::array<C, 4> &_ra, size_t &_rcrtidx){
#if 1
	size_t	crtidx;
	C		crtmin;
	
	if(_ra[0] == _ra[1] && _ra[1] == _ra[2] && _ra[2] == _ra[3]){
		crtidx = _rcrtidx;
		_rcrtidx = (crtidx + 1) % 4;
	}else{
		if(_ra[0] <= _ra[1]){
			crtmin = _ra[0];
			crtidx = 0;
		}else{
			crtmin = _ra[1];
			crtidx = 1;
		}
		if(_ra[2] < crtmin){
			crtmin = _ra[2];
			crtidx = 2;
		}
		if(_ra[3] < crtmin){
			crtidx = 3;
		}
	}
	return crtidx;
#else
	size_t	crtidx;
	C		crtmin;
	size_t	eqlcnt = 0;
	
	if(_ra[0] < _ra[1]){
		crtidx = 0;
		crtmin = _ra[0];
	}else if(_ra[0] > _ra[1]){
		crtidx = 1;
		crtmin = _ra[1];
	}else{//==
		crtidx = 0;
		crtmin = _ra[0];
		eqlcnt = 2;
	}
	
	if(crtmin < _ra[2]){
		
	}else if(crtmin > _ra[2]){
		crtmin = _ra[2];
		crtidx = 2;
	}else{//==
		++eqlcnt;
	}
	
	if(crtmin < _ra[3]){
		return crtidx;
	}else if(crtmin > _ra[3]){
		return 3;
	}else if(eqlcnt < 3){//==
		return crtidx;
	}
		
	crtidx = _rcrtidx;
	_rcrtidx = (crtidx + 1) % 4;
	return crtidx;
#endif
}
template <class C>
size_t find_min(const std::array<C, 5> &_ra, size_t &_rcrtidx){
	const size_t idx = _rcrtidx;
	_rcrtidx = (idx + 1) % 5;
	return idx;
}
template <class C>
size_t find_min(const std::array<C, 6> &_ra, size_t &_rcrtidx){
	const size_t idx = _rcrtidx;
	_rcrtidx = (idx + 1) % 6;
	return idx;
}
template <class C>
size_t find_min(const std::array<C, 7> &_ra, size_t &_rcrtidx){
	const size_t idx = _rcrtidx;
	_rcrtidx = (idx + 1) % 7;
	return idx;
}
template <class C>
size_t find_min(const std::array<C, 8> &_ra, size_t &_rcrtidx){
	const size_t idx = _rcrtidx;
	_rcrtidx = (idx + 1) % 8;
	return idx;
}


}//namespace frame
}//namespace solid

#endif