#include "core/requestuid.hpp"
#include "system/thread.hpp"

namespace clientserver{

struct SpecificRequestUid::ForcedCreate{
};

SpecificRequestUid requestuidptr;//(SpecificRequestUid::ForcedCreate);

static const unsigned	thrspecpos = Thread::specificId();

static void destroy(void *_pv){
	RequestUid *psd = reinterpret_cast<RequestUid*>(_pv);
	delete psd;
}

void SpecificRequestUid::prepareThread(){
	Thread::specific(thrspecpos, new RequestUid, &destroy);
}

SpecificRequestUid::SpecificRequestUid(const ForcedCreate&){
}

RequestUid* SpecificRequestUid::operator->()const{
	return reinterpret_cast<RequestUid*>(Thread::specific(thrspecpos));
}

RequestUid* SpecificRequestUid::ptr() const{
	return reinterpret_cast<RequestUid*>(Thread::specific(thrspecpos));
}

RequestUid& SpecificRequestUid::operator*()const{
	return *ptr();
}


}

