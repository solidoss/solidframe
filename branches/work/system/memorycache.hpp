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
		unsigned _pagecpbts = 0
	);
	~MemoryCache();
	
	void *allocate(size_t _sz);
	void free(void *_pv, size_t _sz);
	
	
	void reserve(size_t _sz, size_t _cnt);
private:
	struct Data;
	Data	&d;
};

}//namespace solid
#endif