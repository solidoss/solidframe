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
	static void on_completion(CompletionHandler&, ReactorContext &);
	
	EventHandler(ObjectProxy const &_rop): CompletionHandler(_rop, &on_completion){}
	
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


/*static*/ void EventHandler::on_completion(CompletionHandler& _rch, ReactorContext &_rctx){
	EventHandler &rthis = static_cast<EventHandler&>(_rch);
	uint64 v;
	
	rthis.dev.read(reinterpret_cast<char*>(&v), sizeof(v));
	rthis.reactor(_rctx).doCompleteEvents();
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
	
	void init(){
		this->registerCompletionHandler(eventhandler);
		this->registerCompletionHandler(dummyhandler);
	}
	EventHandler			eventhandler;
	CompletionHandler		dummyhandler;
};

struct NewTaskStub{
	NewTaskStub(
		UidT const&_ruid, TaskT const&_rtsk, Service &_rsvc, Event const &_revt
	):uid(_ruid), tsk(_rtsk), rsvc(_rsvc), evt(_revt){}
	
	//NewTaskStub(){}
	UidT		uid;
	TaskT		tsk;
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
	):pch(_pch), waitreq(ReactorWaitNone), objidx(_objidx){}
	
	CompletionHandler		*pch;
	ReactorWaitRequestsE	waitreq;
	size_t					objidx;
};


struct ObjectStub{
	ObjectStub():unique(0), psvc(nullptr){}
	
	UniqueT		unique;
	Service		*psvc;
	TaskT		objptr;
};


enum{
	EventCapacity = 4096
};


typedef std::vector<NewTaskStub>			NewTaskVectorT;
typedef std::vector<RaiseEventStub>			RaiseEventVectorT;
typedef std::vector<epoll_event>			EpollEventVectorT;
typedef std::deque<CompletionHandlerStub>	CompletionHandlerDequeT;
typedef std::vector<UidT>					UidVectorT;
typedef std::deque<ObjectStub>				ObjectDequeT;


struct Reactor::Data{
	Data(
		
	):	epollfd(-1), running(0), crtpushtskvecidx(0),
		crtraisevecidx(0), crtpushvecsz(0), crtraisevecsz(0){}
	
	int computeWaitTimeMilliseconds()const{
		//TODO:
		return -1;
	}
	
	int							epollfd;
	AtomicBoolT					running;
	size_t						crtpushtskvecidx;
	size_t						crtraisevecidx;
	AtomicSizeT					crtpushvecsz;
	AtomicSizeT					crtraisevecsz;
	
	Mutex						mtx;
	EpollEventVectorT			eventvec;
	NewTaskVectorT				pushtskvec[2];
	RaiseEventVectorT			raisevec[2];
	EventObject					eventobj;
	CompletionHandlerDequeT		chdq;
	UidVectorT					freeuidvec;
	ObjectDequeT				objdq;
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
	vdbgx(Debug::aio, "");
	d.epollfd = epoll_create(EventCapacity);
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
	
	const size_t objidx = runIndex(d.eventobj);
	
	d.chdq.push_back(CompletionHandlerStub(&d.eventobj.eventhandler, objidx));
	d.chdq.push_back(CompletionHandlerStub(&d.eventobj.dummyhandler, objidx));
	
	d.eventobj.init();
	
	d.chdq.front().waitreq = ReactorWaitRead;
	
	epoll_event ev;
	ev.data.u64 = 0;
	ev.events = EPOLLIN | EPOLLET;
	
	if(epoll_ctl(d.epollfd, EPOLL_CTL_ADD, d.eventobj.eventhandler.descriptor(), &ev)){
		edbgx(Debug::aio, "epoll_ctl: "<<last_system_error().message());
		return false;
	}
	d.eventvec.resize(EventCapacity);
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
	vdbgx(Debug::aio, "<exit>");
}

inline ReactorEventsE systemEventsToReactorEvents(const uint32 _sysevents){
	ReactorEventsE	retval = ReactorEventNone;
	switch(_sysevents){
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

Service& Reactor::service(ReactorContext const &_rctx)const{
	return *d.objdq[_rctx.objidx].psvc;
}
	
Object& Reactor::object(ReactorContext const &_rctx)const{
	return *d.objdq[_rctx.objidx].objptr;
}

void Reactor::doCompleteIo(TimeSpec  const &_rcrttime, const size_t _sz){
	ReactorContext	ctx(*this, _rcrttime);
	for(int i = 0; i < _sz; ++i){
		epoll_event				&rev = d.eventvec[i];
		CompletionHandlerStub	&rch = d.chdq[rev.data.u64];
		
		ctx.reactevn = systemEventsToReactorEvents(rev.events);
		ctx.chidx =  rev.data.u64;
		ctx.objidx = rch.objidx;
		
		rch.pch->handleCompletion(ctx);
		ctx.clearError();
	}
}
void Reactor::doCompleteTimer(TimeSpec  const &_rcrttime){
	
}

void Reactor::doCompleteExec(TimeSpec  const &_rcrttime){
	
}
void Reactor::doCompleteEvents(){
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
		
		for(auto it = crtpushvec.begin(); it != crtpushvec.end(); ++it){
			
		}
		crtpushvec.clear();
		
		for(auto it = crtraisevec.begin(); it != crtraisevec.end(); ++it){
			
		}
		crtraisevec.clear();
	}
}

void Reactor::wait(ReactorContext &_rctx, CompletionHandler const *_pch, const ReactorWaitRequestsE _req){
	vdbgx(Debug::aio, "");
}

void Reactor::registerCompletionHandler(CompletionHandler &_rch){
	vdbgx(Debug::aio, "");
}

void Reactor::unregisterCompletionHandler(CompletionHandler &_rch){
	vdbgx(Debug::aio, "");
}

/*static*/ Reactor* Reactor::safeSpecific(){
	vdbgx(Debug::aio, "");
	return NULL;
}

/*static*/ Reactor& Reactor::specific(){
	vdbgx(Debug::aio, "");
	return *safeSpecific();
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

}//namespace aio
}//namespace frame
}//namespace solid

