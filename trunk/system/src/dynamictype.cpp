#include "system/dynamictype.hpp"
#include "system/thread.hpp"
#include "system/cassert.hpp"
#include "system/mutex.hpp"
#include <vector>

struct DynamicMap::Data{
	typedef std::vector<FncTp>	FncVectorTp;
	FncVectorTp fncvec;
};


/*static*/ uint32 DynamicMap::generateId(){
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
