// system/sharedbackend.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_SHAREDBACKEND_HPP
#define SYSTEM_SHAREDBACKEND_HPP

#include "system/common.hpp"
#ifdef HAS_GNU_ATOMIC
#include <ext/atomicity.h>
#endif

namespace solid{

struct SharedStub{
	typedef void (*DelFncT)(void*);
	SharedStub(const ulong _idx):use(0), ptr(NULL), idx(_idx)/*, uid(0)*/, cbk(0){}
	SharedStub(const ulong _idx, int _use):use(_use), ptr(NULL), idx(_idx)/*, uid(0)*/, cbk(0){}
	int					use;
	void				*ptr;
	const ulong			idx;
	//uint32				uid
	DelFncT				cbk;
};

class SharedBackend{
public:
	static SharedBackend& the();
	static SharedStub& emptyStub();
	
	static SharedStub* create(void *_pv, SharedStub::DelFncT _cbk);
#ifdef HAS_GNU_ATOMIC
	inline static void use(SharedStub &_rss){
		//__sync_add_and_fetch(&_rss.use, 1);
		__gnu_cxx:: __atomic_add_dispatch(&_rss.use, 1);
	}
	
	inline static void release(SharedStub &_rss){
		//if(__sync_sub_and_fetch(&_rss.use, 1) == 0){
		if(__gnu_cxx::__exchange_and_add_dispatch(&_rss.use, -1) == 1){
			the().doRelease(_rss);
		}
	}
#else
	static void use(SharedStub &_rss);
	
	static void release(SharedStub &_rss);
#endif
	
private:
	void doRelease(SharedStub &_rss);
	
	SharedBackend();
	~SharedBackend();
	SharedBackend(const SharedBackend&);
	SharedBackend& operator=(const SharedBackend&);
private:
	struct Data;
	Data &d;
};

}//namespace solid

#endif
