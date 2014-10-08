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

struct Node;

struct Page{
	static size_t	dataCapacity(const size_t _pagecp, const size_t _alignsz){
		const size_t	headsz = sizeof(uint16) - sizeof(Page*) - sizeof(Page*);
		size_t 			headpadd = (_alignsz - (headsz % _alignsz)) %  _alignsz;
		if(headpadd < sizeof(uint16)){
			headpadd += _alignsz;
		}
		return _pagecp - headsz - headpadd - _alignsz;
	}
	uint16	usecount;
	
	Page	*pprev;
	Page	*pnext;
	
	Node*	first()const;
	void*	data()const;
};

struct Node{
	Node *pnext;
};

struct CacheStub{
	void* pop(const size_t _cp, const size_t _pagecp){
		if(!ptop){
			if(!allocate(_cp, _pagecp)){
				return NULL;
			}
		}
		void *pv = ptop;
		ptop = ptop->pnext;
		return pv;
	}
	
	void push(void *_pv, size_t _cp){
		Node *pn = static_cast<Node*>(_pv);
		pn->pnext = ptop;
		ptop = pn;
	}
	
	bool empty()const{
		return true;
	}
	
	bool allocate(const size_t _cp, const size_t _pagecp){
		return false;
	}
	
	Node *ptop;
};

typedef std::vector<CacheStub>	CacheVectorT;
//-----------------------------------------------------------------------------
struct MemoryCache::Data{
	Data(
		size_t _pagecpbts
	):pagecp(_pagecpbts ? 1 << _pagecpbts : page_size()), alignsz(std::alignment_of<long long int>::value){
		cachevec.resize((Page::dataCapacity(pagecp, alignsz) / alignsz) + 1);
	}
	
	bool isSmall(const size_t _sz)const{
		return _sz <= Page::dataCapacity(pagecp, alignsz);
	}
	
	size_t indexToCapacity(const size_t _idx)const{
		return _idx * alignsz;
	}
	
	size_t sizeToIndex(const size_t _sz)const{
		return _sz / alignsz;
	}
	
	CacheVectorT	cachevec;
	const size_t	pagecp;
	const size_t	alignsz;
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
		return cs.pop(cp, d.pagecp);
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
