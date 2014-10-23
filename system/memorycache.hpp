// memorycache.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_SYSTEM_MEMORY_CACHE_HPP
#define SOLID_SYSTEM_MEMORY_CACHE_HPP

#include "system/common.hpp"

namespace solid{

class MemoryCache{
public:
	MemoryCache(
		const size_t _pagecp = 0,
		const size_t _emptypagecnt = 1
	);
	~MemoryCache();
	
	void *allocate(const size_t _sz);
	void free(void *_pv, const size_t _sz);
	
	
	size_t reserve(const size_t _sz, const size_t _cnt, const bool _lazy = true);
	
	void print(const size_t _sz)const;
private:
	struct Data;
	Data	&d;
};

}//namespace solid
#endif