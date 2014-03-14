#include "frame/sharedstore.hpp"
#include "frame/manager.hpp"
#include "system/cassert.hpp"

namespace solid{
namespace frame{
namespace shared{

void PointerBase::doClear(){
	cassert(psb);
	psb->erasePointer(id());
	uid = invalid_uid();
	psb = NULL;
}

//---------------------------------------------------------------
//		StoreBase
//---------------------------------------------------------------

struct StoreBase::Data{
	Data(Manager &_rm):rm(_rm){}
	Manager &rm;
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
	static Mutex m;
	return m;//TODO!!
}

size_t StoreBase::doAllocateIndex(){
	return 0;
}
void StoreBase::erasePointer(UidT const & _ruid){
	
}

/*virtual*/ int StoreBase::execute(ulong _evs, TimeSpec &_rtout){
	
}

}//namespace shared
}//namespace frame
}//namespace solid
