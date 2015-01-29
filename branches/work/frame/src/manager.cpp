// frame/src/manager.cpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <vector>
#include <deque>

#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/memory.hpp"
#include "system/condition.hpp"
#include "system/specific.hpp"
#include "system/exception.hpp"
#include "system/mutualstore.hpp"
#include "system/atomic.hpp"

#include "frame/manager.hpp"
#include "frame/message.hpp"
#include "frame/reactorbase.hpp"
#include "frame/service.hpp"
#include "frame/event.hpp"

#include "utility/stack.hpp"
#include "utility/queue.hpp"

namespace solid{
namespace frame{

typedef ATOMIC_NS::atomic<size_t>			AtomicSizeT;
typedef ATOMIC_NS::atomic<uint>				AtomicUintT;
typedef ATOMIC_NS::atomic<ReactorBase*>		AtomicReactorBaseT;
typedef ATOMIC_NS::atomic<IndexT>			AtomicIndexT;

//---------------------------------------------------------
//POD structure
struct ObjectStub{
	ObjectBase *pobj;
	
};

struct ObjectChunk{
	ObjectChunk(Mutex &_rmtx):rmtx(_rmtx), psvc(NULL){}
	Mutex		&rmtx;
	Service		*psvc;
	
	char *data(){
		return reinterpret_cast<char*>(this);
	}
	ObjectStub* objects(){
		return reinterpret_cast<ObjectStub*>(reinterpret_cast<char*>(this) + sizeof(*this));
	}
};


typedef std::deque<ObjectChunk>				ChunkDequeT;
typedef Queue<size_t>						SizeQueueT;
typedef Stack<size_t>						SizeStackT;
typedef MutualStore<Mutex>					MutexMutualStoreT;

//---------------------------------------------------------
typedef std::vector<Service*>				ServiceVectorT;

//---------------------------------------------------------
struct Manager::Data{
	enum States{
		StateStarting = 1,
		StateRunning,
		StateStopping,
		StateStopped
	};
	Data(
		Manager &_rm
	);
	
	
	ObjectChunk* allocateChunk(Mutex &_rmtx)const{
		char *p = new char[sizeof(ObjectChunk) + chkobjcnt * sizeof(ObjectStub)];
		ObjectChunk *poc = new(p) ObjectChunk(_rmtx);
		return poc;
	}
	
	AtomicSizeT				crtsvcvecidx;
	ServiceVectorT			svcvec[2];
	AtomicSizeT				svcuse[2];
	AtomicSizeT				svccnt;
	Mutex					mtx;
	Condition				cnd;
	SizeStackT				svcfreeidxstk;
	Mutex					*pmtxarr;
	size_t					mtxcnt;
	size_t					chkobjcnt;
	ChunkDequeT				chkdq[2];
	AtomicSizeT				chkuse[2];
	AtomicSizeT				objcnt;
	AtomicSizeT				maxobjcnt;
};


void EventNotifierF::operator()(ObjectBase &_robj){
	Event tmpevt(evt);
	
	if(!sigmsk || _robj.notify(sigmsk)){
		rm.raise(_robj, tmpevt);
	}
}



Manager::Data::Data(
	Manager &_rm
):crtsvcvecidx(0), svccnt(0)
{
	svcuse[0].store(0);
	svcuse[1].store(0);
}


Manager::Manager(
	const size_t _mtxcnt/* = 0*/,
	const size_t _objbucketsize/* = 0*/
):d(*(new Data(*this))){
	if(_mtxcnt == 0){
		d.mtxcnt = memory_page_size() / sizeof(Mutex);
	}else{
		d.mtxcnt =  _mtxcnt;
	}
	
	d.pmtxarr = new Mutex[d.mtxcnt];
	
	if(_objbucketsize == 0){
		d.chkobjcnt = (memory_page_size() - sizeof(ObjectChunk)) / sizeof(ObjectStub);
	}else{
		d.chkobjcnt = _objbucketsize;
	}
	
	
}

/*virtual*/ Manager::~Manager(){
	
	stop(true);
	delete &d;
	delete []d.pmtxarr;
}

void Manager::stop(const bool _wait){
}

bool Manager::registerService(
	Service &_rs
){
	if(_rs.isRegistered()){
		return false;
	}
	Locker<Mutex>	lock(d.mtx);
	
	size_t	crtsvcvecidx = d.crtsvcvecidx;
	size_t	svcidx = -1;
	
	if(d.svcfreeidxstk.size()){
		svcidx = d.svcfreeidxstk.top();
		d.svcfreeidxstk.top();
		
	}else if(d.svccnt < d.svcvec[crtsvcvecidx].size()){
		++d.svccnt;
		svcidx = d.svccnt;
	}else{
		const size_t newsvcvecidx = (crtsvcvecidx + 1) & 1;
		
		
		while(d.svcuse[newsvcvecidx] != 0){
			Thread::yield();
		}
		
		d.svcvec[newsvcvecidx] = d.svcvec[crtsvcvecidx];//update the new vector
		
		
		d.svcvec[newsvcvecidx].push_back(nullptr);
		d.svcvec[newsvcvecidx].resize(d.svcvec[newsvcvecidx].capacity());
		
		d.crtsvcvecidx = newsvcvecidx;
		crtsvcvecidx = newsvcvecidx;
		++d.svccnt;
		svcidx = d.svccnt;
	}
	_rs.idx = svcidx;
	
	d.svcvec[crtsvcvecidx][svcidx] = &_rs;
	return true;
}


void Manager::unregisterService(Service &_rs){
	if(!_rs.isRegistered()){
		return;
	}
	Locker<Mutex>	lock(d.mtx);
	d.svcfreeidxstk.push(_rs.idx);
	d.svcvec[d.crtsvcvecidx][_rs.idx] = nullptr;
}

void Manager::unregisterObject(ObjectBase &_robj){
// 	IndexT		svcidx;
// 	IndexT		objidx;
// 	vdbgx(Debug::frame, (void*)&_robj);
// 	
// 	split_index(svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _robj.fullid);
// 	
// 	if(svcidx < d.svcprovisioncp){
// 		ServiceStub		&rss = d.psvcarr[svcidx];
// 		const uint 		objpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);//set it with release
// 		const size_t	objcnt = rss.objvecsz.load(ATOMIC_NS::memory_order_acquire);
// 		
// 		if(objpermutbts && objidx < objcnt){
// 			Locker<Mutex>	lock(rss.mtxstore.at(objidx, objpermutbts));
// 			
// 			if(rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/) == objpermutbts){
// 				cassert(rss.objvec[objidx].pobj == &_robj);
// 				rss.objvec[objidx].clear();
// 				rss.objfreestk.push(objidx);
// 				if(rss.state == ServiceStub::StateStopping && rss.objfreestk.size() == rss.objvec.size()){
// 					rss.state = ServiceStub::StateStopped;
// 					rss.cnd.broadcast();
// 				}
// 				return;
// 			}
// 		}
// 	}
// 	cassert(false);
}
/*
 * NOTE:
 * 1) rss.objpermuts & rss.objvecsz:
 * 	[0,0] -> [X,0] -> [X,1] .. [X,a]  -> [0,0] -> [Y,0] -> [Y,1] .. [Y,b]
 * Situations:
 * Read:
 * [0,0] OK
 * [0,a] OK
 * [X,0] OK
 * [Y,a]\
 *		prevented with memory_order_acquire
 * [x,b]/ 
*/
bool Manager::notify(ObjectUidT const &_ruid, Event const &_re, const size_t _sigmsk/* = 0*/){
// 	IndexT		svcidx;
// 	IndexT		objidx;
// 	
// 	split_index(svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _ruid.index);
// 	
// 	if(svcidx < d.svcprovisioncp){
// 		ServiceStub		&rss = d.psvcarr[svcidx];
// 		const uint 		objpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);//set it with release
// 		const size_t	objcnt = rss.objvecsz.load(ATOMIC_NS::memory_order_acquire);
// 		
// 		if(objpermutbts && objidx < objcnt){
// 			Locker<Mutex>	lock(rss.mtxstore.at(objidx, objpermutbts));//we don't need safeAt because "objidx < objcnt"
// 			
// 			if(rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/) == objpermutbts && rss.objvec[objidx].unique == _ruid.unique){
// 				ObjectBase &robj = *rss.objvec[objidx].pobj;
// 				if(!_sigmsk || robj.notify(_sigmsk)){
// 					return this->raise(robj, _re);
// 				}
// 			}
// 		}
// 	}
	return false;
}

bool Manager::raise(const ObjectBase &_robj, Event const &_re){
// 	IndexT	selidx;
// 	UidT	uid = _robj.runId();
// 	split_index(selidx, uid.index, d.reactorbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), uid.index);
// 	return d.preactorarr[selidx].load(/*ATOMIC_NS::memory_order_seq_cst*/)->raise(uid, _re);
	return false;
}


Mutex& Manager::mutex(const ObjectBase &_robj)const{
// 	IndexT			svcidx;
// 	IndexT			objidx;
// 	
// 	split_index(svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _robj.fullid);
// 	cassert(svcidx < d.svcprovisioncp);
// 	
// 	ServiceStub		&rss = d.psvcarr[svcidx];
// 	const uint 		objpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 	cassert(objidx < rss.objvecsz.load(ATOMIC_NS::memory_order_acquire));
// 	return rss.mtxstore.at(objidx, objpermutbts);
	static Mutex m;
	return m;
}

ObjectUidT  Manager::id(const ObjectBase &_robj)const{
// 	IndexT			svcidx;
// 	IndexT			objidx;
// 	
// 	split_index(svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _robj.fullid);
// 	cassert(svcidx < d.svcprovisioncp);
// 	
// 	ServiceStub		&rss = d.psvcarr[svcidx];
// 	cassert(objidx < rss.objvecsz.load(/*ATOMIC_NS::memory_order_seq_cst*/));
// 	const uint 		objpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 	Locker<Mutex>	lock(rss.mtxstore.at(objidx, objpermutbts));
// 	return ObjectUidT(_robj.fullid, rss.objvec[objidx].unique);
	return ObjectUidT();
}

ObjectUidT  Manager::unsafeId(const ObjectBase &_robj)const{
// 	IndexT			svcidx;
// 	IndexT			objidx;
// 	
// 	split_index(svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _robj.fullid);
// 	cassert(svcidx < d.svcprovisioncp);
// 	
// 	ServiceStub		&rss = d.psvcarr[svcidx];
// 	cassert(objidx < rss.objvecsz.load(/*ATOMIC_NS::memory_order_seq_cst*/));
// 	cassert(!rss.mtxstore.at(objidx, rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/)).tryLock());
// 	return ObjectUidT(_robj.fullid, rss.objvec[objidx].unique);
	return ObjectUidT();
}

Mutex& Manager::mutex(const Service &_rsvc)const{
// 	cassert(_rsvc.idx < d.svcprovisioncp);
// 	const size_t	svcidx = _rsvc.idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 	ServiceStub		&rss = d.psvcarr[svcidx];
// 	cassert(rss.psvc == &_rsvc);
// 	return rss.mtx;
	static Mutex m;
	return m;
}

ObjectUidT Manager::registerObject(
	const Service &_rsvc,
	ObjectBase &_robj,
	ReactorBase &_rr,
	ScheduleFunctorT &_rfct,
	ErrorConditionT &_rerr
){
// 	cassert(_rsvc.idx < d.svcprovisioncp);
// 	ServiceStub		&rss = d.psvcarr[_rsvc.idx];
// 	cassert(!_rsvc.idx || rss.psvc != NULL);
// 	Locker<Mutex>	lock(rss.mtx);
	return doUnsafeRegisterServiceObject(_rsvc.idx, _robj, _rr, _rfct, _rerr);
}


// ObjectUidT Manager::doRegisterServiceObject(const IndexT _svcidx, Object &_robj){
// 	cassert(_svcidx < d.svcprovisioncp);
// 	ServiceStub		&rss = d.psvcarr[_svcidx];
// 	cassert(!_svcidx || rss.psvc != NULL);
// 	//Locker<Mutex>	lock(rss.mtx);
// 	return doUnsafeRegisterServiceObject(_svcidx, _robj);
// }

ObjectUidT Manager::doUnsafeRegisterServiceObject(
	const IndexT _svcidx,
	ObjectBase &_robj,
	ReactorBase &_rr,
	ScheduleFunctorT &_rfct,
	ErrorConditionT &_rerr
){
// 	ServiceStub		&rss = d.psvcarr[_svcidx];
// 	vdbgx(Debug::frame, ""<<(void*)&_robj);
// 	if(rss.state != ServiceStub::StateRunning){
// 		//TODO: set error
// 		return ObjectUidT();
// 	}
// 	
// 	if(rss.objfreestk.size()){
// 		const IndexT	objidx = rss.objfreestk.top();
// 		
// 		rss.objfreestk.pop();
// 		
// 		const IndexT	fullid = unite_index(_svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/));
// 		Locker<Mutex>	lock2(rss.mtxstore.at(objidx, rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/)));
// 		
// 		_rerr = _rfct();
// 		
// 		if(!_rerr){
// 			rss.objvec[objidx].pobj = &_robj;
// 			_robj.fullid = fullid;
// 			return ObjectUidT(fullid, rss.objvec[objidx].unique);
// 		}else{
// 			rss.objfreestk.push(objidx);
// 			return ObjectUidT();
// 		}
// 	}else{
// 		const size_t	objcnt = rss.objvecsz.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 		const uint		objbts = d.objbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 		const size_t	objmaxcnt = bitsToCount(objbts);
// 		
// 		const uint		objpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 		const size_t	objpermutcnt = bitsToCount(objpermutbts);
// 		const size_t	objaddsz = d.computeObjectAddSize(rss.objvec.size(), objpermutcnt);
// 		size_t			newobjcnt = objcnt + objaddsz;
// 		ObjectUidT		retval;
// 		
// 		if(newobjcnt >= objmaxcnt){
// 			Locker<Mutex>	lockt(d.mtxobj);
// 			const uint		svcbtst = d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 			uint			objbtst = d.objbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 			size_t			objmaxcntt = bitsToCount(objbtst);
// 			
// 			while(newobjcnt >= objmaxcntt){
// 				if((objbtst + svcbtst + 1) > (sizeof(IndexT) * 8)){
// 					break;
// 				}
// 				++objbtst;
// 				objmaxcntt = bitsToCount(objbtst);
// 			}
// 			if(newobjcnt >=  objmaxcntt){
// 				if(objmaxcntt > objcnt){
// 					newobjcnt = objmaxcntt;
// 				}else{
// 					return retval;
// 				}
// 			}else{
// 				d.objbts.store(objbtst/*, ATOMIC_NS::memory_order_seq_cst*/);
// 			}
// 		}
// 		
// 		for(size_t i = objcnt; i < newobjcnt; ++i){
// 			rss.mtxstore.safeAt(i, objpermutbts);
// 			rss.objfreestk.push(newobjcnt - (i - objcnt) - 1);
// 		}
// 		
// 		const IndexT idx = rss.objfreestk.top();
// 		
// 		retval.index = unite_index(_svcidx, idx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/));;
// 		
// 		lock_all(rss.mtxstore, newobjcnt, objpermutbts);
// 		
// 		rss.objvec.resize(newobjcnt);
// 		
// 		_rerr = _rfct();
// 		
// 		if(!_rerr){
// 			rss.objfreestk.pop();
// 			rss.objvec[idx].pobj = &_robj;
// 			_robj.fullid = retval.index;
// 			retval.unique = rss.objvec[idx].unique;
// 		}else{
// 			retval = ObjectUidT();
// 		}
// 		
// 		rss.objvecsz.store(newobjcnt, ATOMIC_NS::memory_order_release);
// 		
// 		unlock_all(rss.mtxstore, newobjcnt, objpermutbts);
// 		return retval;
// 	}
	return ObjectUidT();
}
bool Manager::doForEachServiceObject(const Service &_rsvc, ObjectVisitFunctorT &_fctor){
// 	Locker<Mutex>	lock1(d.mtx);
	const size_t	svcidx = _rsvc.idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 	cassert(svcidx < d.svcprovisioncp);
// 	ServiceStub		&rss = d.psvcarr[svcidx];
// 	cassert(rss.psvc == &_rsvc);
// 	Locker<Mutex>	lock2(rss.mtx);
	return doForEachServiceObject(svcidx, _fctor);
}

bool Manager::doForEachServiceObject(const size_t _svcidx, Manager::ObjectVisitFunctorT &_fctor){
// 	ServiceStub		&rss = d.psvcarr[_svcidx];
// 	const uint		objpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 	if(rss.objvec.empty()){
// 		return false;
// 	}
// 	size_t			loopcnt = 0;
// 	size_t			hitcnt = 0;
// 	const size_t	objcnt = rss.objvec.size() - rss.objfreestk.size();
// 	Mutex			*poldmtx = &rss.mtxstore[0];
// 	
// 	poldmtx->lock();
// 	for(ObjectVectorT::const_iterator it(rss.objvec.begin()); it != rss.objvec.end() && hitcnt < objcnt; ++it){
// 		++loopcnt;
// 		const size_t idx = it - rss.objvec.begin();
// 		Mutex &rmtx = rss.mtxstore.at(idx, objpermutbts);
// 		if(&rmtx != poldmtx){
// 			poldmtx->unlock();
// 			rmtx.lock();
// 			poldmtx = &rmtx;
// 		}
// 		if(it->pobj){
// 			_fctor(*it->pobj);
// 			++hitcnt;
// 		}
// 	}
// 	poldmtx->unlock();
// 	vdbgx(Debug::frame, "Looped "<<loopcnt<<" times for "<<objcnt<<" objects");
// 	return hitcnt != 0;
	return false;
}

Mutex& Manager::mutex(const IndexT &_rfullid)const{
// 	IndexT			svcidx;
// 	IndexT			objidx;
// 	
// 	split_index(svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _rfullid);
// 	cassert(svcidx < d.svcprovisioncp);
// 	
// 	ServiceStub		&rss = d.psvcarr[svcidx];
// 	const uint 		objpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 	cassert(objidx < rss.objvecsz.load(ATOMIC_NS::memory_order_acquire));
// 	return rss.mtxstore.at(objidx, objpermutbts);
	static Mutex m;
	return m;
}

ObjectBase* Manager::unsafeObject(const IndexT &_rfullid)const{
// 	IndexT		svcidx;
// 	IndexT		objidx;
// 	
// 	split_index(svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _rfullid);
// 	
// 	if(svcidx < d.svcprovisioncp){
// 		ServiceStub		&rss = d.psvcarr[svcidx];
// 		const uint 		objpermutbts = rss.objpermutbts.load(ATOMIC_NS::memory_order_acquire);//set it with release
// 		const size_t	objcnt = rss.objvecsz.load(ATOMIC_NS::memory_order_acquire);
// 		
// 		if(objpermutbts && objidx < objcnt){
// 			if(rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/) == objpermutbts){
// 				return rss.objvec[objidx].pobj;
// 			}
// 		}
// 	}
	return NULL;
}

Service& Manager::service(const ObjectBase &_robj)const{
	static Service *psvc = nullptr;
	return *psvc;
}

void Manager::stopService(Service &_rsvc, bool _wait){
// 	//Locker<Mutex>	lock1(d.mtx);
// 	const size_t	svcidx = _rsvc.idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 	cassert(svcidx < d.svcprovisioncp);
// 	ServiceStub		&rss = d.psvcarr[svcidx];
// 	cassert(rss.psvc == &_rsvc);
// 	Locker<Mutex>	lock2(rss.mtx);
// 	
// 	doWaitStopService(svcidx, lock2, _wait);
}


void Manager::doWaitStopService(const size_t _svcidx, Locker<Mutex> &_rlock, bool _wait){
// 	ServiceStub		&rss = d.psvcarr[_svcidx];
// 	while(rss.state != ServiceStub::StateStopped){
// 		if(rss.state == ServiceStub::StateStarting){
// 			if(!_wait){
// 				return;
// 			}
// 			while(rss.state == ServiceStub::StateStarting){
// 				rss.cnd.wait(_rlock);
// 			}
// 			continue;
// 		}
// 		if(rss.state == ServiceStub::StateStopping){
// 			if(!_wait){
// 				return;
// 			}
// 			while(rss.state == ServiceStub::StateStopping){
// 				rss.cnd.wait(_rlock);
// 			}
// 			continue;
// 		}
// 		if(rss.state == ServiceStub::StateRunning){
// 			rss.state = Data::StateStopping;
// 			EventNotifier		notifier(*this, Event(EventDie));
// 			ObjectVisitFunctorT fctor(notifier);
// 			bool b = doForEachServiceObject(_svcidx, fctor);
// 			if(!b){
// 				rss.state = ServiceStub::StateStopped;
// 				rss.cnd.broadcast();
// 			}
// 		}
// 	}
}

void Manager::doResetService(const size_t _svcidx, Locker<Mutex> &_rlock){
// 	//Locker<Mutex>	lock1(d.mtx);
// 	cassert(_svcidx < d.svcprovisioncp);
// 	ServiceStub		&rss = d.psvcarr[_svcidx];
// 	cassert(rss.psvc);
// 	
// 	doWaitStopService(_svcidx, _rlock, true);
// 	
// 	rss.state = ServiceStub::StateRunning;
// 	rss.cnd.broadcast();
}


}//namespace frame
}//namespace solid
