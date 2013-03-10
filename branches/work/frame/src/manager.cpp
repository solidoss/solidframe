/* Implementation file manager.cpp
	
	Copyright 2013 Valentin Palade 
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

#include <vector>
#include <deque>
#include <atomic>

#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/specific.hpp"
#include "system/exception.hpp"
#include "system/mutualstore.hpp"

#include "frame/manager.hpp"
#include "frame/message.hpp"
#include "frame/requestuid.hpp"
#include "frame/selectorbase.hpp"

#include "utility/stack.hpp"
#include "utility/queue.hpp"

namespace solid{
namespace frame{

typedef std::atomic<size_t>			AtomicSizeT;
typedef std::atomic<uint>			AtomicUintT;
typedef std::atomic<SelectorBase*>	AtomicSelectorBaseT;
typedef std::atomic<IndexT>			AtomicIndexT;

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
	
	void release(){
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
	ServiceStub():objvecsz(ATOMIC_VAR_INIT(0)), objpermutbts(ATOMIC_VAR_INIT(0)){}
	AtomicSizeT				objvecsz;
	AtomicUintT				objpermutbts;
	ObjectVectorT			objvec;
	Mutex					mtx;
	MutexMutualStoreT		mtxstore;
};
//---------------------------------------------------------
struct Manager::Data{
	Data(
		const size_t _svcprovisioncp,
		const size_t _selprovisioncp,
		uint _objpermutbts, uint _mutrowsbts, uint _mutcolsbts
	):	svcprovisioncp(_svcprovisioncp), selprovisioncp(_selprovisioncp),
		objpermutbts(_objpermutbts),
		mutrowsbts(_mutrowsbts),
		mutcolsbts(_mutcolsbts),
		selbts(ATOMIC_VAR_INIT(1)), selobjbts(ATOMIC_VAR_INIT(1))
	{
		cassert(svcprovisioncp);
		cassert(selprovisioncp);
		Locker<Mutex>		lock(mtx);
		psvcarr = new ServiceStub[svcprovisioncp];
		pselarr = new AtomicSelectorBaseT[selprovisioncp];
		for(size_t i = svcprovisioncp - 1; i > 0; --i){
			svcfreestk.push(i);
		}
		for(size_t i = selprovisioncp - 1; i > 0; --i){
			svcfreestk.push(i);
		}
		pselarr[0].store(&dummysel, std::memory_order_relaxed);
	}
	
	const size_t			svcprovisioncp;
	const size_t			selprovisioncp;
	const uint				objpermutbts;
	const uint				mutrowsbts;
	const uint				mutcolsbts;
	AtomicUintT				selbts;
	AtomicUintT				svcbts;
	AtomicUintT				selobjbts;
	Mutex					mtx;
	Condition				cnd;
	ServiceStub				*psvcarr;
	SizeStackT				svcfreestk;
	AtomicSelectorBaseT		*pselarr;
	SizeStackT				selfreestk;
	DummySelector			dummysel;
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

/*static*/ Manager& Manager::specific(){
	return *reinterpret_cast<Manager*>(Thread::specific(specificPosition()));
}
	
Manager::Manager(
	const size_t _svcprovisioncp,
	const size_t _selprovisioncp,
	uint _objpermutbts, uint _mutrowsbts, uint _mutcolsbts
):d(*(new Data(_svcprovisioncp, _selprovisioncp, _objpermutbts, _mutrowsbts, _mutcolsbts))){
}

/*virtual*/ Manager::~Manager(){
	stop();
	delete &d;
}

void Manager::stop(){
	
}

bool Manager::registerService(
	Service &_rs,
	uint _objpermutbts
){
	Locker<Mutex>	lock(d.mtx);
}

ObjectUidT	Manager::registerObject(Object &_ro){
	Locker<Mutex>	lock(d.mtx);
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
bool Manager::notify(ulong _sm, const ObjectUidT &_ruid){
	IndexT		svcidx;
	IndexT		objidx;
	split_index(svcidx, objidx, d.svcbts.load(std::memory_order_relaxed), _ruid.first);
	if(svcidx < d.svcprovisioncp){
		ServiceStub		&rss = d.psvcarr[svcidx];
		const uint 		objpermutbts = rss.objpermutbts.load(std::memory_order_acquire);//set it with release
		const size_t	objcnt = rss.objvecsz.load(std::memory_order_relaxed);
		
		if(objpermutbts && objidx < objcnt){
			Locker<Mutex>	lock(rss.mtxstore.at(objidx, objpermutbts));
			if(rss.objvec[objidx].pobj->notify(_sm)){
				this->raise(*rss.objvec[objidx].pobj);
			}
			return true;
		}
	}
	return false;
}

bool Manager::notify(MessagePointerT &_rmsgptr, const ObjectUidT &_ruid){
	IndexT		svcidx;
	IndexT		objidx;
	split_index(svcidx, objidx, d.svcbts.load(std::memory_order_relaxed), _ruid.first);
	if(svcidx < d.svcprovisioncp){
		ServiceStub		&rss = d.psvcarr[svcidx];
		const uint 		objpermutbts = rss.objpermutbts.load(std::memory_order_acquire);//set it with release
		const size_t	objcnt = rss.objvecsz.load(std::memory_order_relaxed);
		
		if(objpermutbts && objidx < objcnt){
			Locker<Mutex>	lock(rss.mtxstore.at(objidx, objpermutbts));
			if(rss.objvec[objidx].pobj->notify(_rmsgptr)){
				this->raise(*rss.objvec[objidx].pobj);
			}
			return true;
		}
	}
	return false;
}

void Manager::raise(const Object &_robj){
	IndexT selidx;
	IndexT objidx;
	split_index(selidx, objidx, d.selbts.load(std::memory_order_relaxed), _robj.thrid.load(std::memory_order_relaxed));
	d.pselarr[selidx].load()->raise(objidx);
}

Mutex& Manager::mutex(const Object &_robj)const{
	
}

ObjectUidT  Manager::id(const Object &_robj)const{
	
}

Mutex& Manager::serviceMutex(const Service &_rsvc){
	
}
ObjectUidT Manager::registerServiceObject(const Service &_rsvc, Object &_robj){
	
}
Object* Manager::nextServiceObject(const Service &_rsvc, VisitContext &_rctx){
	
}

IndexT Manager::computeThreadId(const IndexT &_selidx, const IndexT &_objidx){
	const size_t selbts = d.selbts.load(std::memory_order_relaxed);
	const size_t crtmaxobjcnt = (1 << d.selobjbts.load(std::memory_order_relaxed)) - 1;
	const size_t objbts = (sizeof(IndexT) * 8) - selbts;
	const IndexT selmaxcnt = ((1 << selbts) - 1);
	const IndexT objmaxcnt = ((1 << objbts) - 1);

	if(_objidx <= crtmaxobjcnt){
	}else{
		Locker<Mutex>   lock(d.mtx);
		const size_t	selobjbts2 = d.selobjbts.load(std::memory_order_relaxed);
		const size_t	selbts2 = d.selbts.load(std::memory_order_relaxed);
		const size_t	crtmaxobjcnt2 = (1 << selobjbts2) - 1;
		if(_objidx <= crtmaxobjcnt2){
		}else{
			if((selobjbts2 + 1 + selbts2) <= (sizeof(IndexT) * 8)){
				d.selobjbts.fetch_add(1, std::memory_order_relaxed);
			}else{
				return 0;
			}
		}
	}

	if(_objidx <= objmaxcnt && _selidx <= selmaxcnt){
		return unite_index(_selidx, _objidx, selbts);
	}else{
		return 0;
	}
}

bool Manager::prepareThread(SelectorBase *_ps){
	if(_ps){
		Locker<Mutex> lock(d.mtx);
		if(d.selfreestk.empty()){
			return false;
		}
		const size_t	crtselbts = d.selbts.load(std::memory_order_relaxed);
		const size_t	crtmaxselcnt = bitsToMask(crtselbts);
		const size_t	selidx = d.selfreestk.top();
		
		if(selidx <= crtmaxselcnt){
		}else{
			const size_t	selobjbts2 = d.selobjbts.load(std::memory_order_relaxed);
			if((selobjbts2 + crtselbts + 1) <= (sizeof(IndexT) * 8)){
				d.selbts.fetch_add(1, std::memory_order_relaxed);
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
	
}

void Manager::stopService(Service &_rsvc, bool _wait){
	
}

}//namespace frame
}//namespace solid