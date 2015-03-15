// frame/src/reactor.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <fcntl.h>
#include <vector>
#include <deque>
#include <cerrno>
#include <cstring>
#include <queue>

#include "system/common.hpp"
#include "system/exception.hpp"
#include "system/debug.hpp"
#include "system/timespec.hpp"
#include "system/mutex.hpp"
#include "system/thread.hpp"
#include "system/condition.hpp"
#include "system/device.hpp"
#include "system/error.hpp"

#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "frame/object.hpp"
#include "frame/service.hpp"
#include "frame/common.hpp"
#include "frame/event.hpp"
#include "frame/timestore.hpp"
#include "frame/reactor.hpp"
#include "frame/timer.hpp"
#include "frame/completion.hpp"
#include "frame/reactorcontext.hpp"


namespace solid{
namespace frame{

namespace{

void dummy_completion(CompletionHandler&, ReactorContext &){
}

}//namespace


typedef ATOMIC_NS::atomic<bool>		AtomicBoolT;
typedef ATOMIC_NS::atomic<size_t>	AtomicSizeT;
typedef Reactor::TaskT				TaskT;


class EventObject: public Object{
public:
	EventObject(): dummyhandler(proxy(), dummy_completion){
		use();
	}
	
	void stop(){
		dummyhandler.deactivate();
		dummyhandler.unregister();
	}
	
	template <class F>
	void post(ReactorContext &_rctx, F _f){
		Object::post(_rctx, _f);
	}
	
	CompletionHandler		dummyhandler;
};

struct NewTaskStub{
	NewTaskStub(
		UidT const&_ruid, TaskT const&_robjptr, Service &_rsvc, Event const &_revt
	):uid(_ruid), objptr(_robjptr), rsvc(_rsvc), evt(_revt){}
	
	//NewTaskStub(){}
	UidT		uid;
	TaskT		objptr;
	Service		&rsvc;
	Event		evt;
};

struct RaiseEventStub{
	RaiseEventStub(
		UidT const&_ruid, Event const &_revt
	):uid(_ruid), evt(_revt){}
	
	UidT		uid;
	Event		evt;
};

struct CompletionHandlerStub{
	CompletionHandlerStub(
		CompletionHandler *_pch = nullptr,
		const size_t _objidx = -1
	):pch(_pch), objidx(_objidx), unique(0){}
	
	CompletionHandler		*pch;
	size_t					objidx;
	UniqueT					unique;
};


struct ObjectStub{
	ObjectStub():unique(0), psvc(nullptr){}
	
	UniqueT		unique;
	Service		*psvc;
	TaskT		objptr;
};


enum{
	MinEventCapacity = 32,
	MaxEventCapacity = 1024 * 64
};


struct ExecStub{
	template <class F>
	ExecStub(
		UidT const &_ruid, F _f, Event const &_revt = Event()
	):objuid(_ruid), exefnc(_f), evt(_revt){}
	
	template <class F>
	ExecStub(
		UidT const &_ruid, F _f, UidT const &_rchnuid, Event const &_revt = Event()
	):objuid(_ruid), chnuid(_rchnuid), exefnc(_f), evt(_revt){}
	
	ExecStub(
		UidT const &_ruid, Event const &_revt = Event()
	):objuid(_ruid), evt(_revt){}
	
	UidT			objuid;
	UidT			chnuid;
	EventFunctionT	exefnc;
	Event			evt;
};

typedef std::vector<NewTaskStub>			NewTaskVectorT;
typedef std::vector<RaiseEventStub>			RaiseEventVectorT;
typedef std::deque<CompletionHandlerStub>	CompletionHandlerDequeT;
typedef std::vector<UidT>					UidVectorT;
typedef std::deque<ObjectStub>				ObjectDequeT;
typedef Queue<ExecStub>						ExecQueueT;
typedef Stack<size_t>						SizeStackT;
typedef TimeStore<size_t>					TimeStoreT;

struct Reactor::Data{
	Data(
		
	):	running(0), must_stop(false), crtpushtskvecidx(0),
		crtraisevecidx(0), crtpushvecsz(0), crtraisevecsz(0),
		objcnt(0), timestore(MinEventCapacity)
	{
		pcrtpushtskvec = &pushtskvec[1];
		pcrtraisevec = &raisevec[1];
	}
	
	int computeWaitTimeMilliseconds(TimeSpec const & _rcrt)const{
		if(exeq.size()){
			return 0;
		}else if(timestore.size()){
			if(_rcrt < timestore.next()){
				const int64	maxwait = 1000 * 60; //1 minute
				int64 		diff = 0;
				TimeSpec	delta = timestore.next();
				delta -= _rcrt;
				diff = (delta.seconds() * 1000);
				diff += (delta.nanoSeconds() / 1000000);
				if(diff > maxwait){
					return maxwait;
				}else{
					return diff;
				}
			}else{
				return 0;
			}
		}else{
			return -1;
		}
	}
	
	UidT dummyCompletionHandlerUid()const{
		const size_t idx = eventobj.dummyhandler.idxreactor;
		return UidT(idx, chdq[idx].unique);
	}
	
	bool						running;
	bool						must_stop;
	size_t						crtpushtskvecidx;
	size_t						crtraisevecidx;
	size_t						crtpushvecsz;
	size_t						crtraisevecsz;
	size_t						objcnt;
	TimeStoreT					timestore;
	NewTaskVectorT				*pcrtpushtskvec;
	RaiseEventVectorT			*pcrtraisevec;
	
	Mutex						mtx;
	Condition					cnd;
	NewTaskVectorT				pushtskvec[2];
	
	RaiseEventVectorT			raisevec[2];
	EventObject					eventobj;
	CompletionHandlerDequeT		chdq;
	UidVectorT					freeuidvec;
	ObjectDequeT				objdq;
	ExecQueueT					exeq;
	SizeStackT					chposcache;
};

Reactor::Reactor(
	SchedulerBase &_rsched,
	const size_t _idx 
):ReactorBase(_rsched, _idx), d(*(new Data)){
	vdbgx(Debug::aio, "");
}

Reactor::~Reactor(){
	delete &d;
	vdbgx(Debug::aio, "");
}

bool Reactor::start(){
	doStoreSpecific();
	vdbgx(Debug::aio, "");
	
	d.objdq.push_back(ObjectStub());
	d.objdq.back().objptr = &d.eventobj;
	
	popUid(*d.objdq.back().objptr);
	
	d.eventobj.registerCompletionHandlers();
	
	d.running = true;
	
	return true;
}

/*virtual*/ bool Reactor::raise(UidT const& _robjuid, Event const& _revt){
	vdbgx(Debug::aio,  (void*)this<<" uid = "<<_robjuid.index<<','<<_robjuid.unique<<" event id = "<<_revt.id);
	bool 	rv = true;
	size_t	raisevecsz = 0;
	{
		Locker<Mutex>	lock(d.mtx);
		
		d.raisevec[d.crtraisevecidx].push_back(RaiseEventStub(_robjuid, _revt));
		raisevecsz = d.raisevec[d.crtraisevecidx].size();
		d.crtraisevecsz = raisevecsz;
		if(raisevecsz == 1){
			d.cnd.signal();
		}
	}
	return rv;
}
/*virtual*/ void Reactor::stop(){
	vdbgx(Debug::aio, "");
	Locker<Mutex>	lock(d.mtx);
	d.must_stop = true;
	d.cnd.signal();
}

//Called from outside reactor's thread
bool Reactor::push(TaskT &_robj, Service &_rsvc, Event const &_revt){
	vdbgx(Debug::aio, (void*)this);
	bool 	rv = true;
	size_t	pushvecsz = 0;
	{
		Locker<Mutex>	lock(d.mtx);
		const UidT		uid = this->popUid(*_robj);
		
		vdbgx(Debug::aio, (void*)this<<" uid = "<<uid.index<<','<<uid.unique<<" event id = "<<_revt.id);
			
		d.pushtskvec[d.crtpushtskvecidx].push_back(NewTaskStub(uid, _robj, _rsvc, _revt));
		pushvecsz = d.pushtskvec[d.crtpushtskvecidx].size();
		d.crtpushvecsz = pushvecsz;
		if(pushvecsz == 1){
			d.cnd.signal();
		}
	}
	
	return rv;
}


void Reactor::run(){
	vdbgx(Debug::aio, "<enter>");
	bool		running = true;
	TimeSpec	crttime;
	
	while(running){
		crttime.currentRealTime();
		
		crtload = d.objcnt + d.exeq.size();
		
		if(doWaitEvent(crttime)){
			crttime.currentRealTime();
			doCompleteEvents(crttime);
		}
		crttime.currentRealTime();
		doCompleteTimer(crttime);
		
		crttime.currentRealTime();
		doCompleteExec(crttime);
		
		running = d.running || (d.objcnt != 0) || !d.exeq.empty();
	}
	d.eventobj.stop();
	doClearSpecific();
	vdbgx(Debug::aio, "<exit>");
}

UidT Reactor::objectUid(ReactorContext const &_rctx)const{
	return UidT(_rctx.objidx, d.objdq[_rctx.objidx].unique);
}

Service& Reactor::service(ReactorContext const &_rctx)const{
	return *d.objdq[_rctx.objidx].psvc;
}
	
Object& Reactor::object(ReactorContext const &_rctx)const{
	return *d.objdq[_rctx.objidx].objptr;
}

CompletionHandler *Reactor::completionHandler(ReactorContext const &_rctx)const{
	return d.chdq[_rctx.chnidx].pch;
}

void Reactor::post(ReactorContext &_rctx, EventFunctionT  &_revfn, Event const &_rev){
	d.exeq.push(ExecStub(_rctx.objectUid(), _rev));
	d.exeq.back().exefnc = std::move(_revfn);
	d.exeq.back().chnuid = d.dummyCompletionHandlerUid();
}

void Reactor::post(ReactorContext &_rctx, EventFunctionT  &_revfn, Event const &_rev, CompletionHandler const &_rch){
	d.exeq.push(ExecStub(_rctx.objectUid(), _rev));
	d.exeq.back().exefnc = std::move(_revfn);
	d.exeq.back().chnuid = UidT(_rch.idxreactor, d.chdq[_rch.idxreactor].unique);
}

/*static*/ void Reactor::stop_object(ReactorContext &_rctx, Event const &){
	Reactor			&rthis = _rctx.reactor();
	ObjectStub		&ros = rthis.d.objdq[_rctx.objidx];
	
	
	rthis.stopObject(*ros.objptr, ros.psvc->manager());
	
	ros.objptr.clear();
	ros.psvc = nullptr;
 	++ros.unique;
	--rthis.d.objcnt;
	rthis.d.freeuidvec.push_back(UidT(_rctx.objidx, ros.unique));
}

void Reactor::postObjectStop(ReactorContext &_rctx){
	d.exeq.push(ExecStub(_rctx.objectUid()));
	d.exeq.back().exefnc = &stop_object;
	d.exeq.back().chnuid = d.dummyCompletionHandlerUid();
}

struct ChangeTimerIndexCallback{
	Reactor &r;
	ChangeTimerIndexCallback(Reactor &_r):r(_r){}
	
	void operator()(const size_t _chidx, const size_t _newidx, const size_t _oldidx)const{
		r.doUpdateTimerIndex(_chidx, _newidx, _oldidx);
	}
};

struct TimerCallback{
	Reactor			&r;
	ReactorContext	&rctx;
	TimerCallback(Reactor &_r, ReactorContext &_rctx): r(_r), rctx(_rctx){}
	
	void operator()(const size_t _tidx, const size_t _chidx)const{
		r.onTimer(rctx, _tidx, _chidx);
	}
};

void Reactor::onTimer(ReactorContext &_rctx, const size_t _tidx, const size_t _chidx){
	CompletionHandlerStub	&rch = d.chdq[_chidx];
		
	_rctx.reactevn = ReactorEventTimer;
	_rctx.chnidx =  _chidx;
	_rctx.objidx = rch.objidx;
	
	rch.pch->handleCompletion(_rctx);
	_rctx.clearError();
}

void Reactor::doCompleteTimer(TimeSpec  const &_rcrttime){
	ReactorContext	ctx(*this, _rcrttime);
	TimerCallback 	tcbk(*this, ctx);
	d.timestore.pop(_rcrttime, tcbk, ChangeTimerIndexCallback(*this));
}

void Reactor::doCompleteExec(TimeSpec  const &_rcrttime){
	ReactorContext	ctx(*this, _rcrttime);
	size_t	sz = d.exeq.size();
	while(sz--){
		ExecStub				&rexe(d.exeq.front());
		ObjectStub				&ros(d.objdq[rexe.objuid.index]);
		CompletionHandlerStub	&rcs(d.chdq[rexe.chnuid.index]);
		
		if(ros.unique == rexe.objuid.unique && rcs.unique == rexe.chnuid.unique){
			ctx.clearError();
			ctx.chnidx = rexe.chnuid.index;
			ctx.objidx = rexe.objuid.index;
			rexe.exefnc(ctx, rexe.evt);
		}
		d.exeq.pop();
	}
}

bool Reactor::doWaitEvent(TimeSpec const &_rcrttime){
	bool			rv = false;
	int				waitmsec = d.computeWaitTimeMilliseconds(_rcrttime);
	Locker<Mutex>	lock(d.mtx);
	
	if(d.crtpushvecsz == 0 && d.crtraisevecsz == 0 && !d.must_stop){
		if(waitmsec > 0){
			TimeSpec next_time = _rcrttime;
			next_time += waitmsec;
			d.cnd.wait(lock, next_time);
		}else if(waitmsec < 0){
			d.cnd.wait(lock);
		}
	}
	
	if(d.must_stop){
		d.running = false;
		d.must_stop = false;
	}
	if(d.crtpushvecsz){
		d.crtpushvecsz = 0;
		for(auto it = d.freeuidvec.begin(); it != d.freeuidvec.end(); ++it){
			this->pushUid(*it);
		}
		d.freeuidvec.clear();
		
		const size_t	crtpushvecidx = d.crtpushtskvecidx;
		d.crtpushtskvecidx = ((crtpushvecidx + 1) & 1);
		d.pcrtpushtskvec = &d.pushtskvec[crtpushvecidx];
		rv = true;
	}
	if(d.crtraisevecsz){
		d.crtraisevecsz = 0;
		const size_t crtraisevecidx = d.crtraisevecidx;
		d.crtraisevecidx = ((crtraisevecidx + 1) & 1);
		d.pcrtraisevec = &d.raisevec[crtraisevecidx];
		rv = true;
	}
	return rv;
}

void Reactor::doCompleteEvents(TimeSpec  const &_rcrttime){
	vdbgx(Debug::aio, "");
	
	NewTaskVectorT		&crtpushvec = *d.pcrtpushtskvec;
	RaiseEventVectorT	&crtraisevec = *d.pcrtraisevec;
	ReactorContext		ctx(*this, _rcrttime);
	
	if(crtpushvec.size()){
		
		d.objcnt += crtpushvec.size();
		
		for(auto it = crtpushvec.begin(); it != crtpushvec.end(); ++it){
			NewTaskStub		&rnewobj(*it);
			if(rnewobj.uid.index >= d.objdq.size()){
				d.objdq.resize(rnewobj.uid.index + 1);
			}
			ObjectStub 		&ros = d.objdq[rnewobj.uid.index];
			cassert(ros.unique == rnewobj.uid.unique);
			ros.objptr = std::move(rnewobj.objptr);
			ros.psvc = &rnewobj.rsvc;
			
			ctx.clearError();
			ctx.chnidx =  -1;
			ctx.objidx = rnewobj.uid.index;
			
			ros.objptr->registerCompletionHandlers();
			
			d.exeq.push(ExecStub(rnewobj.uid, &call_object_on_event, d.dummyCompletionHandlerUid(), rnewobj.evt));
		}
		crtpushvec.clear();
	}
	
	if(crtraisevec.size()){	
		for(auto it = crtraisevec.begin(); it != crtraisevec.end(); ++it){
			RaiseEventStub	&revt = *it;
			d.exeq.push(ExecStub(revt.uid, &call_object_on_event, d.dummyCompletionHandlerUid(), revt.evt));
		}
		crtraisevec.clear();
	}
}

/*static*/ void Reactor::call_object_on_event(ReactorContext &_rctx, Event const &_revt){
	_rctx.object().onEvent(_rctx, _revt);
}

bool Reactor::addTimer(CompletionHandler const &_rch, TimeSpec const &_rt, size_t &_rstoreidx){
	if(_rstoreidx != static_cast<size_t>(-1)){
		size_t idx = d.timestore.change(_rstoreidx, _rt);
		cassert(idx == _rch.idxreactor);
	}else{
		_rstoreidx = d.timestore.push(_rt, _rch.idxreactor);
	}
	return true;
}

void Reactor::doUpdateTimerIndex(const size_t _chidx, const size_t _newidx, const size_t _oldidx){
	CompletionHandlerStub &rch = d.chdq[_chidx];
	cassert(static_cast<Timer*>(rch.pch)->storeidx == _oldidx);
	static_cast<Timer*>(rch.pch)->storeidx = _newidx;
}

bool Reactor::remTimer(CompletionHandler const &_rch, size_t const &_rstoreidx){
	if(_rstoreidx != static_cast<size_t>(-1)){
		d.timestore.pop(_rstoreidx, ChangeTimerIndexCallback(*this));
	}
	return true;
}

void Reactor::registerCompletionHandler(CompletionHandler &_rch, Object const &_robj){
	vdbgx(Debug::aio, "");
	size_t idx;
	if(d.chposcache.size()){
		idx = d.chposcache.top();
		d.chposcache.pop();
	}else{
		idx = d.chdq.size();
		d.chdq.push_back(CompletionHandlerStub());
	}
	CompletionHandlerStub &rcs = d.chdq[idx];
	rcs.objidx = _robj.ObjectBase::runId().index;
	rcs.pch =  &_rch;
	//rcs.waitreq = ReactorWaitNone;
	_rch.idxreactor = idx;
	{
		TimeSpec		dummytime;
		ReactorContext	ctx(*this, dummytime);
		
		ctx.reactevn = ReactorEventInit;
		ctx.objidx = rcs.objidx;
		ctx.chnidx = idx;
		
		_rch.handleCompletion(ctx);
	}
}

void Reactor::unregisterCompletionHandler(CompletionHandler &_rch){
	vdbgx(Debug::aio, "");
	CompletionHandlerStub &rcs = d.chdq[_rch.idxreactor];
	{
		TimeSpec		dummytime;
		ReactorContext	ctx(*this, dummytime);
		
		ctx.reactevn = ReactorEventClear;
		ctx.objidx = rcs.objidx;
		ctx.chnidx = _rch.idxreactor;
		
		_rch.handleCompletion(ctx);
	}
	
	
	rcs.pch = &d.eventobj.dummyhandler;
	rcs.objidx = 0;
	++rcs.unique;
}


namespace{
#ifdef HAS_SAFE_STATIC
static const size_t specificPosition(){
	static const size_t	thrspecpos = Thread::specificId();
	return thrspecpos;
}
#else
const size_t specificIdStub(){
	static const size_t id(Thread::specificId());
	return id;
}

void once_stub(){
	specificIdStub();
}

static const size_t specificPosition(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_stub, once);
	return specificIdStub();
}
#endif
}//namespace

/*static*/ Reactor* Reactor::safeSpecific(){
	return reinterpret_cast<Reactor*>(Thread::specific(specificPosition()));
}

/*static*/ Reactor& Reactor::specific(){
	vdbgx(Debug::aio, "");
	return *safeSpecific();
}
void Reactor::doStoreSpecific(){
	Thread::specific(specificPosition(), this);
}
void Reactor::doClearSpecific(){
	Thread::specific(specificPosition(), nullptr);
}
//=============================================================================
//		ReactorContext
//=============================================================================

Object& ReactorContext::object()const{
	return reactor().object(*this);
}
Service& ReactorContext::service()const{
	return reactor().service(*this);
}

UidT ReactorContext::objectUid()const{
	return reactor().objectUid(*this);
}

CompletionHandler*  ReactorContext::completionHandler()const{
	return reactor().completionHandler(*this);
}

//=============================================================================
//		ReactorBase
//=============================================================================
UidT ReactorBase::popUid(ObjectBase &_robj){
        UidT    rv(crtidx, 0);
        if(uidstk.size()){
                rv = uidstk.top();
                uidstk.pop();
        }else{
                ++crtidx;
        }
        _robj.runId(rv);
        return rv;
}

bool ReactorBase::prepareThread(const bool _success){
        return scheduler().prepareThread(idInScheduler(), *this, _success);
}
void ReactorBase::unprepareThread(){
        scheduler().unprepareThread(idInScheduler(), *this);
}



}//namespace frame
}//namespace solid

