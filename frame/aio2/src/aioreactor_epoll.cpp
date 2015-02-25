// frame/aio/src/aioselector_epoll.cpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "system/common.hpp"
#include <fcntl.h>
#include <sys/epoll.h>
#include "system/exception.hpp"

#include <sys/eventfd.h>

#include <vector>
#include <deque>
#include <cerrno>
#include <cstring>

#include "system/debug.hpp"
#include "system/timespec.hpp"
#include "system/mutex.hpp"
#include "system/thread.hpp"
#include "system/device.hpp"
#include "system/error.hpp"

#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "frame/object.hpp"
#include "frame/service.hpp"
#include "frame/common.hpp"
#include "frame/event.hpp"

#include "frame/aio2/aioreactor.hpp"
#include "frame/aio2/aioobject.hpp"
#include "frame/aio2/aiocompletion.hpp"
#include "frame/aio2/aioreactorcontext.hpp"





namespace solid{
namespace frame{
namespace aio{


namespace{

void dummy_completion(CompletionHandler&, ReactorContext &){
}

}//namespace


typedef ATOMIC_NS::atomic<bool>		AtomicBoolT;
typedef ATOMIC_NS::atomic<size_t>	AtomicSizeT;
typedef Reactor::TaskT				TaskT;



struct EventHandler: CompletionHandler{
	static void on_init(CompletionHandler&, ReactorContext &);
	static void on_completion(CompletionHandler&, ReactorContext &);
	
	EventHandler(ObjectProxy const &_rop): CompletionHandler(_rop, &on_init){}
	
	void write(){
		const uint64 v = 1;
		dev.write(reinterpret_cast<const char*>(&v), sizeof(v));
	}
	
	bool init();
	int descriptor()const{
		return dev.descriptor();
	}
private:
	Device		dev;
};

/*static*/ void EventHandler::on_init(CompletionHandler& _rch, ReactorContext &_rctx){
	EventHandler &rthis = static_cast<EventHandler&>(_rch);
	rthis.reactor(_rctx).addDevice(_rctx, rthis.dev);
	rthis.reactor(_rctx).waitDevice(_rctx, rthis, rthis.dev, ReactorWaitRead);
	rthis.completionCallback(&on_completion);
}

/*static*/ void EventHandler::on_completion(CompletionHandler& _rch, ReactorContext &_rctx){
	EventHandler &rthis = static_cast<EventHandler&>(_rch);
	uint64	v = -1;
	int 	rv;
	do{
		rv = rthis.dev.read(reinterpret_cast<char*>(&v), sizeof(v));
		idbgx(Debug::aio, "Read from event "<<rv<<" value = "<<v);
	}while(rv == sizeof(v));
	
	rthis.reactor(_rctx).doCompleteEvents(_rctx);
}

bool EventHandler::init(){
	dev = Device(eventfd(0, EFD_NONBLOCK));
	if(!dev.ok()){
		edbgx(Debug::aio, "eventfd: "<<last_system_error().message());
		return false;
	}
	return true;
}

class EventObject: public Object{
public:
	EventObject():eventhandler(proxy()), dummyhandler(proxy(), dummy_completion){
		use();
	}
	
	EventHandler			eventhandler;
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
	):pch(_pch)/*, waitreq(ReactorWaitNone)*/, objidx(_objidx), unique(0){}
	
	CompletionHandler		*pch;
	//ReactorWaitRequestsE	waitreq;
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
	MinEventCapacity = 4,
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
typedef std::vector<epoll_event>			EpollEventVectorT;
typedef std::deque<CompletionHandlerStub>	CompletionHandlerDequeT;
typedef std::vector<UidT>					UidVectorT;
typedef std::deque<ObjectStub>				ObjectDequeT;
typedef Queue<ExecStub>						ExecQueueT;
typedef Stack<size_t>						SizeStackT;


struct Reactor::Data{
	Data(
		
	):	epollfd(-1), running(0), crtpushtskvecidx(0),
		crtraisevecidx(0), crtpushvecsz(0), crtraisevecsz(0), devcnt(0){}
	
	int computeWaitTimeMilliseconds()const{
		return exeq.size() ? 0 : -1;
	}
	
	UidT dummyCompletionHandlerUid()const{
		const size_t idx = eventobj.dummyhandler.idxreactor;
		return UidT(idx, chdq[idx].unique);
	}
	
	int							epollfd;
	AtomicBoolT					running;
	size_t						crtpushtskvecidx;
	size_t						crtraisevecidx;
	AtomicSizeT					crtpushvecsz;
	AtomicSizeT					crtraisevecsz;
	size_t						devcnt;
	
	Mutex						mtx;
	EpollEventVectorT			eventvec;
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
	d.epollfd = epoll_create(MinEventCapacity);
	if(d.epollfd < 0){
		edbgx(Debug::aio, "epoll_create: "<<last_system_error().message());
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
	d.running = true;
	++d.devcnt;
	
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
	}
	if(raisevecsz == 1){
		d.eventobj.eventhandler.write();
	}
	return rv;
}
/*virtual*/ void Reactor::stop(){
	vdbgx(Debug::aio, "");
	d.running = false;
	d.eventobj.eventhandler.write();
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
	}
	
	if(pushvecsz == 1){
		d.eventobj.eventhandler.write();
	}
	return rv;
}


void Reactor::run(){
	vdbgx(Debug::aio, "<enter>");
	int			selcnt;
	bool		running = true;
	TimeSpec	crttime;
	
	while(running){
		selcnt = epoll_wait(d.epollfd, d.eventvec.data(), d.eventvec.size(), d.computeWaitTimeMilliseconds());
		crttime.currentMonotonic();
		if(selcnt > 0){
			doCompleteIo(crttime, selcnt);
		}else if(selcnt < 0 && errno != EINTR){
			edbgx(Debug::aio, "epoll_wait errno  = "<<last_system_error().message());
			running = false;	
		}
		
		doCompleteTimer(crttime);
		
		doCompleteExec(crttime);
		
	}
	doClearSpecific();
	vdbgx(Debug::aio, "<exit>");
}

inline ReactorEventsE systemEventsToReactorEvents(const uint32 _events){
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
			retval = ReactorEventHangup;break;
		case EPOLLRDHUP:
			retval = ReactorEventRecvHangup;break;
		default:
			cassert(false);
			break;
	}
	return retval;
}

inline uint32 reactorRequestsToSystemEvents(const ReactorWaitRequestsE _requests){
	uint32 evs = 0;
	switch(_requests){
		case ReactorWaitNone:
			break;
		case ReactorWaitRead:
			evs = EPOLLET | EPOLLIN;
			break;
		case ReactorWaitWrite:
			evs = EPOLLET | EPOLLOUT;
		case ReactorWaitReadOrWrite:
			evs = EPOLLET | EPOLLIN | EPOLLOUT;
		default:
			cassert(false);
	}
	return evs;
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

void Reactor::doCompleteIo(TimeSpec  const &_rcrttime, const size_t _sz){
	ReactorContext	ctx(*this, _rcrttime);
	for(int i = 0; i < _sz; ++i){
		epoll_event				&rev = d.eventvec[i];
		CompletionHandlerStub	&rch = d.chdq[rev.data.u64];
		
		ctx.reactevn = systemEventsToReactorEvents(rev.events);
		ctx.chnidx =  rev.data.u64;
		ctx.objidx = rch.objidx;
		
		rch.pch->handleCompletion(ctx);
		ctx.clearError();
	}
}
void Reactor::doCompleteTimer(TimeSpec  const &_rcrttime){
	
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
void Reactor::doCompleteEvents(ReactorContext const &_rctx){
	vdbgx(Debug::aio, "");
	
	if(d.crtpushvecsz || d.crtraisevecsz){
		size_t		crtpushvecidx;
		size_t 		crtraisevecidx;
		{
			Locker<Mutex>		lock(d.mtx);
			
			crtpushvecidx = d.crtpushtskvecidx;
			crtraisevecidx = d.crtraisevecidx;
			
			d.crtpushtskvecidx = ((crtpushvecidx + 1) & 1);
			d.crtraisevecidx = ((crtraisevecidx + 1) & 1);
			
			for(auto it = d.freeuidvec.begin(); it != d.freeuidvec.end(); ++it){
				this->pushUid(*it);
			}
			d.freeuidvec.clear();
		}
		
		NewTaskVectorT		&crtpushvec = d.pushtskvec[crtpushvecidx];
		RaiseEventVectorT	&crtraisevec = d.raisevec[crtraisevecidx];
		
		ReactorContext		ctx(_rctx);
		
		for(auto it = crtpushvec.begin(); it != crtpushvec.end(); ++it){
			NewTaskStub		&rnewobj(*it);
			if(rnewobj.uid.index >= d.objdq.size()){
				d.objdq.resize(rnewobj.uid.index + 1);
			}
			ObjectStub 		&ros = d.objdq[rnewobj.uid.index];
			cassert(ros.unique == rnewobj.uid.unique);
			ros.objptr = rnewobj.objptr;
			ros.psvc = &rnewobj.rsvc;
			
			ctx.clearError();
			ctx.chnidx =  -1;
			ctx.objidx = rnewobj.uid.index;
			
			ros.objptr->registerCompletionHandlers();
			
			d.exeq.push(ExecStub(rnewobj.uid, &call_object_on_event, d.dummyCompletionHandlerUid(), rnewobj.evt));
		}
		crtpushvec.clear();
		
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

bool Reactor::waitDevice(ReactorContext &_rctx, CompletionHandler const &_rch, Device const &_rsd, const ReactorWaitRequestsE _req){
	vdbgx(Debug::aio, "");
	//CompletionHandlerStub &rcs = d.chdq[_rch.idxreactor];
	epoll_event ev;
	
	ev.data.u64 = _rctx.chnidx;
	ev.events = reactorRequestsToSystemEvents(_req);
	
	if(epoll_ctl(d.epollfd, EPOLL_CTL_MOD, _rsd.Device::descriptor(), &ev)){
		edbgx(Debug::aio, "epoll_ctl: "<<last_system_error().message());
		return false;
	}else{
		++d.devcnt;
		if(d.devcnt == (d.eventvec.size() + 1)){
			//TODO: schedule increase d.eventvec.size
		}
		
	}
	return true;
}

bool Reactor::addDevice(ReactorContext &_rctx, Device const &_rsd){
	epoll_event ev;
	
	ev.data.u64 = _rctx.chnidx;
	ev.events = EPOLLET;
	
	if(epoll_ctl(d.epollfd, EPOLL_CTL_ADD, _rsd.Device::descriptor(), &ev)){
		edbgx(Debug::aio, "epoll_ctl: "<<last_system_error().message());
		//d.chdq[_rctx.chnidx].waitreq = ReactorWaitError;
		return false;
	}else{
		++d.devcnt;
		if(d.devcnt == (d.eventvec.size() + 1)){
			//TODO: schedule increase d.eventvec.size
		}
		
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
		ctx.objidx = rcs.objidx;
		ctx.chnidx = idx;
		
		_rch.handleCompletion(ctx);
	}
}

void Reactor::unregisterCompletionHandler(CompletionHandler &_rch){
	vdbgx(Debug::aio, "");
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



}//namespace aio
}//namespace frame
}//namespace solid

