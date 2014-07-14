#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "frame/reactor.hpp"
#include "frame/object.hpp"
#include "frame/timer.hpp"
#include "system/condition.hpp"
#include "system/mutex.hpp"
#include "system/cassert.hpp"
#include <iostream>


using namespace solid;
using namespace std;


namespace {
	Condition	cnd;
	bool		running = true;
	Mutex		mtx;
}

typedef frame::Scheduler<frame::Reactor>	SchedulerT;

class BasicObject: public Dynamic<BasicObject, frame::Object>{
public:
	BasicObject(size_t _repeat = 10):repeat(_repeat), t1(proxy()), t2(proxy()){}
	
	/*virtual*/ void execute(frame::ExecuteContext &_rexectx);
	
private:
	size_t			repeat;
	frame::Timer	t1;
	frame::Timer	t2;
};

int main(int argc, char *argv[]){
	{
		frame::Manager		m;
		SchedulerT			s(m);
		
		if(s.start(0)){
			const size_t	cnt = argc == 2 ? atoi(argv[1]) : 1;
			
			for(size_t i = 0; i < cnt; ++i){
				DynamicPointer<frame::Object>	objptr(new BasicObject(10));
				m.registerObject(objptr, s);
			}
			
			{
				Locker<Mutex>	lock(mtx);
				while(running){
					cnd.wait(lock);
				}
			}
		}else{
			cout<<"Error starting scheduler"<<endl;
		}
		m.stop();
		
	}
	Thread::waitAll();
	return 0;
}


/*virtual*/ void BasicObject::execute(frame::ExecuteContext &_rexectx){
	switch(_rexectx.event().id){
		case frame::EventInit:
			cout<<"EventInit("<<_rexectx.event().index<<") at "<<_rexectx.time()<<endl;
			//t1 should fire first
			t1.waitUntil(_rexectx, _rexectx.time() + 1000 * 5, frame::EventTimer, 1); 
			cassert(!_rexectx.error());
			t2.waitUntil(_rexectx, _rexectx.time() + 1000 * 10, frame::EventTimer, 2);
			cassert(!_rexectx.error());
			break;
		case frame::EventTimer:
			cout<<"EventTimer("<<_rexectx.event().index<<") at "<<_rexectx.time()<<endl;
			if(_rexectx.event().index == 1){
				if(repeat--){
					t2.cancel(_rexectx);
					t1.waitUntil(_rexectx, _rexectx.time() + 1000 * 5, frame::EventTimer, 1); 
					cassert(!_rexectx.error());
					t2.waitUntil(_rexectx, _rexectx.time() + 1000 * 10, frame::EventTimer, 2);
					cassert(!_rexectx.error());
				}else{
					t2.cancel(_rexectx);
					Locker<Mutex>	lock(mtx);
					running = false;
					cnd.signal();
				}
			}else if(_rexectx.event().index == 2){
				cout<<"ERROR: second timer should never fire"<<endl;
				cassert(false);
			}else{
				cout<<"ERROR: unknown timer index"<<endl;
				cassert(false);
			}
		case frame::EventDie:
			_rexectx.die();
			break;
		default:
			break;
	}
	if(_rexectx.error()){
		_rexectx.die();
	}
}

