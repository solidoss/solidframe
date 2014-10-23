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

#ifndef UPIPESIGNAL
	#ifdef HAS_EVENTFD_H
		#include <sys/eventfd.h>
	#else 
		#define UPIPESIGNAL
	#endif
#endif

#include <vector>
#include <cerrno>
#include <cstring>

#include "system/debug.hpp"
#include "system/timespec.hpp"
#include "system/mutex.hpp"

#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "frame/object.hpp"
#include "frame/common.hpp"

#include "frame/aio2/aioreactor.hpp"
#include "frame/aio2/aioobject.hpp"

#include "frame/aio2/aiosocket.hpp"


namespace solid{
namespace frame{
namespace aio{
//=============================================================
// TODO:
// - investigate if you can use timerfd_create (man timerfd_create)
//
//=============================================================

struct Reactor::Stub{
	enum State{
		InExecQueue,
		OutExecQueue
	};
	Stub():
		timepos(TimeSpec::maximum),
		itimepos(TimeSpec::maximum),
		otimepos(TimeSpec::maximum),
		state(OutExecQueue),
		events(0){
	}
	void reset(){
		timepos = TimeSpec::maximum; 
		state = OutExecQueue;
		events = 0;
	}
	ObjectPointerT	objptr;
	TimeSpec		timepos;//object timepos
	TimeSpec		itimepos;//input timepos
	TimeSpec		otimepos;//output timepos
	State			state;
	uint			events;
};

struct Reactor::Data{
	enum{
		EPOLLMASK = EPOLLET | EPOLLIN | EPOLLOUT,
		MAXPOLLWAIT = 0x7FFFFFFF,
		EXIT_LOOP = 1,
		FULL_SCAN = 2,
		READ_PIPE = 4,
		MAX_EVENTS_COUNT = 1024 * 2,
	};
	typedef Stack<uint32>			Uint32StackT;
	typedef Queue<uint32>			Uint32QueueT;
	typedef std::vector<Stub>		StubVectorT;
	
	ulong				objcp;
	ulong				objsz;
//	ulong				sockcp;
	ulong				socksz;
	int					selcnt;
	int					epollfd;
	epoll_event 		events[MAX_EVENTS_COUNT];
	StubVectorT			stubs;
	Uint32QueueT		execq;
	Uint32StackT		freestubsstk;
#ifdef UPIPESIGNAL
	int					pipefds[2];
#else
	int					efd;//eventfd
	Mutex				m;
	Uint32QueueT		sigq;//a signal queue
	uint64				efdv;//the eventfd value
#endif
	TimeSpec			ntimepos;//next timepos == next timeout
	TimeSpec			ctimepos;//current time pos
	
//reporting data:
	uint				rep_fullscancount;
	
public://methods:
	Data();
	~Data();
	int computeWaitTimeout()const;
	void addNewSocket();
	epoll_event* eventPrepare(epoll_event &_ev, const uint32 _objpos, const uint32 _sockpos);
	void stub(uint32 &_objpos, uint32 &_sockpos, const epoll_event &_ev);
};
//-------------------------------------------------------------
Reactor::Data::Data():
	objcp(0), objsz(0), /*sockcp(0),*/ socksz(0), selcnt(0), epollfd(-1),
	rep_fullscancount(0){
#ifdef UPIPESIGNAL
	pipefds[0] = -1;
	pipefds[1] = -1;
#else
	efd = -1;
	efdv = 0;
#endif
}

Reactor::Data::~Data(){
	if(epollfd >= 0){
		close(epollfd);
	}
#ifdef UPIPESIGNAL
	if(pipefds[0] >= 0){
		close(pipefds[0]);
	}
	if(pipefds[1] >= 0){
		close(pipefds[1]);
	}
#else
	close(efd);
#endif
}

int Reactor::Data::computeWaitTimeout()const{
	if(ntimepos.isMax()) return -1;//return MAXPOLLWAIT;
	int rv = (ntimepos.seconds() - ctimepos.seconds()) * 1000;
	rv += ((long)ntimepos.nanoSeconds() - ctimepos.nanoSeconds()) / 1000000;
	if(rv < 0) return MAXPOLLWAIT;
	return rv;
}
void Reactor::Data::addNewSocket(){
	++socksz;
}
inline epoll_event* Reactor::Data::eventPrepare(
	epoll_event &_ev, const uint32 _objpos, const uint32 _sockpos
){
	//TODO:we are now limited to uint32 objects and uint32 sockets per object
	_ev.data.u64 = _objpos;
	_ev.data.u64 <<= 32;
	_ev.data.u64 |= _sockpos;
	return &_ev;
}
void Reactor::Data::stub(uint32 &_objpos, uint32 &_sockpos, const epoll_event &_ev){
	_objpos = (_ev.data.u64 >> 32);
	_sockpos = (_ev.data.u64 & 0xffffffff);
}
//=============================================================
Reactor::Reactor(SchedulerBase &_rsched):ReactorBase(_rsched), d(*(new Data)){
}
Reactor::~Reactor(){
	delete &d;
}

/*virtual*/ bool Reactor::raise(UidT const& _robjuid, Event const& _re){
	return false;
}
/*virtual*/ void Reactor::stop(){
	
}
/*virtual*/ void Reactor::update(){
	
}
	
bool Reactor::init(size_t _cp){
	return true;
}

void Reactor::prepare(){
	//setCurrentTimeSpecific(d.ctimepos);
}
void Reactor::unprepare(){
}

void Reactor::raise(size_t _pos){
}

size_t Reactor::capacity()const{
	return d.objcp;
}
size_t Reactor::size() const{
	return d.objsz;
}
bool Reactor::empty()const{
	return d.objsz == 1;
}
bool Reactor::full()const{
	return d.objsz == d.objcp;
}

bool Reactor::push(JobT &_objptr){
	return true;
}

inline ulong Reactor::doExecuteQueue(){
	ulong		flags = 0;
	ulong		qsz(d.execq.size());
	while(qsz){//we only do a single scan:
		const uint pos = d.execq.front();
		d.execq.pop();
		--qsz;
		flags |= doExecute(pos);
	}
	return flags;
}


void Reactor::run(){
	static const int	maxnbcnt = 16;
	uint 				flags;
	int					nbcnt = -1;	//non blocking opperations count,
									//used to reduce the number of calls for the system time.
	int 				pollwait = 0;
	
	do{
		flags = 0;
		if(nbcnt < 0){
			d.ctimepos.currentMonotonic();
			nbcnt = maxnbcnt;
		}
		
		if(d.selcnt){
			--nbcnt;
			flags |= doAllIo();
		}
		
		if(flags & Data::READ_PIPE){
			--nbcnt;
			flags |= doReadPipe();
		}
		
		if(d.ctimepos >= d.ntimepos || (flags & Data::FULL_SCAN)){
			nbcnt -= 4;
			flags |= doFullScan();
		}
		
		if(d.execq.size()){
			nbcnt -= d.execq.size();
			flags |= doExecuteQueue();
		}
		
		if(empty()) flags |= Data::EXIT_LOOP;
		
		if(flags || d.execq.size()){
			pollwait = 0;
			--nbcnt;
		}else{
			pollwait = d.computeWaitTimeout();
			vdbgx(Debug::aio, "pollwait "<<pollwait<<" ntimepos.s = "<<d.ntimepos.seconds()<<" ntimepos.ns = "<<d.ntimepos.nanoSeconds());
			vdbgx(Debug::aio, "ctimepos.s = "<<d.ctimepos.seconds()<<" ctimepos.ns = "<<d.ctimepos.nanoSeconds());
			nbcnt = -1;
        }
		
		d.selcnt = epoll_wait(d.epollfd, d.events, Data::MAX_EVENTS_COUNT, pollwait);
		vdbgx(Debug::aio, "epollwait = "<<d.selcnt);
		if(d.selcnt < 0) d.selcnt = 0;
	}while(!(flags & Data::EXIT_LOOP));
}

//-------------------------------------------------------------
ulong Reactor::doReadPipe(){
	return 0;
}
void Reactor::doUnregisterObject(Object &_robj, int _lastfailpos){
}

inline ulong Reactor::doIo(Socket &_rsock, ulong _evs, ulong){
	return 0;
}

ulong Reactor::doAllIo(){
	return 0;
}
void Reactor::doFullScanCheck(Stub &_rstub, const ulong _pos){
}
ulong Reactor::doFullScan(){
	++d.rep_fullscancount;
	idbgx(Debug::aio, "fullscan count "<<d.rep_fullscancount);
	d.ntimepos = TimeSpec::maximum;
	for(Data::StubVectorT::iterator it(d.stubs.begin()); it != d.stubs.end(); it += 4){
		if(!it->objptr.empty()){
			doFullScanCheck(*it, it - d.stubs.begin());
		}
		if(!(it + 1)->objptr.empty()){
			doFullScanCheck(*(it + 1), it - d.stubs.begin() + 1);
		}
		if(!(it + 2)->objptr.empty()){
			doFullScanCheck(*(it + 2), it - d.stubs.begin() + 2);
		}
		if(!(it + 3)->objptr.empty()){
			doFullScanCheck(*(it + 3), it - d.stubs.begin() + 3);
		}
	}
	return 0;
}
ulong Reactor::doExecute(const ulong _pos){
	return 0;
}
void Reactor::doPrepareObjectWait(const size_t _pos, const TimeSpec &_timepos){
	
}

ulong Reactor::doAddNewStub(){
	ulong pos = 0;
	
	return pos;
}

//-------------------------------------------------------------
}//namespace aio
}//namespace frame
}//namespace solid

