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
struct ObjectStub{
	
};
typedef std::deque<ObjectStub>				ObjectVectorT;
typedef Queue<size_t>						SizeQueueT;
typedef Stack<size_t>						SizeStackT;
typedef MutualStore<Mutex>					MutexMutualStoreT;
//---------------------------------------------------------

struct ServiceStub{
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

void EventNotifierF::operator()(ObjectBase &_robj){
	Event tmpevt(evt);
	
	if(!sigmsk || _robj.notify(sigmsk)){
		rm.raise(_robj, tmpevt);
	}
}



Manager::Data::Data(
	Manager &_rm
)
{
}

/*static*/ Manager& Manager::specific(){
	return *reinterpret_cast<Manager*>(Thread::specific(specificPosition()));
}
	
Manager::Manager(
	const size_t _mtxcnt/* = 0*/,
	const size_t _objbucketsize/* = 0*/
):d(*(new Data(*this))){
	Thread::specific(specificPosition(), this);
}

/*virtual*/ Manager::~Manager(){
	
	delete &d;
}

void Manager::stop(Event const &_revt){
}

bool Manager::registerService(
	Service &_rs
){
	if(_rs.isRegistered()){
		return false;
	}
	//Locker<Mutex>	lock(d.mtx);
	return doRegisterService(_rs);
}

ObjectUidT Manager::doRegisterObject(ObjectBase &_robj, ObjectScheduleFunctorT &_rfct, ErrorConditionT &_rerr){
	//ServiceStub		&rss = d.psvcarr[0];
	//Locker<Mutex>	lock(rss.mtx);
	return doUnsafeRegisterServiceObject(0, _robj, _rfct, _rerr);
}

void Manager::unregisterService(Service &_rsvc){
// 	if(!_rsvc.isRegistered()){
// 		return;
// 	}
// 	Locker<Mutex>	lock(d.mtx);
// 	const size_t	svcidx = _rsvc.idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 	cassert(svcidx < d.svcprovisioncp);
// 	ServiceStub		&rss = d.psvcarr[svcidx];
// 	cassert(rss.psvc == &_rsvc);
// 	Locker<Mutex>	lock2(rss.mtx);
// 	
// 	doWaitStopService(svcidx, lock2, true);
// 	
// 	rss.psvc = NULL;
// 	d.svcfreestk.push(_rsvc.idx);
// 	_rsvc.idx.store(-1/*, ATOMIC_NS::memory_order_seq_cst*/);
// 	--d.svccnt;
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

ObjectUidT Manager::registerServiceObject(
	const Service &_rsvc, ObjectBase &_robj, ObjectScheduleFunctorT &_rfct, ErrorConditionT &_rerr
){
// 	cassert(_rsvc.idx < d.svcprovisioncp);
// 	ServiceStub		&rss = d.psvcarr[_rsvc.idx];
// 	cassert(!_rsvc.idx || rss.psvc != NULL);
// 	Locker<Mutex>	lock(rss.mtx);
	return doUnsafeRegisterServiceObject(_rsvc.idx, _robj, _rfct, _rerr);
}


bool Manager::doRegisterService(
	Service &_rs
){
// 	if(d.svcfreestk.size()){
// 		const size_t	idx = d.svcfreestk.top();
// 		const uint		svcbts = d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 		const size_t	svcmaxcnt = bitsToCount(svcbts);
// 		
// 		if(idx >= svcmaxcnt){
// 			Locker<Mutex>	lockt(d.mtxobj);
// 			const uint		svcbtst = d.svcbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 			const size_t	svcmaxcntt = bitsToCount(svcbtst);
// 			const uint		objbtst = d.objbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 			
// 			if(idx >= svcmaxcntt){
// 				if((svcbtst + objbtst + 1) > (sizeof(IndexT) * 8)){
// 					return false;
// 				}
// 				d.svcbts.store(svcbtst + 1/*, ATOMIC_NS::memory_order_seq_cst*/);
// 			}
// 		}
// 		
// 		d.svcfreestk.pop();
// 		_rs.idx.store(idx/*, ATOMIC_NS::memory_order_seq_cst*/);
// 		
// 		ServiceStub		&rss = d.psvcarr[idx];
// 		Locker<Mutex>	lock2(rss.mtx);
// 		
// 		rss.psvc = &_rs;
// 		if(!_objpermutbts){
// 			_objpermutbts = d.objpermutbts;
// 		}
// 		
// 		rss.state = ServiceStub::StateRunning;
// 		
// 		const size_t	objcnt = rss.objvecsz.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 		const uint		oldobjpermutbts = rss.objpermutbts.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 		
// 		lock_all(rss.mtxstore, objcnt, oldobjpermutbts);
// 		
// 		rss.objpermutbts.store(_objpermutbts, ATOMIC_NS::memory_order_release);
// 		
// 		while(rss.objfreestk.size()){
// 			rss.objfreestk.pop();
// 		}
// 		for(size_t i = objcnt; i > 0; --i){
// 			rss.objfreestk.push(i - 1);
// 			rss.mtxstore.safeAt(i - 1, _objpermutbts);
// 		}
// 		vdbgx(Debug::frame, "Last safe at = "<<(objcnt - 1));
// 		unlock_all(rss.mtxstore, objcnt, oldobjpermutbts);
// 		++d.svccnt;
// 		return true;
// 	}else{
// 		return false;
// 	}
	return false;
}

// ObjectUidT Manager::doRegisterServiceObject(const IndexT _svcidx, Object &_robj){
// 	cassert(_svcidx < d.svcprovisioncp);
// 	ServiceStub		&rss = d.psvcarr[_svcidx];
// 	cassert(!_svcidx || rss.psvc != NULL);
// 	//Locker<Mutex>	lock(rss.mtx);
// 	return doUnsafeRegisterServiceObject(_svcidx, _robj);
// }

ObjectUidT Manager::doUnsafeRegisterServiceObject(
	const IndexT _svcidx, ObjectBase &_robj, ObjectScheduleFunctorT &_rfct, ErrorConditionT &_rerr
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

bool Manager::prepareThread(){
	if(!doPrepareThread()){
		return false;
	}
	Thread::specific(specificPosition(), this);
	Specific::the().configure();
	return true;
}

void Manager::unprepareThread(){
	doUnprepareThread();
	Thread::specific(specificPosition(), NULL);
}

/*virtual*/ bool Manager::doPrepareThread(){
	return true;
}
/*virtual*/ void Manager::doUnprepareThread(){
	
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
