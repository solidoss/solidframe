#include "utility/memory.hpp"
#include "system/thread.hpp"
#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/mutex.hpp"

namespace solid{

void EmptyChecker::add(){
	Locker<Mutex> lock(Thread::gmutex());
	++v;
}
void EmptyChecker::sub(){
	Locker<Mutex> lock(Thread::gmutex());
	--v;
}

EmptyChecker::~EmptyChecker(){
	if(v){
		idbgx(Debug::utility, "object check failed for "<<v);
	}
	cassert(v == 0);
}

}//namespace solid

