#include "foundation/requestuid.hpp"
#include "system/thread.hpp"

namespace foundation{

struct SpecificRequestUid::ForcedCreate{
};

SpecificRequestUid requestuidptr;//(SpecificRequestUid::ForcedCreate);

#ifdef HAVE_SAFE_STATIC
static const unsigned specificPosition(){
	static const unsigned	thrspecpos = Thread::specificId();
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

