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

#include "frame/manager.hpp"
#include "frame/message.hpp"
#include "frame/requestuid.hpp"
#include "frame/selectorbase.hpp"

namespace solid{
namespace frame{

typedef std::atomic<size_t>			AtomicSizeT;
typedef std::atomic<SelectorBase*>	AtomicSelectorBaseT;
typedef std::atomic<IndexT>			AtomicIndexT;

//---------------------------------------------------------
struct DummySelector: SelectorBase{
	void raise(uint32 _objidx){}
};
//---------------------------------------------------------
struct ObjectStub{
	
};
typedef std::deque<ObjectStub>		ObjectVectorT;
//---------------------------------------------------------

struct ServiceStub{
	ServiceStub():objvecsz(0){}
	AtomicSizeT		objvecsz;
};
//---------------------------------------------------------
struct Manager::Data{
	Data(
		const size_t _svcprovisioncp,
		const size_t _selprovisioncp,
		int _objpermutbts, int _mutrowsbts, int _mutcolsbts
	):	svcprovisioncp(_svcprovisioncp), selprovisioncp(_selprovisioncp),
		objpermutbts(_objpermutbts),
		mutrowsbts(_mutrowsbts),
		mutcolsbts(_mutcolsbts),
		selbts(1), selobjbts(1), selidx(0)
	{
		psvcarr = new ServiceStub[svcprovisioncp];
		pselarr = new AtomicSelectorBaseT[_selprovisioncp];
		pselarr[0].store(&dummysel, std::memory_order_relaxed);

	}
	
	const size_t			svcprovisioncp;
	const size_t			selprovisioncp;
	const int				objpermutbts;
	const int				mutrowsbts;
	const int				mutcolsbts;
	AtomicSizeT				selbts;
	AtomicSizeT				svcbts;
	AtomicSizeT				svccnt;
	AtomicSizeT				selobjbts;
	size_t					selidx;
	Mutex					mtx;
	Condition				cnd;
	ServiceStub				*psvcarr;
	AtomicSelectorBaseT		*pselarr;
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
	int _objpermutbts, int _mutrowsbts, int _mutcolsbts
):d(*(new Data(_svcprovisioncp, _selprovisioncp, _objpermutbts, _mutrowsbts, _mutcolsbts))){
}

/*virtual*/ Manager::~Manager(){
	stop();
	delete &d;
}

void Manager::stop(){
	
}
//void start();
//void stop(bool _wait = true);

bool Manager::registerService(
	Service &_rs,
	int _objpermutbts,
	int _mutrowsbts,
	int _mutcolsbts
){
	Locker<Mutex>	lock(d.mtx);
}

ObjectUidT	Manager::registerObject(Object &_ro){
	Locker<Mutex>	lock(d.mtx);
}

bool Manager::notify(ulong _sm, const ObjectUidT &_ruid){
	size_t		svcidx;
	size_t		objidx;
	
	
// 	_ruid.split(svcidx, objidx, d.marker.load());
// 	
// 	if(svcidx < d.svcprovisioncp){
// 		//lock free at service level
// 		if(svcidx < d.svcvecsz.load()){
// 			ServiceStub &rss(d.svcvec[svcidx]);
// 		}
// 	}else{
// 		
// 	}
}

/*virtual*/ SpecificMapper*  Manager::specificMapper(){
	return NULL;
}
	
/*virtual*/ GlobalMapper* Manager::globalMapper(){
	return NULL;
}

bool Manager::notify(MessagePointerT &_rmsgptr, const ObjectUidT &_ruid){
	
}

void Manager::raise(const Object &_robj){
	IndexT selidx;
	IndexT objidx;
	split_index(selidx, objidx, d.selbts.load(), _robj.thrid.load());
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
                const size_t    selobjbts2 = d.selobjbts.load(std::memory_order_relaxed);
                const size_t    selbts2 = d.selbts.load(std::memory_order_relaxed);
                const size_t    crtmaxobjcnt2 = (1 << selobjbts2) - 1;
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
		if((d.selidx + 1) >= d.selprovisioncp){
			return false;
		}
		const size_t	crtselbts = d.selbts.load(std::memory_order_relaxed);
		const size_t	crtmaxselcnt = ((1 << crtselbts) - 1);
		
		if((d.selidx + 1) <= crtmaxselcnt){
		}else{
			const size_t	selobjbts2 = d.selobjbts.load(std::memory_order_relaxed);
			if((selobjbts2 + crtselbts + 1) <= (sizeof(IndexT) * 8)){
				d.selbts.fetch_add(1, std::memory_order_relaxed);
			}else{
				return false;
			}
		}
		++d.selidx;
		
		_ps->selid = d.selidx;
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
		d.pselarr[_ps->selid] = NULL;
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