#include "foundation/manager.hpp"
#include "foundation/service.hpp"
#include "foundation/scheduler.hpp"
#include "system/thread.hpp"
#include "foundation/aio/aioselector.hpp"
#include "foundation/objectselector.hpp"
#include "foundation/aio/aioobject.hpp"
//#include "foundation/ipc/ipcservice.hpp"

#include <iostream>
using namespace std;

namespace {
	const foundation::IndexT	ipcid(10);
	const foundation::IndexT	firstid(11);
	const foundation::IndexT	secondid(14);
	const foundation::IndexT	thirdid(14);
}


struct ThirdObject: Dynamic<ThirdObject, foundation::Object>{
	ThirdObject(uint _cnt, uint _waitsec):cnt(_cnt), waitsec(_waitsec){}
protected:
	/*virtual*/ int execute(ulong _evs, TimeSpec &_rtout);
private:
	uint	cnt;
	uint	waitsec;
};

/*virtual*/ int ThirdObject::execute(ulong _evs, TimeSpec &_rtout){
	ulong sm(0);
	if(signed()){
		Mutex::Locker lock(mutex());
		sm = grabSignalMask();
		if(sm & foundation::S_KILL){
			return BAD;
		}
	}
	idbg("event on "<<id()<<" remain count "<<cnt<<" wait sec "<<waitsec);
	_rtout.add(waitsec);
	if(cnt == 0) return NOK;
	--cnt;
	if(cnt == 0){
		return BAD;
	}
	return NOK;
}

int main(){
	Thread::init();
#ifdef UDEBUG
	{
	string dbgout;
	Dbg::instance().levelMask("iew");
	Dbg::instance().moduleMask("any");
	Dbg::instance().initStdErr(true);
	}
#endif
	
	typedef foundation::Scheduler<foundation::aio::Selector>	AioSchedulerT;
	typedef foundation::Scheduler<foundation::ObjectSelector>	SchedulerT;
	
	foundation::Manager m;
	
	//AioSchedulerT	*pais = new AioSchedulerT(m);
	SchedulerT		*ps1 = new SchedulerT(m);
	SchedulerT		*ps2 = new SchedulerT(m);
	
	ps1->start();
	
	//m.registerService(new foundation::ipc::Service(), ipcid);
	m.registerService(new foundation::Service, firstid);
	m.registerService(new foundation::Service, secondid);
	m.registerObject(new ThirdObject(0, 10), thirdid);
	
	m.start();
	
	SchedulerT::schedule(foundation::ObjectPointer<>(&m.service(firstid)));
	SchedulerT::schedule(foundation::ObjectPointer<>(&m.service(secondid)));
	SchedulerT::schedule(foundation::ObjectPointer<>(&m.object(thirdid)));
	
	{
		ThirdObject	*po(new ThirdObject(10, 1));
		m.service(firstid).insert(po);
		SchedulerT::schedule(foundation::ObjectPointer<>(po), 0);
	}
	
	{
		ThirdObject	*po(new ThirdObject(20, 2));
		m.service(secondid).insert(po);
		SchedulerT::schedule(foundation::ObjectPointer<>(po), 1);
	}
	
	char c;
	cout<<"> "<<flush;
	cin>>c;
	m.stop(true);
	Thread::waitAll();
	return 0;
}

