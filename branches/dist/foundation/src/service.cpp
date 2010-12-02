/* Implementation file service.cpp
	
	Copyright 2007, 2008 Valentin Palade 
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

#include <deque>
#include <vector>
#include <algorithm>

#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/exception.hpp"

#include "utility/mutualstore.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "foundation/service.hpp"
#include "foundation/object.hpp"
#include "foundation/manager.hpp"
#include "foundation/signal.hpp"
#include "foundation/common.hpp"


namespace foundation{

//---------------------------------------------------------

struct Service::Data{
	typedef std::pair<Object*, uint32>		ObjectPairT;
	typedef std::deque<ObjectPairT>			ObjectVectorT;
	typedef Queue<uint32>					Uint32QueueT;
	
	typedef MutualStore<Mutex>				MutexStoreT;
	
	Data(
		int _objpermutbts,
		int _mutrowsbts,
		int _mutcolsbts
	):	objcnt(0), crtobjtypeid(0),
		mtxstore(_objpermutbts, _mutrowsbts, _mutcolsbts){
		
	}
	void popIndex(const IndexT &_idx);
	IndexT				objcnt;//object count
	uint16				crtobjtypeid;
	MutexStoreT			mtxstore;
	ObjectVectorT		objvec;
	Uint32QueueT		idxque;
	Condition			cnd;
	Condition			objcnd;
	Mutex				mtx;
};
void Service::Data::popIndex(const IndexT &_idx){
	IndexT sz(idxque.size());
	while(sz--){
		if(idxque.front() == _idx){
			idxque.pop();
			return;
		}
		IndexT	tmp(idxque.front());
		idxque.pop();
		idxque.push(tmp);
	}
	cassert(false);
}
//---------------------------------------------------------
Service::Service(
	int _objpermutbts,
	int _mutrowsbts,
	int _mutcolsbts
):d(*(new Data(_objpermutbts, _mutrowsbts, _mutcolsbts))){
	state(Stopped);
	registerObjectType<Object, Service>();
	registerVisitorType<Object, Visitor>();
	idbgx(Dbg::fdt, "");
}
//---------------------------------------------------------
Service::~Service(){
	stop(true);
	idbgx(Dbg::fdt, "");
	cassert(!d.objcnt);
	delete &d;
}
//---------------------------------------------------------
IndexT Service::size()const{
	return d.objcnt;
}
//---------------------------------------------------------
void Service::erase(const Object &_robj){
	const IndexT	oidx(_robj.index());
	Mutex::Locker	lock1(d.mtx);
	Mutex::Locker	lock2(d.mtxstore.at(oidx));
	
	ObjectTypeStub	&rots(objectTypeStub(_robj.typeId()));
	
	(*rots.erase_callback)(_robj, this);
	
	Data::ObjectPairT		&rop(d.objvec[oidx]);
	
	rop.first = NULL;
	++rop.second;
	d.idxque.push(oidx);
	d.objcnd.signal();//only one thread waits for 
}
//---------------------------------------------------------
bool Service::signal(DynamicPointer<Signal> &_rsig){
	if(this->state() < 0){
		return false;//no reason to raise the pool thread!!
	}
	de.push(DynamicPointer<>(_rsig));
	return Object::signal(fdt::S_SIG | fdt::S_RAISE);
}
//---------------------------------------------------------
bool Service::signal(ulong _sm){
	Mutex::Locker lock(d.mtx);
	return doSignalAll(_sm);
}
//---------------------------------------------------------
bool Service::signal(ulong _sm, const ObjectUidT &_ruid){
	return signal(_sm, _ruid.first, _ruid.second);
}
//---------------------------------------------------------
bool Service::signal(ulong _sm, IndexT _fullid, uint32 _uid){
	const IndexT	oidx(compute_index(_fullid));
	
	if(oidx >= d.objvec.size()) return false;
	
	Mutex::Locker	lock(d.mtxstore.at(oidx));
	
	if(_uid != d.objvec[oidx].second) return false;
	
	Object			*pobj(d.objvec[oidx].first);
	
	if(!pobj) return false;
	
	if(pobj->signal(_sm)){
		m().raiseObject(*pobj);
	}
	return true;
}
//---------------------------------------------------------
bool Service::signal(ulong _sm, const Object &_robj){
	const IndexT	oidx(_robj.index());
	
	cassert(oidx < d.objvec.size());
	
	Mutex::Locker	lock(d.mtxstore.at(oidx));
	
	//if(_uid != d.objvec[oidx].second) return false;
	
	Object			*pobj(d.objvec[oidx].first);
	
	if(!pobj) return false;
	
	if(pobj->signal(_sm)){
		m().raiseObject(*pobj);
	}
	return true;
}
//---------------------------------------------------------
bool Service::signal(DynamicSharedPointer<Signal> &_rsig){
	Mutex::Locker lock(d.mtx);
	return doSignalAll(_rsig);
}
//---------------------------------------------------------
bool Service::signal(DynamicPointer<Signal> &_rsig, const Object &_robj){
	const IndexT	oidx(_robj.index());
	
	cassert(oidx < d.objvec.size());
	
	Mutex::Locker	lock(d.mtxstore.at(oidx));
	
	//if(_uid != d.objvec[oidx].second) return false;
	
	Object			*pobj(d.objvec[oidx].first);
	
	if(!pobj) return false;
	
	if(pobj->signal(_rsig)){
		m().raiseObject(*pobj);
	}
	return true;
}
//---------------------------------------------------------
bool Service::signal(DynamicPointer<Signal> &_rsig, const ObjectUidT &_ruid){
	return signal(_rsig, _ruid.first, _ruid.second);
}
//---------------------------------------------------------
bool Service::signal(DynamicPointer<Signal> &_rsig, IndexT _fullid, uint32 _uid){
	const IndexT	oidx(compute_index(_fullid));
	
	if(oidx >= d.objvec.size()) return false;
	
	Mutex::Locker	lock(d.mtxstore.at(oidx));
	
	if(_uid != d.objvec[oidx].second) return false;
	
	Object			*pobj(d.objvec[oidx].first);
	
	if(!pobj) return false;
	
	if(pobj->signal(_rsig)){
		m().raiseObject(*pobj);
	}
	return true;
}
//---------------------------------------------------------
Mutex& Service::mutex(const Object &_robj){
	return d.mtxstore.at(_robj.index());
}
//---------------------------------------------------------
Mutex& Service::mutex(const IndexT &_ridx){
	return d.mtxstore.at(_ridx);
}
//---------------------------------------------------------
uint32 Service::uid(const Object &_robj)const{
	return d.objvec[_robj.index()].second;
}
//---------------------------------------------------------
uint32 Service::uid(const IndexT &_idx)const{
	return d.objvec[_idx].second;
}
//---------------------------------------------------------
bool Service::doStart(ObjectUidT &_robjuid){
	Mutex::Locker	lock(d.mtx);
	if(state() == Running){
		_robjuid = static_cast<Object*>(this)->uid();
		return false;
	}
	if(state() == Stopping){
		do{
			d.cnd.wait(d.mtx);
		}while(state() == Stopping);
		if(state() == Running){
			_robjuid = static_cast<Object*>(this)->uid();
			return false;
		}
	}
	state(Running);
	d.cnd.broadcast();
	doStart();
	_robjuid = m().insertService(this, this->index());
	return true;
}
//---------------------------------------------------------
void Service::stop(bool _wait){
	Mutex::Locker	lock(d.mtx);
	
	if(state() == Running){
		doSignalAll(S_KILL | S_RAISE);
		state(Stopping);
		doStop(false);
	}
	
	if(!_wait) return;
	
	while(d.objcnt){
		d.objcnd.wait(d.mtx);
	}
	
	doStop(true);
	
	m().signal(S_KILL | S_RAISE, static_cast<Object*>(this)->uid());
	
	while(state() == Stopping){
		d.cnd.wait(d.mtx);
	}
	m().eraseService(this);
}
//---------------------------------------------------------
/*virtual*/ int Service::execute(ulong _evs, TimeSpec &_rtout){
	if(signaled()){
		ulong sm;
		{
			Mutex::Locker	lock(Object::mutex());
			sm = grabSignalMask();
			if(sm & fdt::S_SIG){//we have signals
				de.prepareExecute();
			}
		}
		if(sm & fdt::S_SIG){//we've grabed signals, execute them
			de.execute(*this);
		}
		if(sm & fdt::S_KILL){
			Mutex::Locker	lock(d.mtx);
			if(state() == Stopping){
				idbgx(Dbg::ipc, "dying service "<<this->id());
				state(Stopped);
				d.cnd.broadcast();
				return BAD;
			}else{
				edbgx(Dbg::ipc, "Ignore S_KILL for service "<<this->index());
				return NOK;
			}
		}
	}
	return NOK;
}
//---------------------------------------------------------
Mutex &Service::serviceMutex()const{
	return d.mtx;
}
//---------------------------------------------------------
void Service::insertObject(Object &_ro, const ObjectUidT &_ruid){
	//by default do nothing
}
//---------------------------------------------------------
void Service::eraseObject(const Object &_ro){
	//by default do nothing
}
//---------------------------------------------------------
/*virtual*/ void Service::dynamicExecute(DynamicPointer<> &_dp){
	//by default do nothing
}
//---------------------------------------------------------
/*virtual*/ void Service::doStart(){
	//by default do nothing
}
//---------------------------------------------------------
/*virtual*/ void Service::doStop(bool _wait){
	//by default do nothing
}
//---------------------------------------------------------
namespace{

	void visit_lock(Mutex &_rm){
		_rm.lock();
	}

	void visit_unlock(Mutex &_rm){
		_rm.unlock();
	}

}//namespace

ObjectUidT Service::doInsertObject(Object &_ro, uint16 _tid, const IndexT &_ridx){
	uint32 u;
	if(is_invalid_index(_ridx)){
		if(d.idxque.size()){
			{
				const IndexT		idx(d.idxque.front());
				Mutex 				&rmut(d.mtxstore.at(idx));
				Mutex::Locker		lock(rmut);
				Data::ObjectPairT	&rop(d.objvec[idx]);
				
				d.idxque.pop();
				rop.first = &_ro;
				_ro.id(this->index(), idx);
				u = rop.second;
			}
		}else{
			//worst case scenario
			const IndexT	sz(d.objvec.size());
			
			d.mtxstore.safeAt(sz);

			_ro.id(this->index(), sz);
			
			d.mtxstore.visit(sz, visit_lock);//lock all mutexes
			
			d.objvec.push_back(Data::ObjectPairT(&_ro, 0));
			u = 0;
			//reserve some positions
			uint			cnt(63);
			const IndexT	initialsize(d.objvec.size());
			while(cnt--){
				d.mtxstore.safeAt(d.objvec.size());
				d.idxque.push(initialsize + 62 - cnt);
				d.objvec.push_back(Data::ObjectPairT(NULL, 0));
			}
			
			d.mtxstore.visit(sz, visit_unlock);//unlock all mutexes
		}
		++d.objcnt;
		return ObjectUidT(_ro.id(), u);
	}else{
		if(_ridx < d.objvec.size()){
			{
				Mutex 				&rmut(d.mtxstore.at(_ridx));
				Mutex::Locker		lock(rmut);
				Data::ObjectPairT	&rop(d.objvec[_ridx]);
				if(!rop.first){
					rop.first = &_ro;
					_ro.id(this->index(), _ridx);
					u = rop.second;
				}else{
					THROW_EXCEPTION_EX("Multiple inserts on the same index ",_ridx);
					return invalid_uid();
				}
			}
			d.popIndex(_ridx);
			return ObjectUidT(_ro.id(), u);
		}else{
			//worst case scenario
			const IndexT	initialsize(d.objvec.size());
			d.objvec.resize(smart_resize(d.objvec, 256));
			const IndexT	sz(d.objvec.size());
			const IndexT	diffsz(sz - initialsize - 1);
			
			d.mtxstore.safeAt(sz);

			_ro.id(this->index(), _ridx);
			
			d.mtxstore.visit(sz, visit_lock);//lock all mutexes
			
			//reserve some positions
			IndexT		cnt(diffsz + 1);
			while(cnt--){
				d.mtxstore.safeAt(d.objvec.size());
				IndexT crtidx(initialsize + diffsz - cnt);
				if(crtidx != _ridx){
					d.idxque.push(crtidx);
				}
			}
			
			d.objvec[_ridx].first = &_ro;
			u = 0;
			
			d.mtxstore.visit(sz, visit_unlock);//unlock all mutexes
			return ObjectUidT(_ro.id(), u);
		}
	}
}
//---------------------------------------------------------
uint Service::newObjectTypeId(){
	uint r;
	{
		Mutex::Locker lock(serviceMutex());
		r = d.crtobjtypeid;
		++d.crtobjtypeid;
	}
	return r;
}
//---------------------------------------------------------
Object* Service::objectAt(const IndexT &_ridx, uint32 _uid){
	if(_ridx < d.objvec.size() && d.objvec[_ridx].second == _uid){
		return d.objvec[_ridx].first;
	}
	return NULL;
}
//---------------------------------------------------------
Object* Service::objectAt(const IndexT &_ridx){
	return d.objvec[_ridx].first;
}
//---------------------------------------------------------
void Service::doVisit(Object *_po, Visitor &_rv, uint32 _visidx){
	
}
//---------------------------------------------------------
void Service::invalidateService(){
	this->id(invalid_uid().first);
}
//---------------------------------------------------------
bool Service::doSignalAll(ulong _sm){
	ulong	oc(d.objcnt);
	ulong	i(0);
	long	mi(-1);
	bool	signaled(false);
	Manager	&rm(m());
	idbgx(Dbg::fdt, "signalling "<<oc<<" objects");
	
	for(Data::ObjectVectorT::iterator it(d.objvec.begin()); oc && it != d.objvec.end(); ++it, ++i){
		if(it->first){
			if(d.mtxstore.isRangeBegin(i)){
				if(mi >= 0)	d.mtxstore[mi].unlock();
				++mi;
				d.mtxstore[mi].lock();
			}
			if(it->first->signal(_sm)){
				rm.raiseObject(*it->first);
			}
			signaled = true;
			--oc;
		}
	}
	if(mi >= 0)	d.mtxstore[mi].unlock();
	return signaled;
}
//---------------------------------------------------------
bool Service::doSignalAll(DynamicSharedPointer<Signal> &_rsig){
	ulong	oc(d.objcnt);
	ulong	i(0);
	long	mi(-1);
	bool	signaled(false);
	Manager	&rm(m());
	idbgx(Dbg::fdt, "signalling "<<oc<<" objects");
	
	for(Data::ObjectVectorT::iterator it(d.objvec.begin()); oc && it != d.objvec.end(); ++it, ++i){
		if(it->first){
			if(d.mtxstore.isRangeBegin(i)){
				if(mi >= 0)	d.mtxstore[mi].unlock();
				++mi;
				d.mtxstore[mi].lock();
			}
			DynamicPointer<Signal> sig(_rsig);
			if(it->first->signal(sig)){
				rm.raiseObject(*it->first);
			}
			signaled = true;
			--oc;
		}
	}
	if(mi >= 0)	d.mtxstore[mi].unlock();
	return signaled;
}
//---------------------------------------------------------
bool Service::doVisit(Visitor &_rv, uint _visidx){
	IndexT				oc(d.objcnt);
	IndexT				i(0);
	long				mi(-1);
	bool				signaled(false);
	VisitorTypeStub		&rvts(vistpvec[_visidx]);
	
	idbgx(Dbg::fdt, "visiting "<<oc<<" objects");
	
	for(Data::ObjectVectorT::iterator it(d.objvec.begin()); oc && it != d.objvec.end(); ++it, ++i){
		if(it->first){
			if(d.mtxstore.isRangeBegin(i)){
				if(mi >= 0)	d.mtxstore[mi].unlock();
				++mi;
				d.mtxstore[mi].lock();
			}
			Object			*pobj(it->first);
			
			if(pobj->typeId() < rvts.cbkvec.size() && rvts.cbkvec[pobj->typeId()] != NULL){
				(*rvts.cbkvec[pobj->typeId()])(pobj, _rv);
			}else{
				Service::visit_cbk<Object, Visitor>(pobj, _rv);
			}
			
			signaled = true;
			--oc;
		}
	}
	if(mi >= 0)	d.mtxstore[mi].unlock();
	return signaled;
}
//---------------------------------------------------------
bool Service::doVisit(Visitor &_rv, uint _visidx, const ObjectUidT &_ruid){
	const IndexT	oidx(compute_index(_ruid.first));
	
	if(oidx >= d.objvec.size()) return false;
	
	Mutex::Locker	lock(d.mtxstore.at(oidx));
	
	if(_ruid.second != d.objvec[oidx].second) return false;
	
	Object			*pobj(d.objvec[oidx].first);
	
	if(!pobj) return false;
	
	VisitorTypeStub	&rvts(vistpvec[_visidx]);
	
	if(pobj->typeId() < rvts.cbkvec.size() && rvts.cbkvec[pobj->typeId()] != NULL){
		(*rvts.cbkvec[pobj->typeId()])(pobj, _rv);
	}else{
		Service::visit_cbk<Object, Visitor>(pobj, _rv);
	}
	return true;
}
//---------------------------------------------------------
}//namespace fundation


