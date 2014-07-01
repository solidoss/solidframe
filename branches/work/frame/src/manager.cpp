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
#include "system/condition.hpp"
#include "system/specific.hpp"
#include "system/exception.hpp"
#include "system/mutualstore.hpp"
#include "system/atomic.hpp"

#include "frame/manager.hpp"
#include "frame/message.hpp"
#include "frame/selectorbase.hpp"
#include "frame/service.hpp"

#include "utility/stack.hpp"
#include "utility/queue.hpp"

namespace solid{
namespace frame{

typedef ATOMIC_NS::atomic<size_t>			AtomicSizeT;
typedef ATOMIC_NS::atomic<uint>				AtomicUintT;
typedef ATOMIC_NS::atomic<SelectorBase*>	AtomicSelectorBaseT;
typedef ATOMIC_NS::atomic<IndexT>			AtomicIndexT;

//---------------------------------------------------------
struct DummySelector: SelectorBase{
	void raise(uint32 _objidx){}
};
//---------------------------------------------------------
struct ObjectStub{
	ObjectStub(
		Object *_pobj = NULL,
		uint32 _uid = 0
	): pobj(_pobj), uid(_uid){}
	
	void clear(){
		pobj = NULL;
		++uid;
	}
	Object		*pobj;
	uint32		uid;
	
};
typedef std::deque<ObjectStub>				ObjectVectorT;
typedef Queue<size_t>						SizeQueueT;
typedef Stack<size_t>						SizeStackT;
typedef MutualStore<Mutex>					MutexMutualStoreT;
//---------------------------------------------------------

struct ServiceStub{
	enum States{
		StateStarting = 1,
		StateRunning,
		StateStopping,
		StateStopped
	};
	ServiceStub():psvc(NULL), objvecsz(ATOMIC_VAR_INIT(0)), objpermutbts(ATOMIC_VAR_INIT(0)){
		//mtxstore.safeAt(0);
	}
	
	Service					*psvc;
	AtomicSizeT				objvecsz;
	AtomicUintT				objpermutbts;
	ObjectVectorT			objvec;
	SizeStackT				objfreestk;
	Mutex					mtx;
	Condition				cnd;
	MutexMutualStoreT		mtxstore;
	ushort					state;
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
		Manager &_rm,
		const size_t _svcprovisioncp,
		const size_t _selprovisioncp,
		uint _objpermutbts, uint _mutrowsbts, uint _mutcolsbts
	);
	
	size_t computeObjectAddSize(const size_t /*_objvecsz*/, const size_t _objpermutcnt)const{
		return mutcolscnt * _objpermutcnt;
	}
	
	const size_t			svcprovisioncp;
	const size_t			selprovisioncp;
	const uint				objpermutbts;
	const uint				mutrowsbts;
	const uint				mutcolsbts;
	const size_t			mutcolscnt;
	size_t					svccnt;
	AtomicUintT				selbts;
	AtomicUintT				svcbts;
	AtomicUintT				objbts;
	AtomicUintT				selobjbts;
	uint					state;
	Mutex					mtx;
	Mutex					mtxobj;
	Condition				cnd;
	ServiceStub				*psvcarr;
	SizeStackT				svcfreestk;
	AtomicSelectorBaseT		*pselarr;
	SizeStackT				selfreestk;
	DummySelector			dummysel;
	Service					dummysvc;
};


#ifdef HAS_SAFE_STATIC
static const unsigned specificPosition(){
	static const unsigned thrspecpos = Thread::specificId();
	return thrspecpos;
}
#else
const unsigned specificIdStub(){
	static const uint id(Thread::specificId());
	return id;
}
void once_stub(){
	specificIdStub();
}

const unsigned specificPosition(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_stub, once);
	return specificIdStub();
}
#endif

namespace{

	void visit_lock(Mutex &_rm){
		_rm.lock();
	}

	void visit_unlock(Mutex &_rm){
		_rm.unlock();
	}

	void lock_all(MutexMutualStoreT &_rms, const size_t _sz, const uint _objpermutbts){
		_rms.visit(_sz, visit_lock, _objpermutbts);
	}

	void unlock_all(MutexMutualStoreT &_rms, const size_t _sz, const uint _objpermutbts){
		_rms.visit(_sz, visit_unlock, _objpermutbts);
	}
	
	struct EventNotifier{
		EventNotifier(Manager &_rm, SharedEvent const &_revt):rm(_rm), evt(_revt){}
		Manager			&rm;
		SharedEvent		evt;
		
		void operator()(ObjectBase &_robj){
			Event tmpevt(evt);
			rm.raise(_robj, tmpevt);
		}
	};
}//namespace


Manager::Data::Data(
	Manager &_rm,
	const size_t _svcprovisioncp,
	const size_t _selprovisioncp,
	uint _objpermutbts, uint _mutrowsbts, uint _mutcolsbts
):	svcprovisioncp(_svcprovisioncp), selprovisioncp(_selprovisioncp),
	objpermutbts(_objpermutbts),
	mutrowsbts(_mutrowsbts),
	mutcolsbts(_mutcolsbts),
	mutcolscnt(bitsToCount(_mutcolsbts)),
	svccnt(0),
	selbts(ATOMIC_VAR_INIT(1)), svcbts(ATOMIC_VAR_INIT(0)),objbts(ATOMIC_VAR_INIT(0)),
	selobjbts(ATOMIC_VAR_INIT(1)), state(StateRunning), dummysvc(_rm)
{
}

/*static*/ Manager& Manager::specific(){
	return *reinterpret_cast<Manager*>(Thread::specific(specificPosition()));
}
	
Manager::Manager(
	const size_t _svcprovisioncp,
	const size_t _selprovisioncp,
	uint _objpermutbts, uint _mutrowsbts, uint _mutcolsbts
):d(*(new Data(*this, _svcprovisioncp, _selprovisioncp, _objpermutbts, _mutrowsbts, _mutcolsbts))){
	cassert(d.svcprovisioncp);
	cassert(d.selprovisioncp);
	Locker<Mutex>		lock(d.mtx);
	d.psvcarr = new ServiceStub[d.svcprovisioncp];
	d.pselarr = new AtomicSelectorBaseT[d.selprovisioncp];
	
	for(size_t i = d.svcprovisioncp - 1; i > 0; --i){
		d.svcfreestk.push(i);
	}
	d.svcfreestk.push(0);
	for(size_t i = d.selprovisioncp - 1; i > 0; --i){
		d.selfreestk.push(i);
	}
	
	d.pselarr[0].store(&d.dummysel/*, ATOMIC_NS::memory_order_seq_cst*/);
	
	doRegisterService(d.dummysvc);
	Thread::specific(specificPosition(), this);
}

/*virtual*/ Manager::~Manager(){
	Locker<Mutex> lock(d.mtx);
	vdbgx(Debug::frame, "");
	for(size_t i = 0; i < d.svcprovisioncp && d.svccnt; ++i){
		ServiceStub &rss = d.psvcarr[i];
		if(rss.psvc){
			Locker<Mutex>	lock2(rss.mtx);
			doWaitStopService(i, lock2, true);
			rss.psvc->idx.store(-1/*, ATOMIC_NS::memory_order_seq_cst*/);
			rss.psvc = NULL;
			d.svcfreestk.push(i);
			--d.svccnt;
		}
	}
	delete &d;
}

void Manager::stop(){
	Locker<Mutex> lock(d.mtx);
	size_t crtsvccnt = d.svccnt;
	for(size_t i = 0; i < d.svcprovisioncp && crtsvccnt; ++i){
		ServiceStub &rss = d.psvcarr[i];
		if(rss.psvc){
			Locker<Mutex>	lock2(rss.mtx);
			doWaitStopService(i, lock2, false);
			--crtsvccnt;
		}
	}
	crtsvccnt = d.svccnt;
	for(size_t i = 0; i < d.svcprovisioncp && crtsvccnt; ++i){
		ServiceStub &rss = d.psvcarr[i];
		if(rss.psvc){
			Locker<Mutex>	lock2(rss.mtx);
			doWaitStopService(i, lock2, true);
			--crtsvccnt;
		}
	}
}

bool Manager::registerService(
	Service &_rs,
	uint _objpermutbts
){
	if(_rs.isRegistered()){
		return false;
	}
	Locker<Mutex>	lock(d.mtx);
	return doRegisterService(_rs, _objpermutbts);
}

ObjectUidT Manager::doRegisterObject(ObjectBase &_robj, ObjectScheduleFunctorT &_rfct){
	ServiceStub		&rss = d.psvcarr[0];
	Locker<Mutex>	lock(rss.mtx);
	return doUnsafeRegisterServiceObject(0, _robj, _rfct);
}

void Manager::unregisterService(Service &_rsvc){
	if(!_rsvc.isRegistered()){
		return;
	}
	Locker<Mutex>	lock(d.mtx);
	const size_t	svcidx = _rsvc.idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	cassert(svcidx < d.svcprovisioncp);
	ServiceStub		&rss = d.psvcarr[svcidx];
	cassert(rss.psvc == &_rsvc);
	Locker<Mutex>	lock2(rss.mtx);
	
	doWaitStopService(svcidx, lock2, true);
	
	rss.psvc = NULL;
	d.svcfreestk.push(_rsvc.idx);
	_rsvc.idx.store(-1/*, ATOMIC_NS::memory_order_seq_cst*/);
	--d.svccnt;
}

void Manager::unregisterObject(Object &_robj){
	IndexT		svcidx;
	IndexT		objidx;
	vdbgx(Debug::frame, (void*)&_robj);
	
	split_index(svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _robj.fullid);
	
	if(svcidx < d.svcprovisioncp){
		ServiceStub		&rss = d.psvcarr[svcidx];
		const uint 		objpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);//set it with release
		const size_t	objcnt = rss.objvecsz.load(ATOMIC_NS::memory_order_acquire);
		
		if(objpermutbts && objidx < objcnt){
			Locker<Mutex>	lock(rss.mtxstore.at(objidx, objpermutbts));
			
			if(rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/) == objpermutbts){
				cassert(rss.objvec[objidx].pobj == &_robj);
				rss.objvec[objidx].clear();
				rss.objfreestk.push(objidx);
				if(rss.state == ServiceStub::StateStopping && rss.objfreestk.size() == rss.objvec.size()){
					rss.state = ServiceStub::StateStopped;
					rss.cnd.broadcast();
				}
				return;
			}
		}
	}
	cassert(false);
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
	IndexT		svcidx;
	IndexT		objidx;
	
	split_index(svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _ruid.index);
	
	if(svcidx < d.svcprovisioncp){
		ServiceStub		&rss = d.psvcarr[svcidx];
		const uint 		objpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);//set it with release
		const size_t	objcnt = rss.objvecsz.load(ATOMIC_NS::memory_order_acquire);
		
		if(objpermutbts && objidx < objcnt){
			Locker<Mutex>	lock(rss.mtxstore.at(objidx, objpermutbts));
			
			if(rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/) == objpermutbts && rss.objvec[objidx].uid == _ruid.unique){
				ObjectBase &robj = *rss.objvec[objidx].pobj;
				if(!_sigmsk || robj.notify(_sigmsk)){
					return this->raise(robj, _re);
				}
			}
		}
	}
	return false;
}

bool Manager::raise(const ObjectBase &_robj, Event const &_re){
	IndexT selidx;
	IndexT objidx;
	//split_index(selidx, objidx, d.selbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _robj.thrid.load(/*ATOMIC_NS::memory_order_seq_cst*/));
	//d.pselarr[selidx].load(/*ATOMIC_NS::memory_order_seq_cst*/)->raise(objidx);
}

Mutex& Manager::mutex(const Object &_robj)const{
	IndexT			svcidx;
	IndexT			objidx;
	
	split_index(svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _robj.fullid);
	cassert(svcidx < d.svcprovisioncp);
	
	ServiceStub		&rss = d.psvcarr[svcidx];
	const uint 		objpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	cassert(objidx < rss.objvecsz.load(ATOMIC_NS::memory_order_acquire));
	return rss.mtxstore.at(objidx, objpermutbts);
}

ObjectUidT  Manager::id(const Object &_robj)const{
	IndexT			svcidx;
	IndexT			objidx;
	
	split_index(svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _robj.fullid);
	cassert(svcidx < d.svcprovisioncp);
	
	ServiceStub		&rss = d.psvcarr[svcidx];
	cassert(objidx < rss.objvecsz.load(/*ATOMIC_NS::memory_order_seq_cst*/));
	const uint 		objpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	Locker<Mutex>	lock(rss.mtxstore.at(objidx, objpermutbts));
	return ObjectUidT(_robj.fullid, rss.objvec[objidx].uid);
}

Service& Manager::service(const Object &_robj)const{
	IndexT			svcidx;
	IndexT			objidx;
	
	split_index(svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _robj.fullid);
	cassert(svcidx < d.svcprovisioncp);
	
	ServiceStub		&rss = d.psvcarr[svcidx];
	return *rss.psvc;
}

ObjectUidT  Manager::unsafeId(const Object &_robj)const{
	IndexT			svcidx;
	IndexT			objidx;
	
	split_index(svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _robj.fullid);
	cassert(svcidx < d.svcprovisioncp);
	
	ServiceStub		&rss = d.psvcarr[svcidx];
	cassert(objidx < rss.objvecsz.load(/*ATOMIC_NS::memory_order_seq_cst*/));
	cassert(!rss.mtxstore.at(objidx, rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/)).tryLock());
	return ObjectUidT(_robj.fullid, rss.objvec[objidx].uid);
}

Mutex& Manager::serviceMutex(const Service &_rsvc)const{
	cassert(_rsvc.idx < d.svcprovisioncp);
	const size_t	svcidx = _rsvc.idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	ServiceStub		&rss = d.psvcarr[svcidx];
	cassert(rss.psvc == &_rsvc);
	return rss.mtx;
}

ObjectUidT Manager::registerServiceObject(const Service &_rsvc, Object &_robj, ObjectScheduleFunctorT &_rfct){
	cassert(_rsvc.idx < d.svcprovisioncp);
	ServiceStub		&rss = d.psvcarr[_rsvc.idx];
	cassert(!_rsvc.idx || rss.psvc != NULL);
	Locker<Mutex>	lock(rss.mtx);
	return doUnsafeRegisterServiceObject(_rsvc.idx, _robj, _rfct);
}


bool Manager::doRegisterService(
	Service &_rs,
	uint _objpermutbts
){
	if(d.svcfreestk.size()){
		const size_t	idx = d.svcfreestk.top();
		const uint		svcbts = d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
		const size_t	svcmaxcnt = bitsToCount(svcbts);
		
		if(idx >= svcmaxcnt){
			Locker<Mutex>	lockt(d.mtxobj);
			const uint		svcbtst = d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
			const size_t	svcmaxcntt = bitsToCount(svcbtst);
			const uint		objbtst = d.objbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
			if(idx >= svcmaxcntt){
				if((svcbtst + objbtst + 1) > (sizeof(IndexT) * 8)){
					return false;
				}
				d.svcbts.store(svcbtst + 1/*, ATOMIC_NS::memory_order_seq_cst*/);
			}
		}
		
		d.svcfreestk.pop();
		_rs.idx.store(idx/*, ATOMIC_NS::memory_order_seq_cst*/);
		
		ServiceStub		&rss = d.psvcarr[idx];
		Locker<Mutex>	lock2(rss.mtx);
		
		rss.psvc = &_rs;
		if(!_objpermutbts){
			_objpermutbts = d.objpermutbts;
		}
		
		rss.state = ServiceStub::StateRunning;
		
		const size_t	objcnt = rss.objvecsz.load(/*ATOMIC_NS::memory_order_seq_cst*/);
		const uint		oldobjpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
		
		lock_all(rss.mtxstore, objcnt, oldobjpermutbts);
		
		rss.objpermutbts.store(_objpermutbts, ATOMIC_NS::memory_order_release);
		
		while(rss.objfreestk.size()){
			rss.objfreestk.pop();
		}
		for(size_t i = objcnt; i > 0; --i){
			rss.objfreestk.push(i - 1);
			rss.mtxstore.safeAt(i - 1, _objpermutbts);
		}
		vdbgx(Debug::frame, "Last safe at = "<<(objcnt - 1));
		unlock_all(rss.mtxstore, objcnt, oldobjpermutbts);
		++d.svccnt;
		return true;
	}else{
		return false;
	}
}

// ObjectUidT Manager::doRegisterServiceObject(const IndexT _svcidx, Object &_robj){
// 	cassert(_svcidx < d.svcprovisioncp);
// 	ServiceStub		&rss = d.psvcarr[_svcidx];
// 	cassert(!_svcidx || rss.psvc != NULL);
// 	//Locker<Mutex>	lock(rss.mtx);
// 	return doUnsafeRegisterServiceObject(_svcidx, _robj);
// }

ObjectUidT Manager::doUnsafeRegisterServiceObject(const IndexT _svcidx, Object &_robj, ObjectScheduleFunctorT &_rfct){
	ServiceStub		&rss = d.psvcarr[_svcidx];
	vdbgx(Debug::frame, ""<<(void*)&_robj);
	if(rss.state != ServiceStub::StateRunning){
		return ObjectUidT();
	}
	
	if(rss.objfreestk.size()){
		const IndexT	objidx = rss.objfreestk.top();
		
		rss.objfreestk.pop();
		
		const IndexT	fullid = unite_index(_svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/));
		Locker<Mutex>	lock2(rss.mtxstore.at(objidx, rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/)));
		
		if(_rfct()){
			rss.objvec[objidx].pobj = &_robj;
			_robj.fullid = fullid;
			return ObjectUidT(fullid, rss.objvec[objidx].uid);
		}else{
			rss.objfreestk.push(objidx);
			return ObjectUidT();
		}
		
		
	}else{
		const size_t	objcnt = rss.objvecsz.load(/*ATOMIC_NS::memory_order_seq_cst*/);
		const uint		objbts = d.objbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
		const size_t	objmaxcnt = bitsToCount(objbts);
		
		const uint		objpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
		const size_t	objpermutcnt = bitsToCount(objpermutbts);
		const size_t	objaddsz = d.computeObjectAddSize(rss.objvec.size(), objpermutcnt);
		size_t			newobjcnt = objcnt + objaddsz;
		ObjectUidT		retval;
		
		if(newobjcnt >= objmaxcnt){
			Locker<Mutex>	lockt(d.mtxobj);
			const uint		svcbtst = d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
			uint			objbtst = d.objbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
			size_t			objmaxcntt = bitsToCount(objbtst);
			
			while(newobjcnt >= objmaxcntt){
				if((objbtst + svcbtst + 1) > (sizeof(IndexT) * 8)){
					break;
				}
				++objbtst;
				objmaxcntt = bitsToCount(objbtst);
			}
			if(newobjcnt >=  objmaxcntt){
				if(objmaxcntt > objcnt){
					newobjcnt = objmaxcntt;
				}else{
					return retval;
				}
			}else{
				d.objbts.store(objbtst/*, ATOMIC_NS::memory_order_seq_cst*/);
			}
		}
		
		for(size_t i = objcnt; i < newobjcnt; ++i){
			rss.mtxstore.safeAt(i, objpermutbts);
			rss.objfreestk.push(newobjcnt - (i - objcnt) - 1);
		}
		
		const IndexT idx = rss.objfreestk.top();
		
		retval.index = unite_index(_svcidx, idx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/));;
		
		lock_all(rss.mtxstore, newobjcnt, objpermutbts);
		
		rss.objvec.resize(newobjcnt);
		
		if(_rfct()){
			rss.objfreestk.pop();
			rss.objvec[idx].pobj = &_robj;
			_robj.fullid = retval.index;
			retval.unique = rss.objvec[idx].uid;
		}else{
			retval = ObjectUidT();
		}
		
		rss.objvecsz.store(newobjcnt, ATOMIC_NS::memory_order_release);
		
		unlock_all(rss.mtxstore, newobjcnt, objpermutbts);
		return retval;
	}
	
}
bool Manager::doForEachServiceObject(const Service &_rsvc, ObjectVisitFunctorT &_fctor){
	Locker<Mutex>	lock1(d.mtx);
	const size_t	svcidx = _rsvc.idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	cassert(svcidx < d.svcprovisioncp);
	ServiceStub		&rss = d.psvcarr[svcidx];
	cassert(rss.psvc == &_rsvc);
	Locker<Mutex>	lock2(rss.mtx);
	return doForEachServiceObject(svcidx, _fctor);
}

bool Manager::doForEachServiceObject(const size_t _svcidx, Manager::ObjectVisitFunctorT &_fctor){
	ServiceStub		&rss = d.psvcarr[_svcidx];
	const uint		objpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	if(rss.objvec.empty()){
		return false;
	}
	size_t			loopcnt = 0;
	size_t			hitcnt = 0;
	const size_t	objcnt = rss.objvec.size() - rss.objfreestk.size();
	Mutex			*poldmtx = &rss.mtxstore[0];
	
	poldmtx->lock();
	for(ObjectVectorT::const_iterator it(rss.objvec.begin()); it != rss.objvec.end() && hitcnt < objcnt; ++it){
		++loopcnt;
		const size_t idx = it - rss.objvec.begin();
		Mutex &rmtx = rss.mtxstore.at(idx, objpermutbts);
		if(&rmtx != poldmtx){
			poldmtx->unlock();
			rmtx.lock();
			poldmtx = &rmtx;
		}
		if(it->pobj){
			_fctor(*it->pobj);
			++hitcnt;
		}
	}
	poldmtx->unlock();
	vdbgx(Debug::frame, "Looped "<<loopcnt<<" times for "<<objcnt<<" objects");
	return hitcnt != 0;
}

Mutex& Manager::mutex(const IndexT &_rfullid)const{
	IndexT			svcidx;
	IndexT			objidx;
	
	split_index(svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _rfullid);
	cassert(svcidx < d.svcprovisioncp);
	
	ServiceStub		&rss = d.psvcarr[svcidx];
	const uint 		objpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	cassert(objidx < rss.objvecsz.load(ATOMIC_NS::memory_order_acquire));
	return rss.mtxstore.at(objidx, objpermutbts);
}

Object* Manager::unsafeObject(const IndexT &_rfullid)const{
	IndexT		svcidx;
	IndexT		objidx;
	
	split_index(svcidx, objidx, d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/), _rfullid);
	
	if(svcidx < d.svcprovisioncp){
		ServiceStub		&rss = d.psvcarr[svcidx];
		const uint 		objpermutbts = rss.objpermutbts.load(ATOMIC_NS::memory_order_acquire);//set it with release
		const size_t	objcnt = rss.objvecsz.load(ATOMIC_NS::memory_order_acquire);
		
		if(objpermutbts && objidx < objcnt){
			if(rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/) == objpermutbts){
				return rss.objvec[objidx].pobj;
			}
		}
	}
	return NULL;
}

IndexT Manager::computeThreadId(const IndexT &_selidx, const IndexT &_objidx){
	const size_t selbts = d.selbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	const size_t crtmaxobjcnt = bitsToCount(d.selobjbts.load(/*ATOMIC_NS::memory_order_seq_cst*/));
	const size_t objbts = (sizeof(IndexT) * 8) - selbts;
	const IndexT selmaxcnt = bitsToCount(selbts);
	const IndexT objmaxcnt = bitsToCount(objbts);

	if(_objidx < crtmaxobjcnt){
	}else{
		Locker<Mutex>   lock(d.mtx);
		const size_t	selobjbts2 = d.selobjbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
		const size_t	selbts2 = d.selbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
		const size_t	crtmaxobjcnt2 = (1 << selobjbts2) - 1;
		if(_objidx < crtmaxobjcnt2){
		}else{
			if((selobjbts2 + 1 + selbts2) <= (sizeof(IndexT) * 8)){
				d.selobjbts.fetch_add(1/*, ATOMIC_NS::memory_order_seq_cst*/);
			}else{
				return INVALID_INDEX;
			}
		}
	}

	if(_objidx <= objmaxcnt && _selidx <= selmaxcnt){
		return unite_index(_selidx, _objidx, selbts);
	}else{
		return INVALID_INDEX;
	}
}

bool Manager::prepareThread(SelectorBase *_ps){
	if(_ps){
		Locker<Mutex> lock(d.mtx);
		if(d.selfreestk.empty()){
			return false;
		}
		const size_t	crtselbts = d.selbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
		const size_t	crtmaxselcnt = bitsToCount(crtselbts);
		const size_t	selidx = d.selfreestk.top();
		
		if(selidx < crtmaxselcnt){
		}else{
			const size_t	selobjbts2 = d.selobjbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
			if((selobjbts2 + crtselbts + 1) <= (sizeof(IndexT) * 8)){
				d.selbts.fetch_add(1/*, ATOMIC_NS::memory_order_seq_cst*/);
			}else{
				return false;
			}
		}
		d.selfreestk.pop();
		
		_ps->selid = selidx;
		d.pselarr[_ps->selid] = _ps;
	}
	if(!doPrepareThread()){
		return false;
	}
	Thread::specific(specificPosition(), this);
	Specific::prepareThread();
	requestuidptr.prepareThread();
	return true;
}

void Manager::unprepareThread(SelectorBase *_ps){
	doUnprepareThread();
	Thread::specific(specificPosition(), NULL);
	if(_ps){
		Locker<Mutex> lock(d.mtx);
		d.pselarr[_ps->selid] = NULL;
		d.selfreestk.push(_ps->selid);
		_ps->selid = 0;
	}
	//requestuidptr.unprepareThread();
}

/*virtual*/ bool Manager::doPrepareThread(){
	return true;
}
/*virtual*/ void Manager::doUnprepareThread(){
	
}

void Manager::resetService(Service &_rsvc){
	//Locker<Mutex>	lock1(d.mtx);
	const size_t	svcidx = _rsvc.idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	cassert(svcidx < d.svcprovisioncp);
	ServiceStub		&rss = d.psvcarr[svcidx];
	cassert(rss.psvc == &_rsvc);
	Locker<Mutex>	lock2(rss.mtx);
	
	doWaitStopService(svcidx, lock2, true);
	
	rss.state = ServiceStub::StateRunning;
	rss.cnd.broadcast();
}

void Manager::stopService(Service &_rsvc, bool _wait){
	//Locker<Mutex>	lock1(d.mtx);
	const size_t	svcidx = _rsvc.idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	cassert(svcidx < d.svcprovisioncp);
	ServiceStub		&rss = d.psvcarr[svcidx];
	cassert(rss.psvc == &_rsvc);
	Locker<Mutex>	lock2(rss.mtx);
	
	doWaitStopService(svcidx, lock2, _wait);
}


void Manager::doWaitStopService(const size_t _svcidx, Locker<Mutex> &_rlock, bool _wait){
	ServiceStub		&rss = d.psvcarr[_svcidx];
	while(rss.state != ServiceStub::StateStopped){
		if(rss.state == ServiceStub::StateStarting){
			if(!_wait){
				return;
			}
			while(rss.state == ServiceStub::StateStarting){
				rss.cnd.wait(_rlock);
			}
			continue;
		}
		if(rss.state == ServiceStub::StateStopping){
			if(!_wait){
				return;
			}
			while(rss.state == ServiceStub::StateStopping){
				rss.cnd.wait(_rlock);
			}
			continue;
		}
		if(rss.state == ServiceStub::StateRunning){
			rss.state = Data::StateStopping;
			SignalNotifier		notifier(*this, S_KILL | S_RAISE);
			ObjectVisitFunctorT fctor(notifier);
			bool b = doForEachServiceObject(_svcidx, fctor);
			if(!b){
				rss.state = ServiceStub::StateStopped;
				rss.cnd.broadcast();
			}
		}
	}
}

void Manager::doResetService(const size_t _svcidx, Locker<Mutex> &_rlock){
	//Locker<Mutex>	lock1(d.mtx);
	cassert(_svcidx < d.svcprovisioncp);
	ServiceStub		&rss = d.psvcarr[_svcidx];
	cassert(rss.psvc);
	
	doWaitStopService(_svcidx, _rlock, true);
	
	rss.state = ServiceStub::StateRunning;
	rss.cnd.broadcast();
}


}//namespace frame
}//namespace solid
