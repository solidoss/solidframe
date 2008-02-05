/* Implementation file talkerselector.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include "system/debug.h"

#include "core/object.h"
#include "core/common.h"
#include "udp/talkerselector.h"
#include "udp/talker.h"
#include "udp/station.h"

// #include <iostream>
// 
// using namespace std;

namespace clientserver{
namespace udp{

enum{
	UNROLLSZ = 4, 
	UNROLLMSK = 3,
	EPOLLMASK = EPOLLERR | EPOLLHUP | EPOLLET | EPOLLIN | EPOLLOUT,
};

TalkerSelector::TalkerSelector():cp(0),sz(0),
					 selcnt(0),epfd(-1), pevs(NULL),
					 pchs(NULL),//ph(NULL),
					 //btimepos(0),
					 ntimepos(0), ctimepos(0){
	pipefds[0] = pipefds[1] = -1;
}

int TalkerSelector::reserve(ulong _cp){
	cassert(_cp);
	cassert(!sz);
	if(_cp < cp) return OK;
	if(cp){
		cassert(false);
		delete []pevs;delete []pchs;//delete []ph;
	}
	cp = _cp;
	
	//The epoll_event table
	
	pevs = new epoll_event[cp];
	for(uint i = 0; i < cp; ++i){
		pevs[i].events = 0;
		pevs[i].data.ptr = NULL;
	}
	
	//the SelTalker table:
	
	pchs = new SelTalker[cp];
	//no need for initialization
	
	//the station hash table:
	/*ph = new SelTalker*[cp];
	for(int i = 0; i < cp; ++i){
		ph[i] = NULL;
	}*/
	
	//Init the free station stack:
	for(int i = cp - 1; i > 0; --i){
		fstk.push(&pchs[i]);
	}
	
	//init the station queue
	//chq.reserve(cp);
	//zero is reserved for pipe
	if(epfd < 0 && (epfd = epoll_create(cp)) < 0){
		cassert(false);
		return -1;
	}
	//init the pipes:
	if(pipefds[0] < 0){
		if(pipe(pipefds)){
			cassert(false);
			return -1;
		}
		fcntl(pipefds[0],F_SETFL, O_NONBLOCK);
		//fcntl(pipefds[1],F_SETFL, O_NONBLOCK);
		epoll_event ev;
		//ev.data.u64 = 0;
		ev.data.ptr = &pchs[0];
		//pchs[0].fd = pipefds[0];
		ev.events = EPOLLIN | EPOLLPRI;//must be LevelTriggered
		if(epoll_ctl(epfd, EPOLL_CTL_ADD, pipefds[0], &ev)){
			idbg("error epollctl");
			cassert(false);
			return -1;
		}
	}
	idbg("Pipe fds "<<pipefds[0]<<" "<<pipefds[1]);
	//btimepos = time(NULL);
	ctimepos.set(0);
	ntimepos.set(0xffffffff);
	selcnt = 0;
	sz = 1;
	//INF3("reinitserlector selcnt = %d, lastpos = %d, cnt = %d", selcnt, lastpos, _cnt);
	return OK;
}

TalkerSelector::~TalkerSelector(){
	close(pipefds[0]);
	close(pipefds[1]);
	close(epfd);
	delete []pevs;
	delete []pchs;
	//delete []ph;
	idbg("done");
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
inline int computePollWait(const TimeSpec &_rntimepos, const TimeSpec &_rctimepos){
	if(_rntimepos.seconds() == 0xffffffff) return 0x7FFFFFFF;
	int rv = (_rntimepos.seconds() - _rctimepos.seconds()) * 1000;
	rv += (_rntimepos.nanoSeconds() - _rctimepos.nanoSeconds())/1000000 ; 
	if(rv < 0) return 0x7FFFFFFF;
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
			clock_gettime(CLOCK_MONOTONIC, &ctimepos);
			nbcnt = maxnbcnt;
		}
		for(int i = 0; i < selcnt; ++i){
			SelTalker *pc = reinterpret_cast<SelTalker*>(pevs[i].data.ptr);
			if(pc != pchs){
				if(!pc->state && (evs = doIo(pc->tkrptr->station(), pevs[i].events))){
					crttout = ctimepos;
					state |= doExecute(*pc, evs, crttout, ev);
				}
			}else{
				state |= READ_PIPE;
			}
		}
		if(state & READ_PIPE){
			state |= doReadPipe();
		}
		if((state & FULL_SCAN) || ctimepos >= ntimepos){
			idbg("fullscan");
			ntimepos.set(0xffffffff);
			for(uint i = 1; i < cp; ++i){
				SelTalker &pc = pchs[i];
				if(!pc.tkrptr) continue;
				evs = 0;
				if(ctimepos >= pc.timepos) evs |= TIMEOUT;
				else if(ntimepos > pc.timepos) ntimepos = pc.timepos;
				if(pc.tkrptr->signaled(S_RAISE)) evs |= SIGNALED;//should not be checked by objs
				if(evs){
					crttout = ctimepos;
					doExecute(pc, evs, crttout, ev);
				}
			}
		}
		{	
			int qsz = chq.size();
			while(qsz){//we only do a single scan:
				idbg("qsz = "<<qsz<<" queuesz "<<chq.size());
				SelTalker &rc = *chq.front();chq.pop(); --qsz;
				if(rc.state){
					crttout = ctimepos;
					rc.state = 0;
					doExecute(rc, 0, crttout, ev);
				}
			}
		}
		idbg("sz = "<<sz);
		if(empty()) state |= EXIT_LOOP;
		if(state || chq.size()){
			pollwait = 0;
			--nbcnt;
		}else{
			pollwait = computePollWait(ntimepos, ctimepos);
			idbg("pollwait "<<pollwait<<" ntimepos.s = "<<ntimepos.seconds()<<" ntimepos.ns = "<<ntimepos.nanoSeconds());
			idbg("ctimepos.s = "<<ctimepos.seconds()<<" ctimepos.ns = "<<ctimepos.nanoSeconds());
			nbcnt = -1;
        }
		selcnt = epoll_wait(epfd, pevs, sz, pollwait);
		idbg("selcnt = "<<selcnt);
	}while(!(state & EXIT_LOOP));
	idbg("exiting loop");
}

int TalkerSelector::doExecute(SelTalker &_rch, ulong _evs, TimeSpec &_rcrttout, epoll_event &_rev){
	int rv = 0;
	Talker &rcon = *_rch.tkrptr;
	switch(rcon.execute(_evs, _rcrttout)){
		case BAD://close
			idbg("BAD: removing the connection");
			fstk.push(&_rch);
			epoll_ctl(epfd, EPOLL_CTL_DEL, rcon.station().descriptor(), NULL);
			_rch.tkrptr.clear();
			_rch.state = 0;
			--sz;
			if(empty()) rv = EXIT_LOOP;
			break;
		case OK://
			idbg("OK: reentering connection");
			if(!_rch.state) {chq.push(&_rch); _rch.state = 1;}
			_rch.timepos.set(0xFFFFFFFF);
			break;
		case NOK:
			idbg("TOUT: connection waits for signals");
			{	
				ulong t = (EPOLLERR | EPOLLHUP | EPOLLET) | rcon.station().ioRequest();
				if((_rch.evmsk & EPOLLMASK) != t){
					idbg("RTOUT: epollctl");
					_rch.evmsk = _rev.events = t;
					_rev.data.ptr = &_rch;
					cassert(!epoll_ctl(epfd, EPOLL_CTL_MOD, rcon.station().descriptor(), &_rev));
				}
				if(_rcrttout != ctimepos){
					_rch.timepos = _rcrttout;
					if(ntimepos > _rcrttout){
						ntimepos = _rcrttout;
					}
				}else{
					_rch.timepos.set(0xFFFFFFFF);
				}
			}
			break;
		case LEAVE:
			idbg("LEAVE: connection leave");
			fstk.push(&_rch);
			//TODO: remove fd from epoll
			if(sz == cp) rv = EXIT_LOOP;
			_rch.tkrptr.release();
			_rch.state = 0;
			--sz;
			break;
		case REGISTER:{//
			idbg("REGISTER: register connection with new descriptor");
			_rev.data.ptr = &_rch;
			uint ioreq = rcon.station().ioRequest();
			_rch.evmsk = _rev.events = (EPOLLERR | EPOLLHUP | EPOLLET) | ioreq;
			cassert(!epoll_ctl(epfd, EPOLL_CTL_ADD, rcon.station().descriptor(), &_rev));
			if(!ioreq){
				if(!_rch.state) {chq.push(&_rch); _rch.state = 1;}
			}
			}break;
		case UNREGISTER:
			idbg("UNREGISTER: unregister connection's descriptor");
			cassert(!epoll_ctl(epfd, EPOLL_CTL_DEL, rcon.station().descriptor(), NULL));
			if(!_rch.state) {chq.push(&_rch); _rch.state = 1;}
			break;
		default:
			cassert(false);
	}
	idbg("doExecute return "<<rv);
	return rv;
}

/**
 * Returns true if we have to check for new SelTalkers.
 */
int TalkerSelector::doReadPipe(){
	enum {BUFSZ = 128, BUFLEN = BUFSZ * sizeof(uint)};
	uint	buf[BUFSZ];
	int rv = 0;//no
	int rsz = 0;int j = 0;int maxcnt = (cp / BUFSZ) + 1;
	idbg("maxcnt = "<<maxcnt);
	SelTalker	*pch = NULL;
	while((++j <= maxcnt) && ((rsz = read(pipefds[0], buf, BUFLEN)) == BUFLEN)){
		for(int i = 0; i < BUFSZ; ++i){
			idbg("buf["<<i<<"]="<<buf[i]);
			uint pos = buf[i];
			if(pos){
				if(pos < cp && (pch = pchs + pos)->tkrptr && !pch->state && pch->tkrptr->signaled(S_RAISE)){
					chq.push(pch);
					pch->state = 1;
					idbg("pushig "<<pos<<" connection into queue");
				}
			}else rv = EXIT_LOOP;
		}
	}
	if(rsz){
		rsz >>= 2;
		for(int i = 0; i < rsz; ++i){	
			idbg("buf["<<i<<"]="<<buf[i]);
			uint pos = buf[i];
			if(pos){
				if(pos < cp && (pch = pchs + pos)->tkrptr && !pch->state && pch->tkrptr->signaled(S_RAISE)){
					chq.push(pch);
					pch->state = 1;
					idbg("pushig "<<pos<<" connection into queue");
				}
			}else rv = EXIT_LOOP;
		}
	}
	if(j > maxcnt){
		//dummy read:
		rv = EXIT_LOOP | FULL_SCAN;//scan all filedescriptors for events
		idbg("reading dummy");
		while((rsz = read(pipefds[0],buf,BUFSZ)) > 0);
	}
	idbg("readpiperv = "<<rv);
	return rv;
}

void TalkerSelector::push(const TalkerPtrTp &_tkrptr, uint _thid){
	cassert(fstk.size());
	SelTalker *pc = fstk.top(); fstk.pop();
	//pc->fd = _tkrptr->descriptor();
	pc->state = 0;
	_tkrptr->setThread(_thid, pc - pchs);
	//pc->param = _param;
	pc->timepos.set(0xffffffff);
	pc->evmsk = 0;
	//pc->phprev = pc->phnext = NULL;
	epoll_event ev;
	ev.events = 0;
	ev.data.ptr = pc;
	if(_tkrptr->station().descriptor() >= 0 && 
		epoll_ctl(epfd, EPOLL_CTL_ADD, _tkrptr->station().descriptor(), &ev)){
		idbg("error adding filedesc "<<_tkrptr->station().descriptor());
		pc->tkrptr.clear();
		pc->reset(); fstk.push(pc);
		cassert(false);
	}else{
		++sz;
	}
	pc->tkrptr = _tkrptr;
	idbg("pushing "<<&(*(pc->tkrptr))<<" on pos "<<(pc - pchs));
	pc->state = 1;
	//insertHash(pc);
	chq.push(pc);
}

void TalkerSelector::signal(uint _objid){
	idbg("signal connection: "<<_objid);
	write(pipefds[1], &_objid, sizeof(uint));
}

void TalkerSelector::SelTalker::reset(){
	state = 0;
	cassert(!tkrptr.release());
	timepos.set(0);
	evmsk = 0;
}

}//namespace tcp
}//namespace clientserver

