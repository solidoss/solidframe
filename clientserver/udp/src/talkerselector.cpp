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
#include "utility/stack.hpp"
#include "utility/queue.hpp"

#include "core/object.hpp"
#include "core/common.hpp"
#include "udp/talkerselector.hpp"
#include "udp/talker.hpp"
#include "udp/station.hpp"

// #include <iostream>
// 
// using namespace std;

namespace clientserver{
namespace udp{

struct TalkerSelector::SelTalker{
	SelTalker():timepos(0),evmsk(0),state(0){}
	void reset();
	TalkerPtrTp		tkrptr;//4
	TimeSpec		timepos;//4
	ulong			evmsk;//4
	//state: 1 means the object is in queue
	//or if 0 in chq check loop means the channel was already checked
	int				state;//4
};

struct TalkerSelector::Data{
	enum{
		UNROLLSZ = 4, 
		UNROLLMSK = 3,
		EPOLLMASK = EPOLLERR | EPOLLHUP | EPOLLET | EPOLLIN | EPOLLOUT,
		EXIT_LOOP = 1,
		FULL_SCAN = 2,
		READ_PIPE = 4,
		MAXTIMEPOS = 0xffffffff,
		MAXPOLLWAIT = 0x7FFFFFFF,
	};
	typedef Stack<SelTalker*> 	FreeStackTp;
	typedef Queue<SelTalker*>	ChQueueTp;
	
	int computePollWait();
	
	Data();
	~Data();
	int  empty()const{ return sz == 1;}
		
	void initFreeStack();
	void push(SelTalker *_pc){
		chq.push(_pc);
	}
	
	uint			cp;
	uint			sz;
	int				selcnt;
	int				epfd;
	epoll_event 	*pevs;
	SelTalker		*pchs;
	ChQueueTp		chq;
	FreeStackTp		fstk;
	int				pipefds[2];
	TimeSpec        ntimepos;//next timepos == next timeout
	TimeSpec        ctimepos;//current time pos
};

//---------------------------------------------------------------------------

void TalkerSelector::SelTalker::reset(){
	state = 0;
	tkrptr.release();
	timepos.set(0);
	evmsk = 0;
}

//---------------------------------------------------------------------------

TalkerSelector::Data::Data():
	cp(0), sz(0), selcnt(0),
	epfd(-1), pevs(NULL), pchs(NULL),
	ntimepos(0), ctimepos(0)
{
	pipefds[0] = pipefds[1] = -1;
}

TalkerSelector::Data::~Data(){
	close(pipefds[0]);
	close(pipefds[1]);
	close(epfd);
	delete []pevs;
	delete []pchs;
}

void TalkerSelector::Data::initFreeStack(){
	for(int i = cp - 1; i > 0; --i){
		fstk.push(&(pchs[i]));
	}
}

TalkerSelector::TalkerSelector():d(*(new Data)){
}

int TalkerSelector::reserve(ulong _cp){
	cassert(_cp);
	cassert(!d.sz);
	if(_cp < d.cp) return OK;
	if(d.cp){
		cassert(false);
		delete []d.pevs;delete []d.pchs;//delete []ph;
	}
	d.cp = _cp;
	
	//The epoll_event table
	
	d.pevs = new epoll_event[d.cp];
	for(uint i = 0; i < d.cp; ++i){
		d.pevs[i].events = 0;
		d.pevs[i].data.ptr = NULL;
	}
	
	//the SelTalker table:
	
	d.pchs = new SelTalker[d.cp];
	//no need for initialization
		
	//Init the free station stack:
	d.initFreeStack();
	
	//init the station queue
	//chq.reserve(cp);
	//zero is reserved for pipe
	if(d.epfd < 0 && (d.epfd = epoll_create(d.cp)) < 0){
		cassert(false);
		return -1;
	}
	//init the pipes:
	if(d.pipefds[0] < 0){
		if(pipe(d.pipefds)){
			cassert(false);
			return -1;
		}
		fcntl(d.pipefds[0], F_SETFL, O_NONBLOCK);
		//fcntl(pipefds[1],F_SETFL, O_NONBLOCK);
		
		epoll_event ev;
		ev.data.ptr = &d.pchs[0];
		ev.events = EPOLLIN | EPOLLPRI;//must be LevelTriggered
		if(epoll_ctl(d.epfd, EPOLL_CTL_ADD, d.pipefds[0], &ev)){
			idbg("error epollctl");
			cassert(false);
			return -1;
		}
	}
	idbg("Pipe fds "<<d.pipefds[0]<<" "<<d.pipefds[1]);
	d.ctimepos.set(0);
	d.ntimepos.set(Data::MAXTIMEPOS);
	d.selcnt = 0;
	d.sz = 1;
	return OK;
}

TalkerSelector::~TalkerSelector(){
	delete &d;
	idbg("done");
}

uint TalkerSelector::capacity()const{
	return d.cp - 1;
}
uint TalkerSelector::size()const{
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

void TalkerSelector::push(const TalkerPtrTp &_tkrptr, uint _thid){
	cassert(d.fstk.size());
	SelTalker *pc(d.fstk.top());
	d.fstk.pop();
	pc->state = 0;
	
	_tkrptr->setThread(_thid, pc - d.pchs);
	
	pc->timepos.set(Data::MAXTIMEPOS);
	pc->evmsk = 0;
	
	epoll_event ev;
	ev.events = 0;
	ev.data.ptr = pc;
	
	if(_tkrptr->station().descriptor() >= 0 && 
		epoll_ctl(d.epfd, EPOLL_CTL_ADD, _tkrptr->station().descriptor(), &ev)){
		idbg("error adding filedesc "<<_tkrptr->station().descriptor());
		pc->tkrptr.clear();
		pc->reset(); d.fstk.push(pc);
		cassert(false);
	}else{
		++d.sz;
	}
	pc->tkrptr = _tkrptr;
	idbg("pushing "<<&(*(pc->tkrptr))<<" on pos "<<(pc - d.pchs));
	pc->state = 1;
	d.push(pc);
}

void TalkerSelector::signal(uint _objid){
	idbg("signal connection: "<<_objid);
	write(d.pipefds[1], &_objid, sizeof(uint));
}

int TalkerSelector::doIo(Station &_rch, ulong _evs){
	int rv = 0;
	if(_evs & EPOLLIN){
		rv = _rch.doRecv();
	}
	if((_evs & EPOLLOUT) && !(rv & ERRDONE)){
		rv |= _rch.doSend();
	}
	return rv;
}
inline int TalkerSelector::Data::computePollWait(){
	if(ntimepos.seconds() == MAXTIMEPOS)
		return MAXPOLLWAIT;
	int rv = (ntimepos.seconds() - ctimepos.seconds()) * 1000;
	rv += (ntimepos.nanoSeconds() - ctimepos.nanoSeconds())/1000000 ; 
	if(rv < 0) return MAXPOLLWAIT;
	return rv;
}

void TalkerSelector::run(){
	TimeSpec	crttout;
	epoll_event	ev;
	int 		state;
	int			evs;
	const int	maxnbcnt = 256;
	int			nbcnt = -1;
	int 		pollwait = 0;
	do{
		state = 0;
		if(nbcnt < 0){
			clock_gettime(CLOCK_MONOTONIC, &d.ctimepos);
			nbcnt = maxnbcnt;
		}
		for(int i = 0; i < d.selcnt; ++i){
			SelTalker *pc = reinterpret_cast<SelTalker*>(d.pevs[i].data.ptr);
			if(pc != d.pchs){
				if(!pc->state && (evs = doIo(pc->tkrptr->station(), d.pevs[i].events))){
					crttout = d.ctimepos;
					state |= doExecute(*pc, evs, crttout, ev);
				}
			}else{
				state |= Data::READ_PIPE;
			}
		}
		if(state & Data::READ_PIPE){
			state |= doReadPipe();
		}
		if((state & Data::FULL_SCAN) || d.ctimepos >= d.ntimepos){
			idbg("fullscan");
			d.ntimepos.set(Data::MAXTIMEPOS);
			for(uint i = 1; i < d.cp; ++i){
				SelTalker &pc = d.pchs[i];
				if(!pc.tkrptr) continue;
				evs = 0;
				if(d.ctimepos >= pc.timepos) evs |= TIMEOUT;
				else if(d.ntimepos > pc.timepos) d.ntimepos = pc.timepos;
				if(pc.tkrptr->signaled(S_RAISE)) evs |= SIGNALED;//should not be checked by objs
				if(evs){
					crttout = d.ctimepos;
					doExecute(pc, evs, crttout, ev);
				}
			}
		}
		{	
			int qsz = d.chq.size();
			while(qsz){//we only do a single scan:
				idbg("qsz = "<<qsz<<" queuesz "<<d.chq.size());
				SelTalker &rc = *d.chq.front();d.chq.pop(); --qsz;
				if(rc.state){
					crttout = d.ctimepos;
					rc.state = 0;
					doExecute(rc, 0, crttout, ev);
				}
			}
		}
		idbg("sz = "<<d.sz);
		if(d.empty()) state |= Data::EXIT_LOOP;
		if(state || d.chq.size()){
			pollwait = 0;
			--nbcnt;
		}else{
			pollwait = d.computePollWait();
			idbg("pollwait "<<pollwait<<" ntimepos.s = "<<d.ntimepos.seconds()<<" ntimepos.ns = "<<d.ntimepos.nanoSeconds());
			idbg("ctimepos.s = "<<d.ctimepos.seconds()<<" ctimepos.ns = "<<d.ctimepos.nanoSeconds());
			nbcnt = -1;
        }
		d.selcnt = epoll_wait(d.epfd, d.pevs, d.sz, pollwait);
		idbg("selcnt = "<<d.selcnt);
	}while(!(state & Data::EXIT_LOOP));
	idbg("exiting loop");
}

int TalkerSelector::doExecute(
	SelTalker &_rch,
	ulong _evs,
	TimeSpec &_rcrttout,
	epoll_event &_rev
){
	int rv = 0;
	Talker &rcon = *_rch.tkrptr;
	switch(rcon.execute(_evs, _rcrttout)){
		case BAD://close
			idbg("BAD: removing the connection");
			d.fstk.push(&_rch);
			epoll_ctl(d.epfd, EPOLL_CTL_DEL, rcon.station().descriptor(), NULL);
			_rch.tkrptr.clear();
			_rch.state = 0;
			--d.sz;
			if(d.empty()) rv = Data::EXIT_LOOP;
			break;
		case OK://
			idbg("OK: reentering connection");
			if(!_rch.state){
				d.chq.push(&_rch);
				_rch.state = 1;
			}
			_rch.timepos.set(Data::MAXTIMEPOS);
			break;
		case NOK:
			idbg("TOUT: connection waits for signals");
			{	
				ulong t = (EPOLLERR | EPOLLHUP | EPOLLET) | rcon.station().ioRequest();
				if((_rch.evmsk & Data::EPOLLMASK) != t){
					idbg("RTOUT: epollctl");
					_rch.evmsk = _rev.events = t;
					_rev.data.ptr = &_rch;
					int rv = epoll_ctl(d.epfd, EPOLL_CTL_MOD, rcon.station().descriptor(), &_rev);
					cassert(!rv);
				}
				if(_rcrttout != d.ctimepos){
					_rch.timepos = _rcrttout;
					if(d.ntimepos > _rcrttout){
						d.ntimepos = _rcrttout;
					}
				}else{
					_rch.timepos.set(Data::MAXTIMEPOS);
				}
			}
			break;
		case LEAVE:
			idbg("LEAVE: connection leave");
			d.fstk.push(&_rch);
			//TODO: remove fd from epoll
			if(d.sz == d.cp) rv = Data::EXIT_LOOP;
			_rch.tkrptr.release();
			_rch.state = 0;
			--d.sz;
			break;
		case REGISTER:{//
			idbg("REGISTER: register connection with new descriptor");
			_rev.data.ptr = &_rch;
			uint ioreq = rcon.station().ioRequest();
			_rch.evmsk = _rev.events = (EPOLLERR | EPOLLHUP | EPOLLET) | ioreq;
			int rv = epoll_ctl(d.epfd, EPOLL_CTL_ADD, rcon.station().descriptor(), &_rev);
			cassert(!rv);
			if(!ioreq){
				if(!_rch.state){
					d.chq.push(&_rch);
					_rch.state = 1;
				}
			}
			}break;
		case UNREGISTER:{
			idbg("UNREGISTER: unregister connection's descriptor");
			int rv = epoll_ctl(d.epfd, EPOLL_CTL_DEL, rcon.station().descriptor(), NULL);
			cassert(!rv);
			if(!_rch.state){
				d.chq.push(&_rch);
				_rch.state = 1;
			}
			}break;
		default:
			cassert(false);
	}
	idbg("doExecute return "<<rv);
	return rv;
}

int TalkerSelector::doReadPipe(){
	enum {BUFSZ = 128, BUFLEN = BUFSZ * sizeof(uint)};
	uint	buf[BUFSZ];
	int rv = 0;//no
	int rsz = 0;int j = 0;int maxcnt = (d.cp / BUFSZ) + 1;
	idbg("maxcnt = "<<maxcnt);
	SelTalker	*pch = NULL;
	while((++j <= maxcnt) && ((rsz = read(d.pipefds[0], buf, BUFLEN)) == BUFLEN)){
		for(int i = 0; i < BUFSZ; ++i){
			idbg("buf["<<i<<"]="<<buf[i]);
			uint pos = buf[i];
			if(pos){
				if(pos < d.cp && (pch = d.pchs + pos)->tkrptr && !pch->state && pch->tkrptr->signaled(S_RAISE)){
					d.chq.push(pch);
					pch->state = 1;
					idbg("pushig "<<pos<<" connection into queue");
				}
			}else rv = Data::EXIT_LOOP;
		}
	}
	if(rsz){
		rsz >>= 2;
		for(int i = 0; i < rsz; ++i){	
			idbg("buf["<<i<<"]="<<buf[i]);
			uint pos = buf[i];
			if(pos){
				if(pos < d.cp && (pch = d.pchs + pos)->tkrptr && !pch->state && pch->tkrptr->signaled(S_RAISE)){
					d.chq.push(pch);
					pch->state = 1;
					idbg("pushig "<<pos<<" connection into queue");
				}
			}else rv = Data::EXIT_LOOP;
		}
	}
	if(j > maxcnt){
		//dummy read:
		rv = Data::EXIT_LOOP | Data::FULL_SCAN;//scan all filedescriptors for events
		idbg("reading dummy");
		while((rsz = read(d.pipefds[0], buf, BUFSZ)) > 0);
	}
	idbg("readpiperv = "<<rv);
	return rv;
}

}//namespace tcp
}//namespace clientserver

