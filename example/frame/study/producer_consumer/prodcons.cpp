#include <iostream>
#include <memory>
#include <vector>
#include <queue>
#include <array>
#include "system/thread.hpp"
#include "system/debug.hpp"
#include "system/atomic.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/timespec.hpp"
#include "findmin.hpp"

#include <boost/lockfree/queue.hpp>

using namespace std;
using namespace solid;

struct Context{
	Context():strcnt(0), intcnt(0), intv(0), strsz(0){
		ct = TimeSpec::createMonotonic();
	}
	TimeSpec	ct;
	size_t		strcnt;
	size_t		intcnt;
	size_t		intv;
	size_t		strsz;
};

class Task{
public:
	Task(size_t _loopcnt): loopcnt(_loopcnt){}
	virtual ~Task(){}
	virtual bool run(Context &) = 0;
	bool decrement(){
		--loopcnt;
		return loopcnt != 0;
	}
protected:
	size_t		loopcnt;
};

class StringTask: public Task{
	std::string str;
	virtual bool run(Context &_rctx){
		++_rctx.strcnt;
		_rctx.strsz += str.size();
		return decrement();
	}
public:
	StringTask(size_t _loopcnt, const std::string &_str):Task(_loopcnt), str(_str){}
};

class IntTask: public Task{
	int v;
	virtual bool run(Context &_rctx){
		++_rctx.strcnt;
		_rctx.intv += v;
		return decrement();
	}
public:
	IntTask(size_t _loopcnt, int _v = 0):Task(_loopcnt), v(_v){}
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

typedef ATOMIC_NS::atomic<size_t>	AtomicSizeT;
typedef ATOMIC_NS::atomic<bool>		AtomicBoolT;

AtomicSizeT		constskcnt(0);
AtomicSizeT		consloopcnt(0);

//=========================================================
//	lock::Scheduler
//=========================================================

namespace baseline{

typedef std::vector<TaskPtrT>	TaskVectorT;
typedef std::queue<TaskPtrT>	TaskQueueT;

class Scheduler;

class Worker: public Thread{
	friend class 	Scheduler;
	Scheduler		&rsched;
	TaskVectorT		tskvec[2];
	Condition		cnd;
	size_t			crtpushtskvecidx;
	
	
	Worker(Scheduler &_rsched):rsched(_rsched), crtpushtskvecidx(0){}
	
	TaskVectorT* waitTasks(bool _peek, bool &_isrunning);
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
	size_t	computeScheduleWorkerIndex();
	
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
inline size_t	Scheduler::computeScheduleWorkerIndex(){
	size_t cwi = crtwkridx;
	crtwkridx = (cwi + 1) % wkrvec.size();
	return cwi;
}

/*virtual*/ void Scheduler::schedule(TaskPtrT &_rtskptr){
	Locker<Mutex>	lock(mtx);
	if(status == StatusRunningE){
		wkrvec[computeScheduleWorkerIndex()]->schedule(_rtskptr);
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
TaskVectorT* Worker::waitTasks(bool _peek, bool &_risrunning){
	Locker<Mutex>	lock(rsched.mtx);
	if(rsched.status <= Scheduler::StatusRunningE){
		if(tskvec[crtpushtskvecidx].empty()){
			if(_peek){
				return NULL;
			}else{
				do{
					cnd.wait(lock);
				}while(tskvec[crtpushtskvecidx].empty() && rsched.status <= Scheduler::StatusRunningE);
			}
		}
	}else{
		_risrunning = false;
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
	size_t		maxqtsks = 0;
	size_t		conscnt = 0;
	size_t		loopcnt = 0;
	TaskQueueT	tskq;
	
	bool		isrunning = true;
	
	do{
		size_t	tskqsz = tskq.size();
		
		if(tskqsz > maxqtsks){
			maxqtsks = tskqsz;
		}
		loopcnt += tskqsz;
		consloopcnt += tskqsz;
		
		while(tskqsz--){
			if(tskq.front()->run(ctx)){
				tskq.push(std::move(tskq.front()));
				tskq.pop();
			}else{
				tskq.pop();
			}
		}
		ptv = waitTasks(!tskq.empty(), isrunning);
		if(ptv){
			if(ptv->size() > maxtsks){
				maxtsks = ptv->size();
			}
			conscnt += ptv->size();
			loopcnt += ptv->size();
			constskcnt += ptv->size();
			
			for(auto it = ptv->begin(); it != ptv->end(); ++it){
				tskq.push(std::move(*it));
			}
			ptv->clear();
		}
		ctx.ct.currentMonotonic();
	}while(tskq.size() || isrunning);
	
	vdbg("Worker exit - maxtsks = "<<maxtsks<<" maxqtsks = "<<maxqtsks<<" conscnt = "<<conscnt<<" loopcnt = "<<loopcnt);
	cout<<"Worker exit - maxtsks = "<<maxtsks<<" maxqtsks = "<<maxqtsks<<" conscnt = "<<conscnt<<" loopcnt = "<<loopcnt<<endl;
}

}//namespace baseline



//=========================================================
//	optim1::Scheduler
//=========================================================
namespace optim1{


template <class C>
size_t find_min(const std::array<C, 2> &_ra, size_t &_rcrtidx){
	if(_ra[0] < _ra[1]){
		return 0;
	}else if(_ra[0] > _ra[1]){
		return 1;
	}
	const size_t idx = _rcrtidx;
	_rcrtidx = (idx + 1) % 2;
	return idx;
}

template <class C>
size_t find_min(const std::array<C, 3> &_ra, size_t &_rcrtidx){
	size_t	crtidx;
	C		crtmin;
	
	if(_ra[0] < _ra[1]){
		crtidx = 0;
		crtmin = _ra[0];
	}else if(_ra[0] > _ra[1]){
		crtidx = 1;
		crtmin = _ra[1];
	}else if(_ra[1] == _ra[2]){
		const size_t idx = _rcrtidx;
		_rcrtidx = (idx + 1) % 3;
		return idx;
	}else{
		crtidx = 0;
		crtmin = _ra[0];
	}
	
	if(crtmin < _ra[2]){
		return crtidx;
	}else{
		return 2;
	}
	
	
}
template <class C>
size_t find_min(const std::array<C, 4> &_ra, size_t &_rcrtidx){
#if 1
	size_t	crtidx;
	C		crtmin;
	//size_t	eqlcnt = 0;

	if(_ra[0] == _ra[1] && _ra[1] == _ra[2] && _ra[2] == _ra[3]){
		crtidx = _rcrtidx;
		_rcrtidx = (crtidx + 1) % 4;
	}else{
		if(_ra[0] <= _ra[1]){
			crtmin = _ra[0];
			crtidx = 0;
		}else{
			crtmin = _ra[1];
			crtidx = 1;
		}
		if(_ra[2] < crtmin){
			crtmin = _ra[2];
			crtidx = 2;
		}
		if(_ra[3] < crtmin){
			crtidx = 3;
		}
	}
	return crtidx;
#else
	size_t	crtidx;
	C		crtmin;
	size_t	eqlcnt = 0;
	
	if(_ra[0] < _ra[1]){
		crtidx = 0;
		crtmin = _ra[0];
	}else if(_ra[0] > _ra[1]){
		crtidx = 1;
		crtmin = _ra[1];
	}else{//==
		crtidx = 0;
		crtmin = _ra[0];
		eqlcnt = 2;
	}
	
	if(crtmin < _ra[2]){
		
	}else if(crtmin > _ra[2]){
		crtmin = _ra[2];
		crtidx = 2;
	}else{//==
		++eqlcnt;
	}
	
	if(crtmin < _ra[3]){
		return crtidx;
	}else if(crtmin > _ra[3]){
		return 3;
	}else if(eqlcnt < 3){//==
		return crtidx;
	}
		
	crtidx = _rcrtidx;
	_rcrtidx = (crtidx + 1) % 4;
	return crtidx;
#endif
}
template <class C>
size_t find_min(const std::array<C, 5> &_ra, size_t &_rcrtidx){
	const size_t idx = _rcrtidx;
	_rcrtidx = (idx + 1) % 5;
	return idx;
}
template <class C>
size_t find_min(const std::array<C, 6> &_ra, size_t &_rcrtidx){
	const size_t idx = _rcrtidx;
	_rcrtidx = (idx + 1) % 6;
	return idx;
}
template <class C>
size_t find_min(const std::array<C, 7> &_ra, size_t &_rcrtidx){
	const size_t idx = _rcrtidx;
	_rcrtidx = (idx + 1) % 7;
	return idx;
}
template <class C>
size_t find_min(const std::array<C, 8> &_ra, size_t &_rcrtidx){
	const size_t idx = _rcrtidx;
	_rcrtidx = (idx + 1) % 8;
	return idx;
}

typedef std::vector<TaskPtrT>	TaskVectorT;
typedef std::queue<TaskPtrT>	TaskQueueT;

class Scheduler;

class Worker: public Thread{
	friend class 	Scheduler;
	Scheduler		&rsched;
	TaskVectorT		tskvec[2];
	Condition		cnd;
	size_t			crtpushtskvecidx;
	AtomicSizeT		crtpushsz;
	AtomicSizeT		load;
	
	
	Worker(Scheduler &_rsched):rsched(_rsched), crtpushtskvecidx(0), crtpushsz(0), load(0){}
	
	TaskVectorT* waitTasks(bool _peek, bool &_isrunning);
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
	size_t	computeScheduleWorkerIndex();
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

inline size_t	Scheduler::computeScheduleWorkerIndex(){
	switch(wkrvec.size()){
		case 1:
			return 0;
		case 2:{
			const std::array<size_t, 2> arr = {
				wkrvec[0]->load, wkrvec[1]->load
			};
			return find_min(arr, crtwkridx);
		}
		case 3:{
			const std::array<size_t, 3> arr = {
				wkrvec[0]->load, wkrvec[1]->load, wkrvec[2]->load
			};
			return find_min(arr, crtwkridx);
		}
		case 4:{
			const std::array<size_t, 4> arr = {
				wkrvec[0]->load, wkrvec[1]->load, wkrvec[2]->load, wkrvec[3]->load
			};
			return find_min(arr, crtwkridx);
		}
		case 5:{
			const std::array<size_t, 5> arr = {
				wkrvec[0]->load, wkrvec[1]->load, wkrvec[2]->load, wkrvec[3]->load,
				wkrvec[4]->load
			};
			return find_min(arr, crtwkridx);
		}
		case 6:{
			const std::array<size_t, 6> arr = {
				wkrvec[0]->load, wkrvec[1]->load, wkrvec[2]->load, wkrvec[3]->load,
				wkrvec[4]->load, wkrvec[5]->load
			};
			return find_min(arr, crtwkridx);
		}
		case 7:{
			const std::array<size_t, 7> arr = {
				wkrvec[0]->load, wkrvec[1]->load, wkrvec[2]->load, wkrvec[3]->load,
				wkrvec[4]->load, wkrvec[5]->load, wkrvec[6]->load
			};
			return find_min(arr, crtwkridx);
		}
		case 8:{
			const std::array<size_t, 8> arr = {
				wkrvec[0]->load, wkrvec[1]->load, wkrvec[2]->load, wkrvec[3]->load,
				wkrvec[4]->load, wkrvec[5]->load, wkrvec[6]->load, wkrvec[7]->load
			};
			return find_min(arr, crtwkridx);
		}
		default:
			break;
	}
	
	const size_t	cwi = crtwkridx;
	crtwkridx = (cwi + 1) % wkrvec.size();
	return cwi;
}

/*virtual*/ void Scheduler::schedule(TaskPtrT &_rtskptr){
	Locker<Mutex>	lock(mtx);
	if(status == StatusRunningE){
		wkrvec[computeScheduleWorkerIndex()]->schedule(_rtskptr);
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
TaskVectorT* Worker::waitTasks(bool _peek, bool &_risrunning){
	if(_peek && crtpushsz == 0){
		return NULL;
	}
	Locker<Mutex>	lock(rsched.mtx);
	crtpushsz = 0;
	if(rsched.status <= Scheduler::StatusRunningE){
		if(tskvec[crtpushtskvecidx].empty()){
			if(_peek){
				return NULL;
			}else{
				do{
					cnd.wait(lock);
				}while(tskvec[crtpushtskvecidx].empty() && rsched.status <= Scheduler::StatusRunningE);
			}
		}
	}else{
		_risrunning = false;
	}
	if(tskvec[crtpushtskvecidx].size()){
		TaskVectorT *pvec = &tskvec[crtpushtskvecidx];
		crtpushtskvecidx = (crtpushtskvecidx + 1) % 2;
		return pvec;
	}
	return NULL;
}
void Worker::schedule(TaskPtrT &_rtskptr){
	++crtpushsz;
	++load;
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
	size_t		maxqtsks = 0;
	size_t		conscnt = 0;
	size_t		loopcnt = 0;
	TaskQueueT	tskq;
	
	bool		isrunning = true;
	
	size_t		opcnt = 0;
	
	do{
		const size_t	tskqsz = tskq.size();
		if(tskqsz > maxqtsks){
			maxqtsks = tskqsz;
		}
		loopcnt += tskqsz;
		consloopcnt += tskqsz;
		opcnt += tskqsz;
		
		size_t			qcnt = tskqsz;
		while(qcnt--){
			if(tskq.front()->run(ctx)){
				tskq.push(std::move(tskq.front()));
				tskq.pop();
			}else{
				tskq.pop();
 				--load;
			}
		}
		//load -= (tskqsz - tskq.size());
		ptv = waitTasks(!tskq.empty(), isrunning);
		if(ptv){
			if(ptv->size() > maxtsks){
				maxtsks = ptv->size();
			}
			conscnt += ptv->size();
			loopcnt += ptv->size();
			constskcnt += ptv->size();
			
			for(auto it = ptv->begin(); it != ptv->end(); ++it){
				tskq.push(std::move(*it));
			}
			ptv->clear();
		}
		ctx.ct.currentMonotonic();
		load = tskq.size();
	}while(tskq.size() || isrunning);
	
	vdbg("Worker exit - maxtsks = "<<maxtsks<<" maxqtsks = "<<maxqtsks<<" conscnt = "<<conscnt<<" loopcnt = "<<loopcnt);
	cout<<"Worker exit - maxtsks = "<<maxtsks<<" maxqtsks = "<<maxqtsks<<" conscnt = "<<conscnt<<" loopcnt = "<<loopcnt<<endl;
}

}//namespace optim1



//=========================================================
//	optim2::Scheduler
//=========================================================
namespace optim2{



typedef std::vector<TaskPtrT>	TaskVectorT;
typedef std::queue<TaskPtrT>	TaskQueueT;

class Scheduler;

class Worker: public Thread{
	friend class 	Scheduler;
	Scheduler		&rsched;
	TaskVectorT		tskvec[2];
	Mutex			mtx;
	Condition		cnd;
	size_t			crtpushtskvecidx;
	AtomicSizeT		crtpushsz;
	AtomicSizeT		load;
	AtomicBoolT		isrunning;
	
	
	Worker(Scheduler &_rsched):rsched(_rsched), crtpushtskvecidx(0), crtpushsz(0), load(0), isrunning(true){}
	
	TaskVectorT* waitTasks(const bool _peek);
	void run();
	void schedule(TaskPtrT &_rtskptr);
	void stop();
	
public:
	size_t currentLoad()const{
		return load;
	}
};


typedef std::unique_ptr<Worker>		WorkerPtrT;
typedef std::vector<WorkerPtrT>		WorkerVectorT;
typedef std::atomic<WorkerVectorT*>	AtomicWorkerVectorPtrT;

class Scheduler: public ::Scheduler{
	enum Status{
		StatusStoppedE = 0,
		StatusRunningE,
		StatusStoppingE,
		StatusStoppingWaitE
	};
	friend class Worker;
	Mutex					mtx;
	Condition				cnd;
	AtomicWorkerVectorPtrT	pwrkvec;
	WorkerVectorT			wkrvec;
	Status					status;
	size_t					stopwaitcnt;
	size_t					crtwkridx;
	AtomicSizeT				usecnt;
	AtomicSizeT				eqlcount;
	
	~Scheduler();
	virtual bool start(const size_t _conscnt);
	virtual void schedule(TaskPtrT &_rtskptr);
	virtual void stop(bool _wait);
	size_t	computeScheduleWorkerIndex();
public:
	Scheduler():pwrkvec(nullptr), status(StatusStoppedE), stopwaitcnt(0), crtwkridx(0), usecnt(0), eqlcount(0){}
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
	pwrkvec.store(&wkrvec);
	status = StatusRunningE;
	idbg("status = StatusRunningE");
	return true;
}

inline bool wkr_less_cmp(WorkerPtrT const &_rwkr1, WorkerPtrT const &_rwkr2){
	return _rwkr1->currentLoad() < _rwkr2->currentLoad();
}

inline size_t	Scheduler::computeScheduleWorkerIndex(){
	switch(wkrvec.size()){
		case 1:
			return 0;
		case 2:{
			return find_min_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<2>());
		}
		case 3:{
			return find_min_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<3>());
		}
		case 4:{
			return find_min_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<4>());
		}
		case 5:{
			return find_min_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<5>());
		}
		case 6:{
			return find_min_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<6>());
		}
		case 7:{
			return find_min_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<7>());
		}
		case 8:{
			return find_min_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<8>());
		}
		default:
			break;
	}
	
	++eqlcount;
	const size_t	cwi = crtwkridx;
	crtwkridx = (cwi + 1) % wkrvec.size();
	return cwi;
}

/*virtual*/ void Scheduler::schedule(TaskPtrT &_rtskptr){
	++usecnt;
	WorkerVectorT	*pwv = pwrkvec.load();
	if(pwv){
		(*pwv)[computeScheduleWorkerIndex()]->schedule(_rtskptr);
	}
	--usecnt;
}
/*virtual*/ void Scheduler::stop(bool _wait){
	{
		Locker<Mutex>	lock(mtx);
		if(status == StatusRunningE){
			idbg("status == StatusRunningE");
			status = _wait ? StatusStoppingWaitE : StatusStoppingE;
			pwrkvec.store(nullptr);
			for(auto it = wkrvec.begin(); it != wkrvec.end(); ++it){
				(*it)->stop();
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
		while(usecnt){
			Thread::yield();
		}
		for(auto it = wkrvec.begin(); it != wkrvec.end(); ++it){
			(*it)->join();
		}
		Locker<Mutex>	lock(mtx);
		status = StatusStoppedE;
		idbg("status = StatusStoppedE");
		cnd.broadcast();
		cout<<"Equal count "<<eqlcount<<endl;
	}
}

//---------------------------------------------------------
void Worker::stop(){
	cnd.signal();
	isrunning = false;
}
//---------------------------------------------------------
TaskVectorT* Worker::waitTasks(const bool _peek){
	static TaskVectorT emptyvec;
	if(_peek && crtpushsz == 0){
		return isrunning ? &emptyvec : nullptr;
	}
	crtpushsz = 0;
	
	Locker<Mutex>	lock(mtx);
	
	if(tskvec[crtpushtskvecidx].empty()){
		if(_peek){
			return isrunning ? &emptyvec : nullptr;
		}else{
			while(tskvec[crtpushtskvecidx].empty() && isrunning){
				cnd.wait(lock);
			}
		}
	}
	
	if(tskvec[crtpushtskvecidx].size()){
		TaskVectorT *pvec = &tskvec[crtpushtskvecidx];
		crtpushtskvecidx = (crtpushtskvecidx + 1) % 2;
		return pvec;
	}
	return nullptr;
}
//---------------------------------------------------------
void Worker::schedule(TaskPtrT &_rtskptr){
	++crtpushsz;
	++load;

	Locker<Mutex>	lock(mtx);
	tskvec[crtpushtskvecidx].push_back(std::move(_rtskptr));
	if(tskvec[crtpushtskvecidx].size() == 1){
		cnd.signal();
	}
}
//---------------------------------------------------------
void Worker::run(){
	vdbg("Worker enter");
	Context		ctx;
	TaskVectorT	*ptv = NULL;
	size_t		maxtsks = 0;
	size_t		maxqtsks = 0;
	size_t		conscnt = 0;
	size_t		loopcnt = 0;
	TaskQueueT	tskq;
	
	size_t		opcnt = 0;
	
	do{
		const size_t	tskqsz = tskq.size();
		if(tskqsz > maxqtsks){
			maxqtsks = tskqsz;
		}
		loopcnt += tskqsz;
		consloopcnt += tskqsz;
		opcnt += tskqsz;
		
		size_t			qcnt = tskqsz;
		while(qcnt--){
			if(tskq.front()->run(ctx)){
				tskq.push(std::move(tskq.front()));
				tskq.pop();
			}else{
				tskq.pop();
 				--load;
			}
		}
		//load -= (tskqsz - tskq.size());
		ptv = waitTasks(!tskq.empty());
		if(ptv){
			if(ptv->size() > maxtsks){
				maxtsks = ptv->size();
			}
			conscnt += ptv->size();
			loopcnt += ptv->size();
			constskcnt += ptv->size();
			
			for(auto it = ptv->begin(); it != ptv->end(); ++it){
				tskq.push(std::move(*it));
			}
			ptv->clear();
		}
		ctx.ct.currentMonotonic();
		load = tskq.size();
	}while(tskq.size() || ptv);
	
	vdbg("Worker exit - maxtsks = "<<maxtsks<<" maxqtsks = "<<maxqtsks<<" conscnt = "<<conscnt<<" loopcnt = "<<loopcnt);
	cout<<"Worker exit - maxtsks = "<<maxtsks<<" maxqtsks = "<<maxqtsks<<" conscnt = "<<conscnt<<" loopcnt = "<<loopcnt<<endl;
}

}//namespace optim2


//=========================================================
//	optim3::Scheduler
//=========================================================
namespace optim3{

typedef std::vector<TaskPtrT>				TaskVectorT;
typedef std::queue<TaskPtrT>				TaskQueueT;
typedef boost::lockfree::queue<Task*>		TaskLockFreeQueueT;

class Scheduler;

class Worker: public Thread{
	friend class 	Scheduler;
	Scheduler			&rsched;
	TaskLockFreeQueueT	pushq;
	Mutex				mtx;
	Condition			cnd;
	size_t				crtpushtskvecidx;
	AtomicSizeT			crtpushsz;
	AtomicSizeT			load;
	AtomicBoolT			isrunning;
	
	
	Worker(Scheduler &_rsched):rsched(_rsched), pushq(128), crtpushtskvecidx(0), crtpushsz(0), load(0), isrunning(true){}
	
	size_t waitTasks(const bool _peek);
	void run();
	void schedule(TaskPtrT &_rtskptr);
	void stop();
	
public:
	size_t currentLoad()const{
		return load;
	}
};


typedef std::unique_ptr<Worker>		WorkerPtrT;
typedef std::vector<WorkerPtrT>		WorkerVectorT;
typedef std::atomic<WorkerVectorT*>	AtomicWorkerVectorPtrT;

class Scheduler: public ::Scheduler{
	enum Status{
		StatusStoppedE = 0,
		StatusRunningE,
		StatusStoppingE,
		StatusStoppingWaitE
	};
	friend class Worker;
	Mutex					mtx;
	Condition				cnd;
	AtomicWorkerVectorPtrT	pwrkvec;
	WorkerVectorT			wkrvec;
	Status					status;
	size_t					stopwaitcnt;
	size_t					crtwkridx;
	AtomicSizeT				usecnt;
	AtomicSizeT				eqlcount;
	
	~Scheduler();
	virtual bool start(const size_t _conscnt);
	virtual void schedule(TaskPtrT &_rtskptr);
	virtual void stop(bool _wait);
	size_t	computeScheduleWorkerIndex();
public:
	Scheduler():pwrkvec(nullptr), status(StatusStoppedE), stopwaitcnt(0), crtwkridx(0), usecnt(0), eqlcount(0){}
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
	pwrkvec.store(&wkrvec);
	status = StatusRunningE;
	idbg("status = StatusRunningE");
	return true;
}

inline bool wkr_less_cmp(WorkerPtrT const &_rwkr1, WorkerPtrT const &_rwkr2){
	return _rwkr1->currentLoad() < _rwkr2->currentLoad();
}

inline size_t	Scheduler::computeScheduleWorkerIndex(){
	FindRetValT rv(0, true);
	switch(wkrvec.size()){
		case 1:
			return 0;
		case 2:{
			rv = find_min_or_equal_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<2>());
			break;
		}
		case 3:{
			rv = find_min_or_equal_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<3>());
			break;
		}
		case 4:{
			rv = find_min_or_equal_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<4>());
			break;
		}
		case 5:{
			rv = find_min_or_equal_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<5>());
			break;
		}
		case 6:{
			rv = find_min_or_equal_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<6>());
			break;
		}
		case 7:{
			rv = find_min_or_equal_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<7>());
			break;
		}
		case 8:{
			rv = find_min_or_equal_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<8>());
			break;
		}
		default:
			break;
	}
	
	if(!rv.second){
		return rv.first;
	}
	
	++eqlcount;
	const size_t	cwi = crtwkridx;
	crtwkridx = (cwi + 1) % wkrvec.size();
	return cwi;
}

/*virtual*/ void Scheduler::schedule(TaskPtrT &_rtskptr){
	++usecnt;
	WorkerVectorT	*pwv = pwrkvec.load();
	if(pwv){
		(*pwv)[computeScheduleWorkerIndex()]->schedule(_rtskptr);
	}
	--usecnt;
}
/*virtual*/ void Scheduler::stop(bool _wait){
	{
		Locker<Mutex>	lock(mtx);
		if(status == StatusRunningE){
			idbg("status == StatusRunningE");
			status = _wait ? StatusStoppingWaitE : StatusStoppingE;
			pwrkvec.store(nullptr);
			for(auto it = wkrvec.begin(); it != wkrvec.end(); ++it){
				(*it)->stop();
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
		while(usecnt){
			Thread::yield();
		}
		for(auto it = wkrvec.begin(); it != wkrvec.end(); ++it){
			(*it)->join();
		}
		Locker<Mutex>	lock(mtx);
		status = StatusStoppedE;
		idbg("status = StatusStoppedE");
		cnd.broadcast();
		cout<<"Equal count "<<eqlcount<<endl;
	}
}

//---------------------------------------------------------
void Worker::stop(){
	cnd.signal();
	isrunning = false;
}
//---------------------------------------------------------
size_t Worker::waitTasks(const bool _peek){
	if(crtpushsz == 0){
		if(_peek){
			return isrunning ? 0 : -1;
		}else{
			Locker<Mutex>	lock(mtx);
			while(crtpushsz == 0 && isrunning){
				cnd.wait(lock);
			}
		}
	}
	
	size_t	rv;
	rv = crtpushsz.fetch_and(0);
	return rv ? rv : isrunning ? 0 : -1;
}
//---------------------------------------------------------
void Worker::schedule(TaskPtrT &_rtskptr){
	const size_t cpsz =  crtpushsz.fetch_add(1);
	++load;
	pushq.push(_rtskptr.release());
	if(cpsz == 0){
		cnd.signal();
	}
}
//---------------------------------------------------------
void Worker::run(){
	vdbg("Worker enter");
	Context		ctx;
	size_t		maxtsks = 0;
	size_t		maxqtsks = 0;
	size_t		conscnt = 0;
	size_t		loopcnt = 0;
	TaskQueueT	tskq;
	size_t		crtpopcnt;
	size_t		opcnt = 0;
	
	do{
		const size_t	tskqsz = tskq.size();
		if(tskqsz > maxqtsks){
			maxqtsks = tskqsz;
		}
		loopcnt += tskqsz;
		consloopcnt += tskqsz;
		opcnt += tskqsz;
		
		size_t			qcnt = tskqsz;
		while(qcnt--){
			if(tskq.front()->run(ctx)){
				tskq.push(std::move(tskq.front()));
				tskq.pop();
			}else{
				tskq.pop();
 				--load;
			}
		}
		//load -= (tskqsz - tskq.size());
		crtpopcnt = waitTasks(!tskq.empty());
		if(crtpopcnt && crtpopcnt != static_cast<size_t>(-1)){
			if(crtpopcnt > maxtsks){
				maxtsks = crtpopcnt;
			}
			conscnt += crtpopcnt;
			loopcnt += crtpopcnt;
			constskcnt += crtpopcnt;
			Task	*ptsk = nullptr;
			for(size_t i = 0; i < crtpopcnt; ++i){
				while(!pushq.pop(ptsk)){
				}
				tskq.push(TaskPtrT(ptsk));
			}
		}
		ctx.ct.currentMonotonic();
		load = tskq.size();
	}while(tskq.size() || crtpopcnt != static_cast<size_t>(-1));
	
	vdbg("Worker exit - maxtsks = "<<maxtsks<<" maxqtsks = "<<maxqtsks<<" conscnt = "<<conscnt<<" loopcnt = "<<loopcnt);
	cout<<"Worker exit - maxtsks = "<<maxtsks<<" maxqtsks = "<<maxqtsks<<" conscnt = "<<conscnt<<" loopcnt = "<<loopcnt<<endl;
}

}//namespace optim3


//=========================================================
//	optim4::Scheduler
//=========================================================
namespace optim4{



typedef std::vector<TaskPtrT>	TaskVectorT;
typedef std::queue<TaskPtrT>	TaskQueueT;

class Scheduler;

class Worker: public Thread{
	friend class 	Scheduler;
	Scheduler		&rsched;
	TaskVectorT		tskvec[2];
	Condition		cnd;
	size_t			crtpushtskvecidx;
	AtomicSizeT		crtpushsz;
	AtomicSizeT		load;
	AtomicBoolT		isrunning;
	
	
	Worker(Scheduler &_rsched):rsched(_rsched), crtpushtskvecidx(0), crtpushsz(0), load(0), isrunning(true){}
	
	TaskVectorT* waitTasks(const bool _peek);
	void run();
	void schedule(TaskPtrT &_rtskptr);
	void stop();
	
public:
	size_t currentLoad()const{
		return load;
	}
};


typedef std::unique_ptr<Worker>		WorkerPtrT;
typedef std::vector<WorkerPtrT>		WorkerVectorT;
typedef std::atomic<WorkerVectorT*>	AtomicWorkerVectorPtrT;

class Scheduler: public ::Scheduler{
	enum Status{
		StatusStoppedE = 0,
		StatusRunningE,
		StatusStoppingE,
		StatusStoppingWaitE
	};
	friend class Worker;
	Mutex					mtx;
	Condition				cnd;
	AtomicWorkerVectorPtrT	pwrkvec;
	WorkerVectorT			wkrvec;
	Status					status;
	size_t					stopwaitcnt;
	size_t					crtwkridx;
	AtomicSizeT				usecnt;
	AtomicSizeT				eqlcount;
	
	~Scheduler();
	virtual bool start(const size_t _conscnt);
	virtual void schedule(TaskPtrT &_rtskptr);
	virtual void stop(bool _wait);
	size_t	computeScheduleWorkerIndex();
	Mutex& mutex(){
		return mtx;
	}
public:
	Scheduler():pwrkvec(nullptr), status(StatusStoppedE), stopwaitcnt(0), crtwkridx(0), usecnt(0), eqlcount(0){}
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
	pwrkvec.store(&wkrvec);
	status = StatusRunningE;
	idbg("status = StatusRunningE");
	return true;
}

inline bool wkr_less_cmp(WorkerPtrT const &_rwkr1, WorkerPtrT const &_rwkr2){
	return _rwkr1->currentLoad() < _rwkr2->currentLoad();
}

inline size_t	Scheduler::computeScheduleWorkerIndex(){
	switch(wkrvec.size()){
		case 1:
			return 0;
		case 2:{
			return find_min_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<2>());
		}
		case 3:{
			return find_min_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<3>());
		}
		case 4:{
			return find_min_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<4>());
		}
		case 5:{
			return find_min_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<5>());
		}
		case 6:{
			return find_min_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<6>());
		}
		case 7:{
			return find_min_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<7>());
		}
		case 8:{
			return find_min_cmp(wkrvec.begin(), wkr_less_cmp, NumberToType<8>());
		}
		default:
			break;
	}
	
	++eqlcount;
	const size_t	cwi = crtwkridx;
	crtwkridx = (cwi + 1) % wkrvec.size();
	return cwi;
}

/*virtual*/ void Scheduler::schedule(TaskPtrT &_rtskptr){
	++usecnt;
	WorkerVectorT	*pwv = pwrkvec.load();
	if(pwv){
		(*pwv)[computeScheduleWorkerIndex()]->schedule(_rtskptr);
	}
	--usecnt;
}
/*virtual*/ void Scheduler::stop(bool _wait){
	{
		Locker<Mutex>	lock(mtx);
		if(status == StatusRunningE){
			idbg("status == StatusRunningE");
			status = _wait ? StatusStoppingWaitE : StatusStoppingE;
			pwrkvec.store(nullptr);
			for(auto it = wkrvec.begin(); it != wkrvec.end(); ++it){
				(*it)->stop();
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
		while(usecnt){
			Thread::yield();
		}
		for(auto it = wkrvec.begin(); it != wkrvec.end(); ++it){
			(*it)->join();
		}
		Locker<Mutex>	lock(mtx);
		status = StatusStoppedE;
		idbg("status = StatusStoppedE");
		cnd.broadcast();
		cout<<"Equal count "<<eqlcount<<endl;
	}
}

//---------------------------------------------------------
void Worker::stop(){
	cnd.signal();
	isrunning = false;
}
//---------------------------------------------------------
TaskVectorT* Worker::waitTasks(const bool _peek){
	static TaskVectorT emptyvec;
	if(_peek && crtpushsz == 0){
		return isrunning ? &emptyvec : nullptr;
	}
	Locker<Mutex>	lock(rsched.mutex());
	crtpushsz = 0;
	
	if(tskvec[crtpushtskvecidx].empty()){
		if(_peek){
			return isrunning ? &emptyvec : nullptr;
		}else{
			while(tskvec[crtpushtskvecidx].empty() && isrunning){
				cnd.wait(lock);
			}
		}
	}
	
	if(tskvec[crtpushtskvecidx].size()){
		TaskVectorT *pvec = &tskvec[crtpushtskvecidx];
		crtpushtskvecidx = (crtpushtskvecidx + 1) % 2;
		return pvec;
	}
	return nullptr;
}
//---------------------------------------------------------
void Worker::schedule(TaskPtrT &_rtskptr){
	++crtpushsz;
	++load;
	Locker<Mutex>	lock(rsched.mutex());
	tskvec[crtpushtskvecidx].push_back(std::move(_rtskptr));
	if(tskvec[crtpushtskvecidx].size() == 1){
		cnd.signal();
	}
}
//---------------------------------------------------------
void Worker::run(){
	vdbg("Worker enter");
	Context		ctx;
	TaskVectorT	*ptv = NULL;
	size_t		maxtsks = 0;
	size_t		maxqtsks = 0;
	size_t		conscnt = 0;
	size_t		loopcnt = 0;
	TaskQueueT	tskq;
	
	size_t		opcnt = 0;
	
	do{
		const size_t	tskqsz = tskq.size();
		if(tskqsz > maxqtsks){
			maxqtsks = tskqsz;
		}
		loopcnt += tskqsz;
		consloopcnt += tskqsz;
		opcnt += tskqsz;
		
		size_t			qcnt = tskqsz;
		while(qcnt--){
			if(tskq.front()->run(ctx)){
				tskq.push(std::move(tskq.front()));
				tskq.pop();
			}else{
				tskq.pop();
 				--load;
			}
		}
		//load -= (tskqsz - tskq.size());
		ptv = waitTasks(!tskq.empty());
		if(ptv){
			if(ptv->size() > maxtsks){
				maxtsks = ptv->size();
			}
			conscnt += ptv->size();
			loopcnt += ptv->size();
			constskcnt += ptv->size();
			
			for(auto it = ptv->begin(); it != ptv->end(); ++it){
				tskq.push(std::move(*it));
			}
			ptv->clear();
		}
		ctx.ct.currentMonotonic();
		load = tskq.size();
	}while(tskq.size() || ptv);
	
	vdbg("Worker exit - maxtsks = "<<maxtsks<<" maxqtsks = "<<maxqtsks<<" conscnt = "<<conscnt<<" loopcnt = "<<loopcnt);
	cout<<"Worker exit - maxtsks = "<<maxtsks<<" maxqtsks = "<<maxqtsks<<" conscnt = "<<conscnt<<" loopcnt = "<<loopcnt<<endl;
}

}//namespace optim4

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

const size_t cnts[] = {
	1,
	10,
	100,
	1000,
	10,
	100,
	1000,
	10000,
	
};

ATOMIC_NS::atomic<size_t>	prodtskcnt(0);

class Producer: public Thread{
	Scheduler 		&rsched;
	const size_t	taskcnt;
	
	/*virtual*/ void run(){
		this->yield();
		this->yield();
		this->yield();
		
		const size_t	strcnt = (sizeof(strs)/sizeof(const char*));
		const size_t	cntcnt = (sizeof(cnts)/sizeof(size_t));
		
		for(size_t i = 0; i < taskcnt; ++i){
			++prodtskcnt;
			const size_t	loopcnt = cnts[i % cntcnt];
			if(i % 2){
				TaskPtrT		tskptr(new IntTask(loopcnt, i));
				rsched.schedule(tskptr);
			}else{
				size_t	stridx = (i/2) % strcnt;
				
				TaskPtrT		tskptr(new StringTask(loopcnt, strs[stridx]));
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

Scheduler * schedulerFactory(const char _tp){
	switch(_tp){
		case '0':
		case 'b':
		case 'B':
			return new baseline::Scheduler;
		case '1':
			return new optim1::Scheduler;
		case '2':
			return new optim2::Scheduler;
		case '3':
			return new optim3::Scheduler;
		case '4':
			return new optim3::Scheduler;
		default:
			return nullptr;
	}
}

int main(int argc, char *argv[]){
	Thread::init();
	Debug::the().initStdErr(false);
	Debug::the().levelMask("view");
	Debug::the().moduleMask("any");
	
	const size_t		prodcnt = 4;
	const size_t		conscnt = 4;
	
	const size_t		prodtaskcnt = 100000/* * 1000*/;
	
	Scheduler			*psch = nullptr;
	
	if(argc >= 2){
		psch = schedulerFactory(*argv[1]);
	}
	
	if(!psch){
		cout<<"Unknown scheduler type"<<endl;
		return 0;
	}
	
	Scheduler			&sched(*psch);
	
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
	cout<<"consumed "<<constskcnt<<" tasks with "<<consloopcnt<<" loops"<<endl;
	Thread::waitAll();
	
	return 0;
}

