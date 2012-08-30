#include "utility/memory.hpp"
#include "system/thread.hpp"
#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/mutex.hpp"

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
		idbgx(Dbg::utility, "object check failed for "<<v);
	}
	cassert(v == 0);
}
