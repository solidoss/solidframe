#include <iostream>
#include <memory>
#include <vector>
#include "system/thread.hpp"
#include "system/debug.hpp"
#include "system/atomic.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"

using namespace std;
using namespace solid;

struct Context{
	Context():strcnt(0), intcnt(0), intv(0), strsz(0){}
	size_t	strcnt;
	size_t	intcnt;
	size_t	intv;
	size_t	strsz;
};

class Task{
public:
	virtual ~Task(){}
	virtual void run(Context &) = 0;
};

class StringTask: public Task{
	std::string str;
	virtual void run(Context &_rctx){
		++_rctx.strcnt;
		_rctx.strsz += str.size();
	}
public:
	StringTask(const std::string &_str):str(_str){}
};

class IntTask: public Task{
	int v;
	virtual void run(Context &_rctx){
		++_rctx.strcnt;
		_rctx.intv += v;
	}
public:
	IntTask(int _v = 0):v(_v){}
};

//=========================================================
//	Scheduler
//=========================================================

class Scheduler{
public:
	virtual ~Scheduler(){}
	virtual bool start(const size_t _conscnt){
		return false;
	}
	virtual void schedule(Task *_ptsk){}
	virtual void stop(bool _wait = true){}
};

//=========================================================
//	LockScheduler
//=========================================================

typedef std::unique_ptr<Task>	TaskPtrT;
typedef std::vector<TaskPtrT>	TaskVectorT;
class LockScheduler;

class LockWorker: public Thread{
	LockScheduler	&rsched;
	TaskVectorT		tskm[2];
	
	
	LockWorker(LockScheduler &_rsched):rsched(_rsched){}
	
	TaskVectorT* waitTasks();
	void run();
};

class LockScheduler: public Scheduler{
	~LockScheduler();
	virtual bool start(const size_t _conscnt);
	virtual void schedule(Task *_ptsk);
	virtual void stop(bool _wait);
};

LockScheduler::~LockScheduler(){
	
}
/*virtual*/ bool LockScheduler::start(const size_t _conscnt){
	return false;
}
/*virtual*/ void LockScheduler::schedule(Task *_ptsk){
	
}
/*virtual*/ void LockScheduler::stop(bool _wait){
	
}
//---------------------------------------------------------
TaskVectorT* LockWorker::waitTasks(){
	return NULL;
}

void LockWorker::run(){
	Context		ctx;
	TaskVectorT	*ptv = NULL;
	while((ptv = waitTasks())){
		for(auto it = ptv->begin(); it != ptv->end(); ++it){
			(*it)->run(ctx);
			it->reset();
		}
		ptv->clear();
	}
	
}

//=========================================================
//	Producers
//=========================================================
const char *strs[] = {
	"one",
	"two",
	"three",
	"four",
	"five",
	"six",
	"seven",
	"eight",
	"nine",
	"ten"
};

ATOMIC_NS::atomic<size_t>	prodtskcnt(0);

class Producer: public Thread{
	Scheduler 		&rsched;
	const size_t	taskcnt;
	
	/*virtual*/ void run(){
		this->yield();
		this->yield();
		this->yield();
		for(size_t i = 0; i < taskcnt; ++i){
			++prodtskcnt;
			if(i % 2){
				rsched.schedule(new IntTask(i));
			}else{
				size_t stridx = (i/2) % (sizeof(strs)/sizeof(const char*));
				rsched.schedule(new StringTask(strs[stridx]));
			}
		}
	}
public:
	Producer(Scheduler &_rsched, const size_t _taskcnt):rsched(_rsched), taskcnt(_taskcnt){}
	~Producer(){
		
	}
};

typedef std::unique_ptr<Producer>	ProducerPtrT;
typedef std::vector<ProducerPtrT>	ProducerVectorT;

enum SchedulerType{
	SchedulerEmptyE,
	SchedulerLockE,
	SchedulerNoLockE,
};

Scheduler * schedulerFactory(const SchedulerType _tp){
	switch(_tp){
		case SchedulerLockE:
			return new LockScheduler;
		default:
			return new Scheduler;
	}
}

int main(int argc, char *argv[]){
	Thread::init();
	Debug::the().initStdErr(false);
	Debug::the().levelMask("view");
	Debug::the().moduleMask("any");
	
	const size_t		prodcnt = 4;
	const size_t		conscnt = 4;
	
	const size_t		prodtaskcnt = 100 * 100;
	const SchedulerType	schedtype = SchedulerLockE;
	
	Scheduler			&sched(*schedulerFactory(schedtype));
	
	sched.start(conscnt);
	
	ProducerVectorT		prodvec;
	for(size_t i = 0; i < prodcnt; ++i){
		std::unique_ptr<Producer>	uniptr(new Producer(sched, prodtaskcnt));
		if(uniptr->start(false, false)){
			prodvec.push_back(std::move(uniptr));
		}
	}
	
	for(auto it = prodvec.begin(); it != prodvec.end(); ++it){
		(*it)->join();
	}
	idbg("produced "<<prodtskcnt<<" tasks");
	sched.stop();
	
	Thread::waitAll();
	
	return 0;
}

