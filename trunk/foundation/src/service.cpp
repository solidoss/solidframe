/* Implementation file service.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <deque>
#include <vector>
#include <algorithm>

#include "system/debug.hpp"
#include "system/mutex.hpp"

#include "utility/queue.hpp"

#include "foundation/object.hpp"
#include "foundation/manager.hpp"
#include "foundation/signal.hpp"
#include "foundation/readwriteservice.hpp"
#include "foundation/common.hpp"

namespace foundation{

typedef std::pair<Object*, uint32>	ObjPairT;
class ObjectVector:public std::deque<ObjPairT>{};
class IndexStack: public std::stack<ulong>{};

Service::Service(int _objpermutbts, int _mutrowsbts, int _mutcolsbts):
			objv(*(new ObjectVector)),
			inds(*(new IndexStack)),
			mut(NULL),
			objcnt(0),
			mutpool(_objpermutbts, _mutrowsbts, _mutcolsbts){
	state(Running);
	idbgx(Dbg::fdt, "");
}

Service::~Service(){
	//stop();
	idbgx(Dbg::fdt, "");
	cassert(!objcnt);
	delete &objv;
	delete &inds;
}

int Service::insert(Object &_robj, IndexT _srvid){
	Mutex::Locker lock(*mut);
	return doInsert(_robj, _srvid);
}
namespace{
void lock(Mutex &_rm){
	_rm.lock();
}
void unlock(Mutex &_rm){
	_rm.unlock();
}
}
int Service::doInsert(Object &_robj, IndexT _srvid){
	if(state() != Running) return BAD;
	if(inds.size()){
		{
			Mutex &rmut = mutpool.object(inds.top());
			Mutex::Locker lock(rmut);
			_robj.mutex(&rmut);
			objv[inds.top()].first = &_robj;
			_robj.id(_srvid, inds.top());
		}
		inds.pop();
	}else{
		//worst case scenario
		const uint32 sz(objv.size());
		Mutex &rmut(mutpool.safeObject(sz));
		//Mutex::Locker lock(rmut);
		_robj.mutex(&rmut);
		_robj.id(_srvid, sz);
		mutpool.visit(sz, lock);//lock all mutexes
		objv.push_back(ObjPairT(&_robj, 0));	
		mutpool.visit(sz, unlock);//unlock all mutexes
	}
	++objcnt;
	return OK;
}

void Service::mutex(Mutex *_pmut){
	mut = _pmut;
}
void Service::remove(Object &_robj){
	Mutex::Locker	lock1(*mut);
	//obj's mutext should not be locked
	Mutex::Locker	lock2(mutpool.object(_robj.index()));
	idbgx(Dbg::fdt, "removig object with index "<<_robj.index());
	objv[_robj.index()].first = NULL;
	++objv[_robj.index()].second;
	inds.push(_robj.index());
	--objcnt;
	cond.signal();
}
/**
 * Be very carefull when using this function as you can raise/kill
 * other object than you might want.
 */
int Service::signal(IndexT _fullid, uint32 _uid, ulong _sigmask){
	ulong oidx(Object::computeIndex(_fullid));
	if(oidx >= objv.size()) return BAD;
	Mutex::Locker	lock(mutpool.object(oidx));
	if(_uid != objv[oidx].second) return BAD;
	Object *pobj = objv[oidx].first;
	if(!pobj) return BAD;
	if(pobj->signal(_sigmask)){
		Manager::the().raiseObject(*pobj);
	}
	return OK;
}

int Service::signal(Object &_robj, ulong _sigmask){
	Mutex::Locker	lock(mutpool.object(_robj.index()));
	if(_robj.signal(_sigmask)){
		Manager::the().raiseObject(_robj);
	}
	return OK;
}

Mutex& Service::mutex(IndexT _fullid, uint32){
	return mutpool.object(Object::computeIndex(_fullid));
}

Object* Service::object(IndexT _fullid, uint32 _uid){
	ulong oidx(Object::computeIndex(_fullid));
	if(oidx >= objv.size()) return NULL;
	if(_uid != objv[oidx].second) return NULL;
	return objv[oidx].first;
}

int Service::signal(Object &_robj, DynamicPointer<Signal> &_rsig){
	Mutex::Locker	lock(mutpool.object(_robj.index()));
	if(_robj.signal(_rsig)){
		Manager::the().raiseObject(_robj);
	}
	return OK;
}

int Service::signal(IndexT _fullid, uint32 _uid, DynamicPointer<Signal> &_rsig){
	ulong oidx(Object::computeIndex(_fullid));
	if(oidx >= objv.size()) return BAD;
	Mutex::Locker	lock(mutpool.object(oidx));
	if(_uid != objv[oidx].second) return BAD;
	Object *pobj = objv[oidx].first;
	if(!pobj) return BAD;
	if(pobj->signal(_rsig)){
		Manager::the().raiseObject(*pobj);
	}
	return OK;
}

void Service::signalAll(ulong _sigmask){
	Mutex::Locker	lock(*mut);
	doSignalAll(Manager::the(), _sigmask);
}

void Service::signalAll(DynamicPointer<Signal> &_rsig){
	Mutex::Locker	lock(*mut);
	doSignalAll(Manager::the(), _rsig);
}

void Service::doSignalAll(Manager &_rm, ulong _sigmask){
	int oc = objcnt;
	int i = 0;
	int mi = -1;
	idbgx(Dbg::fdt, "signalling "<<oc<<" objects");
	for(ObjectVector::iterator it(objv.begin()); oc && it != objv.end(); ++it, ++i){
		if(it->first){
			//if(!(i & objpermutmsk)){
			if(mutpool.isRangeBegin(i)){
				if(mi >= 0)	mutpool[mi].unlock();
				++mi;
				mutpool[mi].lock();
			}
			if(it->first->signal(_sigmask)){
				_rm.raiseObject(*it->first);
			}
			--oc;
		}
	}
	if(mi >= 0)	mutpool[mi].unlock();
}

void Service::doSignalAll(Manager &_rm, DynamicPointer<Signal> &_rsig){
	int oc = objcnt;
	int i = 0;
	int mi = -1;
	idbgx(Dbg::fdt, "signalling "<<oc<<" objects");
	for(ObjectVector::iterator it(objv.begin()); oc && it != objv.end(); ++it, ++i){
		if(it->first){
			//if(!(i & objpermutmsk)){
			if(mutpool.isRangeBegin(i)){
				if(mi >= 0)	mutpool[mi].unlock();
				++mi;
				mutpool[mi].lock();
			}
			DynamicPointer<Signal> sig(_rsig.ptr());
			if(it->first->signal(sig)){
				_rm.raiseObject(*it->first);
			}
			--oc;
		}
	}
	if(mi >= 0)	mutpool[mi].unlock();
}

void Service::visit(Visitor &_rov){
	Mutex::Locker	lock(*mut);
	int oc = objcnt;
	int i = 0;
	int mi = -1;
	for(ObjectVector::iterator it(objv.begin()); oc && it != objv.end(); ++it, ++i){
		if(it->first){
			if(mutpool.isRangeBegin(i)){
				if(mi >= 0)	mutpool[mi].unlock();
				++mi;
				mutpool[mi].lock();
			}
			if(it->first->accept(_rov)){
				Manager::the().raiseObject(*it->first);
			}
			--oc;
		}
	}
	if(mi >= 0)	mutpool[mi].unlock();
}

Mutex& Service::mutex(Object &_robj){
	return mutpool.object(_robj.index());
}

uint32 Service::uid(Object &_robj)const{
	return objv[_robj.index()].second;
}

uint32 Service::uid(const uint32 _idx)const{
	return objv[_idx].second;
}

int Service::start(){
	Mutex::Locker	lock(*mut);
	if(state() != Stopped) return OK;
	state(Running);
	return OK;
}

int Service::stop(bool _wait){
	Mutex::Locker	lock(*mut);
	if(state() == Running){
		doSignalAll(Manager::the(), S_KILL | S_RAISE);
		state(Stopping);
	}
	if(!_wait) return OK;
	while(objcnt){
		cond.wait(*mut);
	}
	state(Stopped);
	return OK;
}
int Service::execute(){
	return BAD;
}
//***********************************************************************
ReadWriteService::~ReadWriteService(){
}
Condition* ReadWriteService::popCondition(Object &_robj){
	CondStackT &rs = cndpool.object(_robj.index());
	Condition *pc = NULL;
	if(rs.size()){
		pc = rs.top();rs.pop();
	}else pc = new Condition;
	return pc;
}
void ReadWriteService:: pushCondition(Object &_robj, Condition *&_rpc){
	if(_rpc){
		cndpool.object(_robj.index()).push(_rpc);
		_rpc = NULL;
	}
}
ReadWriteService::ReadWriteService(int _objpermutbts,
									int _mutrowsbts,
									int _mutcolsbts
									):
									Service(_objpermutbts, _mutrowsbts, _mutcolsbts){}
int ReadWriteService::insert(Object &_robj, ulong _srvid){
	Mutex::Locker lock(*mut);
	if(state() != Running) return BAD;
	if(inds.size()){
		{
			Mutex::Locker lock(mutpool.object(inds.top()));
			objv[inds.top()].first = &_robj;
			_robj.id(_srvid, inds.top());
		}
		inds.pop();
	}else{
		//worst case scenario
		Mutex::Locker lock(mutpool.safeObject(objv.size()));
		//just ensure the resevation in cndpool
		cndpool.safeObject(objv.size());
		_robj.id(_srvid, objv.size());
		objv.push_back(ObjPairT(&_robj, 0));	
	}
	++objcnt;
	return OK;
}
}//namespace

