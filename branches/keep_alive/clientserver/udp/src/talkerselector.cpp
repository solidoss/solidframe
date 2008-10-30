/* Implementation file talkerselector.cpp
	
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

#include "system/debug.hpp"
#include "system/timespec.hpp"

#include <new>
#include <cerrno>
#include "utility/stack.hpp"
#include "utility/queue.hpp"

#include "core/object.hpp"
#include "core/common.hpp"
#include "udp/talkerselector.hpp"
#include "udp/talker.hpp"
#include "udp/station.hpp"

namespace clientserver{
namespace udp{
//-------------------------------------------------------------------
struct TalkerSelector::Stub{
	enum State{
		OutExecQueue = 0,
		InExecQueue = 1,
	};
	
	Stub():timepos(0),events(0),state(OutExecQueue){}
	void reset();
	ObjectTp		objptr;
	TimeSpec		timepos;
	ulong			events;
	uint			state;
};

struct TalkerSelector::Data{
	enum{
		UNROLLSZ = 4, 
		UNROLLMSK = 3,
		EPOLLMASK = EPOLLET | EPOLLIN | EPOLLOUT,
		MAXTIMEPOS = 0xffffffff,
		MAXPOLLWAIT = 0x7FFFFFFF,
		EXIT_LOOP = 1,
		FULL_SCAN = 2,
		READ_PIPE = 4
	};
	
	typedef Stack<Stub*> 		FreeStackTp;
	typedef Queue<Stub*>		StubQueueTp;
	
	
	Data();
	~Data();
	
	int computePollWait();
	bool empty()const{return sz == 1;}
	
	uint				cp;
	uint				sz;
	int					selcnt;
	int					epfd;
	epoll_event 		*pevs;
	Stub				*pstubs;
	StubQueueTp			execq;
	FreeStackTp			fstk;
	int					pipefds[2];
	TimeSpec			ntimepos;//next timepos == next timeout
	TimeSpec			ctimepos;//current time pos
	
//reporting data:
	uint32				rep_fullscancount;
};

//-------------------------------------------------------------------
void TalkerSelector::Stub::reset(){
	state = OutExecQueue;
	cassert(!objptr.release());
	timepos.set(0);
	events = 0;
}
//-------------------------------------------------------------------
TalkerSelector::Data::Data():
	cp(0),sz(0),selcnt(0),epfd(-1), pevs(NULL),
	pstubs(NULL),	ntimepos(0), ctimepos(0),rep_fullscancount(0)
{
	pipefds[0] = pipefds[1] = -1;
}
TalkerSelector::Data::~Data(){
	close(pipefds[0]);
	close(pipefds[1]);
	close(epfd);
	delete []pevs;
	delete []pstubs;
}

inline int TalkerSelector::Data::computePollWait(){
	//TODO: think of a way to eliminate this if - e.g. MAXTIMEPOS==0?!
	if(ntimepos.seconds() == MAXTIMEPOS) return -1;//return MAXPOLLWAIT;
	int rv = (ntimepos.seconds() - ctimepos.seconds()) * 1000;
	rv += ((long)ntimepos.nanoSeconds() - ctimepos.nanoSeconds()) / 1000000;
	if(rv < 0) return MAXPOLLWAIT;
	return rv;
}
//-------------------------------------------------------------------
TalkerSelector::TalkerSelector():d(*(new Data)){
}

TalkerSelector::~TalkerSelector(){
	delete &d;
}

int TalkerSelector::reserve(ulong _cp){
	cassert(_cp);
	cassert(!d.sz);
	if(_cp < d.cp) return OK;
	if(d.cp){
		cassert(false);
		delete []d.pevs;delete []d.pstubs;
	}
	d.cp = _cp;
	
	//The epoll_event table
	
	d.pevs = new epoll_event[d.cp];
	for(uint i = 0; i < d.cp; ++i){
		d.pevs[i].events = 0;
		d.pevs[i].data.ptr = NULL;
	}
	
	//the Stub table:
	
	d.pstubs = new Stub[d.cp];
	//no need for initialization
	
	//Init the free station stack:
	for(int i = d.cp - 1; i; --i){
		d.fstk.push(&d.pstubs[i]);
	}
	
	//init the station queue
	//execq.reserve(cp);
	//zero is reserved for pipe
	if(d.epfd < 0 && (d.epfd = epoll_create(d.cp)) < 0){
		edbgx(Dbg::udp, "epoll_create "<<strerror(errno));
		cassert(false);
		return BAD;
	}
	//init the pipes:
	if(d.pipefds[0] < 0){
		if(pipe(d.pipefds)){
			cassert(false);
			return BAD;
		}
		fcntl(d.pipefds[0], F_SETFL, O_NONBLOCK);
		fcntl(d.pipefds[1], F_SETFL, O_NONBLOCK);
		epoll_event ev;
		ev.data.ptr = &d.pstubs[0];
		ev.events = EPOLLIN | EPOLLPRI;//must be LevelTriggered
		if(epoll_ctl(d.epfd, EPOLL_CTL_ADD, d.pipefds[0], &ev)){
			edbgx(Dbg::udp, "epoll_ctl "<<strerror(errno));
			cassert(false);
			return BAD;
		}
	}
	idbgx(Dbg::udp, "pipe fds "<<d.pipefds[0]<<" "<<d.pipefds[1]);
	d.ctimepos.set(0);
	d.ntimepos.set(Data::MAXTIMEPOS);
	d.selcnt = 0;
	d.sz = 1;
	return OK;
}

uint TalkerSelector::capacity()const{
	return d.cp - 1;
}

uint TalkerSelector::size() const{
	return d.sz;
}
int  TalkerSelector::empty()const{
	return d.sz == 1;
}

int  TalkerSelector::full()const{
	return d.sz == d.cp;
}

void TalkerSelector::prepare(){
}

void TalkerSelector::unprepare(){
}

void TalkerSelector::push(const ObjectTp &_objptr, uint _thid){
	cassert(d.fstk.size());
	Stub *pstub = d.fstk.top(); d.fstk.pop();
	_objptr->setThread(_thid, pstub - d.pstubs);
	pstub->timepos.set(Data::MAXTIMEPOS);
	pstub->events = 0;
	epoll_event ev;
	ev.data.ptr = NULL;
	ev.events = 0;
	ev.data.ptr = pstub;
	if(
		_objptr->station().descriptor() >= 0 && 
		epoll_ctl(d.epfd, EPOLL_CTL_ADD, _objptr->station().descriptor(), &ev)
	){
		edbgx(Dbg::udp, "epoll_ctl adding filedesc "<<_objptr->station().descriptor()<<" err = "<<strerror(errno));
		cassert(false);
		pstub->reset();
		d.fstk.push(pstub);
	}else{
		++d.sz;
		//_objptr->station().prepare();
		pstub->objptr = _objptr;
		idbgx(Dbg::udp, "pushing connection "<<&(*(pstub->objptr))<<" on position "<<(pstub - d.pstubs));
		pstub->state = Stub::InExecQueue;
		d.execq.push(pstub);
	}
}

void TalkerSelector::signal(uint _objid){
	idbgx(Dbg::udp, "signal connection: "<<_objid);
	write(d.pipefds[1], &_objid, sizeof(uint32));
}

uint TalkerSelector::doIo(Station &_r, ulong _evs){
	if(_evs & (EPOLLERR | EPOLLHUP)){
		return ERRDONE;
	}
	int rv = 0;
	if(_evs & EPOLLIN){
		rv = _r.doRecv();
	}
	if((_evs & EPOLLOUT) && !(rv & ERRDONE)){
		rv |= _r.doSend();
	}
	return rv;
}
/*
NOTE:
1) Because connections can only be added while exiting the loop, and because
the execqueue is executed at the end of the loop, a connection that has left
the selector, and set the state to "not in queue", will surely be taken out
of the queue, before its associated Stub will be reused by a newly added
connection.
*/
void TalkerSelector::run(){
	uint 		flags;
	const int	maxnbcnt = 64;
	int			nbcnt = -1;	//non blocking opperations count,
							//used to reduce the number of calls for the system time.
	int 		pollwait = 0;
	//int			ioqsz = 0;
	
	do{
		flags = 0;
		if(nbcnt < 0){
			clock_gettime(CLOCK_MONOTONIC, &d.ctimepos);
			nbcnt = maxnbcnt;
		}
		
		if(d.selcnt){
			flags |= doAllIo();
		}
		
		if((flags & Data::FULL_SCAN) || d.ctimepos >= d.ntimepos){
			flags |= doFullScan();
		}
		
		if(d.execq.size()){
			flags |= doExecuteQueue();
		}
		
		if(flags & Data::READ_PIPE){
			flags |= doReadPipe();
		}
		
		if(d.empty()) flags |= Data::EXIT_LOOP;
		
		if(flags || d.execq.size()){
			pollwait = 0;
			--nbcnt;
		}else{
			pollwait = d.computePollWait();
			idbgx(Dbg::udp, "pollwait "<<pollwait<<" ntimepos.s = "<<d.ntimepos.seconds()<<" ntimepos.ns = "<<d.ntimepos.nanoSeconds());
			idbgx(Dbg::udp, "ctimepos.s = "<<d.ctimepos.seconds()<<" ctimepos.ns = "<<d.ctimepos.nanoSeconds());
			nbcnt = -1;
        }
		
		d.selcnt = epoll_wait(d.epfd, d.pevs, d.sz, pollwait);
		if(d.selcnt < 0){
			edbgx(Dbg::udp, "epoll_wait "<<strerror(errno));
		}
		idbgx(Dbg::udp, "epollwait = "<<d.selcnt);
	}while(!(flags & Data::EXIT_LOOP));
}

uint TalkerSelector::doAllIo(){
	uint		flags = 0;
	TimeSpec	crttout;
	epoll_event	ev;
	uint		evs;
	idbgx(Dbg::udp, "selcnt = "<<d.selcnt);
	for(int i = 0; i < d.selcnt; ++i){
		Stub *pc = reinterpret_cast<Stub*>(d.pevs[i].data.ptr);
		if(pc != d.pstubs){
			idbgx(Dbg::udp, "state = "<<pc->state);
			if((evs = doIo(pc->objptr->station(), d.pevs[i].events))){
				crttout = d.ctimepos;
				flags |= doExecute(*pc, evs, crttout, ev);
			}
		}else{
			flags |= Data::READ_PIPE;
		}
	}
	return flags;
}
uint TalkerSelector::doFullScan(){
	TimeSpec	crttout;
	uint		flags = 0;
	uint		evs;
	epoll_event	ev;
	++d.rep_fullscancount;
	idbgx(Dbg::udp, "fullscan count "<<d.rep_fullscancount);
	d.ntimepos.set(Data::MAXTIMEPOS);
	for(uint i = 1; i < d.cp; ++i){
		Stub &rcs = d.pstubs[i];
		if(!rcs.objptr) continue;
		evs = 0;
		if(d.ctimepos >= rcs.timepos){
			evs |= TIMEOUT;
		}else if(d.ntimepos > rcs.timepos){
			d.ntimepos = rcs.timepos;
		}
		if(rcs.objptr->signaled(S_RAISE)){
			evs |= SIGNALED;//should not be checked by objs
		}
		if(evs){
			crttout = d.ctimepos;
			doExecute(rcs, evs, crttout, ev);
		}
	}
	return flags;
}
uint TalkerSelector::doExecuteQueue(){
	TimeSpec	crttout;
	uint		flags = 0;
	uint		evs;
	epoll_event	ev;
	uint		qsz(d.execq.size());
	while(qsz){//we only do a single scan:
		Stub &rc = *d.execq.front();d.execq.pop(); --qsz;
		if(rc.state == Stub::InExecQueue){
			//rc.state = Stub::NotInQ;//moved in doExecute
			evs = 0;
			crttout = d.ctimepos;
			flags |= doExecute(rc, evs, crttout, ev);
		}
	}
	return flags;
}

int TalkerSelector::doExecute(Stub &_rstub, ulong _evs, TimeSpec &_crttout, epoll_event &_rev){
	int rv = 0;
	Talker	&robj = *_rstub.objptr;
	_rstub.state = Stub::OutExecQueue;//drop it from exec queue so we limit the exec count
	switch(robj.execute(_evs, _crttout)){
		case BAD://close
			idbgx(Dbg::udp, "BAD: removing the talker");
			d.fstk.push(&_rstub);
			if(epoll_ctl(d.epfd, EPOLL_CTL_DEL, robj.station().descriptor(), NULL)){
				edbgx(Dbg::udp, "epoll_ctl "<<strerror(errno));
				cassert(false);
			}
			//rcon.station().unprepare();
			_rstub.objptr.clear();
			_rstub.state = Stub::OutExecQueue;
			--d.sz;
			rv = Data::EXIT_LOOP;
			break;
		case OK://
			idbgx(Dbg::udp, "OK: reentering talker "<<robj.station().ioRequest());
			d.execq.push(&_rstub);
			_rstub.state = Stub::InExecQueue;
			_rstub.timepos.set(Data::MAXTIMEPOS);
			break;
		case NOK:
			idbgx(Dbg::udp, "TOUT: talker waits for signals");
			{	
				ulong t = (EPOLLET) | robj.station().ioRequest();
				if((_rstub.events & Data::EPOLLMASK) != t){
					_rstub.events = _rev.events = t;
					_rev.data.ptr = &_rstub;
					if(epoll_ctl(d.epfd, EPOLL_CTL_MOD, robj.station().descriptor(), &_rev)){
						edbgx(Dbg::udp, "epoll_ctl "<<strerror(errno));
						cassert(false);
					}
				}
				if(_crttout != d.ctimepos){
					_rstub.timepos = _crttout;
					if(d.ntimepos > _crttout) d.ntimepos = _crttout;
				}else{
					_rstub.timepos.set(Data::MAXTIMEPOS);
				}
			}
			break;
		case LEAVE:
			idbgx(Dbg::udp, "LEAVE: talker leave");
			d.fstk.push(&_rstub);
			if(epoll_ctl(d.epfd, EPOLL_CTL_DEL, robj.station().descriptor(), NULL)){
				edbgx(Dbg::udp, "epoll_ctl "<<strerror(errno));
				cassert(false);
			}
			--d.sz;
			_rstub.objptr.release();
			_rstub.state = Stub::OutExecQueue;
			//rcon.station().unprepare();
			rv = Data::EXIT_LOOP;
			break;
		case REGISTER:{//
			idbgx(Dbg::udp, "REGISTER: register talker with new descriptor");
			_rev.data.ptr = &_rstub;
			uint ioreq = robj.station().ioRequest();
			_rstub.events = _rev.events = (EPOLLET) | ioreq;
			if(epoll_ctl(d.epfd, EPOLL_CTL_ADD, robj.station().descriptor(), &_rev)){
				edbgx(Dbg::udp, "epoll_ctl "<<strerror(errno));
				cassert(false);
			}
			if(!ioreq){
				d.execq.push(&_rstub);
				_rstub.state = Stub::InExecQueue;
			}
			}break;
		case UNREGISTER:
			idbgx(Dbg::udp, "UNREGISTER: unregister talker's descriptor");
			if(epoll_ctl(d.epfd, EPOLL_CTL_DEL, robj.station().descriptor(), NULL)){
				edbgx(Dbg::udp, "epoll_ctl "<<strerror(errno));
				cassert(false);
			}
			d.execq.push(&_rstub);
			_rstub.state = Stub::InExecQueue;
			break;
		default:
			cassert(false);
	}
	//idbgx(Dbg::udp, "return "<<rv);
	return rv;
}

/**
 * Returns true if we have to check for new Stubs.
 */
int TalkerSelector::doReadPipe(){
	enum {BUFSZ = 128, BUFLEN = BUFSZ * sizeof(uint)};
	uint32		buf[128];
	int			rv(0);//no
	int			rsz(0);
	int			j(0);
	int			maxcnt((d.cp / BUFSZ) + 1);
	Stub		*pstub(NULL);
	
	while((++j <= maxcnt) && ((rsz = read(d.pipefds[0], buf, BUFLEN)) == BUFLEN)){
		for(int i = 0; i < BUFSZ; ++i){
			uint pos(buf[i]);
			if(pos){
				if(pos < d.cp && (pstub = d.pstubs + pos)->objptr && pstub->state == Stub::OutExecQueue && pstub->objptr->signaled(S_RAISE)){
					d.execq.push(pstub);
					pstub->state = Stub::InExecQueue;
				}
			}else rv = Data::EXIT_LOOP;
		}
	}
	if(rsz){
		rsz >>= 2;
		for(int i = 0; i < rsz; ++i){	
			uint pos(buf[i]);
			if(pos){
				if(pos < d.cp && (pstub = d.pstubs + pos)->objptr && pstub->state == Stub::OutExecQueue && pstub->objptr->signaled(S_RAISE)){
					d.execq.push(pstub);
 					pstub->state = Stub::InExecQueue;
				}
			}else rv = Data::EXIT_LOOP;
		}
	}
	if(j > maxcnt){
		//dummy read:
		rv = Data::EXIT_LOOP | Data::FULL_SCAN;//scan all filedescriptors for events
		idbgx(Dbg::udp, "reading pipe dummy");
		while((rsz = read(d.pipefds[0], buf, BUFSZ)) > 0);
	}
	return rv;
}

}//namespace udp
}//namespace clientserver

