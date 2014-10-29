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
#include <vector>

namespace solid{


struct Configuration{
	Configuration(
		const size_t _pagecp = 0,
		const size_t _alignsz = 0,
		const size_t _emptypagecnt = 1
	):pagecp(_pagecp), alignsz(_alignsz), emptypagecnt(_emptypagecnt){}
	
	void reset(
		const size_t _pagecp,
		const size_t _alignsz,
		const size_t _emptypagecnt = 1
	){
		pagecp = _pagecp;
		alignsz = _alignsz;
		emptypagecnt = _emptypagecnt;
	}
	
	size_t	pagecp;
	size_t	alignsz;
	size_t	emptypagecnt;
};
struct Page;
struct CacheStub{
	CacheStub(
		Configuration const &_rcfg
	);
	
	~CacheStub();
	
	void clear();
	
	void* pop(const size_t _cp, Configuration const &_rcfg);
	
	void push(void *_pv, size_t _cp, Configuration const &_rcfg);
	
	size_t allocate(const size_t _cp, Configuration const &_rcfg);
	
    bool shouldFreeEmptyPage(Configuration const &_rcfg);
    
    void print(size_t _cp, Configuration const &_rcfg)const;
	
	Page	*pfrontpage;
	Page	*pbackpage;
	size_t	emptypagecnt;
	size_t	pagecnt;
	size_t	keeppagecnt;
#ifdef UDEBUG
	size_t	itemcnt;
#endif
};

class MemoryCache{
public:
	MemoryCache();
	
	MemoryCache(
		const size_t _pagecp,
		const size_t _emptypagecnt = 1
	);
	
	~MemoryCache();
	
	void configure(
		const size_t _pagecp = 0,
		const size_t _emptypagecnt = 1
	);
	
	void *allocate(const size_t _sz);
	void free(void *_pv, const size_t _sz);
	
	
	size_t reserve(const size_t _sz, const size_t _cnt, const bool _lazy = true);
	
	void print(const size_t _sz)const;
private:
	bool isSmall(const size_t _sz)const;
	
	size_t indexToCapacity(const size_t _idx)const;
	
	size_t sizeToIndex(const size_t _sz)const;
private:
	typedef std::vector<CacheStub>	CacheVectorT;
	CacheVectorT		cachevec;
	Configuration 		cfg;
	size_t				pagedatacp;
};

}//namespace solid
#endif