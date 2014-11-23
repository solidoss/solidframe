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
typedef std::unique_ptr<Task>	TaskPtrT;

//=========================================================
//	Scheduler
//=========================================================

class Scheduler{
public:
	virtual ~Scheduler(){}
	virtual bool start(const size_t _conscnt){
		return false;
	}
	virtual void schedule(TaskPtrT &_tskptr){}
	virtual void stop(bool _wait = true){}
};
ATOMIC_NS::atomic<size_t>	constskcnt(0);
//=========================================================
//	lock::Scheduler
//=========================================================

namespace lock{

typedef std::vector<TaskPtrT>	TaskVectorT;

class Scheduler;

class Worker: public Thread{
	friend class 	Scheduler;
	Scheduler		&rsched;
	TaskVectorT		tskvec[2];
	Condition		cnd;
	size_t			crtpushtskvecidx;
	
	
	Worker(Scheduler &_rsched):rsched(_rsched), crtpushtskvecidx(0){}
	
	TaskVectorT* waitTasks();
	void run();
	void schedule(TaskPtrT &_rtskptr);
};


typedef std::unique_ptr<Worker>		WorkerPtrT;
typedef std::vector<WorkerPtrT>		WorkerVectorT;

class Scheduler: public ::Scheduler{
	enum Status{
		StatusStoppedE = 0,
		StatusRunningE,
		StatusStoppingE,
		StatusStoppingWaitE
	};
	friend class Worker;
	Mutex			mtx;
	Condition		cnd;
	WorkerVectorT	wkrvec;
	Status			status;
	size_t			stopwaitcnt;
	size_t			crtwkridx;
	
	~Scheduler();
	virtual bool start(const size_t _conscnt);
	virtual void schedule(TaskPtrT &_rtskptr);
	virtual void stop(bool _wait);
public:
	Scheduler():status(StatusStoppedE), stopwaitcnt(0), crtwkridx(0){}
};

Scheduler::~Scheduler(){
	
}
/*virtual*/ bool Scheduler::start(const size_t _conscnt){
	bool			err = false;
	{
		Locker<Mutex>	lock(mtx);
		
		if(status == StatusRunningE){
			idbg("status == StatusRunningE");
			return true;
		}else if(status != StatusStoppedE || stopwaitcnt){
			idbg("status != StatusStoppedE and stopwaitcnt = "<<stopwaitcnt);
			do{
				cnd.wait(lock);
			}while(status != StatusStoppedE || stopwaitcnt);
		}
		
		for(size_t i = 0; i < _conscnt; ++i){
			WorkerPtrT	wkrptr(new Worker(*this));
			if(wkrptr->start(false, false)){
				wkrvec.push_back(std::move(wkrptr));
			}else{
				err = true;
				break;
			}
		}
		
		if(err){
			edbg("status = StatusStoppingWaitE");
			status = StatusStoppingWaitE;
		}
	}
	if(err){
		edbg("before join all");
		for(auto it = wkrvec.begin(); it != wkrvec.end(); ++it){
			(*it)->join();
		}
		edbg("after join all");
		Locker<Mutex>	lock(mtx);
		status = StatusStoppedE;
		cnd.broadcast();
		idbg("status = StatusStoppedE");
		return false;
	}
	status = StatusRunningE;
	idbg("status = StatusRunningE");
	return true;
}
/*virtual*/ void Scheduler::schedule(TaskPtrT &_rtskptr){
	Locker<Mutex>	lock(mtx);
	if(status == StatusRunningE){
		wkrvec[crtwkridx]->schedule(_rtskptr);
		crtwkridx = (crtwkridx + 1) % wkrvec.size();
	}
}
/*virtual*/ void Scheduler::stop(bool _wait){
	{
		Locker<Mutex>	lock(mtx);
		if(status == StatusRunningE){
			idbg("status == StatusRunningE");
			status = _wait ? StatusStoppingWaitE : StatusStoppingE;
			for(auto it = wkrvec.begin(); it != wkrvec.end(); ++it){
				(*it)->cnd.signal();
			}
		}else if(status == StatusStoppingE){
			idbg("status == StatusStoppingE");
			status = _wait ? StatusStoppingWaitE : StatusStoppingE;
		}else if(status == StatusStoppingWaitE){
			idbg("status == StatusStoppingWaitE and _wait = "<<_wait);
			if(_wait){
				++stopwaitcnt;
				do{
					cnd.wait(lock);
				}while(status != StatusStoppedE);
				--stopwaitcnt;
				cnd.signal();
			}
			return;
		}else if(status == StatusStoppedE){
			idbg("status == StatusStoppedE");
			return;
		}
	}
	if(_wait){
		for(auto it = wkrvec.begin(); it != wkrvec.end(); ++it){
			(*it)->join();
		}
		Locker<Mutex>	lock(mtx);
		status = StatusStoppedE;
		idbg("status = StatusStoppedE");
		cnd.broadcast();
	}
}

//---------------------------------------------------------
TaskVectorT* Worker::waitTasks(){
	Locker<Mutex>	lock(rsched.mtx);
	if(rsched.status <= Scheduler::StatusRunningE){
		if(tskvec[crtpushtskvecidx].empty()){
			do{
				cnd.wait(lock);
			}while(tskvec[crtpushtskvecidx].empty() && rsched.status <= Scheduler::StatusRunningE);
		}
	}
	if(tskvec[crtpushtskvecidx].size()){
		TaskVectorT *pvec = &tskvec[crtpushtskvecidx];
		crtpushtskvecidx = (crtpushtskvecidx + 1) % 2;
		return pvec;
	}
	return NULL;
}
void Worker::schedule(TaskPtrT &_rtskptr){
	tskvec[crtpushtskvecidx].push_back(std::move(_rtskptr));
	if(tskvec[crtpushtskvecidx].size() == 1){
		cnd.signal();
	}
}
void Worker::run(){
	vdbg("Worker enter");
	Context		ctx;
	TaskVectorT	*ptv = NULL;
	size_t		maxtsks = 0;
	size_t		conscnt = 0;
	while((ptv = waitTasks())){
		for(auto it = ptv->begin(); it != ptv->end(); ++it){
			(*it)->run(ctx);
			it->reset();
		}
		if(ptv->size() > maxtsks){
			maxtsks = ptv->size();
		}
		conscnt += ptv->size();
		constskcnt += ptv->size();
		ptv->clear();
	}
	vdbg("Worker exit - maxtsks = "<<maxtsks<<" conscnt = "<<conscnt);
}

}//namespace lock


//=========================================================
//	waitfree::Scheduler
//=========================================================
namespace waitfree{

typedef std::vector<TaskPtrT>	TaskVectorT;

class Scheduler;

class Worker: public Thread{
	friend class 	Scheduler;
	Scheduler		&rsched;
	TaskVectorT		tskvec[2];
	Condition		cnd;
	size_t			crtpushtskvecidx;
	
	
	Worker(Scheduler &_rsched):rsched(_rsched), crtpushtskvecidx(0){
		tskvec[0].reserve(1024*4);
		tskvec[1].reserve(1024*4);
	}
	
	TaskVectorT* waitTasks();
	void run();
	void schedule(TaskPtrT &_rtskptr);
};


typedef std::unique_ptr<Worker>		WorkerPtrT;
typedef std::vector<WorkerPtrT>		WorkerVectorT;

class Scheduler: public ::Scheduler{
	enum Status{
		StatusStoppedE = 0,
		StatusRunningE,
		StatusStoppingE,
		StatusStoppingWaitE
	};
	friend class Worker;
	Mutex			mtx;
	Condition		cnd;
	WorkerVectorT	wkrvec;
	Status			status;
	size_t			stopwaitcnt;
	size_t			crtwkridx;
	
	~Scheduler();
	virtual bool start(const size_t _conscnt);
	virtual void schedule(TaskPtrT &_rtskptr);
	virtual void stop(bool _wait);
public:
	Scheduler():status(StatusStoppedE), stopwaitcnt(0), crtwkridx(0){}
};

Scheduler::~Scheduler(){
	
}
/*virtual*/ bool Scheduler::start(const size_t _conscnt){
	bool			err = false;
	{
		Locker<Mutex>	lock(mtx);
		
		if(status == StatusRunningE){
			idbg("status == StatusRunningE");
			return true;
		}else if(status != StatusStoppedE || stopwaitcnt){
			idbg("status != StatusStoppedE and stopwaitcnt = "<<stopwaitcnt);
			do{
				cnd.wait(lock);
			}while(status != StatusStoppedE || stopwaitcnt);
		}
		
		for(size_t i = 0; i < _conscnt; ++i){
			WorkerPtrT	wkrptr(new Worker(*this));
			if(wkrptr->start(false, false)){
				wkrvec.push_back(std::move(wkrptr));
			}else{
				err = true;
				break;
			}
		}
		
		if(err){
			edbg("status = StatusStoppingWaitE");
			status = StatusStoppingWaitE;
		}
	}
	if(err){
		edbg("before join all");
		for(auto it = wkrvec.begin(); it != wkrvec.end(); ++it){
			(*it)->join();
		}
		edbg("after join all");
		Locker<Mutex>	lock(mtx);
		status = StatusStoppedE;
		cnd.broadcast();
		idbg("status = StatusStoppedE");
		return false;
	}
	status = StatusRunningE;
	idbg("status = StatusRunningE");
	return true;
}
/*virtual*/ void Scheduler::schedule(TaskPtrT &_rtskptr){
	Locker<Mutex>	lock(mtx);
	if(status == StatusRunningE){
		wkrvec[crtwkridx]->schedule(_rtskptr);
		crtwkridx = (crtwkridx + 1) % wkrvec.size();
	}
}
/*virtual*/ void Scheduler::stop(bool _wait){
	{
		Locker<Mutex>	lock(mtx);
		if(status == StatusRunningE){
			idbg("status == StatusRunningE");
			status = _wait ? StatusStoppingWaitE : StatusStoppingE;
			for(auto it = wkrvec.begin(); it != wkrvec.end(); ++it){
				(*it)->cnd.signal();
			}
		}else if(status == StatusStoppingE){
			idbg("status == StatusStoppingE");
			status = _wait ? StatusStoppingWaitE : StatusStoppingE;
		}else if(status == StatusStoppingWaitE){
			idbg("status == StatusStoppingWaitE and _wait = "<<_wait);
			if(_wait){
				++stopwaitcnt;
				do{
					cnd.wait(lock);
				}while(status != StatusStoppedE);
				--stopwaitcnt;
				cnd.signal();
			}
			return;
		}else if(status == StatusStoppedE){
			idbg("status == StatusStoppedE");
			return;
		}
	}
	if(_wait){
		for(auto it = wkrvec.begin(); it != wkrvec.end(); ++it){
			(*it)->join();
		}
		Locker<Mutex>	lock(mtx);
		status = StatusStoppedE;
		idbg("status = StatusStoppedE");
		cnd.broadcast();
	}
}

//---------------------------------------------------------
TaskVectorT* Worker::waitTasks(){
	Locker<Mutex>	lock(rsched.mtx);
	if(rsched.status <= Scheduler::StatusRunningE){
		if(tskvec[crtpushtskvecidx].empty()){
			do{
				cnd.wait(lock);
			}while(tskvec[crtpushtskvecidx].empty() && rsched.status <= Scheduler::StatusRunningE);
		}
	}
	if(tskvec[crtpushtskvecidx].size()){
		TaskVectorT *pvec = &tskvec[crtpushtskvecidx];
		crtpushtskvecidx = (crtpushtskvecidx + 1) % 2;
		return pvec;
	}
	return NULL;
}
void Worker::schedule(TaskPtrT &_rtskptr){
	tskvec[crtpushtskvecidx].push_back(std::move(_rtskptr));
	if(tskvec[crtpushtskvecidx].size() == 1){
		cnd.signal();
	}
}
void Worker::run(){
	vdbg("Worker enter");
	Context		ctx;
	TaskVectorT	*ptv = NULL;
	size_t		maxtsks = 0;
	size_t		conscnt = 0;
	while((ptv = waitTasks())){
		for(auto it = ptv->begin(); it != ptv->end(); ++it){
			(*it)->run(ctx);
			it->reset();
		}
		if(ptv->size() > maxtsks){
			maxtsks = ptv->size();
		}
		conscnt += ptv->size();
		constskcnt += ptv->size();
		ptv->clear();
	}
	vdbg("Worker exit - maxtsks = "<<maxtsks<<" conscnt = "<<conscnt);
}

}//namespace waitfree


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
				TaskPtrT		tskptr(new IntTask(i));
				rsched.schedule(tskptr);
			}else{
				size_t stridx = (i/2) % (sizeof(strs)/sizeof(const char*));
				TaskPtrT		tskptr(new StringTask(strs[stridx]));
				rsched.schedule(tskptr);
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
	SchedulerWaitFreeE,
};

Scheduler * schedulerFactory(const SchedulerType _tp){
	switch(_tp){
		case SchedulerLockE:
			return new lock::Scheduler;
		case SchedulerWaitFreeE:
			return new waitfree::Scheduler;
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
	
	const size_t		prodtaskcnt = 1000 * 1000;
	const SchedulerType	schedtype = SchedulerWaitFreeE;
	
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
	
	sched.stop();
	
	idbg("produced "<<prodtskcnt<<" tasks");
	idbg("consumed "<<constskcnt<<" tasks");
	cout<<"produced "<<prodtskcnt<<" tasks"<<endl;
	cout<<"consumed "<<constskcnt<<" tasks"<<endl;
	Thread::waitAll();
	
	return 0;
}

