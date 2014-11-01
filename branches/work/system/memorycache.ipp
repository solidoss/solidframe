// system/memorycache.ipp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#ifdef NINLINES
#define inline
#endif


inline void *MemoryCache::allocate(const size_t _sz){
	if(isSmall(_sz)){
		
		const size_t	idx = sizeToIndex(_sz);
		const size_t	cp = indexToCapacity(idx);
		
		CacheStub		&cs(cachevec[idx]);
		void* const		pv = cs.pop(cp, cfg);
#ifdef UDEBUG
		vdbgx(dbgid, "Allocated "<<pv<<" of capacity "<<cp<<" using cachestub "<<idx);
#endif
		return pv;
	}else{
		void*			pv = new char[_sz];
#ifdef UDEBUG
		vdbgx(dbgid, "Allocated "<<pv<<" using default allocator");
#endif
		return pv;
	}
}

inline void MemoryCache::free(void *_pv, const size_t _sz){
	if(isSmall(_sz)){
		const size_t	idx = sizeToIndex(_sz);
		const size_t	cp = indexToCapacity(idx);
		CacheStub		&cs(cachevec[idx]);
		cs.push(_pv, cp, cfg);
#ifdef UDEBUG
		vdbgx(dbgid, "Freed "<<_pv<<" of capacity "<<cp<<" using cachestub "<<idx);
#endif
	}else{
#ifdef UDEBUG
		vdbgx(dbgid, "Freed "<<_pv<<" using default allocator");
#endif
		delete []static_cast<char*>(_pv);
	}
}


#ifdef NINLINES
#undef inline
#endif

