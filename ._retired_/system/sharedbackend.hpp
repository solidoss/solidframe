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
#include "atomic.hpp"

namespace solid{

struct SharedStub{
	typedef void (*DelFncT)(void*);
	SharedStub(const ulong _idx):use(0), ptr(NULL), idx(_idx)/*, uid(0)*/, cbk(0){}
	SharedStub(const ulong _idx, const int _use):use(_use), ptr(NULL), idx(_idx)/*, uid(0)*/, cbk(0){}
	
	SharedStub(SharedStub &&_rss):use(_rss.use.load()), ptr(_rss.ptr), idx(_rss.idx), cbk(_rss.cbk){
		_rss.ptr = nullptr;
		_rss.cbk = nullptr;
	}
	
	std::atomic<int>	use;
	void				*ptr;
	const ulong			idx;
	DelFncT				cbk;
};

class SharedBackend{
public:
	static SharedBackend& the();
	static SharedStub& emptyStub();
	
	static SharedStub* create(void *_pv, SharedStub::DelFncT _cbk);
	inline static void use(SharedStub &_rss){
		++_rss.use;
	}
	
	inline static void release(SharedStub &_rss){
		if(_rss.use.fetch_sub(1) == 1){
			the().doRelease(_rss);
		}
	}
	
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
