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
	ObjectChunk(Mutex &_rmtx):rmtx(_rmtx), svcidx(-1), nextchk(-1), objcnt(0){}
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

enum State{
	StateStartingE = 1,
	StateRunningE,
	StateStoppingE,
	StateStoppedE
};

struct ServiceStub{
	ServiceStub(
		Mutex &_rmtx
	):	psvc(nullptr), rmtx(_rmtx), firstchk(-1),
		lastchk(-1), state(StateRunningE),
		crtobjidx(-1), endobjidx(-1), objcnt(0){}
	
	void reset(Service *_psvc = nullptr){
		psvc = _psvc;
		firstchk = -1;
		lastchk = -1;
		crtobjidx = -1;
		endobjidx = -1;
		objcnt = 0;
		state = StateRunningE;
		while(objcache.size())objcache.pop();
	}
	
	Service 	*psvc;
	Mutex		&rmtx;
	size_t		firstchk;
	size_t		lastchk;
	State		state;
	size_t		crtobjidx;
	size_t		endobjidx;
	size_t		objcnt;
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

typedef ATOMIC_NS::atomic<State>	AtomicStateT;

//---------------------------------------------------------
struct Manager::Data{
	Data(
		Manager &_rm
	);
	~Data();
	
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
	AtomicSizeT				crtsvcidx;
	size_t					svccnt;
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
	
	AtomicStateT			state;
	
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
):crtsvcstoreidx(0), crtsvcidx(0), svccnt(0), crtobjstoreidx(0), state(StateRunningE)
{
}

Manager::Data::~Data(){
	size_t objstoreidx = 0;
	if(objstore[1].vec.size() > objstore[0].vec.size()){
		objstoreidx = 1;
	}
	ChunkDequeT &rchdq = objstore[objstoreidx].vec;
	for(auto it = rchdq.begin(); it != rchdq.end(); ++it){
		delete [](*it)->data();
	}
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
	
	stop();
	delete []d.pobjmtxarr;
	delete []d.psvcmtxarr;
	
	delete &d;
}

bool Manager::registerService(
	Service &_rs
){
	Locker<Mutex>	lock(d.mtx);
	
	if(_rs.isRegistered()){
		return false;
	}
	
	if(d.state != StateRunningE){
		return false;
	}
	
	size_t	crtsvcstoreidx = d.crtsvcstoreidx;
	size_t	svcidx = -1;
	
	if(d.svccache.size()){
		
		svcidx = d.svccache.top();
		d.svccache.top();
		
	}else if(d.crtsvcidx < d.svcstore[crtsvcstoreidx].vec.size()){
		
		svcidx = d.crtsvcidx;
		d.svcdq.push_back(ServiceStub(d.psvcmtxarr[svcidx % d.svcmtxcnt]));
		d.svcstore[crtsvcstoreidx].vec[svcidx] = &d.svcdq[svcidx];
		++d.crtsvcidx;
		
	}else{
		
		const size_t newsvcvecidx = d.aquireWriteServiceStore();
		
		
		d.svcstore[newsvcvecidx].vec = d.svcstore[crtsvcstoreidx].vec;//update the new vector
		
		d.svcstore[newsvcvecidx].vec.push_back(nullptr);
		d.svcstore[newsvcvecidx].vec.resize(d.svcstore[newsvcvecidx].vec.capacity());
		
		svcidx = d.crtsvcidx;
		
		d.svcdq.push_back(ServiceStub(d.psvcmtxarr[svcidx % d.svcmtxcnt]));
		d.svcstore[newsvcvecidx].vec[svcidx] = &d.svcdq[svcidx];
		
		crtsvcstoreidx = d.releaseWriteServiceStore(newsvcvecidx);
		++d.crtsvcidx;

	}
	_rs.idx = svcidx;
	++d.svccnt;
	d.svcstore[crtsvcstoreidx].vec[svcidx]->reset(&_rs);
	return true;
}


void Manager::unregisterService(Service &_rsvc){
	const size_t	svcidx = _rsvc.idx;
	
	if(svcidx == static_cast<size_t>(-1)){
		return;
	}
	
	const size_t	svcstoreidx = d.aquireReadServiceStore();//can lock d.mtx
	ServiceStub		&rss = *d.svcstore[svcstoreidx].vec[svcidx];
	
	Locker<Mutex>	lock(rss.rmtx);
	
	if(rss.psvc != &_rsvc){
		return;
	}
	
	
	d.releaseReadServiceStore(svcstoreidx);
	
	doUnregisterService(rss);
}

void Manager::doUnregisterService(ServiceStub &_rss){
	Locker<Mutex>	lock2(d.mtx);
	
	const size_t	objvecidx = d.crtobjstoreidx;//inside lock, so crtobjstoreidx will not change
	
	d.svccache.push(_rss.psvc->idx);
	
	_rss.psvc->idx = -1;
	
	size_t			chkidx = _rss.firstchk;
	
	while(chkidx != static_cast<size_t>(-1)){
		ObjectChunk *pchk = d.objstore[objvecidx].vec[chkidx];
		chkidx = pchk->nextchk;
		pchk->clear();
		d.chkcache.push(chkidx);
	}
	
	_rss.reset();
	--d.svccnt;
	if(d.svccnt == 0 && d.state == StateStoppingE){
		d.cnd.broadcast();
	}
}

ObjectUidT Manager::registerObject(
	const Service &_rsvc,
	ObjectBase &_robj,
	ReactorBase &_rr,
	ScheduleFunctorT &_rfct,
	ErrorConditionT &_rerr
){
	ObjectUidT		retval;
	
	const size_t	svcidx = _rsvc.idx;
	
	if(svcidx == static_cast<size_t>(-1)){
		return retval;
	}
	
	size_t			objidx = -1;
	const size_t	svcstoreidx = d.aquireReadServiceStore();//can lock d.mtx
	ServiceStub		&rss = *d.svcstore[svcstoreidx].vec[svcidx];
	Locker<Mutex>	lock(rss.rmtx);
	
	d.releaseReadServiceStore(svcstoreidx);
	
	if(rss.psvc != &_rsvc || rss.state != StateRunningE){
		return retval;
	}
	
	if(rss.objcache.size()){
		
		objidx = rss.objcache.top();
		rss.objcache.pop();
		
	}else if(rss.crtobjidx < rss.endobjidx){
		
		objidx = rss.crtobjidx;
		++rss.crtobjidx;
		
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
		rss.crtobjidx = objidx + 1;
		rss.endobjidx = objidx + d.objchkcnt;
		
		if(rss.firstchk == static_cast<size_t>(-1)){
			rss.firstchk = rss.lastchk = chkidx;
		}else{
			
			//make the link with the last chunk
			const size_t	objstoreidx = d.crtobjstoreidx;//d.mtx is locked so it is safe to fetch the value of 

			ObjectChunk 	&laschk(*d.objstore[objstoreidx].vec[rss.lastchk]);

			Locker<Mutex>	lock3(laschk.rmtx);
			
			laschk.nextchk = chkidx;
		}
		rss.lastchk = chkidx;
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
		
		_robj.id(objidx);
		
		if(_rfct(_rr)){
			//the object is scheduled
			ObjectStub &ros= robjchk.object(objidx % d.objchkcnt);
			ros.pobject = &_robj;
			ros.preactor = &_rr;
			retval.index = objidx;
			retval.unique = ros.unique;
			++robjchk.objcnt;
			++rss.objcnt;
		}else{
			_robj.id(-1);
			//the object was not scheduled
			rss.objcache.push(objidx);
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
		
		objidx = _robj.id();
			
		d.releaseReadObjectStore(objstoreidx);
		
		ObjectStub 		&ros = robjchk.object(_robj.id() % d.objchkcnt);
		
		ros.pobject = nullptr;
		ros.preactor = nullptr;
		++ros.unique;
		
		_robj.id(-1);
		
		--robjchk.objcnt;
		svcidx = robjchk.svcidx;
	}
	{
		cassert(objidx != static_cast<size_t>(-1));
		cassert(svcidx != static_cast<size_t>(-1));
		
		const size_t	svcstoreidx = d.aquireReadServiceStore();//can lock d.mtx
		ServiceStub		&rss = *d.svcstore[svcstoreidx].vec[svcidx];
		Locker<Mutex>	lock(rss.rmtx);
		
		rss.objcache.push(objidx);
		--rss.objcnt;
		if(rss.objcnt == 0 && rss.state == StateStoppingE){
			d.cnd.broadcast();
		}
	}
}

bool Manager::notify(ObjectUidT const &_ruid, Event const &_re, const size_t _sigmsk/* = 0*/){
	bool	retval = false;
	if(_ruid.index < d.objcnt){
		const size_t		objstoreidx = d.aquireReadObjectStore();
		ObjectChunk			&rchk(*d.chunk(objstoreidx, _ruid.index));
		{
			Locker<Mutex>		lock(rchk.rmtx);
			ObjectStub const 	&ros(d.object(objstoreidx, _ruid.index));
			
			if(ros.unique == _ruid.unique && ros.pobject){
				if(!_sigmsk || ros.pobject->notify(_sigmsk)){
					retval = ros.preactor->raise(ros.pobject->runId(), _re);
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
	ObjectStub const 	&ros(d.object(objstoreidx, _robj.id()));
	d.releaseReadObjectStore(objstoreidx);
	
	cassert(ros.pobject == &_robj);
	
	return ros.preactor->raise(ros.pobject->runId(), _re);
}


Mutex& Manager::mutex(const ObjectBase &_robj)const{
	Mutex	*pmtx;
	{
		const size_t		objstoreidx = d.aquireReadObjectStore();
		ObjectChunk			&rchk(*d.chunk(objstoreidx, _robj.id()));
		
		d.releaseReadObjectStore(objstoreidx);
		pmtx = &rchk.rmtx;
	}
	return *pmtx;
}

ObjectUidT  Manager::id(const ObjectBase &_robj)const{
	const IndexT		objidx = _robj.id();
	if(objidx != static_cast<IndexT>(-1)){
		const size_t		objstoreidx = d.aquireReadObjectStore();
		ObjectStub const 	&ros(d.object(objstoreidx, objidx));
		d.releaseReadObjectStore(objstoreidx);
		
		cassert(ros.pobject == &_robj);
		return ObjectUidT(objidx, ros.unique);
	}else{
		return ObjectUidT();
	}
}


Mutex& Manager::mutex(const Service &_rsvc)const{
	const size_t	svcidx = _rsvc.idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	const size_t	svcstoreidx = d.aquireReadServiceStore();//can lock d.mtx
	ServiceStub		&rsvc = *d.svcstore[svcstoreidx].vec[svcidx];
		
	d.releaseReadServiceStore(svcstoreidx);
		
	return rsvc.rmtx;
}


bool Manager::doForEachServiceObject(const Service &_rsvc, ObjectVisitFunctorT &_fctor){
	if(!_rsvc.isRegistered()){
		return false;
	}
	const size_t	svcidx = _rsvc.idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	size_t			chkidx = -1;
	{
		const size_t	svcstoreidx = d.aquireReadServiceStore();//can lock d.mtx
		{
			ServiceStub		&rss = *d.svcstore[svcstoreidx].vec[svcidx];
			Locker<Mutex>	lock(rss.rmtx);
		
			chkidx = rss.firstchk;
		}
		d.releaseReadServiceStore(svcstoreidx);
	}
	
	return doForEachServiceObject(chkidx, _fctor);
}

bool Manager::doForEachServiceObject(const size_t _chkidx, Manager::ObjectVisitFunctorT &_fctor){
	
	size_t	crtchkidx = _chkidx;
	bool	retval = false;
	
	
	while(crtchkidx != static_cast<size_t>(-1)){
		
		const size_t	objstoreidx = d.aquireReadObjectStore();//can lock d.mtx
		
		ObjectChunk		&rchk = *d.objstore[objstoreidx].vec[crtchkidx];
		
		d.releaseReadObjectStore(objstoreidx);
		
		Locker<Mutex>	lock(rchk.rmtx);
		ObjectStub		*poss = rchk.objects();
		for(size_t i(0), cnt(0); i < d.objchkcnt && cnt < rchk.objcnt; ++i){
			if(poss[i].pobject){
				_fctor(*poss[i].pobject);
				retval = true;
			}
		}
		
		crtchkidx = rchk.nextchk;
	}
	
	return retval;
}

Service& Manager::service(const ObjectBase &_robj)const{
	Service		*psvc = nullptr;	
	size_t		svcidx = -1;
	{
		const size_t		objstoreidx = d.aquireReadObjectStore();
		ObjectChunk			&rchk(*d.chunk(objstoreidx, _robj.id()));
		
		d.releaseReadObjectStore(objstoreidx);
		
		{
			Locker<Mutex>		lock(rchk.rmtx);
			
			cassert(rchk.object(_robj.id() % d.objchkcnt).pobject == &_robj);
			
			svcidx = rchk.svcidx;
		}
	}
	{
		const size_t	svcstoreidx = d.aquireReadServiceStore();
		psvc = d.svcstore[svcstoreidx].vec[svcidx]->psvc;
		d.releaseReadServiceStore(svcstoreidx);
	}
	return *psvc;
}

void Manager::stopService(Service &_rsvc, const bool _wait){
	if(!_rsvc.isRegistered()){
		return;
	}
	const size_t	svcidx = _rsvc.idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	const size_t	svcstoreidx = d.aquireReadServiceStore();//can lock d.mtx
	ServiceStub		&rss = *d.svcstore[svcstoreidx].vec[svcidx];
	
	d.releaseReadServiceStore(svcstoreidx);
	
	Locker<Mutex>	lock(rss.rmtx);
	
	if(rss.state == StateStoppedE){
		return;
	}
	if(rss.state == StateRunningE){
		EventNotifierF		evtntf(*this, _rsvc.stopevent);
		ObjectVisitFunctorT fctor(evtntf);
		const bool		any = doForEachServiceObject(rss.firstchk, fctor);
		if(!any){
			rss.state = StateStoppedE;
			return;
		}
		rss.state = StateStoppingE;
	}
	
	if(rss.state == StateStoppingE && _wait){
		while(rss.objcnt){
			d.cnd.wait(lock);
		}
		rss.state = StateStoppedE;
	}
}

void Manager::stop(){
	{
		Locker<Mutex>	lock(d.mtx);
		if(d.state == StateStoppedE){
			return;
		}
		if(d.state == StateRunningE){
			d.state = StateStoppingE;
		}else if(d.state == StateStoppingE){
			while(d.svccnt){
				d.cnd.wait(lock);
			}
		}else{
			return;
		}
	}
	
	const size_t		svcstoreidx = d.aquireReadServiceStore();//can lock d.mtx
	
	ServiceStoreStub	&rsvcstore = d.svcstore[svcstoreidx];
	
	//broadcast to all objects to stop
	for(auto it = rsvcstore.vec.begin(); it != rsvcstore.vec.end(); ++it){
		ServiceStub		&rss = *(*it);
		
		Locker<Mutex>	lock(rss.rmtx);
		
		if(rss.psvc && rss.state == StateRunningE){
			EventNotifierF		evtntf(*this, rss.psvc->stopevent);
			ObjectVisitFunctorT fctor(evtntf);
			const bool			any = doForEachServiceObject(rss.firstchk, fctor);
			
			rss.state = any ? StateStoppingE : StateStoppedE;
		}
	}
	
	//wait for all services to stop
	for(auto it = rsvcstore.vec.begin(); it != rsvcstore.vec.end(); ++it){
		ServiceStub		&rss = *(*it);
		
		Locker<Mutex>	lock(rss.rmtx);
		
		if(rss.psvc && rss.state == StateStoppingE){
			while(rss.objcnt){
				d.cnd.wait(lock);
			}
			rss.state = StateStoppedE;
		}
		doUnregisterService(rss);
	}
	
	d.releaseReadServiceStore(svcstoreidx);
	
	{
		Locker<Mutex>	lock(d.mtx);
		d.state = StateStoppedE;
		d.cnd.broadcast();
	}
}


}//namespace frame
}//namespace solid
