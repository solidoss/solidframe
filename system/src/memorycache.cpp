// system/src/memorycache.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "system/memorycache.hpp"
#include "system/memory.hpp"
#include <vector>
#include <type_traits>

namespace solid{

struct Configuration{
	Configuration(
		const size_t _pagecp,
		const size_t _alignsz,
		const size_t _cachepagecnt = 1
	):pagecp(_pagecp), alignsz(_alignsz), cachepagecnt(_cachepagecnt){}
	
	size_t	pagecp;
	size_t	alignsz;
	size_t	cachepagecnt;
};

struct Node{
	Node *pnext;
};

struct Page{
	static size_t	dataCapacity(Configuration const &_rcfg){
		const size_t	headsz = sizeof(uint16) - sizeof(Page*) - sizeof(Page*);
		size_t 			headpadd = (_rcfg.alignsz - (headsz % _rcfg.alignsz)) %  _rcfg.alignsz;
		if(headpadd < sizeof(uint16)){
			headpadd += _rcfg.alignsz;
		}
		return _rcfg.pagecp - headsz - headpadd;
	}
	static Page* computePage(void *_pv, Configuration const &_rcfg){
		return NULL;
	}
	
	bool empty()const{
		return false;
	}
	
	bool full()const{
		return true;
	}
	
	void* pop(const size_t _cp, Configuration const &_rcfg){
		return NULL;
	}
	
	void push(void *_pv, const size_t _cp, Configuration const &_rcfg){
		
	}
	
	uint16	usecount;
	
	Page	*pprev;
	Page	*pnext;
	
	Node*	first()const;
	void*	data()const;
};


struct CacheStub{
	CacheStub():ptoppage(NULL), pagecnt(0){}
	
	void* pop(const size_t _cp, Configuration const &_rcfg){
		if(!pfrontpage || pfrontpage->full()){
			if(!allocate(_cp, _rcfg)){
				return NULL;
			}
		}
		return pfrontpage->pop(_cp, _rcfg);
	}
	
	void push(void *_pv, size_t _cp, Configuration const &_rcfg){
		Page *ppage = Page::computePage(_pv, _rcfg);
		ppage->push(_pv, _cp, _rcfg);
		if(ppage->empty()){
			
		}else if(ppage->full()){
			
		}
	}
	
	bool empty()const{
		return true;
	}
	
	bool allocate(const size_t _cp, Configuration const &_rcfg){
		return false;
	}
    
	
	Page	*pfrontpage;
	Page	*pbackpage;
	size_t	pagecnt;
};

typedef std::vector<CacheStub>	CacheVectorT;
//-----------------------------------------------------------------------------
struct MemoryCache::Data{
	Data(
		size_t _pagecpbts
	):cfg((_pagecpbts ? 1 << _pagecpbts : memory_page_size()), std::alignment_of<long long int>::value){
		cachevec.resize((Page::dataCapacity(cfg.pagecp, cfg.alignsz) / cfg.alignsz) + 1);
	}
	
	bool isSmall(const size_t _sz)const{
		return _sz <= Page::dataCapacity(cfg);
	}
	
	size_t indexToCapacity(const size_t _idx)const{
		return _idx * cfg.alignsz;
	}
	
	size_t sizeToIndex(const size_t _sz)const{
		return _sz / cfg.alignsz;
	}
	
	CacheVectorT		cachevec;
	const Configuration cfg;
};

//-----------------------------------------------------------------------------
MemoryCache::MemoryCache(
	unsigned _pagecpbts
):d(*(new Data(_pagecpbts))){}

MemoryCache::~MemoryCache(){
	delete &d;
}

void *MemoryCache::allocate(size_t _sz){
	if(d.isSmall(_sz)){
		const size_t idx = d.sizeToIndex(_sz);
		const size_t cp = d.indexToCapacity(_sz);
		
		CacheStub		&cs(d.cachevec[idx]);
		return cs.pop(cp, d.cfg);
	}else{
		return new char[_sz];
	}
}

void MemoryCache::free(void *_pv, size_t _sz){
	if(d.isSmall(_sz)){
		const size_t	idx = d.sizeToIndex(_sz);
		const size_t	cp = d.indexToCapacity(_sz);
		CacheStub		&cs(d.cachevec[idx]);
		cs.push(_pv, cp);
	}else{
		delete []static_cast<char*>(_pv);
	}
}
//-----------------------------------------------------------------------------

}//namespace solid
