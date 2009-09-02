#include "system/dynamictype.hpp"
#include "system/dynamicpointer.hpp"
#include "system/thread.hpp"
#include "system/debug.hpp"
#include "system/cassert.hpp"
#include "system/mutex.hpp"
#include <vector>



//---------------------------------------------------------------------
//----	DynamicPointer	----
//---------------------------------------------------------------------

void DynamicPointerBase::clear(DynamicBase *_pdyn){
	cassert(_pdyn);
	if(_pdyn->release()) delete _pdyn;
}

void DynamicPointerBase::use(DynamicBase *_pdyn){
	_pdyn->use();
}


/*virtual*/ void DynamicBase::use(){
	idbgx(Dbg::system, "Use dynamicbase");
}
//! Used by DynamicPointer to know if the object must be deleted
/*virtual*/ int DynamicBase::release(){
	idbgx(Dbg::system, "Release signal");
	return BAD;
}


struct DynamicMap::Data{
	typedef std::vector<FncTp>	FncVectorTp;
	FncVectorTp fncvec;
};


/*static*/ uint32 DynamicMap::generateId(){
	//TODO: staticproblem
	static uint32 u(0);
	Thread::gmutex().lock();
	uint32 v = ++u;
	Thread::gmutex().unlock();
	return v;
}

DynamicMap::DynamicMap(RegisterFncTp _prfnc):d(*(new Data)){
	(*_prfnc)(*this);
}
DynamicMap::~DynamicMap(){
	delete &d;
}

void DynamicMap::callback(uint32 _tid, FncTp _pf){
	//Thread::gmutex().lock();
	if(_tid >= d.fncvec.size()){
		d.fncvec.resize(_tid + 1);
	}
	cassert(!d.fncvec[_tid]);
	d.fncvec[_tid] = _pf;
	//Thread::gmutex().unlock();
}

DynamicMap::FncTp DynamicMap::callback(uint32 _id)const{
	FncTp pf = NULL;
	//Thread::gmutex().lock();
	if(_id < d.fncvec.size()){
		pf = d.fncvec[_id];
	}
	//Thread::gmutex().unlock();
	return pf;
}



DynamicBase::~DynamicBase(){}
DynamicMap::FncTp DynamicBase::callback(const DynamicMap &_rdm){
	return NULL;
}
