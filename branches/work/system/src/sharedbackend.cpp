/* Implementation file sharedbackend.cpp
	
	Copyright 2012 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "system/sharedbackend.hpp"
#include "system/mutex.hpp"
#include "system/mutualstore.hpp"

#include <deque>
#include <stack>

struct SharedBackend::Data{
	typedef std::deque<SharedStub>	StubVectorT;
	typedef std::stack<ulong>		UlongStackT;
	typedef MutualStore<Mutex>		MutexStoreT;
	
	Mutex			mtx;
	StubVectorT		stubvec;
	UlongStackT		freestk;
	MutexStoreT		mtxstore;
};

/*static*/ SharedBackend& SharedBackend::the(){
	static SharedBackend sb;
	return sb;
}
	
SharedStub* SharedBackend::create(void *_pv, SharedStub::DelFncT _cbk){
	SharedStub		*pss;
	Locker<Mutex>	lock(d.mtx);
	if(d.freestk.size()){
		const ulong		pos = d.freestk.top();
		d.freestk.pop();
		Mutex			&rmtx = d.mtxstore.at(pos);
		Locker<Mutex>	lock(rmtx);
		SharedStub		&rss(d.stubvec[pos]);
		rss.cbk = _cbk;
		rss.ptr = _pv;
		rss.use = 1;
		pss = &rss;
	}else{
		const ulong		pos = d.stubvec.size();
		Mutex			&rmtx = d.mtxstore.safeAt(pos);
		Locker<Mutex>	lock(rmtx);
		
		d.stubvec.push_back(SharedStub(pos));
		SharedStub		&rss(d.stubvec[pos]);
		rss.cbk = _cbk;
		rss.ptr = _pv;
		rss.use = 1;
		pss = &rss;
	}
	return pss;
}

void SharedBackend::use(SharedStub &_rss){
    Mutex &rmtx = d.mtxstore.at(_rss.idx);
    Locker<Mutex> lock(rmtx);
    ++_rss.use;
}
void SharedBackend::release(SharedStub &_rss){
	Mutex &rmtx = d.mtxstore.at(_rss.idx);
	ulong v;
	{
		Locker<Mutex> lock(rmtx);
		--_rss.use;
		v = _rss.use;
	}
	if(v == 0){
		(*_rss.cbk)(_rss.ptr);
		Locker<Mutex>	lock(d.mtx);
		Locker<Mutex>	lock2(rmtx);
		d.freestk.push(_rss.idx);
		++_rss.uid;
	}
}

SharedBackend::SharedBackend():d(*(new Data)){
}
SharedBackend::~SharedBackend(){
	delete &d;
}

