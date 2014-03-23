#include "frame/sharedstore.hpp"
#include "frame/manager.hpp"
#include "system/cassert.hpp"
#include "system/mutualstore.hpp"
#include "system/atomic.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"


namespace solid{
namespace frame{
namespace shared{

void PointerBase::doClear(const bool _isalive){
	cassert(psb);
	psb->erasePointer(id(), _isalive);
	uid = invalid_uid();
	psb = NULL;
}

//---------------------------------------------------------------
//		StoreBase
//---------------------------------------------------------------
typedef ATOMIC_NS::atomic<size_t>			AtomicSizeT;
typedef MutualStore<Mutex>					MutexMutualStoreT;
typedef Queue<UidT>							UidQueueT;
typedef Stack<size_t>						SizeStackT;
typedef Stack<void*>						VoidPtrStackT;


struct StoreBase::Data{
	Data(Manager &_rm):rm(_rm), objmaxcnt(ATOMIC_VAR_INIT(0)){}
	Manager				&rm;
	AtomicSizeT			objmaxcnt;
	MutexMutualStoreT	mtxstore;
	UidVectorT			erasevec[2];
	UidVectorT			*pfillerasevec;
	SizeVectorT			cacheobjidxvec;
	SizeStackT			cacheobjidxstk;
	ExecWaitVectorT		exewaitvec;
	VoidPtrStackT		cachewaitstk;
};

StoreBase::StoreBase(Manager &_rm):d(*(new Data(_rm))){}

/*virtual*/ StoreBase::~StoreBase(){
	delete &d;
}


Manager& StoreBase::manager(){
	return d.rm;
}

Mutex &StoreBase::mutex(){
	return manager().mutex(*this);
}
Mutex &StoreBase::mutex(const size_t _idx){
	return d.mtxstore.at(_idx);
}

size_t StoreBase::atomicMaxCount()const{
	return d.objmaxcnt.load();
}

UidVectorT& StoreBase::eraseVector(){
	return *d.pfillerasevec;
}

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
	
}//namespace

size_t StoreBase::doAllocateIndex(){
	//mutex is locked
	size_t		rv;
	if(d.cacheobjidxstk.size()){
		rv = d.cacheobjidxstk.top();
		d.cacheobjidxstk.pop();
	}else{
		const size_t objcnt = d.objmaxcnt.load();
		lock_all(d.mtxstore, objcnt);
		doResizeObjectVector(objcnt + 1024);
		for(size_t i = objcnt + 1023; i > objcnt; --i){
			d.cacheobjidxstk.push(i);
		}
		d.objmaxcnt.store(objcnt + 1024);
		unlock_all(d.mtxstore, objcnt);
		rv = objcnt;
	}
	return rv;
}
void StoreBase::erasePointer(UidT const & _ruid, const bool _isalive){
	if(_ruid.first < d.objmaxcnt.load()){
		bool	do_notify = false;
		{
			Locker<Mutex>	lock(mutex(_ruid.first));
			do_notify = doDecrementObjectUseCount(_ruid, _isalive);
		}
		bool do_raise = false;
		if(do_notify){
			Locker<Mutex>	lock(mutex());
			d.pfillerasevec->push_back(_ruid);
			do_raise = Object::notify(frame::S_SIG | frame::S_RAISE);
		}
		if(do_raise){
			manager().raise(*this);
		}
	}
}

/*virtual*/ void StoreBase::execute(ExecuteContext &_rexectx){
	UidVectorT		*pconserasevec = NULL;
	if(notified()){
		Locker<Mutex>	lock(mutex());
		ulong sm = grabSignalMask();
		if(sm & frame::S_KILL){
			_rexectx.close();
			return;
		}
		if(sm & frame::S_RAISE){
			if(d.pfillerasevec->size()){
				pconserasevec = d.pfillerasevec;
				if(d.pfillerasevec == &d.erasevec[0]){
					d.pfillerasevec = &d.erasevec[1];
				}else{
					d.pfillerasevec = &d.erasevec[0];
				}
				d.pfillerasevec->clear();
			}
		}
	}
	if(pconserasevec && pconserasevec->size()){//we have something 
		if(doExecute(*pconserasevec, d.cacheobjidxvec, d.exewaitvec)){
			_rexectx.reschedule();
		}
	}
}
void StoreBase::doCacheObjectIndex(const size_t _idx){
	d.cacheobjidxstk.push(_idx);
}
void StoreBase::doExecuteCache(){
	for(ExecWaitVectorT::const_iterator it(d.exewaitvec.begin()); it != d.exewaitvec.end(); ++it){
		d.cachewaitstk.push(it->pw);
	}
	d.cacheobjidxvec.clear();
	d.exewaitvec.clear();
}

void* StoreBase::doTryAllocateWait(){
	if(d.cachewaitstk.size()){
		void *rv = d.cachewaitstk.top();
		d.cachewaitstk.pop();
		return rv;
	}
	return NULL;
}

}//namespace shared
}//namespace frame
}//namespace solid
