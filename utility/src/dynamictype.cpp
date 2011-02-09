#include "system/thread.hpp"
#include "system/debug.hpp"
#include "system/cassert.hpp"
#include "system/mutex.hpp"

#include "utility/mutualstore.hpp"
#include "utility/dynamictype.hpp"
#include "utility/dynamicpointer.hpp"
#include "utility/shared.hpp"

#include <vector>

//---------------------------------------------------------------------
//----	Shared	----
//---------------------------------------------------------------------

namespace{
typedef MutualStore<Mutex>	MutexStoreT;

MutexStoreT &mutexStore(){
	//TODO: staticproblem
	static MutexStoreT		mtxstore(3, 2, 2);
	return mtxstore;
}

uint32 specificId(){
	//TODO: staticproblem
	static const uint32 id(Thread::specificId());
	return id;
}


}//namespace

/*static*/ Mutex& Shared::mutex(void *_pv){
	return mutexStore().at((uint32)reinterpret_cast<ulong>(_pv));
}
Shared::Shared(){
	mutexStore().safeAt((uint32)reinterpret_cast<ulong>(this));
}
Mutex& Shared::mutex(){
	return mutex(this);
}

//---------------------------------------------------------------------
//----	DynamicPointer	----
//---------------------------------------------------------------------

void DynamicPointerBase::clear(DynamicBase *_pdyn){
	cassert(_pdyn);
	if(!_pdyn->release()) delete _pdyn;
}

void DynamicPointerBase::use(DynamicBase *_pdyn){
	_pdyn->use();
}

void DynamicPointerBase::storeSpecific(void *_pv)const{
	Thread::specific(specificId(), _pv);
}

/*static*/ void* DynamicPointerBase::fetchSpecific(){
	return Thread::specific(specificId());
}


struct DynamicMap::Data{
	Data(){}
	Data(Data& _rd):fncvec(_rd.fncvec){}
	typedef std::vector<FncT>	FncVectorT;
	FncVectorT fncvec;
};


/*static*/ uint32 DynamicMap::generateId(){
	//TODO: staticproblem
	static uint32 u(0);
	Thread::gmutex().lock();
	uint32 v = ++u;
	Thread::gmutex().unlock();
	return v;
}

static Mutex & dynamicMutex(){
	//TODO: staticproblem
	static Mutex mtx;
	return mtx;
}

void DynamicRegistererBase::lock(){
	dynamicMutex().lock();
}
void DynamicRegistererBase::unlock(){
	dynamicMutex().unlock();
}

DynamicMap::DynamicMap(DynamicMap *_pdm):d(*(_pdm ? new Data(_pdm->d) : new Data)){
}

DynamicMap::~DynamicMap(){
	delete &d;
}

void DynamicMap::callback(uint32 _tid, FncT _pf){
	if(_tid >= d.fncvec.size()){
		d.fncvec.resize(_tid + 1);
	}
	//cassert(!d.fncvec[_tid]);
	d.fncvec[_tid] = _pf;
}

DynamicMap::FncT DynamicMap::callback(uint32 _id)const{
	FncT pf = NULL;
	if(_id < d.fncvec.size()){
		pf = d.fncvec[_id];
	}
	return pf;
}



DynamicBase::~DynamicBase(){}

DynamicMap::FncT DynamicBase::callback(const DynamicMap &_rdm){
	return NULL;
}
/*virtual*/ void DynamicBase::use(){
	idbgx(Dbg::utility, "DynamicBase");
}
//! Used by DynamicPointer to know if the object must be deleted
/*virtual*/ int DynamicBase::release(){
	idbgx(Dbg::utility, "DynamicBase");
	return OK;
}
/*virtual*/ bool DynamicBase::isTypeDynamic(uint32 _id)const{
	return false;
}

void DynamicSharedImpl::doUse(){
	idbgx(Dbg::utility, "DynamicSharedImpl");
	Mutex::Locker	lock(this->mutex());
	++usecount;
}
int DynamicSharedImpl::doRelease(){
	idbgx(Dbg::utility, "DynamicSharedImpl");
	Mutex::Locker	lock(this->mutex());
	--usecount;
	return usecount;
}
