// solid/frame/aio/src/aioreactor_epoll.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <fcntl.h>

#include "solid/system/common.hpp"

#if defined(SOLID_USE_EPOLL)

#include <sys/epoll.h>
#include <sys/eventfd.h>

#elif defined(SOLID_USE_KQUEUE)

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#endif

#include <vector>
#include <deque>
#include <cerrno>
#include <cstring>
#include <queue>

#include "solid/system/exception.hpp"
#include "solid/system/debug.hpp"
#include "solid/system/nanotime.hpp"
#include <mutex>
#include <thread>
#include "solid/system/device.hpp"
#include "solid/system/error.hpp"

#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"
#include "solid/utility/event.hpp"

#include "solid/frame/object.hpp"
#include "solid/frame/service.hpp"
#include "solid/frame/common.hpp"
#include "solid/frame/timestore.hpp"

#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aiotimer.hpp"
#include "solid/frame/aio/aiocompletion.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"

using namespace std;

namespace solid{
namespace frame{
namespace aio{

//=============================================================================

namespace{

void dummy_completion(CompletionHandler&, ReactorContext &){
}

inline void* indexToVoid(const size_t _idx){
	return reinterpret_cast<void*>(_idx);
}

inline size_t voidToIndex(const void* _ptr){
	return reinterpret_cast<size_t>(_ptr);
}


}//namespace

//=============================================================================

typedef std::atomic<bool>		AtomicBoolT;
typedef std::atomic<size_t>	AtomicSizeT;
typedef Reactor::TaskT				TaskT;

//=============================================================================
//	EventHandler
//=============================================================================

struct EventHandler: CompletionHandler{
	static void on_init(CompletionHandler&, ReactorContext &);
	static void on_completion(CompletionHandler&, ReactorContext &);
	
	EventHandler(ObjectProxy const &_rop): CompletionHandler(_rop, &on_init){}
	
	void write(Reactor &_rreactor);
	
	bool init();
	
	int descriptor()const{
		return dev.descriptor();
	}
private:
	Device		dev;
};

//-----------------------------------------------------------------------------

/*static*/ void EventHandler::on_init(CompletionHandler& _rch, ReactorContext &_rctx){
	EventHandler &rthis = static_cast<EventHandler&>(_rch);

#if defined(SOLID_USE_EPOLL)
	rthis.reactor(_rctx).addDevice(_rctx, rthis, rthis.dev, ReactorWaitRead);
#elif defined(SOLID_USE_KQUEUE)
	rthis.reactor(_rctx).addDevice(_rctx, rthis, rthis.dev, ReactorWaitUser);
#endif
	rthis.completionCallback(&on_completion);
}

//-----------------------------------------------------------------------------

/*static*/ void EventHandler::on_completion(CompletionHandler& _rch, ReactorContext &_rctx){
	EventHandler	&rthis = static_cast<EventHandler&>(_rch);
#if defined(SOLID_USE_EPOLL)
	uint64_t		v = -1;
	int 			rv;
	
	do{
		rv = rthis.dev.read(reinterpret_cast<char*>(&v), sizeof(v));
		idbgx(Debug::aio, "Read from event "<<rv<<" value = "<<v);
	}while(rv == sizeof(v));
#endif
	rthis.reactor(_rctx).doCompleteEvents(_rctx);
}

//-----------------------------------------------------------------------------

bool EventHandler::init(){
#if defined(SOLID_USE_EPOLL)
	dev = Device(eventfd(0, EFD_NONBLOCK));
	
	if(!dev.ok()){
		edbgx(Debug::aio, "eventfd: "<<last_system_error().message());
		return false;
	}
#endif
	return true;
}

//=============================================================================
//	EventObject
//=============================================================================

class EventObject: public Object{
public:
	EventObject():eventhandler(proxy()), dummyhandler(proxy(), dummy_completion){
		use();
	}
	
	void stop(){
		eventhandler.deactivate();
		eventhandler.unregister();
		dummyhandler.deactivate();
		dummyhandler.unregister();
	}
	
	template <class F>
	void post(ReactorContext &_rctx, F _f){
		Object::post(_rctx, _f);
	}
	
	EventHandler			eventhandler;
	CompletionHandler		dummyhandler;
};

//=============================================================================

struct NewTaskStub{
	NewTaskStub(
		UniqueId const&_ruid, TaskT const&_robjptr, Service &_rsvc, Event &&_revent
	):uid(_ruid), objptr(_robjptr), rsvc(_rsvc), event(std::move(_revent)){}
	
	NewTaskStub(const NewTaskStub&) = delete;
	
	NewTaskStub(
		NewTaskStub && _unts
	):uid(_unts.uid), objptr(std::move(_unts.objptr)), rsvc(_unts.rsvc), event(std::move(_unts.event)){}
	
	
	UniqueId	uid;
	TaskT		objptr;
	Service		&rsvc;
	Event		event;
};

//=============================================================================

struct RaiseEventStub{
	RaiseEventStub(
		UniqueId const&_ruid, Event &&_revent
	):uid(_ruid), event(std::move(_revent)){}
	
	RaiseEventStub(
		UniqueId const&_ruid, Event const &_revent
	):uid(_ruid), event(_revent){}
	
	RaiseEventStub(const RaiseEventStub&) = delete;
	RaiseEventStub(
		RaiseEventStub &&_uevs
	):uid(_uevs.uid), event(std::move(_uevs.event)){}
	
	UniqueId	uid;
	Event		event;
};

//=============================================================================

struct CompletionHandlerStub{
	CompletionHandlerStub(
		CompletionHandler *_pch = nullptr,
		const size_t _objidx = InvalidIndex()
	):pch(_pch), objidx(_objidx), unique(0){}
	
	CompletionHandler		*pch;
	size_t					objidx;
	UniqueT					unique;
};

//=============================================================================

struct ObjectStub{
	ObjectStub():unique(0), psvc(nullptr){}
	
	UniqueT		unique;
	Service		*psvc;
	TaskT		objptr;
};

//=============================================================================

enum{
	MinEventCapacity = 32,
	MaxEventCapacity = 1024 * 64
};

//=============================================================================

struct ExecStub{
	template <class F>
	ExecStub(
		UniqueId const &_ruid, F _f, Event &&_uevent = Event()
	):objuid(_ruid), exefnc(_f), event(std::move(_uevent)){}
	
	template <class F>
	ExecStub(
		UniqueId const &_ruid, F _f, UniqueId const &_rchnuid, Event &&_uevent = Event()
	):objuid(_ruid), chnuid(_rchnuid), exefnc(_f), event(std::move(_uevent)){}
	
	ExecStub(
		UniqueId const &_ruid, Event &&_uevent = Event()
	):objuid(_ruid), event(std::move(_uevent)){}
	
	ExecStub(const ExecStub&) = delete;
	
	ExecStub(
		ExecStub &&_res
	):objuid(std::move(_res.objuid)), chnuid(std::move(_res.chnuid)), exefnc(std::move(_res.exefnc)), event(std::move(_res.event)){
		//objuid = std::move(_res.objuid);
		//chnuid = std::move(_res.chnuid);
	}
	
	UniqueId					objuid;
	UniqueId					chnuid;
	Reactor::EventFunctionT		exefnc;
	Event						event;
};

//=============================================================================

typedef std::vector<NewTaskStub>			NewTaskVectorT;
typedef std::vector<RaiseEventStub>			RaiseEventVectorT;

#if defined(SOLID_USE_EPOLL)

typedef std::vector<epoll_event>			EventVectorT;

#elif defined(SOLID_USE_KQUEUE)

typedef std::vector<struct kevent>			EventVectorT;

#endif

typedef std::deque<CompletionHandlerStub>	CompletionHandlerDequeT;
typedef std::vector<UniqueId>				UidVectorT;
typedef std::deque<ObjectStub>				ObjectDequeT;
typedef Queue<ExecStub>						ExecQueueT;
typedef Stack<size_t>						SizeStackT;
typedef TimeStore<size_t>					TimeStoreT;

//=============================================================================
//	Reactor::Data
//=============================================================================
struct Reactor::Data{
	Data(
		
	):	reactor_fd(-1), running(0), crtpushtskvecidx(0),
		crtraisevecidx(0), crtpushvecsz(0), crtraisevecsz(0), devcnt(0),
		objcnt(0), timestore(MinEventCapacity){}
#if defined(SOLID_USE_EPOLL)
	int computeWaitTimeMilliseconds(NanoTime const & _rcrt)const{
		
		if(exeq.size()){
			return 0;
		}else if(timestore.size()){
			
			if(_rcrt < timestore.next()){
				
				const int64_t	maxwait = 1000 * 60 * 10; //ten minutes
				int64_t 		diff = 0;
				NanoTime		delta = timestore.next();
				
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
#elif defined(SOLID_USE_KQUEUE)
	NanoTime computeWaitTimeMilliseconds(NanoTime const & _rcrt)const{
		
		if(exeq.size()){
			return NanoTime();
		}else if(timestore.size()){
			
			if(_rcrt < timestore.next()){
				const NanoTime	maxwait(1000 * 60 * 10); //ten minutes
				NanoTime		delta = timestore.next();
				
				delta -= _rcrt;
				
				if(delta > maxwait){
					return maxwait;
				}else{
					return delta;
				}
			

			}else{
				return NanoTime();
			}
		}else{
			return NanoTime::maximum;
		}
	}
#endif

	UniqueId dummyCompletionHandlerUid()const{
		const size_t idx = eventobj.dummyhandler.idxreactor;
		return UniqueId(idx, chdq[idx].unique);
	}
	
	int							reactor_fd;
	AtomicBoolT					running;
	size_t						crtpushtskvecidx;
	size_t						crtraisevecidx;
	AtomicSizeT					crtpushvecsz;
	AtomicSizeT					crtraisevecsz;
	size_t						devcnt;
	size_t						objcnt;
	TimeStoreT					timestore;
	
	mutex						mtx;
	EventVectorT				eventvec;
	NewTaskVectorT				pushtskvec[2];
	RaiseEventVectorT			raisevec[2];
	EventObject					eventobj;
	CompletionHandlerDequeT		chdq;
	UidVectorT					freeuidvec;
	ObjectDequeT				objdq;
	ExecQueueT					exeq;
	SizeStackT					chposcache;
};
//-----------------------------------------------------------------------------
void EventHandler::write(Reactor &_rreactor){
#if defined(SOLID_USE_EPOLL)
	const uint64_t v = 1;
	dev.write(reinterpret_cast<const char*>(&v), sizeof(v));
#elif defined(SOLID_USE_KQUEUE)
	struct	kevent ev;
	
	vdbgx(Debug::aio, "trigger user event");
 	
	EV_SET(&ev, dev.descriptor(), EVFILT_USER, 0, NOTE_TRIGGER, 0, indexToVoid(this->indexWithinReactor()) );
	
	if(kevent (_rreactor.d.reactor_fd, &ev, 1, NULL, 0, NULL)){
		edbgx(Debug::aio, "kevent: "<<last_system_error().message());
		SOLID_ASSERT(false);
	}
#endif
}
//=============================================================================
//-----------------------------------------------------------------------------
//	Reactor
//-----------------------------------------------------------------------------


Reactor::Reactor(
	SchedulerBase &_rsched,
	const size_t _idx 
):ReactorBase(_rsched, _idx), d(*(new Data)){
	vdbgx(Debug::aio, "");
}

//-----------------------------------------------------------------------------

Reactor::~Reactor(){
	delete &d;
	vdbgx(Debug::aio, "");
}

//-----------------------------------------------------------------------------

bool Reactor::start(){

	vdbgx(Debug::aio, "");	
	
	doStoreSpecific();

#if defined(SOLID_USE_EPOLL)
	d.reactor_fd = epoll_create(MinEventCapacity);
#elif defined(SOLID_USE_KQUEUE)
	d.reactor_fd = kqueue();
#endif
	
	if(d.reactor_fd < 0){
		edbgx(Debug::aio, "reactor create: "<<last_system_error().message());
		return false;
	}
	
	if(!d.eventobj.eventhandler.init()){
		return false;
	}
	
	d.objdq.push_back(ObjectStub());
	d.objdq.back().objptr = &d.eventobj;
	
	popUid(*d.objdq.back().objptr);
	
	d.eventobj.registerCompletionHandlers();
	
	d.eventvec.resize(MinEventCapacity);
	d.eventvec.resize(d.eventvec.capacity());
	d.running = true;
	++d.devcnt;
	
	return true;
}

//-----------------------------------------------------------------------------

/*virtual*/ bool Reactor::raise(UniqueId const& _robjuid, Event && _uevent){
	vdbgx(Debug::aio,  (void*)this<<" uid = "<<_robjuid.index<<','<<_robjuid.unique<<" event = "<<_uevent);
	bool 	rv = true;
	size_t	raisevecsz = 0;
	{
		unique_lock<std::mutex>	lock(d.mtx);
		
		d.raisevec[d.crtraisevecidx].push_back(RaiseEventStub(_robjuid, std::move(_uevent)));
		raisevecsz = d.raisevec[d.crtraisevecidx].size();
		d.crtraisevecsz = raisevecsz;
	}
	if(raisevecsz == 1){
		d.eventobj.eventhandler.write(*this);
	}
	return rv;
}

//-----------------------------------------------------------------------------

/*virtual*/ bool Reactor::raise(UniqueId const& _robjuid, const Event & _revent){
	vdbgx(Debug::aio,  (void*)this<<" uid = "<<_robjuid.index<<','<<_robjuid.unique<<" event = "<<_revent);
	bool 	rv = true;
	size_t	raisevecsz = 0;
	{
		unique_lock<std::mutex>	lock(d.mtx);
		
		d.raisevec[d.crtraisevecidx].push_back(RaiseEventStub(_robjuid, _revent));
		raisevecsz = d.raisevec[d.crtraisevecidx].size();
		d.crtraisevecsz = raisevecsz;
	}
	if(raisevecsz == 1){
		d.eventobj.eventhandler.write(*this);
	}
	return rv;
}

//-----------------------------------------------------------------------------

/*virtual*/ void Reactor::stop(){
	vdbgx(Debug::aio, "");
	d.running = false;
	d.eventobj.eventhandler.write(*this);
}

//-----------------------------------------------------------------------------

//Called from outside reactor's thread
bool Reactor::push(TaskT &_robj, Service &_rsvc, Event &&_uevent){
	vdbgx(Debug::aio, (void*)this);
	bool 	rv = true;
	size_t	pushvecsz = 0;
	{
		unique_lock<std::mutex>		lock(d.mtx);
		const UniqueId		uid = this->popUid(*_robj);
		
		vdbgx(Debug::aio, (void*)this<<" uid = "<<uid.index<<','<<uid.unique<<" event = "<<_uevent);
			
		d.pushtskvec[d.crtpushtskvecidx].push_back(NewTaskStub(uid, _robj, _rsvc, std::move(_uevent)));
		pushvecsz = d.pushtskvec[d.crtpushtskvecidx].size();
		d.crtpushvecsz = pushvecsz;
	}
	
	if(pushvecsz == 1){
		d.eventobj.eventhandler.write(*this);
	}
	return rv;
}

//-----------------------------------------------------------------------------

/*NOTE:
	
	We MUST call doCompleteEvents before doCompleteExec
	because we must ensure that on successful Event notification from
	frame::Manager, the Object actually receives the Event before stopping.
	
	For that, on Object::postStop, we mark the Object as “unable to
	receive any notifications” (we do not unregister it, because the
	Object may want access to it’s mutex on events already waiting
	to be delivered to the object.

*/
void Reactor::run(){
	idbgx(Debug::aio, "<enter>");
	int			selcnt;
	bool		running = true;
	NanoTime	crttime;
	int			waitmsec;
	NanoTime	waittime;
	
	while(running){
		crttime.currentMonotonic();
		
		crtload = d.objcnt + d.devcnt + d.exeq.size();
#if defined(SOLID_USE_EPOLL)
		waitmsec = d.computeWaitTimeMilliseconds(crttime);
		
		vdbgx(Debug::aio, "epoll_wait msec = "<<waitmsec);
		
		selcnt = epoll_wait(d.reactor_fd, d.eventvec.data(), d.eventvec.size(), waitmsec);
#elif defined(SOLID_USE_KQUEUE)
		waittime = d.computeWaitTimeMilliseconds(crttime);
		
		vdbgx(Debug::aio, "kqueue msec = "<<waittime.seconds()<<':'<<waittime.nanoSeconds());
		
		selcnt = kevent(d.reactor_fd, NULL, 0, d.eventvec.data(), d.eventvec.size(), waittime != NanoTime::maximum ? &waittime : NULL);
#endif
		crttime.currentMonotonic();
		
		if(selcnt > 0){
			crtload += selcnt;
			doCompleteIo(crttime, selcnt);
		}else if(selcnt < 0 && errno != EINTR){
			edbgx(Debug::aio, "epoll_wait errno  = "<<last_system_error().message());
			running = false;	
		}else{
			vdbgx(Debug::aio, "epoll_wait done");
		}
		
		crttime.currentMonotonic();
		doCompleteTimer(crttime);
		
		crttime.currentMonotonic();
		doCompleteEvents(crttime);//See NOTE above
		doCompleteExec(crttime);
		
		running = d.running || (d.objcnt != 0) || !d.exeq.empty();
	}
	d.eventobj.stop();
	doClearSpecific();
	idbgx(Debug::aio, "<exit>");
	(void)waitmsec;
	(void)waittime;
}

//-----------------------------------------------------------------------------
#if defined(SOLID_USE_EPOLL)
inline ReactorEventsE systemEventsToReactorEvents(const uint32_t _events){
	ReactorEventsE	retval = ReactorEventNone;

	switch(_events){
		case EPOLLIN:
			retval = ReactorEventRecv;break;
		case EPOLLOUT:
			retval = ReactorEventSend;break;
		case EPOLLOUT | EPOLLIN:
			retval = ReactorEventRecvSend;break;
		case EPOLLPRI:
			retval = ReactorEventOOB;break;
		case EPOLLOUT | EPOLLPRI:
			retval = ReactorEventOOBSend;break;
		case EPOLLERR:
			retval = ReactorEventError;break;
		case EPOLLHUP:
		case EPOLLHUP | EPOLLOUT:
		case EPOLLHUP | EPOLLIN:
		case EPOLLHUP | EPOLLERR | EPOLLIN | EPOLLOUT:
		case EPOLLIN  | EPOLLOUT | EPOLLHUP:
		case EPOLLERR | EPOLLOUT | EPOLLIN:
			retval = ReactorEventHangup;break;
		case EPOLLRDHUP:
			retval = ReactorEventRecvHangup;break;
			
		default:
			SOLID_ASSERT(false);
			break;
	}
	return retval;
}

#elif defined(SOLID_USE_KQUEUE)

inline ReactorEventsE systemEventsToReactorEvents(const unsigned short _flags, const short _filter){
	ReactorEventsE	retval = ReactorEventNone;
	if(_flags & (EV_EOF | EV_ERROR)){
		return ReactorEventHangup;
	}
	if(_filter == EVFILT_READ){
		return ReactorEventRecv;
	}else if(_filter == EVFILT_WRITE){
		return ReactorEventSend;
	}else if(_filter == EVFILT_USER){
		return ReactorEventRecv;
	}

	return retval;
}
#endif


//-----------------------------------------------------------------------------
#if defined(SOLID_USE_EPOLL)
inline uint32_t reactorRequestsToSystemEvents(const ReactorWaitRequestsE _requests){
	uint32_t evs = 0;
	switch(_requests){
		case ReactorWaitNone:
			break;
		case ReactorWaitRead:
			evs = EPOLLET | EPOLLIN;
			break;
		case ReactorWaitWrite:
			evs = EPOLLET | EPOLLOUT;
			break;
		case ReactorWaitReadOrWrite:
			evs = EPOLLET | EPOLLIN | EPOLLOUT;
			break;
		default:
			SOLID_ASSERT(false);
	}
	return evs;
}
#endif
//-----------------------------------------------------------------------------

UniqueId Reactor::objectUid(ReactorContext const &_rctx)const{
	return UniqueId(_rctx.object_index_, d.objdq[_rctx.object_index_].unique);
}

//-----------------------------------------------------------------------------

Service& Reactor::service(ReactorContext const &_rctx)const{
	return *d.objdq[_rctx.object_index_].psvc;
}

//-----------------------------------------------------------------------------

Object& Reactor::object(ReactorContext const &_rctx)const{
	return *d.objdq[_rctx.object_index_].objptr;
}

//-----------------------------------------------------------------------------

CompletionHandler *Reactor::completionHandler(ReactorContext const &_rctx)const{
	return d.chdq[_rctx.channel_index_].pch;
}

//-----------------------------------------------------------------------------

void Reactor::doPost(ReactorContext &_rctx, Reactor::EventFunctionT  &_revfn, Event &&_uev){
	vdbgx(Debug::aio, "exeq "<<d.exeq.size());
	d.exeq.push(ExecStub(_rctx.objectUid(), std::move(_uev)));
	d.exeq.back().exefnc = std::move(_revfn);
	d.exeq.back().chnuid = d.dummyCompletionHandlerUid();
}

//-----------------------------------------------------------------------------

void Reactor::doPost(ReactorContext &_rctx, Reactor::EventFunctionT  &_revfn, Event &&_uev, CompletionHandler const &_rch){
	vdbgx(Debug::aio, "exeq "<<d.exeq.size()<<' '<<&_rch);
	d.exeq.push(ExecStub(_rctx.objectUid(), std::move(_uev)));
	d.exeq.back().exefnc = std::move(_revfn);
	d.exeq.back().chnuid = UniqueId(_rch.idxreactor, d.chdq[_rch.idxreactor].unique);
}

//-----------------------------------------------------------------------------

/*static*/ void Reactor::stop_object(ReactorContext &_rctx, Event &&){
	Reactor			&rthis = _rctx.reactor();
	rthis.doStopObject(_rctx);
}

//-----------------------------------------------------------------------------

/*static*/ void Reactor::stop_object_repost(ReactorContext &_rctx, Event &&){
	Reactor			&rthis = _rctx.reactor();
	
	rthis.d.exeq.push(ExecStub(_rctx.objectUid()));
	rthis.d.exeq.back().exefnc = &stop_object;
	rthis.d.exeq.back().chnuid = rthis.d.dummyCompletionHandlerUid();
}

//-----------------------------------------------------------------------------

/*NOTE:
	We do not stop the object rightaway - we make sure that any
	pending Events are delivered to the object before we stop
*/
void Reactor::postObjectStop(ReactorContext &_rctx){
	d.exeq.push(ExecStub(_rctx.objectUid()));
	d.exeq.back().exefnc = &stop_object_repost;
	d.exeq.back().chnuid = d.dummyCompletionHandlerUid();
}

//-----------------------------------------------------------------------------

void Reactor::doStopObject(ReactorContext &_rctx){
	ObjectStub		&ros = this->d.objdq[_rctx.object_index_];
	
	this->stopObject(*ros.objptr, ros.psvc->manager());
	
	ros.objptr.clear();
	ros.psvc = nullptr;
 	++ros.unique;
	--this->d.objcnt;
	this->d.freeuidvec.push_back(UniqueId(_rctx.object_index_, ros.unique));
}

//-----------------------------------------------------------------------------

void Reactor::doCompleteIo(NanoTime  const &_rcrttime, const size_t _sz){
	ReactorContext	ctx(*this, _rcrttime);
	
	vdbgx(Debug::aio, "selcnt = "<<_sz);
	
	for(size_t i = 0; i < _sz; ++i){
#if defined(SOLID_USE_EPOLL)
		epoll_event				&rev = d.eventvec[i];
		CompletionHandlerStub	&rch = d.chdq[rev.data.u64];
		
		ctx.reactor_event_ = systemEventsToReactorEvents(rev.events);
		ctx.channel_index_ =  rev.data.u64;
#elif defined(SOLID_USE_KQUEUE)
		struct kevent			&rev = d.eventvec[i];
		CompletionHandlerStub	&rch = d.chdq[voidToIndex(rev.udata)];
		
		ctx.reactor_event_ = systemEventsToReactorEvents(rev.flags, rev.filter);
		ctx.channel_index_ = voidToIndex(rev.udata);
#endif
		ctx.object_index_ = rch.objidx;
		
		rch.pch->handleCompletion(ctx);
		ctx.clearError();
	}
}

//-----------------------------------------------------------------------------

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
		
	_rctx.reactor_event_ = ReactorEventTimer;
	_rctx.channel_index_ =  _chidx;
	_rctx.object_index_ = rch.objidx;
	
	rch.pch->handleCompletion(_rctx);
	_rctx.clearError();
}

//-----------------------------------------------------------------------------

void Reactor::doCompleteTimer(NanoTime  const &_rcrttime){
	ReactorContext	ctx(*this, _rcrttime);
	TimerCallback 	tcbk(*this, ctx);
	d.timestore.pop(_rcrttime, tcbk, ChangeTimerIndexCallback(*this));
}

//-----------------------------------------------------------------------------

void Reactor::doCompleteExec(NanoTime  const &_rcrttime){
	ReactorContext	ctx(*this, _rcrttime);
	size_t			sz = d.exeq.size();
	
	while(sz--){

		vdbgx(Debug::aio, sz<<" qsz = "<<d.exeq.size());

		ExecStub				&rexe(d.exeq.front());
		ObjectStub				&ros(d.objdq[rexe.objuid.index]);
		CompletionHandlerStub	&rcs(d.chdq[rexe.chnuid.index]);
		
		if(ros.unique == rexe.objuid.unique && rcs.unique == rexe.chnuid.unique){
			ctx.clearError();
			ctx.channel_index_ = rexe.chnuid.index;
			ctx.object_index_ = rexe.objuid.index;
			rexe.exefnc(ctx, std::move(rexe.event));
		}
		d.exeq.pop();
	}
}

//-----------------------------------------------------------------------------

void Reactor::doCompleteEvents(NanoTime  const &_rcrttime){
	ReactorContext	ctx(*this, _rcrttime);
	doCompleteEvents(ctx);
}

//-----------------------------------------------------------------------------

void Reactor::doCompleteEvents(ReactorContext const &_rctx){
	vdbgx(Debug::aio, "");
	
	if(d.crtpushvecsz || d.crtraisevecsz){
		size_t		crtpushvecidx;
		size_t 		crtraisevecidx;
		{
			unique_lock<std::mutex>		lock(d.mtx);
			
			crtpushvecidx = d.crtpushtskvecidx;
			crtraisevecidx = d.crtraisevecidx;
			
			d.crtpushtskvecidx = ((crtpushvecidx + 1) & 1);
			d.crtraisevecidx = ((crtraisevecidx + 1) & 1);
			
			for(auto it = d.freeuidvec.begin(); it != d.freeuidvec.end(); ++it){
				this->pushUid(*it);
			}
			d.freeuidvec.clear();
			
			d.crtpushvecsz = d.crtraisevecsz = 0;
		}
		
		NewTaskVectorT		&crtpushvec = d.pushtskvec[crtpushvecidx];
		RaiseEventVectorT	&crtraisevec = d.raisevec[crtraisevecidx];
		
		ReactorContext		ctx(_rctx);
		
		d.objcnt += crtpushvec.size();
		
		vdbgx(Debug::aio, d.exeq.size());
		
		for(auto it = crtpushvec.begin(); it != crtpushvec.end(); ++it){
			
			NewTaskStub		&rnewobj(*it);
			if(rnewobj.uid.index >= d.objdq.size()){
				d.objdq.resize(rnewobj.uid.index + 1);
			}
			ObjectStub 		&ros = d.objdq[rnewobj.uid.index];
			SOLID_ASSERT(ros.unique == rnewobj.uid.unique);
			ros.objptr = std::move(rnewobj.objptr);
			ros.psvc = &rnewobj.rsvc;
			
			ctx.clearError();
			ctx.channel_index_ =  InvalidIndex();
			ctx.object_index_ = rnewobj.uid.index;
			
			ros.objptr->registerCompletionHandlers();
			
			d.exeq.push(ExecStub(rnewobj.uid, &call_object_on_event, d.dummyCompletionHandlerUid(), std::move(rnewobj.event)));
		}
		
		vdbgx(Debug::aio, d.exeq.size());
		crtpushvec.clear();
		
		for(auto it = crtraisevec.begin(); it != crtraisevec.end(); ++it){
			RaiseEventStub	&revent = *it;
			d.exeq.push(ExecStub(revent.uid, &call_object_on_event, d.dummyCompletionHandlerUid(), std::move(revent.event)));
		}
		
		vdbgx(Debug::aio, d.exeq.size());
		
		crtraisevec.clear();
	}
}

//-----------------------------------------------------------------------------

/*static*/ void Reactor::call_object_on_event(ReactorContext &_rctx, Event &&_uevent){
	_rctx.object().onEvent(_rctx, std::move(_uevent));
}

//-----------------------------------------------------------------------------

/*static*/ void Reactor::increase_event_vector_size(ReactorContext &_rctx, Event &&/*_rev*/){
	Reactor &rthis = _rctx.reactor();
	
	idbgx(Debug::aio, ""<<rthis.d.devcnt<<" >= "<<rthis.d.eventvec.size());
	
	if(rthis.d.devcnt >= rthis.d.eventvec.size()){
		rthis.d.eventvec.resize(rthis.d.devcnt);
		rthis.d.eventvec.resize(rthis.d.eventvec.capacity());
	}
}

//-----------------------------------------------------------------------------

bool Reactor::waitDevice(ReactorContext &_rctx, CompletionHandler const &_rch, Device const &_rsd, const ReactorWaitRequestsE _req){
	idbgx(Debug::aio, _rsd.descriptor());
#if defined(SOLID_USE_EPOLL)
	epoll_event ev;
	
	ev.data.u64 = _rch.idxreactor;
	ev.events = reactorRequestsToSystemEvents(_req);
	
	if(epoll_ctl(d.reactor_fd, EPOLL_CTL_MOD, _rsd.Device::descriptor(), &ev)){
		edbgx(Debug::aio, "epoll_ctl: "<<last_system_error().message());
		return false;
	}
#elif defined(SOLID_USE_KQUEUE)
	int				read_flags = 0;
	int				write_flags = 0;
	struct kevent	ev[2];
	
	switch(_req){
		case ReactorWaitNone:
			read_flags |= EV_DISABLE;
			write_flags |= EV_DISABLE;
		case ReactorWaitRead:
			read_flags |= (EV_ENABLE | EV_CLEAR);
			write_flags |= EV_DISABLE;
			break;
		case ReactorWaitWrite:
			read_flags |= EV_DISABLE;
			write_flags |= (EV_ENABLE | EV_CLEAR);
			break;
		case ReactorWaitReadOrWrite:
			read_flags |= (EV_ENABLE | EV_CLEAR);
			write_flags |= (EV_ENABLE | EV_CLEAR);
			break;
		case ReactorWaitUser:{
			
			struct kevent ev;
			
			EV_SET( &ev, _rsd.descriptor(), EVFILT_USER, EV_ADD, 0, 0, indexToVoid(_rch.idxreactor));
			
			if(kevent (d.reactor_fd, &ev, 1, NULL, 0, NULL)){
				edbgx(Debug::aio, "kevent: "<<last_system_error().message());
				SOLID_ASSERT(false);
				return false;
			}else{
				++d.devcnt;
				if(d.devcnt == (d.eventvec.size() + 1)){
					d.eventobj.post(_rctx, &Reactor::increase_event_vector_size);
				}
			}
			return true;
		}
		default:
			SOLID_ASSERT(false);
			return false;
	}
	
	EV_SET (&ev[0], _rsd.descriptor(), EVFILT_READ,  read_flags, 0, 0, indexToVoid(_rch.idxreactor));
	EV_SET (&ev[1], _rsd.descriptor(), EVFILT_WRITE, write_flags, 0, 0, indexToVoid(_rch.idxreactor));
	
	if(kevent (d.reactor_fd, ev, 2, NULL, 0, NULL)){
		edbgx(Debug::aio, "kevent: "<<last_system_error().message());
		SOLID_ASSERT(false);
		return false;
	}else{
		++d.devcnt;
		if(d.devcnt == (d.eventvec.size() + 1)){
			d.eventobj.post(_rctx, &Reactor::increase_event_vector_size);
		}
	}
#endif
	return true;
}

//-----------------------------------------------------------------------------

bool Reactor::addDevice(ReactorContext &_rctx, CompletionHandler const &_rch, Device const &_rsd, const ReactorWaitRequestsE _req){
	idbgx(Debug::aio, _rsd.descriptor());
#if defined(SOLID_USE_EPOLL)
	epoll_event ev;
	ev.data.u64 = _rch.idxreactor;
	ev.events = reactorRequestsToSystemEvents(_req);
	
	if(epoll_ctl(d.reactor_fd, EPOLL_CTL_ADD, _rsd.Device::descriptor(), &ev)){
		edbgx(Debug::aio, "epoll_ctl: "<<last_system_error().message());
		return false;
	}else{
		++d.devcnt;
		if(d.devcnt == (d.eventvec.size() + 1)){
			d.eventobj.post(_rctx, &Reactor::increase_event_vector_size);
		}
		
	}
#elif defined(SOLID_USE_KQUEUE)
	int read_flags = EV_ADD;
	int write_flags = EV_ADD;
	
	switch(_req){
		case ReactorWaitNone:
			read_flags |= EV_DISABLE;
			write_flags |= EV_DISABLE;
		case ReactorWaitRead:
			read_flags |= EV_ENABLE;
			write_flags |= EV_DISABLE;
			break;
		case ReactorWaitWrite:
			read_flags |= EV_DISABLE;
			write_flags |= EV_ENABLE;
			break;
		case ReactorWaitReadOrWrite:
			read_flags |= EV_ENABLE;
			write_flags |= EV_ENABLE;
			break;
		case ReactorWaitUser:{
			struct kevent ev;
			EV_SET( &ev, _rsd.descriptor(), EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, indexToVoid(_rch.idxreactor));
			if(kevent (d.reactor_fd, &ev, 1, NULL, 0, NULL)){
				edbgx(Debug::aio, "kevent: "<<last_system_error().message());
				SOLID_ASSERT(false);
				return false;
			}else{
				++d.devcnt;
				if(d.devcnt == (d.eventvec.size() + 1)){
					d.eventobj.post(_rctx, &Reactor::increase_event_vector_size);
				}
			}
			return true;
		}
		default:
			SOLID_ASSERT(false);
			return false;
	}
	
	struct kevent ev[2];
	EV_SET (&ev[0], _rsd.descriptor(), EVFILT_READ,  read_flags | EV_CLEAR, 0, 0, indexToVoid(_rch.idxreactor));
	EV_SET (&ev[1], _rsd.descriptor(), EVFILT_WRITE, write_flags | EV_CLEAR, 0, 0, indexToVoid(_rch.idxreactor));
	if(kevent (d.reactor_fd, ev, 2, NULL, 0, NULL)){
		edbgx(Debug::aio, "kevent: "<<last_system_error().message());
		SOLID_ASSERT(false);
		return false;
	}else{
		++d.devcnt;
		if(d.devcnt == (d.eventvec.size() + 1)){
			d.eventobj.post(_rctx, &Reactor::increase_event_vector_size);
		}
	}
#endif
	return true;
}

//-----------------------------------------------------------------------------

bool Reactor::remDevice(CompletionHandler const &_rch, Device const &_rsd){
	idbgx(Debug::aio, _rsd.descriptor());
#if defined(SOLID_USE_EPOLL)
	epoll_event ev;
	
	if(!_rsd.ok()){
		return false;
	}
	
	if(epoll_ctl(d.reactor_fd, EPOLL_CTL_DEL, _rsd.Device::descriptor(), &ev)){
		edbgx(Debug::aio, "epoll_ctl: "<<last_system_error().message());
		return false;
	}else{
		--d.devcnt;
	}
#elif defined(SOLID_USE_KQUEUE)
	struct kevent ev[2];
	
	if(_rsd.ok()){
		EV_SET (&ev[0], _rsd.descriptor(), EVFILT_READ,  EV_DELETE, 0, 0, 0);
		EV_SET (&ev[1], _rsd.descriptor(), EVFILT_WRITE, EV_DELETE, 0, 0, 0);
		if(kevent (d.reactor_fd, ev, 2, NULL, 0, NULL)){
			edbgx(Debug::aio, "kevent: "<<last_system_error().message());
			SOLID_ASSERT(false);
			return false;
		}else{
			--d.devcnt;
		}
	}else{
		EV_SET( ev, _rsd.descriptor(), EVFILT_USER, EV_DELETE, 0, 0, 0 );
		if(kevent(d.reactor_fd, ev, 1, NULL, 0, NULL )){
			edbgx(Debug::aio, "kevent: "<<last_system_error().message());
			SOLID_ASSERT(false);
			return false;
		}else{
			--d.devcnt;
		}
	}
#endif
	return true;
}

//-----------------------------------------------------------------------------

bool Reactor::addTimer(CompletionHandler const &_rch, NanoTime const &_rt, size_t &_rstoreidx){
	if(_rstoreidx != InvalidIndex()){
		size_t idx = d.timestore.change(_rstoreidx, _rt);
		SOLID_ASSERT(idx == _rch.idxreactor);
	}else{
		_rstoreidx = d.timestore.push(_rt, _rch.idxreactor);
	}
	return true;
}

//-----------------------------------------------------------------------------

void Reactor::doUpdateTimerIndex(const size_t _chidx, const size_t _newidx, const size_t _oldidx){
	CompletionHandlerStub &rch = d.chdq[_chidx];
	SOLID_ASSERT(static_cast<Timer*>(rch.pch)->storeidx == _oldidx);
	static_cast<Timer*>(rch.pch)->storeidx = _newidx;
}

//-----------------------------------------------------------------------------

bool Reactor::remTimer(CompletionHandler const &_rch, size_t const &_rstoreidx){
	if(_rstoreidx != InvalidIndex()){
		d.timestore.pop(_rstoreidx, ChangeTimerIndexCallback(*this));
	}
	return true;
}

//-----------------------------------------------------------------------------

void Reactor::registerCompletionHandler(CompletionHandler &_rch, Object const &_robj){
	size_t					idx;
	
	if(d.chposcache.size()){
		idx = d.chposcache.top();
		d.chposcache.pop();
	}else{
		idx = d.chdq.size();
		d.chdq.push_back(CompletionHandlerStub());
	}
	
	CompletionHandlerStub	&rcs = d.chdq[idx];
	
	rcs.objidx = _robj.ObjectBase::runId().index;
	rcs.pch =  &_rch;
	
	_rch.idxreactor = idx;
	
	idbgx(Debug::aio, "idx "<<idx<<" chdq.size = "<<d.chdq.size()<<" this "<<this);
	
	{
		NanoTime		dummytime;
		ReactorContext	ctx(*this, dummytime);
		
		ctx.reactor_event_ = ReactorEventInit;
		ctx.object_index_ = rcs.objidx;
		ctx.channel_index_ = idx;
		
		_rch.handleCompletion(ctx);
	}
}

//-----------------------------------------------------------------------------

void Reactor::unregisterCompletionHandler(CompletionHandler &_rch){
	idbgx(Debug::aio, "");
	
	CompletionHandlerStub &rcs = d.chdq[_rch.idxreactor];
	
	{
		NanoTime		dummytime;
		ReactorContext	ctx(*this, dummytime);
		
		ctx.reactor_event_ = ReactorEventClear;
		ctx.object_index_ = rcs.objidx;
		ctx.channel_index_ = _rch.idxreactor;
		
		_rch.handleCompletion(ctx);
	}
	
	d.chposcache.push(_rch.idxreactor);
	rcs.pch = &d.eventobj.dummyhandler;
	rcs.objidx = 0;
	++rcs.unique;
}


//-----------------------------------------------------------------------------

thread_local Reactor	*thread_local_reactor = nullptr;

/*static*/ Reactor* Reactor::safeSpecific(){
	return thread_local_reactor;
}

void Reactor::doStoreSpecific(){
	thread_local_reactor = this;
}
void Reactor::doClearSpecific(){
	thread_local_reactor = nullptr;
}

/*static*/ Reactor& Reactor::specific(){
	vdbgx(Debug::aio, "");
	return *safeSpecific();
}


//-----------------------------------------------------------------------------
//		ReactorContext
//-----------------------------------------------------------------------------

Object& ReactorContext::object()const{
	return reactor().object(*this);
}

//-----------------------------------------------------------------------------

Service& ReactorContext::service()const{
	return reactor().service(*this);
}

//-----------------------------------------------------------------------------

UniqueId ReactorContext::objectUid()const{
	return reactor().objectUid(*this);
}

//-----------------------------------------------------------------------------

CompletionHandler*  ReactorContext::completionHandler()const{
	return reactor().completionHandler(*this);
}

//-----------------------------------------------------------------------------
}//namespace aio
}//namespace frame
}//namespace solid

