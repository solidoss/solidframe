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

#include <climits>

#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/memory.hpp"
#include "system/condition.hpp"
#include "system/specific.hpp"
#include "system/exception.hpp"
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
typedef ATOMIC_NS::atomic<bool>				AtomicBoolT;
typedef ATOMIC_NS::atomic<uint>				AtomicUintT;
typedef ATOMIC_NS::atomic<long>				AtomicLongT;
typedef ATOMIC_NS::atomic<ReactorBase*>		AtomicReactorBaseT;
typedef ATOMIC_NS::atomic<IndexT>			AtomicIndexT;

typedef Queue<size_t>						SizeQueueT;
typedef Stack<size_t>						SizeStackT;

//---------------------------------------------------------
//POD structure
struct ObjectStub{
	ObjectBase 		*pobject;
	ReactorBase		*preactor;
	UniqueT			unique;
};

struct ObjectChunk{
	ObjectChunk(Mutex &_rmtx):rmtx(_rmtx), svcidx(-1), objcnt(0){}
	Mutex		&rmtx;
	size_t		svcidx;
	size_t		nextchk;
	size_t		objcnt;
	
	void clear(){
		svcidx = -1;
		nextchk = -1;
	}
	
	char *data(){
		return reinterpret_cast<char*>(this);
	}
	ObjectStub* objects(){
		return reinterpret_cast<ObjectStub*>(reinterpret_cast<char*>(this) + sizeof(*this));
	}
	
	ObjectStub& object(const size_t _idx){
		return objects()[_idx];
	}
	
	const ObjectStub* objects()const{
		return reinterpret_cast<const ObjectStub*>(reinterpret_cast<const char*>(this) + sizeof(*this));
	}
	
	ObjectStub const& object(const size_t _idx)const{
		return objects()[_idx];
	}
};

struct ServiceStub{
	ServiceStub():psvc(nullptr), firstchk(-1), lastchk(-1){}
	
	void reset(Service *_psvc = nullptr){
		psvc = _psvc;
		firstchk = -1;
		lastchk = -1;
		while(objcache.size())objcache.pop();
	}
	
	Service 	*psvc;
	size_t		firstchk;
	size_t		lastchk;
	SizeStackT	objcache;
};


typedef std::deque<ObjectChunk*>			ChunkDequeT;
typedef std::deque<ServiceStub>				ServiceDequeT;
typedef std::vector<ServiceStub*>			ServiceVectorT;


struct ServiceContainerStub{
	ServiceContainerStub(): nowantwrite(true), usecnt(0){}
	AtomicBoolT			nowantwrite;
	AtomicLongT			usecnt;
	ServiceVectorT		vec;
};

struct ChunkContainerStub{
	ChunkContainerStub(): nowantwrite(true), usecnt(0){}
	AtomicBoolT			nowantwrite;
	AtomicLongT			usecnt;
	ChunkDequeT			vec;
};

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
	
	ObjectChunk* chunk(const size_t _containeridx, const size_t _idx)const{
		return chk[_containeridx].vec[_idx / chkobjcnt];
	}
	
	
	ObjectStub& object(const size_t _containeridx, const size_t _idx){
		return chunk(_containeridx, _idx)->object(_idx % chkobjcnt);
	}
	
	size_t aquireReadServiceContainer(){
		size_t idx = crtsvccontaineridx;
		if(svc[idx].nowantwrite && svc[idx].usecnt.fetch_add(1) >= 0){
			return idx;
		}
		Locker<Mutex>	lock(mtx);
		idx = crtsvccontaineridx;
		long v = svc[idx].usecnt.fetch_add(1);
		cassert(v >= 0);
		return idx;
	}
	
	void releaseReadServiceContainer(const size_t _idx){
		svc[_idx].usecnt.fetch_sub(1);
	}
	
	size_t aquireWriteServiceContainer(){
		//NOTE: mtx must be locked
		size_t	idx = (crtsvccontaineridx + 1) % 2;
		long	expect = 0;
		
		svc[idx].nowantwrite = false;
		
		while(!svc[idx].usecnt.compare_exchange_weak(expect, LONG_MIN)){
			expect = 0;
			Thread::yield();
		}
		return idx;
	}
	
	size_t releaseWriteServiceContainer(const size_t _idx){
		svc[_idx].usecnt = 0;
		crtsvccontaineridx = _idx;
		return _idx;
	}
	
	size_t aquireReadChunkContainer(){
		size_t idx = crtchkcontaineridx;
		if(chk[idx].nowantwrite && chk[idx].usecnt.fetch_add(1) >= 0){
			return idx;
		}
		Locker<Mutex>	lock(mtx);
		idx = crtchkcontaineridx;
		long v = chk[idx].usecnt.fetch_add(1);
		cassert(v >= 0);
		return idx;
	}
	void releaseReadChunkContainer(const size_t _idx){
		chk[_idx].usecnt.fetch_sub(1);
	}
	
	size_t aquireWriteChunkContainer(){
		//NOTE: mtx must be locked
		size_t	idx = (crtchkcontaineridx + 1) % 2;
		long	expect = 0;
		
		chk[idx].nowantwrite = false;
		
		while(!chk[idx].usecnt.compare_exchange_weak(expect, LONG_MIN)){
			expect = 0;
			Thread::yield();
		}
		return idx;
	}
	size_t releaseWriteChunkContainer(const size_t _idx){
		chk[_idx].usecnt = 0;
		crtchkcontaineridx = _idx;
		return _idx;
	}
	
	AtomicSizeT				crtsvccontaineridx;
	ServiceDequeT			svcdq;
	ServiceContainerStub	svc[2];
	AtomicSizeT				svccnt;
	SizeStackT				svccache;
	
	Mutex					*pmtxarr;
	size_t					mtxcnt;
	
	AtomicSizeT				crtchkcontaineridx;
	ChunkContainerStub		chk[2];
	SizeStackT				chkcache;
	size_t					chkobjcnt;
	AtomicSizeT				objcnt;
	AtomicSizeT				maxobjcnt;
	
	Mutex					mtx;
	Condition				cnd;
	
};


void EventNotifierF::operator()(ObjectBase &_robj){
	Event tmpevt(evt);
	
	if(!sigmsk || _robj.notify(sigmsk)){
		rm.raise(_robj, tmpevt);
	}
}



Manager::Data::Data(
	Manager &_rm
):crtsvccontaineridx(0), svccnt(0), crtchkcontaineridx(0)
{
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
	
	size_t	crtsvccontaineridx = d.crtsvccontaineridx;
	size_t	svcidx = -1;
	
	if(d.svccache.size()){
		
		svcidx = d.svccache.top();
		d.svccache.top();
		
	}else if(d.svccnt < d.svc[crtsvccontaineridx].vec.size()){
		
		svcidx = d.svccnt;
		d.svcdq.push_back(ServiceStub());
		d.svc[crtsvccontaineridx].vec[svcidx] = &d.svcdq[svcidx];
		++d.svccnt;
		
	}else{
		
		const size_t newsvcvecidx = d.aquireWriteServiceContainer();
		
		
		d.svc[newsvcvecidx].vec = d.svc[crtsvccontaineridx].vec;//update the new vector
		
		d.svc[newsvcvecidx].vec.push_back(nullptr);
		d.svc[newsvcvecidx].vec.resize(d.svc[newsvcvecidx].vec.capacity());
		
		svcidx = d.svccnt;
		
		d.svcdq.push_back(ServiceStub());
		d.svc[newsvcvecidx].vec[svcidx] = &d.svcdq[svcidx];
		
		crtsvccontaineridx = d.releaseWriteServiceContainer(newsvcvecidx);
		++d.svccnt;

	}
	_rs.idx = svcidx;
	
	d.svc[crtsvccontaineridx].vec[svcidx]->reset(&_rs);
	return true;
}


void Manager::unregisterService(Service &_rs){
	if(!_rs.isRegistered()){
		return;
	}
	
	Locker<Mutex>	lock(d.mtx);
	const size_t	chkvecidx = d.crtchkcontaineridx;//inside lock, so crtchkcontaineridx will not change
	const size_t	svcvecidx = d.crtsvccontaineridx;//inside lock, so crtsvccontaineridx will not change
	d.svccache.push(_rs.idx);
	
	ServiceStub		&rss = *d.svc[svcvecidx].vec[_rs.idx];
	
	size_t			chkidx = rss.firstchk;
	
	while(chkidx != static_cast<size_t>(-1)){
		ObjectChunk *pchk = d.chk[chkvecidx].vec[chkidx];
		chkidx = pchk->nextchk;
		pchk->clear();
		d.chkcache.push(chkidx);
	}
	rss.reset();
}

ObjectUidT Manager::registerObject(
	const Service &_rsvc,
	ObjectBase &_robj,
	ReactorBase &_rr,
	ScheduleFunctorT &_rfct,
	ErrorConditionT &_rerr
){
	
	return ObjectUidT();
}

void Manager::unregisterObject(ObjectBase &_robj){

}

bool Manager::notify(ObjectUidT const &_ruid, Event const &_re, const size_t _sigmsk/* = 0*/){
	bool	retval = false;
	if(_ruid.index < d.objcnt){
		const size_t		chkcontaineridx = d.aquireReadChunkContainer();
		ObjectChunk			&rchk(*d.chunk(chkcontaineridx, _ruid.index));
		{
			Locker<Mutex>		lock(rchk.rmtx);
			ObjectStub const 	&robj(d.object(chkcontaineridx, _ruid.index));
			
			if(robj.unique == _ruid.unique && robj.pobject){
				if(!_sigmsk || robj.pobject->notify(_sigmsk)){
					retval = robj.preactor->raise(robj.pobject->runId(), _re);
				}
			}
		}
		d.releaseReadChunkContainer(chkcontaineridx);
	}
	return retval;
}

bool Manager::raise(const ObjectBase &_robj, Event const &_re){
	//Current chunk's mutex must be locked
	
	const size_t		chkcontaineridx = d.aquireReadChunkContainer();
	ObjectStub const 	&robj(d.object(chkcontaineridx, _robj.id()));
	d.releaseReadChunkContainer(chkcontaineridx);
	
	cassert(robj.pobject == &_robj);
	
	return robj.preactor->raise(robj.pobject->runId(), _re);
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


Mutex& Manager::mutex(const Service &_rsvc)const{
// 	cassert(_rsvc.idx < d.svcprovisioncp);
// 	const size_t	svcidx = _rsvc.idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 	ServiceStub		&rss = d.psvcarr[svcidx];
// 	cassert(rss.psvc == &_rsvc);
// 	return rss.mtx;
	static Mutex m;
	return m;
}


bool Manager::doForEachServiceObject(const Service &_rsvc, ObjectVisitFunctorT &_fctor){
	if(!_rsvc.isRegistered()){
		return false;
	}
	const size_t	svcidx = _rsvc.idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	return doForEachServiceObject(svcidx, _fctor);
}

bool Manager::doForEachServiceObject(const size_t _svcidx, Manager::ObjectVisitFunctorT &_fctor){
	
	size_t	crtchkidx;
	bool	retval = false;
	
	{
		const size_t	svcvecidx = d.aquireReadServiceContainer();//can lock d.mtx
		{
			Locker<Mutex>	lock(d.mtx);
			crtchkidx = d.svc[svcvecidx].vec[_svcidx]->firstchk;
		}
		d.releaseReadServiceContainer(svcvecidx);
	}
	
	while(crtchkidx != static_cast<size_t>(-1)){
		
		const size_t	chkvecidx = d.aquireReadChunkContainer();//can lock d.mtx
		
		ObjectChunk		&rchk = *d.chk[chkvecidx].vec[crtchkidx];
		
		d.releaseReadChunkContainer(chkvecidx);
		
		Locker<Mutex>	lock(rchk.rmtx);
		ObjectStub		*pobjs = rchk.objects();
		for(size_t i(0), cnt(0); i < d.chkobjcnt && cnt < rchk.objcnt; ++i){
			if(pobjs[i].pobject){
				_fctor(*pobjs[i].pobject);
				retval = true;
			}
		}
		
		crtchkidx = rchk.nextchk;
	}
	
	return retval;
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

Service& Manager::service(const ObjectBase &_robj)const{
	Service		*psvc = nullptr;
	
	if(_robj.id() < d.objcnt){
		size_t				svcidx = -1;
		{
			const size_t		chkcontaineridx = d.aquireReadChunkContainer();
			ObjectChunk			&rchk(*d.chunk(chkcontaineridx, _robj.id()));
			
			{
				Locker<Mutex>		lock(rchk.rmtx);
				
				cassert(d.object(chkcontaineridx, _robj.id()).pobject == &_robj);
				
				svcidx = rchk.svcidx;
			}
			d.releaseReadChunkContainer(chkcontaineridx);
		}
		{
			const size_t	svccontaineridx = d.aquireReadServiceContainer();
			psvc = d.svc[svccontaineridx].vec[svcidx]->psvc;
		}
	}
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
