// system/common.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_BINARY_HPP
#define SYSTEM_BINARY_HPP

#include "system/common.hpp"

namespace solid{

template <size_t S>
struct Binary{
	enum{
		Size = S,
		Capacity = S
	};
	size_t size()const{return Size;}
	size_t capacity()const{return Capacity;}
	
	uint8& operator[](const size_t &_off){
		return data[_off];
	}
	
	uint8 operator[](const size_t &_off)const{
		return data[_off];
	}
	
	uint8 data[Capacity];
};


}//namespace solid

#endif
