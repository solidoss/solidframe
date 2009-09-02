#include "foundation/requestuid.hpp"
#include "system/thread.hpp"

namespace foundation{

struct SpecificRequestUid::ForcedCreate{
};

SpecificRequestUid requestuidptr;//(SpecificRequestUid::ForcedCreate);

static const unsigned specificPosition(){
	//TODO: staticproblem
	static const unsigned	thrspecpos = Thread::specificId();
	return thrspecpos;
}

static void destroy(void *_pv){
	RequestUid *psd = reinterpret_cast<RequestUid*>(_pv);
	delete psd;
}

void SpecificRequestUid::prepareThread(){
	Thread::specific(specificPosition(), new RequestUid, &destroy);
}

SpecificRequestUid::SpecificRequestUid(const ForcedCreate&){
}

RequestUid* SpecificRequestUid::operator->()const{
	return reinterpret_cast<RequestUid*>(Thread::specific(specificPosition()));
}

RequestUid* SpecificRequestUid::ptr() const{
	return reinterpret_cast<RequestUid*>(Thread::specific(specificPosition()));
}

RequestUid& SpecificRequestUid::operator*()const{
	return *ptr();
}


}

