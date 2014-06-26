#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "frame/selector.hpp"
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

typedef frame::Scheduler<frame::Selector>	SchedulerT;

class BasicObject: public Dynamic<BasicObjec, frame::Object>{
public:
	BasicObject(size_t _repeat = 10):repeat(_repeat), t1(*this), t2(*this){}
	
	/*virtual*/ void execute(ExecuteContext &_rexectx);
	
private:
	size_t			repeat;
	frame::Timer	t1;
	frame::Timer	t2;
};

int main(int argc, char *argv[]){
	{
		frame::Manager		m;
		SchedulerT			s(m);
		
		m.registerObject(DynamicPointer<frame::Object>(new BasicObject(10)), s);
		
		{
			Locker<Mutex>	lock(mtx);
			while(running){
				cnd.wait(lock);
			}
		}
		m.stop();
	}
	Thread::waitAll();
	return 0;
}


/*virtual*/ void BasicObject::execute(ExecuteContext &_rexectx){
	ERROR_NS::error_code	err;
	switch(_rexectx.event().id){
		case frame::EventInit:
			cout<<"EventInit("<<_rexectx.event().index<<") at "<<_rexectx.currentTime()<<endl;
			//t1 should fire first
			t1.waitUntil(_rexectx, _rexectx.time() + 1000 * 5, err, frame::EventTimer, 1); 
			t2.waitUntil(_rexectx, _rexectx.time() + 1000 * 10, err, frame::EventTimer, 2);
			break;
		case frame::EventTimer:
			cout<<"EventTimer("<<_rexectx.event().index<<") at "<<_rexectx.currentTime()<<endl;
			if(_rexectx.event().index == 1){
				if(repeat--){
					t2.cancel();
					t1.waitUntil(_rexectx, _rexectx.time() + 1000 * 5, err, frame::EventTimer, 1); 
					t2.waitUntil(_rexectx, _rexectx.time() + 1000 * 10, err, frame::EventTimer, 2);
				}else{
					t2.cancel();
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
}

