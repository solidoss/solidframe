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
#include "system/condition.hpp"

#include "utility/mutualstore.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "foundation/object.hpp"
#include "foundation/manager.hpp"
#include "foundation/signal.hpp"
#include "foundation/readwriteservice.hpp"
#include "foundation/common.hpp"


namespace foundation{

struct Service::Data{
	typedef std::pair<Object*, uint32>		ObjectPairT;
	typedef std::deque<ObjectPairT>			ObjectVectorT;
	typedef Stack<ulong>					ULongStackT;
	
	typedef MutualStore<Mutex>				MutexStoreT;
	
	Data(
		int _objpermutbts,
		int _mutrowsbts,
		int _mutcolsbts
	):	pmtx(NULL), objcnt(0),
		mtxstore(_objpermutbts, _mutrowsbts, _mutcolsbts){}
	
	ObjectVectorT		objv;
	ULongStackT			inds;
	Condition			cond;
	Mutex				*pmtx;
	ulong				objcnt;//object count
	MutexStoreT			mtxstore;
};
	
typedef std::pair<Object*, uint32>	ObjPairT;
class ObjectVector:public std::deque<ObjPairT>{};
class IndexStack: public std::stack<ulong>{};

Service::Service(
	int _objpermutbts,
	int _mutrowsbts,
	int _mutcolsbts
):d(*(new Data(_objpermutbts, _mutrowsbts, _mutcolsbts))){
	state(Running);
	idbgx(Dbg::fdt, "");
}

Service::~Service(){
	//stop();
	idbgx(Dbg::fdt, "");
	cassert(!d.objcnt);
	delete &d;
}

int Service::insert(Object &_robj, IndexT _srvid){
	Mutex::Locker lock(*d.pmtx);
	return doInsert(_robj, _srvid);
}

namespace{

void visit_lock(Mutex &_rm){
	_rm.lock();
}

void visit_unlock(Mutex &_rm){
	_rm.unlock();
}

}

int Service::doInsert(Object &_robj, IndexT _srvid){
	if(state() != Running) return BAD;
	if(d.inds.size()){
		{
			Mutex 			&rmut(d.mtxstore.at(d.inds.top()));
			Mutex::Locker	lock(rmut);
			
			_robj.mutex(&rmut);
			d.objv[d.inds.top()].first = &_robj;
			_robj.id(_srvid, d.inds.top());
		}
		d.inds.pop();
	}else{
		//worst case scenario
		const ulong	sz(d.objv.size());
		Mutex 		&rmut(d.mtxstore.safeAt(sz));

		_robj.mutex(&rmut);
		_robj.id(_srvid, sz);
		
		d.mtxstore.visit(sz, visit_lock);//lock all mutexes
		
		d.objv.push_back(Data::ObjectPairT(&_robj, 0));
		
		//reserve some positions
		uint			cnt(63);
		const ulong		initialsize(d.objv.size());
		while(cnt--){
			d.mtxstore.safeAt(d.objv.size());
			d.inds.push(initialsize + cnt);
			d.objv.push_back(Data::ObjectPairT(NULL, 0));
		}
		
		d.mtxstore.visit(sz, visit_unlock);//unlock all mutexes
	}
	++d.objcnt;
	return OK;
}

void Service::mutex(Mutex *_pmtx){
	d.pmtx = _pmtx;
}

Mutex* Service::mutex()const{
	return d.pmtx;
}

void Service::remove(Object &_robj){
	Mutex::Locker	lock1(*d.pmtx);
	//obj's mutext should not be locked
	Mutex::Locker	lock2(d.mtxstore.at(_robj.index()));
	
	idbgx(Dbg::fdt, "removig object with index "<<_robj.index());
	
	d.objv[_robj.index()].first = NULL;
	++d.objv[_robj.index()].second;
	d.inds.push(_robj.index());
	--d.objcnt;
	d.cond.signal();
}
/**
 * Be very carefull when using this function as you can raise/kill
 * other object than you might want.
 */
int Service::signal(IndexT _fullid, uint32 _uid, ulong _sigmask){
	ulong			oidx(Object::computeIndex(_fullid));
	
	if(oidx >= d.objv.size()) return BAD;
	
	Mutex::Locker	lock(d.mtxstore.at(oidx));
	
	if(_uid != d.objv[oidx].second) return BAD;
	
	Object			*pobj(d.objv[oidx].first);
	
	if(!pobj) return BAD;
	
	if(pobj->signal(_sigmask)){
		Manager::the().raiseObject(*pobj);
	}
	return OK;
}

int Service::signal(Object &_robj, ulong _sigmask){
	Mutex::Locker	lock(d.mtxstore.at(_robj.index()));
	
	if(_robj.signal(_sigmask)){
		Manager::the().raiseObject(_robj);
	}
	return OK;
}

Mutex& Service::mutex(IndexT _fullid, uint32){
	return d.mtxstore.at(Object::computeIndex(_fullid));
}

Object* Service::object(IndexT _fullid, uint32 _uid){
	const ulong oidx(Object::computeIndex(_fullid));
	
	if(oidx >= d.objv.size()) return NULL;
	
	if(_uid != d.objv[oidx].second) return NULL;
	
	return d.objv[oidx].first;
}

int Service::signal(Object &_robj, DynamicPointer<Signal> &_rsig){
	Mutex::Locker	lock(d.mtxstore.at(_robj.index()));
	
	if(_robj.signal(_rsig)){
		Manager::the().raiseObject(_robj);
	}
	return OK;
}

int Service::signal(IndexT _fullid, uint32 _uid, DynamicPointer<Signal> &_rsig){
	const ulong		oidx(Object::computeIndex(_fullid));
	
	if(oidx >= d.objv.size()) return BAD;
	
	Mutex::Locker	lock(d.mtxstore.at(oidx));
	
	if(_uid != d.objv[oidx].second) return BAD;
	
	Object 			*pobj = d.objv[oidx].first;
	
	if(!pobj) return BAD;
	
	if(pobj->signal(_rsig)){
		Manager::the().raiseObject(*pobj);
	}
	return OK;
}

void Service::signalAll(ulong _sigmask){
	Mutex::Locker	lock(*d.pmtx);
	doSignalAll(Manager::the(), _sigmask);
}

void Service::signalAll(DynamicPointer<Signal> &_rsig){
	Mutex::Locker	lock(*d.pmtx);
	doSignalAll(Manager::the(), _rsig);
}

void Service::doSignalAll(Manager &_rm, ulong _sigmask){
	ulong	oc(d.objcnt);
	ulong	i(0);
	long	mi(-1);
	
	idbgx(Dbg::fdt, "signalling "<<oc<<" objects");
	
	for(Data::ObjectVectorT::iterator it(d.objv.begin()); oc && it != d.objv.end(); ++it, ++i){
		if(it->first){
			if(d.mtxstore.isRangeBegin(i)){
				if(mi >= 0)	d.mtxstore[mi].unlock();
				++mi;
				d.mtxstore[mi].lock();
			}
			if(it->first->signal(_sigmask)){
				_rm.raiseObject(*it->first);
			}
			--oc;
		}
	}
	if(mi >= 0)	d.mtxstore[mi].unlock();
}

void Service::doSignalAll(Manager &_rm, DynamicPointer<Signal> &_rsig){
	ulong	oc(d.objcnt);
	ulong	i(0);
	long	mi(-1);
	
	idbgx(Dbg::fdt, "signalling "<<oc<<" objects");
	
	for(Data::ObjectVectorT::iterator it(d.objv.begin()); oc && it != d.objv.end(); ++it, ++i){
		if(it->first){
			if(d.mtxstore.isRangeBegin(i)){
				if(mi >= 0)	d.mtxstore[mi].unlock();
				++mi;
				d.mtxstore[mi].lock();
			}
			DynamicPointer<Signal> sig(_rsig.ptr());
			if(it->first->signal(sig)){
				_rm.raiseObject(*it->first);
			}
			--oc;
		}
	}
	if(mi >= 0)	d.mtxstore[mi].unlock();
}

void Service::visit(Visitor &_rov){
	Mutex::Locker	lock(*d.pmtx);
	ulong			oc(d.objcnt);
	ulong			i(0);
	long			mi(-1);
	for(Data::ObjectVectorT::iterator it(d.objv.begin()); oc && it != d.objv.end(); ++it, ++i){
		if(it->first){
			if(d.mtxstore.isRangeBegin(i)){
				if(mi >= 0)	d.mtxstore[mi].unlock();
				++mi;
				d.mtxstore[mi].lock();
			}
			if(it->first->accept(_rov)){
				Manager::the().raiseObject(*it->first);
			}
			--oc;
		}
	}
	if(mi >= 0)	d.mtxstore[mi].unlock();
}

Mutex& Service::mutex(const Object &_robj){
	return d.mtxstore.at(_robj.index());
}

uint32 Service::uid(const Object &_robj)const{
	return d.objv[_robj.index()].second;
}

uint32 Service::uid(const uint32 _idx)const{
	return d.objv[_idx].second;
}

int Service::start(){
	Mutex::Locker	lock(*d.pmtx);
	if(state() != Stopped) return OK;
	state(Running);
	return OK;
}

int Service::stop(bool _wait){
	Mutex::Locker	lock(*d.pmtx);
	
	if(state() == Running){
		doSignalAll(Manager::the(), S_KILL | S_RAISE);
		state(Stopping);
	}
	
	if(!_wait) return OK;
	
	while(d.objcnt){
		d.cond.wait(*d.pmtx);
	}
	state(Stopped);
	return OK;
}
int Service::execute(){
	return BAD;
}

ulong Service::indexStackSize()const{
	return d.inds.size();
}
ulong Service::indexStackTop()const{
	return d.inds.top();
}
void Service::indexStackPop(){
	d.inds.pop();
}
Mutex& Service::mutexAt(ulong _idx){
	return d.mtxstore.at(_idx);
}
void Service::lockForPushBack(){
	const ulong	sz(d.objv.size());
	d.mtxstore.safeAt(sz);
	d.mtxstore.visit(sz, visit_lock);//lock all mutexes
}
void Service::unlockForPushBack(){
	const ulong	sz(d.objv.size() - 1);
	d.mtxstore.visit(sz, visit_unlock);//unlock all mutexes
}
void Service::insertObject(Object &_robj, ulong _srvid){
	d.objv[d.inds.top()].first = &_robj;
	_robj.id(_srvid, d.inds.top());
	_robj.mutex(&mutexAt(d.inds.top()));
	++d.objcnt;
}
void Service::appendObject(Object &_robj, ulong _srvid){
	_robj.id(_srvid, d.objv.size());
	_robj.mutex(&mutexAt(d.objv.size()));
	d.objv.push_back(Data::ObjectPairT(&_robj, 0));
	++d.objcnt;
}
ulong Service::objectVectorSize()const{
	return d.objv.size();
}

//***********************************************************************

ReadWriteService::~ReadWriteService(){
}

Condition* ReadWriteService::popCondition(Object &_robj){
	CondStackT &rs = cndstore.at(_robj.index());
	Condition *pc = NULL;
	if(rs.size()){
		pc = rs.top();rs.pop();
	}else pc = new Condition;
	return pc;
}

void ReadWriteService:: pushCondition(Object &_robj, Condition *&_rpc){
	if(_rpc){
		cndstore.at(_robj.index()).push(_rpc);
		_rpc = NULL;
	}
}

ReadWriteService::ReadWriteService(
	int _objpermutbts,
	int _mutrowsbts,
	int _mutcolsbts
):Service(_objpermutbts, _mutrowsbts, _mutcolsbts){}


int ReadWriteService::insert(Object &_robj, ulong _srvid){
	Mutex::Locker lock(*mutex());
	if(state() != Running) return BAD;
	if(indexStackSize()){
		{
			Mutex::Locker lock(mutexAt(indexStackTop()));
			insertObject(_robj, _srvid);
		}
		indexStackPop();
	}else{
		//worst case scenario
		
		lockForPushBack();

		//just ensure the resevation in cndstore
		cndstore.safeAt(objectVectorSize());
		appendObject(_robj, _srvid);
		
		unlockForPushBack();
	}
	//++objcnt;
	return OK;
}
}//namespace

