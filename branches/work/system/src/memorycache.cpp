#include "system/memorycache.hpp"

namespace solid{

struct Page{
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
	
	void allocate();
	
	Node *ptop;
};

typedef std::vector<CacheStub>	CacheVectorT;

struct MemoryCache::Data{
	bool isSmall(const size_t _sz)const{
		return false;
	}
	
	size_t indexToCapacity(const size_t _sz)const{
		return _sz;
	}
	
	size_t sizeToIndex(const size_t _sz)const{
		return 0;
	}
	
	CacheVectorT	cachevec;
};

void *MemoryCache::allocate(size_t _sz){
	if(d.isSmall(_sz)){
		const size_t	idx = d.sizeToIndex(_sz);
		const size_t	cp = d.indexToCapacity(_sz);
		
		CacheStub		&cs(d.cachevec[idx];
		return cs.pop(cp);
	}else{
		return new char[_sz];
	}
}

void MemoryCache::free(void *_pv, size_t _sz){
	if(d.isSmall(_sz)){
		const size_t	idx = d.sizeToIndex(_sz);
		const size_t	cp = d.indexToCapacity(_sz);
		CacheStub		&cs(d.cachevec[idx];
		cs.push(_pv, cp);
	}else{
		delete []static_cast<char*>(_pv);
	}
}

}//namespace solid
