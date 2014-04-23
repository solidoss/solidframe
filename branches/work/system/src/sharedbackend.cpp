// system/src/sharedbackend.cpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "system/sharedbackend.hpp"
#include "system/mutex.hpp"
#include "system/mutualstore.hpp"
#ifdef HAS_GNU_ATOMIC
#include <ext/concurrence.h>
#endif
#include <deque>
#include <stack>
#include <queue>

namespace solid{

struct SharedBackend::Data{
	typedef std::deque<SharedStub>	StubVectorT;
	typedef std::stack<ulong>		UlongStackT;
	typedef std::queue<ulong>		UlongQueueT;
	typedef MutualStore<Mutex>		MutexStoreT;
	Data(){
		stubvec.push_back(SharedStub(0));
#ifndef HAS_GNU_ATOMIC
		mtxstore.safeAt(0);
#endif
	}
	Mutex			mtx;
	StubVectorT		stubvec;
	//UlongStackT		freestk;
	UlongQueueT		freeque;
#ifndef HAS_GNU_ATOMIC
	MutexStoreT		mtxstore;
#endif
};

/*static*/ SharedStub& SharedBackend::emptyStub(){
	static SharedStub ss(0, 1);
	return ss;
}
/*static*/ SharedBackend& SharedBackend::the(){
	static SharedBackend sb;
	return sb;
}
	
SharedStub* SharedBackend::create(void *_pv, SharedStub::DelFncT _cbk){
	SharedStub		*pss;
	SharedBackend	&th = the();
	Locker<Mutex>	lock(th.d.mtx);
	if(th.d.freeque.size()){
		const ulong		pos = th.d.freeque.front();
		th.d.freeque.pop();
#ifndef HAS_GNU_ATOMIC
		Mutex			&rmtx = th.d.mtxstore.at(pos);
		Locker<Mutex>	lock(rmtx);
#endif
		SharedStub		&rss(th.d.stubvec[pos]);
		rss.use = 1;
		rss.ptr = _pv;
		rss.cbk = _cbk;
		pss = &rss;
	}else{
		const ulong		pos = th.d.stubvec.size();
#ifndef HAS_GNU_ATOMIC
		Mutex			&rmtx = th.d.mtxstore.safeAt(pos);
		Locker<Mutex>	lock(rmtx);
#endif
		th.d.stubvec.push_back(SharedStub(pos));
		SharedStub		&rss(th.d.stubvec[pos]);
		rss.ptr = _pv;
		rss.use = 1;
		rss.cbk = _cbk;
		pss = &rss;
	}
	return pss;
}
#ifndef HAS_GNU_ATOMIC
void SharedBackend::use(SharedStub &_rss){
	Mutex &rmtx = the().d.mtxstore.at(_rss.idx);
	Locker<Mutex> lock(rmtx);
	++_rss.use;
}
void SharedBackend::release(SharedStub &_rss){
	ulong			cnt;
	SharedBackend	&th = the();
	{
		Mutex			&rmtx = th.d.mtxstore.at(_rss.idx);
		Locker<Mutex>	lock(rmtx);
		cnt = _rss.use;
		--_rss.use;
	}
	if(cnt == 1){
		(*_rss.cbk)(_rss.ptr);
		Locker<Mutex>	lock(th.d.mtx);
		//Locker<Mutex>	lock2(rmtx);
		th.d.freeque.push(_rss.idx);
	}
}
void SharedBackend::doRelease(SharedStub &_rss){
}
#else
void SharedBackend::doRelease(SharedStub &_rss){
	(*_rss.cbk)(_rss.ptr);
	Locker<Mutex>	lock(d.mtx);
	//Locker<Mutex>	lock2(rmtx);
	d.freeque.push(_rss.idx);
	//++_rss.uid;
}
#endif
SharedBackend::SharedBackend():d(*(new Data)){
}
SharedBackend::~SharedBackend(){
	//delete &d;
}

}//namespace solid

