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

/*static*/ const Service::EraseCbkT		Service::ObjectTypeStub::default_erase_cbk(&Service::erase_cbk<Object, Service>);
/*static*/ const Service::InsertCbkT	Service::ObjectTypeStub::default_insert_cbk(&Service::insert_cbk<Object, Service>);
//---------------------------------------------------------

struct Service::Data{
	enum States{
		Starting,
		Running,
		Stopping,
		StoppingDone,
		Stopped
	};
	enum ExeStates{
		ExeStarting,
		ExeRunning,
		ExeStopping,
		ExeStoppingWait,
		ExeStopped,
		ExeDying,
		ExeDyingWait,
		ExeDie,
	};
	typedef std::pair<Object*, uint32>		ObjectPairT;
	typedef std::deque<ObjectPairT>			ObjectVectorT;
	typedef Queue<uint32>					Uint32QueueT;
	
	typedef MutualStore<Mutex>				MutexStoreT;
	
	Data(
		bool _started,
		int _objpermutbts,
		int _mutrowsbts,
		int _mutcolsbts
	):	objcnt(0), expcnt(0), st(_started ? Starting : Stopped),
		mtxstore(_objpermutbts, _mutrowsbts, _mutcolsbts), mtx(NULL){
		mtxstore.safeAt(0);
	}
	void popIndex(const IndexT &_idx);
	
	IndexT				objcnt;//object count
	IndexT				expcnt;
	int					st;
	MutexStoreT			mtxstore;
	ObjectVectorT		objvec;
	Uint32QueueT		idxque;
	Condition			cnd;
	//Mutex				mtx;
	Mutex				*mtx;
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
/*static*/ void Service::dynamicRegister(){
	
}
Service::Service(
	bool _started,
	int _objpermutbts,
	int _mutrowsbts,
	int _mutcolsbts
):d(*(new Data(_started, _objpermutbts, _mutrowsbts, _mutcolsbts))){
	if(_started){
		state(Data::ExeStarting);
	}else{
		state(Data::Stopped);
	}
	registerObjectType<Object>(this);
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
	ObjectUidT u;
	{
		const IndexT		oidx(_robj.index());
		Mutex::Locker		lock1(*d.mtx);
		Mutex::Locker		lock2(d.mtxstore.at(oidx));
		
		ObjectTypeStub		&rots(objectTypeStub(_robj.dynamicTypeId()));
		
		(*rots.erase_callback)(_robj, this);
		
		Data::ObjectPairT	&rop(d.objvec[oidx]);
		
		rop.first = NULL;
		++rop.second;
		--d.objcnt;
		vdbgx(Dbg::fdt, "erase object "<<d.objcnt<<" state "<<d.st);
		d.idxque.push(oidx);
		if(d.st == Data::Stopping && d.objcnt == d.expcnt){
			vdbgx(Dbg::fdt, "signal service");
			d.st = Data::StoppingDone;
			u = static_cast<Object*>(this)->uid();
		}else return;
	}
	m().signal(S_RAISE | S_UPDATE, u);
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
	Mutex::Locker lock(*d.mtx);
	return doSignalAll(_sm);
}
//---------------------------------------------------------
bool Service::signal(ulong _sm, const ObjectUidT &_ruid){
	return signal(_sm, _ruid.first, _ruid.second);
}
//---------------------------------------------------------
bool Service::signal(ulong _sm, IndexT _fullid, uint32 _uid){
	const IndexT	oidx(Manager::the().computeIndex(_fullid));
	
	if(oidx >= d.objvec.size()){
		return false;
	}
	
	Mutex::Locker	lock(d.mtxstore.at(oidx));
	
	if(_uid != d.objvec[oidx].second){
		return false;
	}
	
	Object			*pobj(d.objvec[oidx].first);
	
	if(!pobj){
		return false;
	}
	
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
	
	if(!pobj){
		return false;
	}
	
	if(pobj->signal(_sm)){
		m().raiseObject(*pobj);
	}
	return true;
}
//---------------------------------------------------------
bool Service::signal(DynamicSharedPointer<Signal> &_rsig){
	Mutex::Locker lock(*d.mtx);
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
	const IndexT	oidx(Manager::the().computeIndex(_fullid));
	
	if(oidx >= d.objvec.size()){
		return false;
	}
	
	Mutex::Locker	lock(d.mtxstore.at(oidx));
	
	if(_uid != d.objvec[oidx].second){
		return false;
	}
	
	Object			*pobj(d.objvec[oidx].first);
	
	if(!pobj){
		return false;
	}
	
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
/*virtual*/ int Service::start(bool _wait){
	{
		Mutex::Locker	lock(*d.mtx);
		bool			reenter;
		do{
			reenter = false;
			if(d.st == Data::Running){
				return OK;
			}else if(d.st < Data::Running){
				if(!_wait) return NOK;
				do{
					d.cnd.wait(*d.mtx);
				}while(d.st != Data::Running && d.st != Data::Stopped);
				if(d.st == Data::Running){
					return OK;
				}
				return BAD;
			}else if(d.st < Data::Stopped){
				//if(!_wait) return false;
				do{
					d.cnd.wait(*d.mtx);
				}while(d.st != Data::Stopped && d.st != Data::Running);
				if(d.st == Data::Running){
					reenter = true;
				}
			}
		}while(reenter);
		
		cassert(d.st == Data::Stopped);
		
		d.st = Data::Starting;
	}
	
	m().signal(S_RAISE | S_UPDATE, static_cast<Object*>(this)->uid());
	
	if(!_wait) return NOK;
	{
		Mutex::Locker	lock(*d.mtx);
		do{
			d.cnd.wait(*d.mtx);
		}while(d.st != Data::Running && d.st != Data::Stopped);
		if(d.st == Data::Running){
			return OK;
		}
		return BAD;
	}
}
//---------------------------------------------------------
/*virtual*/ int Service::stop(bool _wait){
	{
		Mutex::Locker	lock(*d.mtx);
		
		if(d.st == Data::Stopped){
			return OK;
		}else if(d.st < Data::Running){
			do{
				d.cnd.wait(*d.mtx);
			}while(d.st != Data::Running && d.st != Data::Stopped);
			if(d.st == Data::Stopped){
				return OK;
			}
		}else if(d.st == Data::Running){
			
		}else if(d.st < Data::Stopped){
			do{
				d.cnd.wait(*d.mtx);
			}while(d.st != Data::Stopped && d.st != Data::Running);
			if(d.st == Data::Stopped){
				return OK;
			}
		}
		cassert(d.st == Data::Running);
		
		d.st = Data::Stopping;
	}
	
	m().signal(S_RAISE | S_UPDATE, static_cast<Object*>(this)->uid());
	
	if(!_wait) return NOK;
	{
		Mutex::Locker	lock(*d.mtx);
		do{
			d.cnd.wait(*d.mtx);
		}while(d.st != Data::Stopped && d.st != Data::Running);
		if(d.st == Data::Running){
			return BAD;
		}
	}
	return OK;
}
//---------------------------------------------------------
/*virtual*/ int Service::execute(ulong _evs, TimeSpec &_rtout){
	ulong sm(0);
	if(signaled()){
		{
			Mutex::Locker	lock(Object::mutex());
			sm = grabSignalMask();
			if(sm & fdt::S_SIG){//we have signals
				de.prepareExecute();
			}
		}
		if(sm & fdt::S_SIG){//we've grabed signals, execute them
			de.executeAll(*this);
		}
		if(sm & S_KILL){
			if(state() < Data::ExeDying){
				state(Data::ExeDying);
			}
		}
	}
	idbgx(Dbg::fdt, ""<<sm);
	switch(state()){
		case Data::ExeStarting:
			idbgx(Dbg::fdt, "ExeStarting");
			switch(doStart(_evs, _rtout)){
				case BAD:{
					Mutex::Locker	lock(*d.mtx);
					state(Data::ExeStopping);
					d.st = Data::Stopping;
					return OK;
				}	
				case OK:{
					Mutex::Locker	lock(*d.mtx);
					state(Data::ExeRunning);
					d.st = Data::Running;
					d.cnd.broadcast();
					//return OK;
				}	
				case NOK:
					return NOK;
			}
		case Data::ExeRunning:
			idbgx(Dbg::fdt, "ExeRunning");
			if(sm & S_UPDATE){
				if(state() == Data::ExeRunning){
					Mutex::Locker	lock(*d.mtx);
					if(d.st == Data::Stopping){
						state(Data::ExeStopping);
						return OK;
					}
				}
			}
			switch(doRun(_evs, _rtout)){
				case BAD:{
					Mutex::Locker	lock(*d.mtx);
					state(Data::ExeStopping);
					d.st = Data::Stopping;
					return OK;
				}
				case OK:
					return OK;
				case NOK:
					return NOK;
			}
		case Data::ExeStopping:
			idbgx(Dbg::fdt, "ExeStopping");
			switch(doStop(_evs, _rtout)){
				case BAD:
				case OK:{
					Mutex::Locker	lock(*d.mtx);
					if(d.objcnt > d.expcnt){
						state(Data::ExeStoppingWait);
					}else{
						d.st = Data::Stopped;
						state(Data::ExeStopped);
						d.cnd.broadcast();
					}
					//d.st = Data::Stopped;
					return OK;
				}
				case NOK:
					return NOK;
			}
		case Data::ExeStoppingWait:
			idbgx(Dbg::fdt, "ExeStoppingWait");
			if(sm & S_UPDATE){
				Mutex::Locker	lock(*d.mtx);
				if(d.st == Data::StoppingDone){
					d.st = Data::Stopped;
					state(Data::ExeStopped);
					d.cnd.broadcast();
				}else{
					cassert(d.st == Data::Stopping);
				}
			}
			return NOK;
		case Data::ExeStopped:
			idbgx(Dbg::fdt, "ExeStopped");
			if(sm & S_UPDATE){
				Mutex::Locker	lock(*d.mtx);
				if(d.st == Data::Starting){
					state(Data::ExeStarting);
					return OK;
				}
			}
			return NOK;
		case Data::ExeDying:
			idbgx(Dbg::fdt, "ExeDying");
			switch(doStop(_evs, _rtout)){
				case BAD:
				case OK:{
					Mutex::Locker	lock(*d.mtx);
					if(d.objcnt > d.expcnt){
						state(Data::ExeDyingWait);
						d.st = Data::Stopping;
					}else{
						d.st = Data::Stopped;
						state(Data::ExeDie);
						d.cnd.broadcast();
					}
					return OK;
				}
				case NOK: return NOK;
			}
		case Data::ExeDyingWait:
			idbgx(Dbg::fdt, "ExeDyingWait");
			if(sm & S_UPDATE){
				Mutex::Locker	lock(*d.mtx);
				if(d.st == Data::StoppingDone){
					d.st = Data::Stopped;
					state(Data::ExeDie);
					d.cnd.broadcast();
					return OK;
				}else{
					cassert(d.st == Data::Stopping);
				}
			}
			return NOK;
		case Data::ExeDie:
			idbgx(Dbg::fdt, "ExeDie");
			m().eraseObject(*this);
			return BAD;
			
	}
	return NOK;
}
//---------------------------------------------------------
/*virtual*/ int Service::doStart(ulong _evs, TimeSpec &_rtout){
	return OK;
}
//---------------------------------------------------------
/*virtual*/ int Service::doRun(ulong _evs, TimeSpec &_rtout){
	return NOK;
}
//---------------------------------------------------------
/*virtual*/ int Service::doStop(ulong _evs, TimeSpec &_rtout){
	signal(S_RAISE | S_KILL);
	return OK;
}
//---------------------------------------------------------
Mutex& Service::serviceMutex()const{
	return *d.mtx;
}
//---------------------------------------------------------
void Service::insertObject(Object &_ro, const ObjectUidT &_ruid){
	//by default do nothing
	vdbgx(Dbg::fdt, "insert object "<<Manager::the().computeServiceId(_ruid.first)<<' '<<Manager::the().computeIndex(_ruid.first)<<' '<<_ruid.second);
}
//---------------------------------------------------------
void Service::eraseObject(const Object &_ro){
	//by default do nothing
	ObjectUidT objuid(_ro.uid());
	vdbgx(Dbg::fdt, "erase object "<<Manager::the().computeServiceId(objuid.first)<<' '<<Manager::the().computeIndex(objuid.first)<<' '<<objuid.second);
}
//---------------------------------------------------------
void Service::expectedCount(const IndexT &_rcnt){
	d.expcnt = _rcnt;
}
//---------------------------------------------------------
const IndexT& Service::expectedCount()const{
	return d.expcnt;
}
//---------------------------------------------------------
/*virtual*/ void Service::dynamicExecute(DynamicPointer<> &_dp){
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

ObjectUidT Service::doInsertObject(Object &_ro, uint32 _tid, const IndexT &_ridx){
	uint32 u;
	cassert(!serviceMutex().tryLock());
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
				_ro.init(&rmut);
				u = rop.second;
			}
		}else{
			//worst case scenario
			const IndexT	sz(d.objvec.size());
			
			d.mtxstore.safeAt(sz);
			
			d.mtxstore.visit(sz, visit_lock);//lock all mutexes
			
			d.objvec.push_back(Data::ObjectPairT(&_ro, 0));
			
			_ro.id(this->index(), sz);
			_ro.init(&d.mtxstore.at(sz));
			
			u = 0;
			//reserve some positions
			uint			cnt(255);
			const IndexT	initialsize(d.objvec.size());
			while(cnt--){
				d.mtxstore.safeAt(d.objvec.size());
				d.idxque.push(initialsize + 254 - cnt);
				d.objvec.push_back(Data::ObjectPairT(static_cast<fdt::Object*>(NULL), 0));
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
					_ro.init(&rmut);
					u = rop.second;
				}else{
					THROW_EXCEPTION_EX("Multiple inserts on the same index ",_ridx);
					return invalid_uid();
				}
			}
			++d.objcnt;
			d.popIndex(_ridx);
			return ObjectUidT(_ro.id(), u);
		}else{
			//worst case scenario
			const IndexT	initialsize(d.objvec.size());
			d.objvec.resize(fast_smart_resize(d.objvec, 8/*256*/));
			const IndexT	sz(d.objvec.size());
			const IndexT	diffsz(sz - initialsize - 1);
			
			d.mtxstore.safeAt(sz);
			
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
			_ro.id(this->index(), _ridx);
			_ro.init(&d.mtxstore.at(sz));
			
			d.mtxstore.visit(sz, visit_unlock);//unlock all mutexes
			++d.objcnt;
			return ObjectUidT(_ro.id(), u);
		}
	}
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
	long	oc(d.objcnt);
	ulong	i(0);
	long	mi(-1);
	bool	signaled(false);
	Manager	&rm(m());
	
	idbgx(Dbg::fdt, "signalling "<<oc<<" objects");
	
	cassert((d.objvec.size() % 4) == 0);
	
	for(Data::ObjectVectorT::iterator it(d.objvec.begin()); oc > 0 && it != d.objvec.end(); it += 4, i += 4){
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
		if((it + 1)->first){
			if(d.mtxstore.isRangeBegin(i + 1)){
				if(mi >= 0)	d.mtxstore[mi].unlock();
				++mi;
				d.mtxstore[mi].lock();
			}
			if((it + 1)->first->signal(_sm)){
				rm.raiseObject(*(it + 1)->first);
			}
			signaled = true;
			--oc;
		}
		if((it + 2)->first){
			if(d.mtxstore.isRangeBegin(i + 2)){
				if(mi >= 0)	d.mtxstore[mi].unlock();
				++mi;
				d.mtxstore[mi].lock();
			}
			if((it + 2)->first->signal(_sm)){
				rm.raiseObject(*(it + 2)->first);
			}
			signaled = true;
			--oc;
		}
		if((it + 3)->first){
			if(d.mtxstore.isRangeBegin(i + 3)){
				if(mi >= 0)	d.mtxstore[mi].unlock();
				++mi;
				d.mtxstore[mi].lock();
			}
			if((it + 3)->first->signal(_sm)){
				rm.raiseObject(*(it + 3)->first);
			}
			signaled = true;
			--oc;
		}
	}
	if(mi >= 0){
		d.mtxstore[mi].unlock();
	}
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
	if(mi >= 0){
		d.mtxstore[mi].unlock();
	}
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
			const uint32	tid(pobj->dynamicTypeId());
			if(tid < rvts.cbkvec.size() && rvts.cbkvec[tid] != NULL){
				(*rvts.cbkvec[tid])(pobj, _rv);
			}else{
				Service::visit_cbk<Object, Visitor>(pobj, _rv);
			}
			
			signaled = true;
			--oc;
		}
	}
	if(mi >= 0){
		d.mtxstore[mi].unlock();
	}
	return signaled;
}
//---------------------------------------------------------
bool Service::doVisit(Visitor &_rv, uint _visidx, const ObjectUidT &_ruid){
	const IndexT	oidx(Manager::the().computeIndex(_ruid.first));
	
	if(oidx >= d.objvec.size()){
		return false;
	}
	
	Mutex::Locker	lock(d.mtxstore.at(oidx));
	
	if(_ruid.second != d.objvec[oidx].second){
		return false;
	}
	
	Object			*pobj(d.objvec[oidx].first);
	
	if(!pobj) return false;
	
	VisitorTypeStub	&rvts(vistpvec[_visidx]);
	const uint32	tid(pobj->dynamicTypeId());
	if(tid < rvts.cbkvec.size() && rvts.cbkvec[tid] != NULL){
		(*rvts.cbkvec[tid])(pobj, _rv);
	}else{
		Service::visit_cbk<Object, Visitor>(pobj, _rv);
	}
	return true;
}
//---------------------------------------------------------
/*virtual*/ void Service::init(Mutex *_pmtx){
	if(!d.mtx){
		d.mtx = _pmtx;
	}
}
//---------------------------------------------------------
}//namespace fundation


