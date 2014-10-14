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
#include "system/cassert.hpp"
#include <vector>
#include <boost/type_traits.hpp>
#include <memory>

namespace solid{

struct Configuration{
	Configuration(
		const size_t _pagecp,
		const size_t _alignsz,
		const size_t _emptypagecnt = 1
	):pagecp(_pagecp), alignsz(_alignsz), emptypagecnt(_emptypagecnt){}
	
	size_t	pagecp;
	size_t	alignsz;
	size_t	emptypagecnt;
};

struct Node{
	Node *pnext;
};


inline void *align( std::size_t alignment, std::size_t size,
                    void *&ptr, std::size_t &space ) {
	std::uintptr_t pn = reinterpret_cast< std::uintptr_t >( ptr );
	std::uintptr_t aligned = ( pn + alignment - 1 ) & - alignment;
	std::size_t padding = aligned - pn;
	if ( space < size + padding ) return nullptr;
	space -= padding;
	return ptr = reinterpret_cast< void * >( aligned );
}

struct Page{
	static size_t	dataCapacity(Configuration const &_rcfg){
		const size_t	headsz = sizeof(Page);
		size_t 			headpadd = (_rcfg.alignsz - (headsz % _rcfg.alignsz)) %  _rcfg.alignsz;
		return _rcfg.pagecp - headsz - headpadd;
	}
	static Page* computePage(void *_pv, Configuration const &_rcfg){
		std::uintptr_t pn = reinterpret_cast< std::uintptr_t >( _pv );
		pn -= (pn % _rcfg.pagecp);
		return reinterpret_cast<Page*>(pn);
	}
	
	bool empty()const{
		return usecount == 0;
	}
	
	bool full()const{
		return ptop == NULL;
	}
	
	void* pop(const size_t _cp, Configuration const &_rcfg){
		void *pv = ptop;
		ptop = ptop->pnext;
		++usecount;
		return pv;
	}
	
	void push(void *_pv, const size_t _cp, Configuration const &_rcfg){
		--usecount;
		Node *pnode = static_cast<Node*>(_pv);
		pnode->pnext = ptop;
		ptop = pnode;
	}
	
	void init(const size_t _cp, Configuration const &_rcfg){
		pprev = pnext = NULL;
		ptop = NULL;
		usecount = 0;
		
		char 	*pc = reinterpret_cast<char *>(this);
		void 	*pv = pc + sizeof(*this);
		size_t	sz = _rcfg.pagecp - sizeof(*this);
		Node	*pn = NULL;
		
		while(align(_rcfg.alignsz, _cp, pv, sz)){
			pn = reinterpret_cast<Node*>(pv);
			pn->pnext = ptop;
			ptop = pn;
			uint8 *pu = static_cast<uint8*>(pv);
			pv = pu + _cp;
		}
	}
	
	Page	*pprev;
	Page	*pnext;
	
	Node	*ptop;
	
	uint16	usecount;
};


struct CacheStub{
	CacheStub():pfrontpage(NULL), pbackpage(NULL), emptypagecnt(0){}
	
	~CacheStub(){
		clear();
	}
	
	void clear(){
		Page *pit = pfrontpage;
		while(pit){
			Page *ptmp = pit;
			pit = pit->pprev;
			
			memory_free_aligned(ptmp);
		}
		pfrontpage = pbackpage = NULL;
	}
	
	void* pop(const size_t _cp, Configuration const &_rcfg){
		if(!pfrontpage || pfrontpage->full()){
			if(!allocate(_cp, _rcfg)){
				return NULL;
			}
		}
		
		if(pfrontpage->empty()){
			--emptypagecnt;
		}
		
		void *pv = pfrontpage->pop(_cp, _rcfg);
		
		if(pfrontpage->full() && pfrontpage != pbackpage && !pfrontpage->pprev->full()){
			//move the frontpage to back
			Page *ptmp = pfrontpage;
			
			pfrontpage = ptmp->pprev;
			pfrontpage->pnext = NULL;
			
			pbackpage->pprev = ptmp;
			ptmp->pnext = pbackpage;
			pbackpage = ptmp;
		}
		return pv;
	}
	
	void push(void *_pv, size_t _cp, Configuration const &_rcfg){
		Page *ppage = Page::computePage(_pv, _rcfg);
		
		ppage->push(_pv, _cp, _rcfg);
		
		if(ppage->empty() && shouldFreeEmptyPage(_rcfg)){
			//the page can be deleted
			if(ppage->pnext){
				ppage->pnext->pprev = ppage->pprev;
			}else{
				cassert(pfrontpage == ppage);
				pfrontpage = ppage->pprev;
			}
			
			if(ppage->pprev){
				ppage->pprev->pnext = ppage->pnext;
			}else{
				cassert(pbackpage == ppage);
				pbackpage = ppage->pnext;
			}
			--emptypagecnt;
			memory_free_aligned(ppage);
		}else if(pfrontpage != ppage){
			//move the page to front
			ppage->pnext->pprev = ppage->pprev;
			if(ppage->pprev){
				ppage->pprev->pnext = ppage->pnext;
			}
			
			ppage->pnext = NULL;
			ppage->pprev = pfrontpage;
			pfrontpage->pnext = ppage;
			pfrontpage = ppage;
		}
		
		
	}
	
	bool allocate(const size_t _cp, Configuration const &_rcfg){
		void *pv = memory_allocate_aligned(_rcfg.pagecp, _rcfg.pagecp);
		if(pv){
			Page *ppage = reinterpret_cast<Page*>(pv);
			ppage->init(_cp, _rcfg);
			return true;
		}
		return false;
	}
	
    bool shouldFreeEmptyPage(Configuration const &_rcfg){
		++emptypagecnt;
		return emptypagecnt > _rcfg.emptypagecnt;
	}
    
	
	Page	*pfrontpage;
	Page	*pbackpage;
	size_t	emptypagecnt;
};

typedef std::vector<CacheStub>	CacheVectorT;
//-----------------------------------------------------------------------------
struct MemoryCache::Data{
	Data(
		size_t _pagecpbts
	):cfg((_pagecpbts ? 1 << _pagecpbts : memory_page_size()), boost::alignment_of<long>::value){
		cachevec.resize((Page::dataCapacity(cfg) / cfg.alignsz) + 1);
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
		const size_t cp = d.indexToCapacity(idx);
		
		CacheStub		&cs(d.cachevec[idx]);
		return cs.pop(cp, d.cfg);
	}else{
		return new char[_sz];
	}
}

void MemoryCache::free(void *_pv, size_t _sz){
	if(d.isSmall(_sz)){
		const size_t	idx = d.sizeToIndex(_sz);
		const size_t	cp = d.indexToCapacity(idx);
		CacheStub		&cs(d.cachevec[idx]);
		cs.push(_pv, cp, d.cfg);
	}else{
		delete []static_cast<char*>(_pv);
	}
}

void MemoryCache::reserve(size_t _sz, size_t _cnt){
	
}

//-----------------------------------------------------------------------------

}//namespace solid
