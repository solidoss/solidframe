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
	ObjectStub():idx(-1), thridx(-1), value(0), flags(0){}
	size_t			idx;
	AtomicSizeT		thridx;
	size_t			value;
	AtomicSizeT		flags;
	SizeVectorT		valvec;
};

typedef std::pair<size_t, ObjectStub*>			JobT;
typedef std::deque<ObjectStub>					ObjectVectorT;
typedef std::deque<ObjectStub*>					ObjectPointerVectorT;

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
				idx = it->second->thridx = _rwkr.objvec.size();
				_rwkr.objvec.push_back(it->second);
			}else{
				idx = it->first;
			}
			rsom.executeObject(*_rwkr.objvec[idx]);
		}
	}
};


struct SharedObjectManager::Data: WorkPoolControllerBase{
	Data(SharedObjectManager &_rsom):wp(_rsom){}
	
	WorkPoolT		wp;
	ObjectVectorT	objvec;
};

//=========================================================================

SharedObjectManager::SharedObjectManager():d(*(new Data(*this))){
	
}
SharedObjectManager::~SharedObjectManager(){
	delete &d;
}

bool SharedObjectManager::start(){
	return false;
}

void SharedObjectManager::insert(size_t _v){
	
}

bool SharedObjectManager::notify(size_t _idx, uint32 _flags){
	return false;
}
bool SharedObjectManager::notify(size_t _idx, uint32 _flags, size_t _v){
	return false;
}

bool SharedObjectManager::notifyAll(uint32 _flags){
	return false;
}
bool SharedObjectManager::notifyAll(uint32 _flags, size_t _v){
	return false;
}

void SharedObjectManager::executeObject(ObjectStub &_robj){
	
}

void SharedObjectManager::stop(){
	
}

