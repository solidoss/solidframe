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
		objcnt = 0;
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
	ServiceStub(Mutex &_rmtx):psvc(nullptr), rmtx(_rmtx), firstchk(-1), lastchk(-1){}
	
	void reset(Service *_psvc = nullptr){
		psvc = _psvc;
		firstchk = -1;
		lastchk = -1;
		while(objcache.size())objcache.pop();
	}
	
	Service 	*psvc;
	Mutex		&rmtx;
	size_t		firstchk;
	size_t		lastchk;
	size_t		crtobjidx;
	size_t		endobjidx;
	SizeStackT	objcache;
};


typedef std::deque<ObjectChunk*>			ChunkDequeT;
typedef std::deque<ServiceStub>				ServiceDequeT;
typedef std::vector<ServiceStub*>			ServiceVectorT;


struct ServiceStoreStub{
	ServiceStoreStub(): nowantwrite(true), usecnt(0){}
	AtomicBoolT			nowantwrite;
	AtomicLongT			usecnt;
	ServiceVectorT		vec;
};

struct ObjectStoreStub{
	ObjectStoreStub(): nowantwrite(true), usecnt(0){}
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
		char *p = new char[sizeof(ObjectChunk) + objchkcnt * sizeof(ObjectStub)];
		ObjectChunk *poc = new(p) ObjectChunk(_rmtx);
		return poc;
	}
	
	ObjectChunk* chunk(const size_t _storeidx, const size_t _idx)const{
		return objstore[_storeidx].vec[_idx / objchkcnt];
	}
	
	
	ObjectStub& object(const size_t _storeidx, const size_t _idx){
		return chunk(_storeidx, _idx)->object(_idx % objchkcnt);
	}
	
	size_t aquireReadServiceStore(){
		size_t idx = crtsvcstoreidx;
		if(svcstore[idx].nowantwrite && svcstore[idx].usecnt.fetch_add(1) >= 0){
			return idx;
		}
		Locker<Mutex>	lock(mtx);
		idx = crtsvcstoreidx;
		long v = svcstore[idx].usecnt.fetch_add(1);
		cassert(v >= 0);
		return idx;
	}
	
	void releaseReadServiceStore(const size_t _idx){
		svcstore[_idx].usecnt.fetch_sub(1);
	}
	
	size_t aquireWriteServiceStore(){
		//NOTE: mtx must be locked
		size_t	idx = (crtsvcstoreidx + 1) % 2;
		long	expect = 0;
		
		svcstore[idx].nowantwrite = false;
		
		while(!svcstore[idx].usecnt.compare_exchange_weak(expect, LONG_MIN)){
			expect = 0;
			Thread::yield();
		}
		return idx;
	}
	
	size_t releaseWriteServiceStore(const size_t _idx){
		svcstore[_idx].usecnt = 0;
		crtsvcstoreidx = _idx;
		return _idx;
	}
	
	size_t aquireReadObjectStore(){
		size_t idx = crtobjstoreidx;
		if(objstore[idx].nowantwrite && objstore[idx].usecnt.fetch_add(1) >= 0){
			return idx;
		}
		Locker<Mutex>	lock(mtx);
		idx = crtobjstoreidx;
		long v = objstore[idx].usecnt.fetch_add(1);
		cassert(v >= 0);
		return idx;
	}
	void releaseReadObjectStore(const size_t _idx){
		objstore[_idx].usecnt.fetch_sub(1);
	}
	
	size_t aquireWriteObjectStore(){
		//NOTE: mtx must be locked
		size_t	idx = (crtobjstoreidx + 1) % 2;
		long	expect = 0;
		
		objstore[idx].nowantwrite = false;
		
		while(!objstore[idx].usecnt.compare_exchange_weak(expect, LONG_MIN)){
			expect = 0;
			Thread::yield();
		}
		return idx;
	}
	size_t releaseWriteObjectStore(const size_t _idx){
		objstore[_idx].usecnt = 0;
		crtobjstoreidx = _idx;
		return _idx;
	}
	
	AtomicSizeT				crtsvcstoreidx;
	ServiceDequeT			svcdq;
	ServiceStoreStub		svcstore[2];
	AtomicSizeT				svccnt;
	SizeStackT				svccache;
	
	Mutex					*pobjmtxarr;
	size_t					objmtxcnt;
	
	Mutex					*psvcmtxarr;
	size_t					svcmtxcnt;
	
	AtomicSizeT				crtobjstoreidx;
	ObjectStoreStub			objstore[2];
	size_t					objchkcnt;
	AtomicSizeT				objcnt;
	AtomicSizeT				maxobjcnt;
	
	SizeStackT				chkcache;
	
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
):crtsvcstoreidx(0), svccnt(0), crtobjstoreidx(0)
{
}


Manager::Manager(
	const size_t _svcmtxcnt/* = 0*/,
	const size_t _objmtxcnt/* = 0*/,
	const size_t _objbucketsize/* = 0*/
):d(*(new Data(*this))){
	if(_objmtxcnt == 0){
		d.objmtxcnt = memory_page_size() / sizeof(Mutex);
	}else{
		d.objmtxcnt =  _objmtxcnt;
	}
	
	if(_svcmtxcnt == 0){
		d.svcmtxcnt = memory_page_size() / sizeof(Mutex);
	}else{
		d.svcmtxcnt =  _svcmtxcnt;
	}
	
	d.pobjmtxarr = new Mutex[d.objmtxcnt];
	
	d.psvcmtxarr = new Mutex[d.svcmtxcnt];
	
	
	if(_objbucketsize == 0){
		d.objchkcnt = (memory_page_size() - sizeof(ObjectChunk)) / sizeof(ObjectStub);
	}else{
		d.objchkcnt = _objbucketsize;
	}
	
	
}

/*virtual*/ Manager::~Manager(){
	
	stop(true);
	delete &d;
	delete []d.pobjmtxarr;
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
	
	size_t	crtsvcstoreidx = d.crtsvcstoreidx;
	size_t	svcidx = -1;
	
	if(d.svccache.size()){
		
		svcidx = d.svccache.top();
		d.svccache.top();
		
	}else if(d.svccnt < d.svcstore[crtsvcstoreidx].vec.size()){
		
		svcidx = d.svccnt;
		d.svcdq.push_back(ServiceStub(d.psvcmtxarr[svcidx % d.svcmtxcnt]));
		d.svcstore[crtsvcstoreidx].vec[svcidx] = &d.svcdq[svcidx];
		++d.svccnt;
		
	}else{
		
		const size_t newsvcvecidx = d.aquireWriteServiceStore();
		
		
		d.svcstore[newsvcvecidx].vec = d.svcstore[crtsvcstoreidx].vec;//update the new vector
		
		d.svcstore[newsvcvecidx].vec.push_back(nullptr);
		d.svcstore[newsvcvecidx].vec.resize(d.svcstore[newsvcvecidx].vec.capacity());
		
		svcidx = d.svccnt;
		
		d.svcdq.push_back(ServiceStub(d.psvcmtxarr[svcidx % d.svcmtxcnt]));
		d.svcstore[newsvcvecidx].vec[svcidx] = &d.svcdq[svcidx];
		
		crtsvcstoreidx = d.releaseWriteServiceStore(newsvcvecidx);
		++d.svccnt;

	}
	_rs.idx = svcidx;
	
	d.svcstore[crtsvcstoreidx].vec[svcidx]->reset(&_rs);
	return true;
}


void Manager::unregisterService(Service &_rs){
	if(!_rs.isRegistered()){
		return;
	}
	
	Locker<Mutex>	lock(d.mtx);
	const size_t	objvecidx = d.crtobjstoreidx;//inside lock, so crtobjstoreidx will not change
	const size_t	svcvecidx = d.crtsvcstoreidx;//inside lock, so crtsvcstoreidx will not change
	d.svccache.push(_rs.idx);
	
	ServiceStub		&rss = *d.svcstore[svcvecidx].vec[_rs.idx];
	
	size_t			chkidx = rss.firstchk;
	
	while(chkidx != static_cast<size_t>(-1)){
		ObjectChunk *pchk = d.objstore[objvecidx].vec[chkidx];
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
	ObjectUidT		retval;
	
	if(!_rsvc.isRegistered()){
		return retval;
	}
	
	size_t			objidx = -1;
	const size_t	svcstoreidx = d.aquireReadServiceStore();//can lock d.mtx
	ServiceStub		&rsvc = *d.svcstore[svcstoreidx].vec[_rsvc.idx];
	Locker<Mutex>	lock(rsvc.rmtx);
	
	d.releaseReadServiceStore(svcstoreidx);
	
	if(rsvc.objcache.size()){
		
		objidx = rsvc.objcache.top();
		rsvc.objcache.pop();
		
	}else if(rsvc.crtobjidx < rsvc.endobjidx){
		
		objidx = rsvc.crtobjidx;
		++rsvc.crtobjidx;
		
	}else{
		
		Locker<Mutex>	lock2(d.mtx);
		size_t			chkidx = -1;
		if(d.chkcache.size()){
			chkidx = d.chkcache.top();
			d.chkcache.pop();
		}else{
			const size_t 	newobjstoreidx = d.aquireWriteObjectStore();
			const size_t 	oldobjstoreidx = (newobjstoreidx + 1) % 2;
			
			ObjectStoreStub	&rnewobjstore = d.objstore[newobjstoreidx];
			
			//rnewobjstore.vec = d.objstore[oldobjstoreidx].vec;
			rnewobjstore.vec.insert(
				rnewobjstore.vec.end(),
				d.objstore[oldobjstoreidx].vec.begin() + rnewobjstore.vec.size(),
				d.objstore[oldobjstoreidx].vec.end()
			);
			chkidx = rnewobjstore.vec.size();
			rnewobjstore.vec.push_back(d.allocateChunk(d.pobjmtxarr[chkidx % d.objmtxcnt]));
			
			d.releaseWriteObjectStore(newobjstoreidx);
		}
		
		objidx = chkidx * d.objchkcnt;
		rsvc.crtobjidx = objidx + 1;
		rsvc.endobjidx = objidx + d.objchkcnt;
		
		if(rsvc.firstchk == static_cast<size_t>(-1)){
			rsvc.firstchk = rsvc.lastchk = chkidx;
		}else{
			
			//make the link with the last chunk
			const size_t	objstoreidx = d.aquireReadObjectStore();
			ObjectChunk 	&laschk(*d.objstore[objstoreidx].vec[rsvc.lastchk]);
			Locker<Mutex>	lock3(laschk.rmtx);
			d.releaseReadObjectStore(objstoreidx);
			
			laschk.nextchk = chkidx;
		}
		rsvc.lastchk = chkidx;
	}
	{
		const size_t	objstoreidx = d.aquireReadObjectStore();
		ObjectChunk		&robjchk(*d.chunk(objstoreidx, objidx));
		Locker<Mutex>	lock2(robjchk.rmtx);
		
		d.releaseReadObjectStore(objstoreidx);
		
		if(robjchk.svcidx == static_cast<size_t>(-1)){
			robjchk.svcidx = _rsvc.idx;
		}
		
		cassert(robjchk.svcidx == _rsvc.idx);
		
		if(_rfct(_rr)){
			//the object is scheduled
			ObjectStub &robj = robjchk.object(objidx % d.objchkcnt);
			robj.pobject = &_robj;
			robj.preactor = &_rr;
			_robj.id(objidx);
			retval.index = objidx;
			retval.unique = robj.unique;
			++robjchk.objcnt;
		}else{
			//the object was not scheduled
			rsvc.objcache.push(objidx);
		}
	}
	return retval;
}

void Manager::unregisterObject(ObjectBase &_robj){
	size_t		svcidx = -1;
	size_t		objidx = -1;
	{
		const size_t	objstoreidx = d.aquireReadObjectStore();
		ObjectChunk		&robjchk(*d.chunk(objstoreidx, _robj.id()));
		Locker<Mutex>	lock2(robjchk.rmtx);
			
		d.releaseReadObjectStore(objstoreidx);
		
		ObjectStub 		&robj = robjchk.object(_robj.id() % d.objchkcnt);
		
		robj.pobject = nullptr;
		robj.preactor = nullptr;
		++robj.unique;
		
		_robj.id(-1);
		
		--robjchk.objcnt;
		svcidx = robjchk.svcidx;
	}
	{
		cassert(objidx != static_cast<size_t>(-1));
		cassert(svcidx != static_cast<size_t>(-1));
		
		const size_t	svcstoreidx = d.aquireReadServiceStore();//can lock d.mtx
		ServiceStub		&rsvc = *d.svcstore[svcstoreidx].vec[svcidx];
		Locker<Mutex>	lock(rsvc.rmtx);
		
		rsvc.objcache.push(objidx);
	}
}

bool Manager::notify(ObjectUidT const &_ruid, Event const &_re, const size_t _sigmsk/* = 0*/){
	bool	retval = false;
	if(_ruid.index < d.objcnt){
		const size_t		objstoreidx = d.aquireReadObjectStore();
		ObjectChunk			&rchk(*d.chunk(objstoreidx, _ruid.index));
		{
			Locker<Mutex>		lock(rchk.rmtx);
			ObjectStub const 	&robj(d.object(objstoreidx, _ruid.index));
			
			if(robj.unique == _ruid.unique && robj.pobject){
				if(!_sigmsk || robj.pobject->notify(_sigmsk)){
					retval = robj.preactor->raise(robj.pobject->runId(), _re);
				}
			}
		}
		d.releaseReadObjectStore(objstoreidx);
	}
	return retval;
}

bool Manager::raise(const ObjectBase &_robj, Event const &_re){
	//Current chunk's mutex must be locked
	
	const size_t		objstoreidx = d.aquireReadObjectStore();
	ObjectStub const 	&robj(d.object(objstoreidx, _robj.id()));
	d.releaseReadObjectStore(objstoreidx);
	
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
		const size_t	svcvecidx = d.aquireReadServiceStore();//can lock d.mtx
		{
			Locker<Mutex>	lock(d.mtx);
			crtchkidx = d.svcstore[svcvecidx].vec[_svcidx]->firstchk;
		}
		d.releaseReadServiceStore(svcvecidx);
	}
	
	while(crtchkidx != static_cast<size_t>(-1)){
		
		const size_t	objstoreidx = d.aquireReadObjectStore();//can lock d.mtx
		
		ObjectChunk		&rchk = *d.objstore[objstoreidx].vec[crtchkidx];
		
		d.releaseReadObjectStore(objstoreidx);
		
		Locker<Mutex>	lock(rchk.rmtx);
		ObjectStub		*pobjs = rchk.objects();
		for(size_t i(0), cnt(0); i < d.objchkcnt && cnt < rchk.objcnt; ++i){
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
			const size_t		objstoreidx = d.aquireReadObjectStore();
			ObjectChunk			&rchk(*d.chunk(objstoreidx, _robj.id()));
			
			{
				Locker<Mutex>		lock(rchk.rmtx);
				
				cassert(d.object(objstoreidx, _robj.id()).pobject == &_robj);
				
				svcidx = rchk.svcidx;
			}
			d.releaseReadObjectStore(objstoreidx);
		}
		{
			const size_t	svcstoreidx = d.aquireReadServiceStore();
			psvc = d.svcstore[svcstoreidx].vec[svcidx]->psvc;
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
