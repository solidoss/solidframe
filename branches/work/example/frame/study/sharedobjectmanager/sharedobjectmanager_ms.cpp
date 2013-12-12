#include "sharedobjectmanager.hpp"
#include "system/atomic.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/mutualstore.hpp"
#include "utility/workpool.hpp"
#include <deque>
#include <vector>

//sharedobject manager - using mutualstore

using namespace solid;
using namespace std;

typedef ATOMIC_NS::atomic<size_t>			AtomicSizeT;
typedef ATOMIC_NS::atomic<uint32>			AtomicUint32T;
typedef std::vector<size_t>					SizeVectorT;

struct ObjectStub{
	ObjectStub(
	):	idx(-1), thridx(-1), value(0), flags(0), flag1cnt(0),
		flag2cnt(0),flag3cnt(0), flag4cnt(0), minvecsz(-1), maxvecsz(0), eventcnt(0), raisecnt(0)
	{
// 		thridx = -1;
// 		flags = 0;
	}
	ObjectStub(const ObjectStub &_ros){
		*this = _ros;
	}
	ObjectStub& operator=(const ObjectStub &_ros){
		idx = _ros.idx;
		thridx = _ros.thridx.load();
		value = _ros.value;
		flags = _ros.flags.load();
		valvec = _ros.valvec;
		flag1cnt = _ros.flag1cnt;
		flag2cnt = _ros.flag2cnt;
		flag3cnt = _ros.flag3cnt;
		flag4cnt = _ros.flag4cnt;
		minvecsz = _ros.minvecsz;
		maxvecsz = _ros.maxvecsz;
		eventcnt = _ros.eventcnt;
		raisecnt = _ros.raisecnt;
		return *this;
	}
	size_t			idx;
	AtomicSizeT		thridx;
	uint64			value;
	AtomicSizeT		flags;
	SizeVectorT		valvec;
	size_t			flag1cnt;
	size_t			flag2cnt;
	size_t			flag3cnt;
	size_t			flag4cnt;
	size_t			minvecsz;
	size_t			maxvecsz;
	size_t			eventcnt;
	size_t			raisecnt;
};

typedef std::pair<size_t, ObjectStub*>					JobT;
typedef std::deque<ObjectStub>							ObjectVectorT;
typedef std::deque<ObjectStub*>							ObjectPointerVectorT;
typedef MutualStore<Mutex>								MutexMutualStoreT;

struct Worker: WorkerBase{
	ObjectPointerVectorT	objvec;
};

typedef WorkPool<JobT, WorkPoolController, Worker>		WorkPoolT;

struct WorkPoolController: WorkPoolControllerBase{
	typedef std::vector<JobT>		JobVectorT;
	
	SharedObjectManager &rsom;
	
	WorkPoolController(SharedObjectManager &_rsom):rsom(_rsom){}
	
	bool createWorker(WorkPoolT &_rwp){
		_rwp.createMultiWorker(32)->start();
		return true;
	}
	
	void execute(Worker &_rwkr, JobVectorT &_rjobvec){
		for(JobVectorT::const_iterator it(_rjobvec.begin()); it != _rjobvec.end(); ++it){
			size_t	idx;
			if(it->second){
				idx = _rwkr.objvec.size();
				it->second->thridx = idx;
				_rwkr.objvec.push_back(it->second);
			}else{
				idx = it->first;
			}
			rsom.executeObject(*_rwkr.objvec[idx]);
		}
	}
};
namespace{
void visit_lock(Mutex &_rm){
	_rm.lock();
}

void visit_unlock(Mutex &_rm){
	_rm.unlock();
}
void lock_all(MutexMutualStoreT &_rms, const size_t _sz){
	_rms.visit(_sz, visit_lock);
}

void unlock_all(MutexMutualStoreT &_rms, const size_t _sz){
	_rms.visit(_sz, visit_unlock);
}
}

struct SharedObjectManager::Data: WorkPoolControllerBase{
	Data(SharedObjectManager &_rsom):crtidx(0), wp(_rsom){}
	
	Mutex& mutex(const ObjectStub& _robj){
		return mtxstore.at(_robj.idx);
	}
	
	AtomicSizeT			crtidx;
	WorkPoolT			wp;
	ObjectVectorT		objvec;
	Mutex				mtx;
	MutexMutualStoreT	mtxstore;
};


//=========================================================================

SharedObjectManager::SharedObjectManager():d(*(new Data(*this))){
	
}
SharedObjectManager::~SharedObjectManager(){
	delete &d;
}

bool SharedObjectManager::start(){
	d.wp.start(1);
	return true;
}

void SharedObjectManager::insert(size_t _v){
	Locker<Mutex>	lock(d.mtx);
	if(d.crtidx < d.objvec.size()){
	}else{
		d.mtxstore.safeAt(d.crtidx);
		const size_t sz = d.objvec.size();
		lock_all(d.mtxstore, sz);
		d.objvec.resize(sz + d.mtxstore.rowRangeSize());
		unlock_all(d.mtxstore, sz);
	}
	Locker<Mutex>	lock2(d.mtxstore.at(d.crtidx));
	ObjectStub		&robj = d.objvec[d.crtidx];
	robj.value = _v;
	robj.flags = RaiseFlag;
	robj.idx = d.crtidx;
	++d.crtidx;
	d.wp.push(JobT(-1, &robj));
}

bool SharedObjectManager::notify(size_t _idx, uint32 _flags){
	_flags |= (RaiseFlag);
	_idx %= d.crtidx;
	Locker<Mutex>	lock2(d.mtxstore.at(_idx));
	ObjectStub		&robj = d.objvec[_idx];
	const size_t	flags = robj.flags.fetch_or(_flags);
	const size_t 	thridx = robj.thridx;
	if(!(flags & RaiseFlag) && thridx != static_cast<const size_t>(-1)){
		d.wp.push(JobT(thridx, NULL));
	}
	return true;
}
bool SharedObjectManager::notify(size_t _idx, uint32 _flags, size_t _v){
	_flags |= (EventFlag | RaiseFlag);
	_idx %= d.crtidx;
	Locker<Mutex>	lock2(d.mtxstore.at(_idx));
	ObjectStub		&robj = d.objvec[_idx];
	const size_t	flags = robj.flags.fetch_or(_flags);
	robj.valvec.push_back(_v);
	const size_t 	thridx = robj.thridx;
	if(!(flags & RaiseFlag) && thridx != static_cast<const size_t>(-1)){
		d.wp.push(JobT(thridx, NULL));
	}
	return true;
}

bool SharedObjectManager::notifyAll(uint32 _flags){
	_flags |= (RaiseFlag);
	Locker<Mutex>	lock1(d.mtx);
	const size_t	objcnt = d.crtidx;
	for(size_t i = 0; i < objcnt; ++i){
		ObjectStub		&robj = d.objvec[i];
		Locker<Mutex>	lock2(d.mtxstore.at(robj.idx));
		const size_t	flags = robj.flags.fetch_or(_flags);
		const size_t 	thridx = robj.thridx;
		if(!(flags & RaiseFlag) && thridx != static_cast<const size_t>(-1)){
			d.wp.push(JobT(thridx, NULL));
		}
	}
	return true;
}
bool SharedObjectManager::notifyAll(uint32 _flags, size_t _v){
	_flags |= (EventFlag | RaiseFlag);
	Locker<Mutex>	lock1(d.mtx);
	const size_t	objcnt = d.crtidx;
	for(size_t i = 0; i < objcnt; ++i){
		ObjectStub		&robj = d.objvec[i];
		Locker<Mutex>	lock2(d.mtxstore.at(robj.idx));
		const size_t	flags = robj.flags.fetch_or(_flags);
		robj.valvec.push_back(_v);
		const size_t 	thridx = robj.thridx;
		if(!(flags & RaiseFlag) && thridx != static_cast<const size_t>(-1)){
			d.wp.push(JobT(thridx, NULL));
		}
	}
	return true;
}

void SharedObjectManager::executeObject(ObjectStub &_robj){
	size_t flags = _robj.flags.fetch_and(0);
	if(flags & EventFlag){
		Locker<Mutex>	lock(d.mutex(_robj));
		flags |= _robj.flags.fetch_and(0);	//this is to prevent from the following cassert - 
											//between the first fetch_and and the lock, other thread can
											//add new values in valvec.
		cassert(_robj.valvec.size());
		for(SizeVectorT::const_iterator it(_robj.valvec.begin()); it != _robj.valvec.end(); ++it){
			_robj.value += *it;
		}
		_robj.eventcnt += _robj.valvec.size();
		if(_robj.valvec.size() < _robj.minvecsz){
			_robj.minvecsz = _robj.valvec.size();
		}
		if(_robj.valvec.size() > _robj.maxvecsz){
			_robj.maxvecsz = _robj.valvec.size();
		}
		_robj.valvec.clear();
	}
	++_robj.raisecnt;
	if(flags & Flag1){
		++_robj.flag1cnt;
	}
	if(flags & Flag2){
		++_robj.flag2cnt;
	}
	if(flags & Flag3){
		++_robj.flag3cnt;
	}
	if(flags & Flag4){
		++_robj.flag4cnt;
	}
}

void SharedObjectManager::stop(std::ostream &_ros){
	d.wp.stop(true);
	size_t		minvecsz(-1);
	size_t		maxvecsz(0);
	size_t		mineventcnt(-1);
	size_t		maxeventcnt(0);
	size_t		minraisecnt(-1);
	size_t		maxraisecnt(0);
	uint64		eventcnt(0);
	uint64		raisecnt(0);
	for(ObjectVectorT::const_iterator it(d.objvec.begin()); it != d.objvec.end(); ++it){
		if((it - d.objvec.begin()) == d.crtidx) break;
		eventcnt += it->eventcnt;
		raisecnt += it->raisecnt;
		if(it->eventcnt < mineventcnt){
			mineventcnt = it->eventcnt;
		}
		if(it->eventcnt > maxeventcnt){
			maxeventcnt = it->eventcnt;
		}
		if(it->raisecnt < minraisecnt){
			minraisecnt = it->raisecnt;
		}
		if(it->raisecnt > maxraisecnt){
			maxraisecnt = it->raisecnt;
		}
		if(it->minvecsz < minvecsz){
			minvecsz = it->minvecsz;
		}
		if(it->maxvecsz > maxvecsz){
			maxvecsz = it->maxvecsz;
		}
	}
	_ros<<"objvec.size       =	"<<d.crtidx<<" of "<<d.objvec.size()<<endl;
	_ros<<"eventcnt          =	"<<eventcnt<<endl;
	_ros<<"raisecnt          =	"<<raisecnt<<endl;
	_ros<<"minvecsz          =	"<<minvecsz<<endl;
	_ros<<"maxvecsz          =	"<<maxvecsz<<endl;
	_ros<<"mineventcnt       =	"<<mineventcnt<<endl;
	_ros<<"maxeventcnt       =	"<<maxeventcnt<<endl;
	_ros<<"minraisecnt       =	"<<minraisecnt<<endl;
	_ros<<"maxraisecnt       =	"<<maxraisecnt<<endl;
#if 0
	for(ObjectVectorT::const_iterator it(d.objvec.begin()); it != d.objvec.end(); ++it){
		if((it - d.objvec.begin()) == d.crtidx) break;
		if(it->eventcnt == mineventcnt){
			_ros<<"mineventcnt found for "<<it->idx<<endl;
		}
	}
#endif
}

