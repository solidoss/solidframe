/* Implementation file selector.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <vector>
#include <cerrno>
#include <cstring>

#include "system/debug.hpp"
#include "system/timespec.hpp"

#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "core/object.hpp"
#include "core/common.hpp"

#include "clientserver/aio/aioselector.hpp"
#include "clientserver/aio/aioobject.hpp"
#include "aiosocket.hpp"


namespace clientserver{

namespace aio{
//=============================================================

struct Selector::Stub{
	enum State{
		InExecQueue,
		OutExecQueue
	};
	Stub():timepos(0xffffffff, 0xffffffff), state(OutExecQueue), events(0){}
	void reset(){
		timepos.set(0xffffffff, 0xffffffff);
		state = OutExecQueue;
		events = 0;
	}
	ObjectPtrTp	objptr;
	TimeSpec	timepos;
	State		state;
	uint		events;
};

struct Selector::Data{
	enum{
		EPOLLMASK = EPOLLET | EPOLLIN | EPOLLOUT,
		MAXTIMEPOS = 0xffffffff,
		MAXPOLLWAIT = 0x7FFFFFFF,
		EXIT_LOOP = 1,
		FULL_SCAN = 2,
		READ_PIPE = 4
	};
	typedef Stack<uint32>			PositionStackTp;
	typedef Queue<uint32>			PositionQueueTp;
	typedef std::vector<Stub>		StubVectorTp;
	
	uint				objcp;
	uint				objsz;
	uint				sockcp;
	uint				socksz;
	int					selcnt;
	int					epollfd;
	epoll_event 		*events;
	StubVectorTp		stubs;
	PositionQueueTp		execq;
	PositionStackTp		freestubsstk;
	int					pipefds[2];
	TimeSpec			ntimepos;//next timepos == next timeout
	TimeSpec			ctimepos;//current time pos
	
//reporting data:
	uint				rep_fullscancount;
	
public://methods:
	Data();
	~Data();
	int computeWaitTimeout()const;
	void addNewSocket();
	uint newStub();
	epoll_event* eventPrepare(epoll_event &_ev, const uint32 _objpos, const uint32 _sockpos);
	void stub(uint32 &_objpos, uint32 &_sockpos, const epoll_event &_ev);
};
//-------------------------------------------------------------
Selector::Data::Data():
	objcp(0), objsz(0), sockcp(0), socksz(0), selcnt(0), epollfd(-1), events(NULL),
	rep_fullscancount(0){
	pipefds[0] = -1;
	pipefds[1] = -1;
}

Selector::Data::~Data(){
	delete []events;
	if(epollfd >= 0){
		close(epollfd);
	}
}

int Selector::Data::computeWaitTimeout()const{
	if(ntimepos.seconds() == MAXTIMEPOS) return -1;//return MAXPOLLWAIT;
	int rv = (ntimepos.seconds() - ctimepos.seconds()) * 1000;
	rv += ((long)ntimepos.nanoSeconds() - ctimepos.nanoSeconds()) / 1000000;
	if(rv < 0) return MAXPOLLWAIT;
	return rv;
}
void Selector::Data::addNewSocket(){
	++socksz;
	if(socksz > sockcp){
		uint oldcp = sockcp;
		sockcp += 64;//TODO: improve!!
		epoll_event *pevs = new epoll_event[sockcp];
		memcpy(pevs, events, oldcp * sizeof(epoll_event));
		delete []events;
// 		for(uint i = 0; i < sockcp; ++i){
// 			pevs[i].events = 0;
// 			pevs[i].data.u64 = 0;
// 		}
	}
}
uint Selector::Data::newStub(){
	uint pos = 0;
	if(this->freestubsstk.size()){
		pos = freestubsstk.top();
		freestubsstk.pop();
	}else{
		uint cp = stubs.capacity();
		pos = stubs.size();
		stubs.push_back(Stub());
		if(cp != stubs.capacity()){
			//we need to reset the aioobject's pointer to timepos
			for(Data::StubVectorTp::iterator it(stubs.begin()); it != stubs.end(); ++it){
				if(it->objptr){
					it->objptr->ptimepos = &it->timepos;
				}
			}
		}
	}
	return pos;
}
inline epoll_event* Selector::Data::eventPrepare(
	epoll_event &_ev, const uint32 _objpos, const uint32 _sockpos
){
	_ev.data.u64 = _objpos;
	_ev.data.u64 <<= 32;
	_ev.data.u64 |= _sockpos;
	return &_ev;
}
void Selector::Data::stub(uint32 &_objpos, uint32 &_sockpos, const epoll_event &_ev){
	_objpos = (_ev.data.u64 >> 32);
	_sockpos = (_ev.data.u64 & 0xffffffff);
}
//=============================================================
Selector::Selector():d(*(new Data)){
}
Selector::~Selector(){
	delete &d;
}
int Selector::reserve(uint _cp){
	cassert(_cp);
	d.objcp = _cp;
	d.sockcp = _cp;
	
	//first create the epoll descriptor:
	cassert(d.epollfd < 0);
	d.epollfd = epoll_create(d.sockcp);
	if(d.epollfd < 0){
		edbgx(Dbg::aio, "epoll_create: "<<strerror(errno));
		cassert(false);
		return BAD;
	}
	
	//next create the pipefds:
	cassert(d.pipefds[0] < 0 && d.pipefds[1] < 0);
	if(pipe(d.pipefds)){
		edbgx(Dbg::aio, "pipe: "<<strerror(errno));
		cassert(false);
		return BAD;
	}
	
	//make the pipes nonblocking
	fcntl(d.pipefds[0], F_SETFL, O_NONBLOCK);
	fcntl(d.pipefds[1], F_SETFL, O_NONBLOCK);
	
	//register the pipes onto epoll
	epoll_event ev;
	ev.data.u64 = 0;
	ev.events = EPOLLIN | EPOLLPRI;//must be LevelTriggered
	if(epoll_ctl(d.epollfd, EPOLL_CTL_ADD, d.pipefds[0], &ev)){
		edbgx(Dbg::aio, "epoll_ctl: "<<strerror(errno));
		cassert(false);
		return BAD;
	}
	
	//allocate the events
	d.events = new epoll_event[d.sockcp];
	for(uint i = 0; i < d.sockcp; ++i){
		d.events[i].events = 0;
		d.events[i].data.u64 = 0L;
	}
	//We need to have the stubs preallocated
	//because of aio::Object::ptimeout
	d.stubs.reserve(d.objcp);
	//add the pipe stub:
	d.stubs.push_back(Stub());
	
	d.ctimepos.set(0);
	d.ntimepos.set(Data::MAXTIMEPOS);
	d.objsz = 1;
	d.socksz = 1;
	return OK;
}

void Selector::prepare(){
}
void Selector::unprepare(){
}

void Selector::signal(uint _pos)const{
	idbgx(Dbg::aio, "signal connection: "<<_pos);
	write(d.pipefds[1], &_pos, sizeof(uint));
}

uint Selector::capacity()const{
	return d.objcp - 1;
}
uint Selector::size() const{
	return d.objsz;
}
bool Selector::empty()const{
	return d.objsz == 1;
}
bool Selector::full()const{
	return d.objsz == d.objcp;
}

void Selector::push(const ObjectTp &_objptr, uint _thid){
	cassert(!full());
	uint stubpos = d.newStub();
	Stub &stub = d.stubs[stubpos];
	
	_objptr->setThread(_thid, stubpos);
	_objptr->ptimepos = &stub.timepos;
	
	stub.timepos.set(Data::MAXTIMEPOS);
	
	//add events for every socket
	bool fail = false;
	uint failpos = 0;
	{
		epoll_event ev;
		ev.data.u64 = 0L;
		ev.events = 0;
		
		Object::SocketStub *psockstub = _objptr->pstubs;
		for(uint i = 0; i < _objptr->stubcp; ++i, ++psockstub){
			Socket *psock = psockstub->psock;
			if(psock && psock->descriptor() >= 0){
				if(
					epoll_ctl(d.epollfd, EPOLL_CTL_ADD, psock->descriptor(), d.eventPrepare(ev, stubpos, i))
				){
					edbgx(Dbg::aio, "epoll_ctl adding filedesc "<<psock->descriptor()<<" stubpos = "<<stubpos<<" pos = "<<i<<" err = "<<strerror(errno));
					fail = true;
					failpos = i;
					break;
				}else{
					//success adding new
					d.addNewSocket();
					psock->doPrepare();
				}
			}
		}
	}
	if(fail){
		doUnregisterObject(*_objptr, failpos);
		stub.reset();
		d.freestubsstk.push(stubpos);
	}else{
		++d.objsz;
		stub.objptr = _objptr;
		stub.objptr->doPrepare(&stub.timepos);
		idbgx(Dbg::aio, "pushing object "<<&(*(stub.objptr))<<" on position "<<stubpos);
		stub.state = Stub::InExecQueue;
		d.execq.push(stubpos);
	}
}

void Selector::run(){
	static const int	maxnbcnt = 16;
	uint 				flags;
	int					nbcnt = -1;	//non blocking opperations count,
									//used to reduce the number of calls for the system time.
	int 				pollwait = 0;
	
	do{
		flags = 0;
		if(nbcnt < 0){
			clock_gettime(CLOCK_MONOTONIC, &d.ctimepos);
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
		
		if((flags & Data::FULL_SCAN) || d.ctimepos >= d.ntimepos){
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
			idbgx(Dbg::aio, "pollwait "<<pollwait<<" ntimepos.s = "<<d.ntimepos.seconds()<<" ntimepos.ns = "<<d.ntimepos.nanoSeconds());
			idbgx(Dbg::aio, "ctimepos.s = "<<d.ctimepos.seconds()<<" ctimepos.ns = "<<d.ctimepos.nanoSeconds());
			nbcnt = -1;
        }
		
		d.selcnt = epoll_wait(d.epollfd, d.events, d.socksz, pollwait);
		idbgx(Dbg::aio, "epollwait = "<<d.selcnt);
#ifdef UDEBUG
		if(d.selcnt < 0) d.selcnt = 0;
#endif
	}while(!(flags & Data::EXIT_LOOP));
}

//-------------------------------------------------------------
uint Selector::doReadPipe(){
	enum {BUFSZ = 128, BUFLEN = BUFSZ * sizeof(uint)};
	uint		buf[128];
	uint		rv(0);//no
	int			rsz(0);
	int			j(0);
	int			maxcnt((d.objcp / BUFSZ) + 1);
	Stub		*pstub(NULL);
	
	while((++j <= maxcnt) && ((rsz = read(d.pipefds[0], buf, BUFLEN)) == BUFLEN)){
		for(int i = 0; i < BUFSZ; ++i){
			uint pos(buf[i]);
			if(pos){
				if(pos < d.stubs.size() && (pstub = &d.stubs[pos])->objptr && pstub->objptr->signaled(S_RAISE)){
					pstub->events |= SIGNALED;
					if(pstub->state == Stub::OutExecQueue){
						d.execq.push(pos);
						pstub->state = Stub::InExecQueue;
					}
				}
			}else rv = Data::EXIT_LOOP;
		}
	}
	if(rsz){
		rsz >>= 2;
		for(int i = 0; i < rsz; ++i){	
			uint pos(buf[i]);
			if(pos){
				if(pos < d.stubs.size() && (pstub = &d.stubs[pos])->objptr && pstub->objptr->signaled(S_RAISE)){
					pstub->events |= SIGNALED;
					if(pstub->state == Stub::OutExecQueue){
						d.execq.push(pos);
						pstub->state = Stub::InExecQueue;
					}
				}
			}else rv = Data::EXIT_LOOP;
		}
	}
	if(j > maxcnt){
		//dummy read:
		rv = Data::EXIT_LOOP | Data::FULL_SCAN;//scan all filedescriptors for events
		idbgx(Dbg::aio, "reading pipe dummy");
		while((rsz = read(d.pipefds[0], buf, BUFSZ)) > 0);
	}
	return rv;
}
void Selector::doUnregisterObject(Object &_robj, int _lastfailpos){
	Object::SocketStub *psockstub = _robj.pstubs;
	uint to = _robj.stubcp;
	if(_lastfailpos >= 0){
		to = _lastfailpos + 1;
	}
	for(uint i = 0; i < to; ++i, ++psockstub){
		Socket *psock = psockstub->psock;
		if(psock && psock->descriptor() >= 0){
			if(!epoll_ctl(d.epollfd, EPOLL_CTL_DEL, psock->descriptor(), NULL)){
				--d.socksz;
				psock->doUnprepare();
			}else{
				edbgx(Dbg::aio, "epoll_ctl "<<strerror(errno));
			}
		}
	}
}
uint Selector::doAllIo(){
	uint		flags = 0;
	TimeSpec	crttout;
	uint		evs;
	uint		stubpos;
	uint		sockpos;
	const uint	selcnt = d.selcnt;
	for(int i = 0; i < selcnt; ++i){
		d.stub(stubpos, sockpos, d.events[i]);
		if(stubpos){
			cassert(stubpos < d.stubs.size());
			Stub &stub(d.stubs[stubpos]);
			cassert(sockpos < stub.objptr->stubcp);
			Socket *psock(stub.objptr->pstubs[sockpos].psock);
			cassert(psock);
			if((evs = doIo(*psock, d.events[i].events))){
				//first mark the socket in connection
				idbgx(Dbg::aio, "evs = "<<evs<<" indone = "<<INDONE<<" stubpos = "<<stubpos);
				stub.objptr->doAddSignaledSocketNext(sockpos, evs);
				stub.events |= evs;
				//push channel execqueue
				if(stub.state == Stub::OutExecQueue){
					d.execq.push(stubpos);
					stub.state = Stub::InExecQueue;
				}
			}
		}else{//the pipe stub
			flags |= Data::READ_PIPE;
		}
	}
	return flags;
}
uint Selector::doFullScan(){
	uint		evs;
	++d.rep_fullscancount;
	idbgx(Dbg::aio, "fullscan count "<<d.rep_fullscancount);
	d.ntimepos.set(Data::MAXTIMEPOS);
	for(Data::StubVectorTp::iterator it(d.stubs.begin() + 1); it != d.stubs.end(); ++it){
		Stub &stub = *it;
		if(!stub.objptr) continue;
		evs = 0;
		if(d.ctimepos >= stub.timepos){
			evs |= TIMEOUT;
			stub.objptr->doAddTimeoutSockets(d.ctimepos);
		}else if(d.ntimepos > stub.timepos){
			d.ntimepos = stub.timepos;
		}
		if(stub.objptr->signaled(S_RAISE)){
			evs |= SIGNALED;//should not be checked by objs
		}
		if(evs){
			stub.events |= evs;
			if(stub.state == Stub::OutExecQueue){
				d.execq.push(it - d.stubs.begin());
				stub.state = Stub::InExecQueue;
			}
		}
	}
	return 0;
}
uint Selector::doExecuteQueue(){
	uint		flags = 0;
	uint		qsz(d.execq.size());
	while(qsz){//we only do a single scan:
		const uint pos = d.execq.front();
		d.execq.pop();
		--qsz;
		flags |= doExecute(pos);
	}
	return flags;
}
uint Selector::doIo(Socket &_rsock, ulong _evs){
	if(_evs & (EPOLLERR | EPOLLHUP)){
		_rsock.doClear();
		idbgx(Dbg::aio, "epollerr evs = "<<_evs);
		return ERRDONE;
	}
	int rv = 0;
	if(_evs & EPOLLIN){
		rv = _rsock.doRecv();
	}
	if(!(rv & ERRDONE) && (_evs & EPOLLOUT)){
		rv |= _rsock.doSend();
	}
	return rv;
}
uint Selector::doExecute(const uint _pos){
	Stub &stub(d.stubs[_pos]);
	cassert(stub.state == Stub::InExecQueue);
	stub.state = Stub::OutExecQueue;
	uint	rv(0);
	stub.timepos.set(0xffffffff, 0xffffffff);
	TimeSpec timepos(d.ctimepos);
	uint evs = stub.events;
	stub.events = 0;
	stub.objptr->doClearRequests();//clears the requests from object to selector
	switch(stub.objptr->execute(evs, timepos)){
		case BAD:
			idbgx(Dbg::aio, "BAD: removing the connection");
			d.freestubsstk.push(_pos);
			//unregister all channels
			doUnregisterObject(*stub.objptr);
			stub.objptr->doUnprepare();
			stub.objptr.clear();
			stub.timepos.set(0xffffffff, 0xffffffff);
			--d.objsz;
			rv = Data::EXIT_LOOP;
			break;
		case OK:
			d.execq.push(_pos);
			stub.state = Stub::InExecQueue;
			stub.timepos.set(0xffffffff, 0xffffffff);
			stub.objptr->doClearResponses();//clears the responses from the selector to the object
			break;
		case NOK:
			doPrepareObjectWait(_pos, timepos);
			stub.objptr->doClearResponses();//clears the responses from the selector to the object
			break;
		case LEAVE:
			d.freestubsstk.push(_pos);
			doUnregisterObject(*stub.objptr);
			stub.timepos.set(0xffffffff, 0xffffffff);
			--d.objsz;
			stub.objptr->doUnprepare();
			stub.objptr->doClearResponses();//clears the responses from the selector to the object
			stub.objptr.release();
			rv = Data::EXIT_LOOP;
		default:
			cassert(false);
	}
	return rv;
}
void Selector::doPrepareObjectWait(const uint _pos, const TimeSpec &_timepos){
	Stub &stub(d.stubs[_pos]);
	const int32 *pend(stub.objptr->reqpos);
	bool mustwait = true;
	for(int32 *pit(stub.objptr->reqbeg); pit != pend ; ++pit){
		Object::SocketStub &sockstub(stub.objptr->pstubs[*pit]);
		sockstub.chnevents = 0;
		switch(sockstub.request){
			case Object::SocketStub::IORequest:{
				epoll_event ev;
				uint t = (EPOLLET) | sockstub.psock->ioRequest();
				if((sockstub.selevents & Data::EPOLLMASK) != t){
					sockstub.selevents = ev.events = t;
					if(epoll_ctl(d.epollfd, EPOLL_CTL_MOD, sockstub.psock->descriptor(), d.eventPrepare(ev, _pos, *pit))){
						edbgx(Dbg::aio, "epoll_ctl: "<<strerror(errno)<<" desc = "<<sockstub.psock->descriptor());
						cassert(false);
					}
				}
			}break;
			case Object::SocketStub::RegisterRequest:{
				epoll_event ev;
				sockstub.psock->doPrepare();
				sockstub.selevents = ev.events = (EPOLLET);
				if(!epoll_ctl(d.epollfd, EPOLL_CTL_ADD, sockstub.psock->descriptor(), d.eventPrepare(ev, _pos, *pit))){
					stub.objptr->doAddSignaledSocketFirst(*pit, OKDONE);
					d.addNewSocket();
					mustwait = false;
				}else{
					edbgx(Dbg::aio, "epoll_ctl "<<strerror(errno));
					cassert(false);
				}
			}break;
			case Object::SocketStub::UnregisterRequest:{
				if(sockstub.psock->ok()){
					if(!epoll_ctl(d.epollfd, EPOLL_CTL_DEL, sockstub.psock->descriptor(), NULL)){
						--d.socksz;
						sockstub.psock->doUnprepare();
						stub.objptr->doAddSignaledSocketFirst(*pit, OKDONE);
					}else{
						edbgx(Dbg::aio, "epoll_ctl: "<<strerror(errno));
					}
					mustwait = false;
				}
			}break;
			default:
				cassert(false);
		}
	}
	if(mustwait){
		if(_timepos < stub.timepos){
			if(_timepos != d.ctimepos){
				stub.timepos = _timepos;
			}
		}
		if(stub.timepos == d.ctimepos){
			stub.timepos.set(0xffffffff, 0xffffffff);
		}else if(d.ntimepos > stub.timepos){
			d.ntimepos = stub.timepos;
		}
	}else{
		d.execq.push(_pos);
		stub.state = Stub::InExecQueue;
	}
}
//-------------------------------------------------------------
}//namespace aio

}//namespace clientserver

