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
#include <cstring>

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

struct BufferNode{
	BufferNode *pnext;
};

struct MutualStub{
	typedef std::pair<BufferNode*, uint>	BufferPairT;
	typedef std::vector<BufferPairT>		BufferNodeVectorT;
	Mutex				mtx;
	BufferNodeVectorT	nodevec;
	
	~MutualStub();
	
	void* popBuffer(const uint32 _sz, uint32 &_rcp);
	void pushBuffer(void *_pb, const uint32 _cp);
	
	
	static int sizeToIndex(const uint _sz);
	static uint capacityToIndex(const uint _cp);
	static uint indexToCapacity(const uint _idx);
	
};


MutualStub::~MutualStub(){
	for(BufferNodeVectorT::iterator it(nodevec.begin()); it != nodevec.end(); ++it){
		if(it->first){
			BufferNode	*pbn(it->first);
			BufferNode	*pnbn;
			uint 		cnt(0);
			do{
				pnbn = pbn->pnext;
				delete []reinterpret_cast<char*>(pbn);
				++cnt;
				pbn = pnbn;
			}while(pbn);
			if(cnt != it->second){
				THROW_EXCEPTION_EX("Memory leak ", (uint)(it - nodevec.begin()));
			}
		}else if(it->second){
			THROW_EXCEPTION_EX("Memory leak ", (uint)(it - nodevec.begin()));
		}
	}
}

/*static*/ int MutualStub::sizeToIndex(const uint _sz){
	if(_sz <= sizeof(BufferNode)){
		return 0;
	}else if(_sz <= (2 * sizeof(BufferNode))){
		return 1;
	}else if(_sz <= (4 * sizeof(BufferNode))){
		return 2;
	}else if(_sz <= (8 * sizeof(BufferNode))){
		return 3;
	}else if(_sz <= (16 * sizeof(BufferNode))){
		return 4;
	}else if(_sz <= (32 * sizeof(BufferNode))){
		return 5;
	}else if(_sz <= (64 * sizeof(BufferNode))){
		return 6;
	}else if(_sz <= (128 * sizeof(BufferNode))){
		return 7;
	}else{
		return -1;
	}
		
}
/*static*/ uint MutualStub::capacityToIndex(const uint _cp){
	switch(_cp){
		case sizeof(BufferNode):		return 0;
		case 2 * sizeof(BufferNode):	return 1;
		case 4 * sizeof(BufferNode):	return 2;
		case 8 * sizeof(BufferNode):	return 3;
		case 16 * sizeof(BufferNode):	return 4;
		case 32 * sizeof(BufferNode):	return 5;
		case 64 * sizeof(BufferNode):	return 6;
		case 128 * sizeof(BufferNode):	return 7;
		default: return -1;
	}
}
/*static*/ uint MutualStub::indexToCapacity(const uint _idx){
	static const uint cps[] = {
		sizeof(BufferNode),
		2 * sizeof(BufferNode),
		4 * sizeof(BufferNode),
		8 * sizeof(BufferNode),
		16 * sizeof(BufferNode),
		32 * sizeof(BufferNode),
		64 * sizeof(BufferNode),
		128 * sizeof(BufferNode)
	};
	return cps[_idx];
}

void* MutualStub::popBuffer(const uint32 _sz, uint32 &_rcp){
	int idx = sizeToIndex(_sz);
	if(idx < 0){
		_rcp = ((_sz >> 7) + 1) << 7;//((_sz / 128) + 1) * 128;
		char *pbuf = new char[_rcp];
		return pbuf;
	}
	_rcp = indexToCapacity(idx);
	if(idx >= nodevec.size()){
		nodevec.resize(idx + 1);
	}
	BufferNode	*pbn(nodevec[idx].first);
	if(pbn){
		BufferNode	*pnbn(pbn->pnext);
		nodevec[idx].first = pnbn;
		return pbn;
	}else{
		char *pbuf = new char[_rcp];
		++nodevec[idx].second;
		return pbuf;
	}
}
void MutualStub::pushBuffer(void *_pb, const uint32 _cp){
	int idx = capacityToIndex(_cp);
	if(idx < 0){
		delete []reinterpret_cast<char*>(_pb);
	}else{
		cassert(idx < nodevec.size());
		BufferNode *pbn(reinterpret_cast<BufferNode*>(_pb));
		pbn->pnext = nodevec[idx].first;
		nodevec[idx].first = pbn;
	}
}

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
	struct ObjectStub{
		ObjectStub(
			Object *_pobj = NULL,
			const uint32 _uid = 0
		):pobj(_pobj), uid(_uid){
			v[1] = v[0] = NULL;
			vsz[1] = vsz[0] = 0;
			vcp[1] = vcp[0] = 0;
		}
		Object				*pobj;
		uint32				uid;
		DynamicPointer<>	*v[2];
		uint32				vsz[2];
		uint32				vcp[2];
	};
	//typedef std::pair<Object*, uint32>		ObjectPairT;
	typedef std::deque<ObjectStub>			ObjectVectorT;
	typedef Queue<uint32>					Uint32QueueT;
	
	typedef MutualStore<MutualStub>			MutualStoreT;
	
	Data(
		bool _started,
		int _objpermutbts,
		int _mutrowsbts,
		int _mutcolsbts
	):	objcnt(0), expcnt(0), st(_started ? Starting : Stopped),
		mutualstore(_objpermutbts, _mutrowsbts, _mutcolsbts), mtx(NULL){
		mutualstore.safeAt(0);
	}
	void popIndex(const IndexT &_idx);
	
	IndexT				objcnt;//object count
	IndexT				expcnt;
	int					st;
	MutualStoreT		mutualstore;
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
		Locker<Mutex>		lock1(*d.mtx);
		Locker<Mutex>		lock2(d.mutualstore.at(oidx).mtx);
		
		ObjectTypeStub		&rots(objectTypeStub(_robj.dynamicTypeId()));
		
		(*rots.erase_callback)(_robj, this);
		
		Data::ObjectStub	&rop(d.objvec[oidx]);
		
		rop.pobj = NULL;
		++rop.uid;
		
		pointerStoreClear(oidx, 0);
		pointerStoreClear(oidx, 1);
		
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
	de.push(this, DynamicPointer<>(_rsig));
	return Object::signal(fdt::S_SIG | fdt::S_RAISE);
}
//---------------------------------------------------------
bool Service::signal(ulong _sm){
	Locker<Mutex> lock(*d.mtx);
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
	
	Locker<Mutex>	lock(d.mutualstore.at(oidx).mtx);
	
	if(_uid != d.objvec[oidx].uid){
		return false;
	}
	
	Object			*pobj(d.objvec[oidx].pobj);
	
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
	
	Locker<Mutex>	lock(d.mutualstore.at(oidx).mtx);
	
	//if(_uid != d.objvec[oidx].second) return false;
	
	Object			*pobj(d.objvec[oidx].pobj);
	
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
	Locker<Mutex> lock(*d.mtx);
	return doSignalAll(_rsig);
}
//---------------------------------------------------------
bool Service::signal(DynamicPointer<Signal> &_rsig, const Object &_robj){
	const IndexT	oidx(_robj.index());
	
	cassert(oidx < d.objvec.size());
	
	Locker<Mutex>	lock(d.mutualstore.at(oidx).mtx);
	
	//if(_uid != d.objvec[oidx].second) return false;
	
	Object			*pobj(d.objvec[oidx].pobj);
	
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
	
	Locker<Mutex>	lock(d.mutualstore.at(oidx).mtx);
	
	if(_uid != d.objvec[oidx].uid){
		return false;
	}
	
	Object			*pobj(d.objvec[oidx].pobj);
	
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
	return d.mutualstore.at(_robj.index()).mtx;
}
//---------------------------------------------------------
Mutex& Service::mutex(const IndexT &_ridx){
	return d.mutualstore.at(_ridx).mtx;
}
//---------------------------------------------------------
uint32 Service::uid(const Object &_robj)const{
	return d.objvec[_robj.index()].uid;
}
//---------------------------------------------------------
uint32 Service::uid(const IndexT &_idx)const{
	return d.objvec[_idx].uid;
}
//---------------------------------------------------------
/*virtual*/ int Service::start(bool _wait){
	{
		Locker<Mutex>	lock(*d.mtx);
		bool			reenter;
		do{
			reenter = false;
			if(d.st == Data::Running){
				return OK;
			}else if(d.st < Data::Running){
				if(!_wait) return NOK;
				do{
					d.cnd.wait(lock);
				}while(d.st != Data::Running && d.st != Data::Stopped);
				if(d.st == Data::Running){
					return OK;
				}
				return BAD;
			}else if(d.st < Data::Stopped){
				//if(!_wait) return false;
				do{
					d.cnd.wait(lock);
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
		Locker<Mutex>	lock(*d.mtx);
		do{
			d.cnd.wait(lock);
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
		Locker<Mutex>	lock(*d.mtx);
		
		if(d.st == Data::Stopped){
			return OK;
		}else if(d.st < Data::Running){
			do{
				d.cnd.wait(lock);
			}while(d.st != Data::Running && d.st != Data::Stopped);
			if(d.st == Data::Stopped){
				return OK;
			}
		}else if(d.st == Data::Running){
			
		}else if(d.st < Data::Stopped){
			do{
				d.cnd.wait(lock);
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
		Locker<Mutex>	lock(*d.mtx);
		do{
			d.cnd.wait(lock);
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
			Locker<Mutex>	lock(Object::mutex());
			sm = grabSignalMask();
			if(sm & fdt::S_SIG){//we have signals
				de.prepareExecute(this);
			}
		}
		if(sm & fdt::S_SIG){//we've grabed signals, execute them
			de.executeAll(this);
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
					Locker<Mutex>	lock(*d.mtx);
					state(Data::ExeStopping);
					d.st = Data::Stopping;
					return OK;
				}	
				case OK:{
					Locker<Mutex>	lock(*d.mtx);
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
					Locker<Mutex>	lock(*d.mtx);
					if(d.st == Data::Stopping){
						state(Data::ExeStopping);
						return OK;
					}
				}
			}
			switch(doRun(_evs, _rtout)){
				case BAD:{
					Locker<Mutex>	lock(*d.mtx);
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
					Locker<Mutex>	lock(*d.mtx);
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
				Locker<Mutex>	lock(*d.mtx);
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
				Locker<Mutex>	lock(*d.mtx);
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
					Locker<Mutex>	lock(*d.mtx);
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
				Locker<Mutex>	lock(*d.mtx);
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

	void visit_lock(MutualStub &_rm){
		_rm.mtx.lock();
	}

	void visit_unlock(MutualStub &_rm){
		_rm.mtx.unlock();
	}

}//namespace

ObjectUidT Service::doInsertObject(Object &_ro, uint32 _tid, const IndexT &_ridx){
	uint32 u;
	cassert(!serviceMutex().tryLock());
	if(is_invalid_index(_ridx)){
		if(d.idxque.size()){
			{
				const IndexT		idx(d.idxque.front());
				Mutex 				&rmut(d.mutualstore.at(idx).mtx);
				Locker<Mutex>		lock(rmut);
				Data::ObjectStub	&rop(d.objvec[idx]);
				
				d.idxque.pop();
				rop.pobj = &_ro;
				_ro.id(this->index(), idx);
				_ro.init(&rmut);
				u = rop.uid;
			}
		}else{
			//worst case scenario
			const IndexT	sz(d.objvec.size());
			
			d.mutualstore.safeAt(sz);
			
			d.mutualstore.visit(sz, visit_lock);//lock all mutexes
			
			d.objvec.push_back(Data::ObjectStub(&_ro, 0));
			
			_ro.id(this->index(), sz);
			_ro.init(&d.mutualstore.at(sz).mtx);
			
			u = 0;
			//reserve some positions
			uint			cnt(255);
			const IndexT	initialsize(d.objvec.size());
			while(cnt--){
				d.mutualstore.safeAt(d.objvec.size());
				d.idxque.push(initialsize + 254 - cnt);
				d.objvec.push_back(Data::ObjectStub(static_cast<fdt::Object*>(NULL), 0));
			}
			
			d.mutualstore.visit(sz, visit_unlock);//unlock all mutexes
		}
		++d.objcnt;
		return ObjectUidT(_ro.id(), u);
	}else{
		if(_ridx < d.objvec.size()){
			{
				Mutex 				&rmut(d.mutualstore.at(_ridx).mtx);
				Locker<Mutex>		lock(rmut);
				Data::ObjectStub	&rop(d.objvec[_ridx]);
				if(!rop.pobj){
					rop.pobj = &_ro;
					_ro.id(this->index(), _ridx);
					_ro.init(&rmut);
					u = rop.uid;
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
			
			d.mutualstore.safeAt(sz);
			
			d.mutualstore.visit(sz, visit_lock);//lock all mutexes
			
			//reserve some positions
			IndexT		cnt(diffsz + 1);
			while(cnt--){
				d.mutualstore.safeAt(d.objvec.size());
				IndexT crtidx(initialsize + diffsz - cnt);
				if(crtidx != _ridx){
					d.idxque.push(crtidx);
				}
			}
			
			d.objvec[_ridx].pobj = &_ro;
			u = 0;
			_ro.id(this->index(), _ridx);
			_ro.init(&d.mutualstore.at(sz).mtx);
			
			d.mutualstore.visit(sz, visit_unlock);//unlock all mutexes
			++d.objcnt;
			return ObjectUidT(_ro.id(), u);
		}
	}
}
//---------------------------------------------------------
Object* Service::objectAt(const IndexT &_ridx, uint32 _uid){
	if(_ridx < d.objvec.size() && d.objvec[_ridx].uid == _uid){
		return d.objvec[_ridx].pobj;
	}
	return NULL;
}
//---------------------------------------------------------
Object* Service::objectAt(const IndexT &_ridx){
	return d.objvec[_ridx].pobj;
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
		if(it->pobj){
			if(d.mutualstore.isRangeBegin(i)){
				if(mi >= 0)	d.mutualstore[mi].mtx.unlock();
				++mi;
				d.mutualstore[mi].mtx.lock();
			}
			if(it->pobj->signal(_sm)){
				rm.raiseObject(*it->pobj);
			}
			signaled = true;
			--oc;
		}
		if((it + 1)->pobj){
			if(d.mutualstore.isRangeBegin(i + 1)){
				if(mi >= 0)	d.mutualstore[mi].mtx.unlock();
				++mi;
				d.mutualstore[mi].mtx.lock();
			}
			if((it + 1)->pobj->signal(_sm)){
				rm.raiseObject(*(it + 1)->pobj);
			}
			signaled = true;
			--oc;
		}
		if((it + 2)->pobj){
			if(d.mutualstore.isRangeBegin(i + 2)){
				if(mi >= 0)	d.mutualstore[mi].mtx.unlock();
				++mi;
				d.mutualstore[mi].mtx.lock();
			}
			if((it + 2)->pobj->signal(_sm)){
				rm.raiseObject(*(it + 2)->pobj);
			}
			signaled = true;
			--oc;
		}
		if((it + 3)->pobj){
			if(d.mutualstore.isRangeBegin(i + 3)){
				if(mi >= 0)	d.mutualstore[mi].mtx.unlock();
				++mi;
				d.mutualstore[mi].mtx.lock();
			}
			if((it + 3)->pobj->signal(_sm)){
				rm.raiseObject(*(it + 3)->pobj);
			}
			signaled = true;
			--oc;
		}
	}
	if(mi >= 0){
		d.mutualstore[mi].mtx.unlock();
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
		if(it->pobj){
			if(d.mutualstore.isRangeBegin(i)){
				if(mi >= 0)	d.mutualstore[mi].mtx.unlock();
				++mi;
				d.mutualstore[mi].mtx.lock();
			}
			DynamicPointer<Signal> sig(_rsig);
			if(it->pobj->signal(sig)){
				rm.raiseObject(*it->pobj);
			}
			signaled = true;
			--oc;
		}
	}
	if(mi >= 0){
		d.mutualstore[mi].mtx.unlock();
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
		if(it->pobj){
			if(d.mutualstore.isRangeBegin(i)){
				if(mi >= 0)	d.mutualstore[mi].mtx.unlock();
				++mi;
				d.mutualstore[mi].mtx.lock();
			}
			Object			*pobj(it->pobj);
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
		d.mutualstore[mi].mtx.unlock();
	}
	return signaled;
}
//---------------------------------------------------------
bool Service::doVisit(Visitor &_rv, uint _visidx, const ObjectUidT &_ruid){
	const IndexT	oidx(Manager::the().computeIndex(_ruid.first));
	
	if(oidx >= d.objvec.size()){
		return false;
	}
	
	Locker<Mutex>	lock(d.mutualstore.at(oidx).mtx);
	
	if(_ruid.second != d.objvec[oidx].uid){
		return false;
	}
	
	Object			*pobj(d.objvec[oidx].pobj);
	
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
//---------------------------------------------------------
void Service::pointerStorePushBack(
	const IndexT &_ridx,
	const uint _idx,
	const DynamicPointer<DynamicBase> &_dp
){
	Data::ObjectStub	&ros(d.objvec[_ridx]);
	MutualStub			&rms(d.mutualstore.at(_ridx));
	if(ros.vsz[_idx] == (ros.vcp[_idx] / sizeof(DynamicPointer<>))){
		uint32	newcp(0);
		uint32	newsz(ros.vsz[_idx] + 1);
		void	*pnewbuf = rms.popBuffer(newsz * sizeof(DynamicPointer<>), newcp);
		memcpy(pnewbuf, ros.v[_idx], ros.vsz[_idx] * sizeof(DynamicPointer<>));
		rms.pushBuffer(ros.v[_idx], ros.vcp[_idx]);
		ros.v[_idx] = reinterpret_cast<DynamicPointer<>*>(pnewbuf);
		ros.vcp[_idx] = newcp;
		new(reinterpret_cast<void*>(&ros.v[_idx][ros.vsz[_idx]])) DynamicPointer<>;
	}
	ros.v[_idx][ros.vsz[_idx]] = _dp;
	++ros.vsz[_idx];
}
//---------------------------------------------------------
size_t Service::pointerStoreSize(
	const IndexT &_ridx,
	const uint _idx
)const{
	const Data::ObjectStub	&ros(d.objvec[_ridx]);
	return ros.vsz[_idx];
}
//---------------------------------------------------------
bool Service::pointerStoreIsNotLast(
	const IndexT &_ridx,
	const uint _idx,
	const uint _pos
)const{
	const Data::ObjectStub	&ros(d.objvec[_ridx]);
	return  _pos < ros.vsz[_idx];
}
//---------------------------------------------------------
const DynamicPointer<DynamicBase> &Service::pointerStorePointer(
	const IndexT &_ridx,
	const uint _idx,
	const uint _pos
)const{
	const Data::ObjectStub	&ros(d.objvec[_ridx]);
	return ros.v[_idx][_pos];
}
//---------------------------------------------------------
DynamicPointer<DynamicBase> &Service::pointerStorePointer(
	const IndexT &_ridx,
	const uint _idx,
	const uint _pos
){
	Data::ObjectStub	&ros(d.objvec[_ridx]);
	return ros.v[_idx][_pos];
}
//---------------------------------------------------------
void Service::pointerStoreClear(
	const IndexT &_ridx,
	const uint _idx
){
	Data::ObjectStub	&ros(d.objvec[_ridx]);
	MutualStub			&rms(d.mutualstore.at(_ridx));
	for(uint32 i(0); i < ros.vsz[_idx]; ++i){
		ros.v[_idx][i].clear();
	}
	rms.pushBuffer(ros.v[_idx], ros.vcp[_idx]);
	ros.vcp[_idx] = ros.vsz[_idx] = 0;
	ros.v[_idx] = NULL;
}
//---------------------------------------------------------
}//namespace fundation


