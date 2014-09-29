// memorycache.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_MUTUALSTORE_HPP
#define SYSTEM_MUTUALSTORE_HPP

namespace solid{

class MemoryCache{
public:
	MemoryCache(
		unsigned _pagecpbts = 0,	//0 -> pagefile size will be used
		unsigned _paddingbts = 0	//0 -> sizeof(long) will be used
	);
	~MemoryCache();
	
	void *allocate(size_t _sz);
	void free(void *_pv, size_t _sz);
private:
	struct Data;
	Data	&d;
};

}//namespace solid
#endif