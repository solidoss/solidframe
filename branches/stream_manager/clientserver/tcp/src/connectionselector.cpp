/* Implementation file connectionselector.cpp
	
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
#include "system/thread.h"

#include "core/object.h"
#include "core/common.h"

#include "tcp/connectionselector.h"
#include "tcp/channel.h"
#include "tcp/connection.h"

#include "channeldata.h"

//#include <iostream>
//using namespace std;

namespace clientserver{
namespace tcp{

enum{
	UNROLLSZ = 4, 
	UNROLLMSK = 3,
	EPOLLMASK = EPOLLET | EPOLLIN | EPOLLOUT,
};

struct ConnectionSelector::SelChannel{
	SelChannel():timepos(0),evmsk(0),state(0){}
	void reset();
	ConnectionPtrTp	conptr;//4
	TimeSpec		timepos;//4
	ulong			evmsk;//4
	//state: 1 means the object is in queue
	//or if 0 in chq check loop means the channel was already checked
	int				state;//4
};


ConnectionSelector::ConnectionSelector():cp(0),sz(0),
					 selcnt(0),epfd(-1), pevs(NULL),
					 pchs(NULL),//ph(NULL),
					 //btimepos(0),
					 ntimepos(0), ctimepos(0){
	pipefds[0] = pipefds[1] = -1;
}

int ConnectionSelector::reserve(ulong _cp){
	assert(_cp);
	assert(!sz);
	if(_cp < cp) return OK;
	if(cp){
		assert(false);
		delete []pevs;delete []pchs;//delete []ph;
	}
	cp = _cp;
	
	//The epoll_event table
	
	pevs = new epoll_event[cp];
	for(uint i = 0; i < cp; ++i){
		pevs[i].events = 0;
		pevs[i].data.ptr = NULL;
	}
	
	//the SelChannel table:
	
	pchs = new SelChannel[cp];
	//no need for initialization
	
	//the channel hash table:
	/*ph = new SelChannel*[cp];
	for(int i = 0; i < cp; ++i){
		ph[i] = NULL;
	}*/
	
	//Init the free channel stack:
	for(int i = cp - 1; i; --i){
		fstk.push(&pchs[i]);
	}
	
	//init the channel queue
	//chq.reserve(cp);
	//zero is reserved for pipe
	if(epfd < 0 && (epfd = epoll_create(cp)) < 0){
		assert(false);
		return -1;
	}
	//init the pipes:
	if(pipefds[0] < 0){
		if(pipe(pipefds)){
			assert(false);
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
			assert(false);
			return -1;
		}
	}
	idbg("Pipe fds "<<pipefds[0]<<" "<<pipefds[1]);
	//btimepos = time(NULL);
	//clock_gettime(CLOCK_MONOTONIC, &btimepos);
	ctimepos.set(0);
	ntimepos.set(0xffffffff);
	selcnt = 0;
	sz = 1;
	//INF3("reinitserlector selcnt = %d, lastpos = %d, cnt = %d", selcnt, lastpos, _cnt);
	return OK;
}

ConnectionSelector::~ConnectionSelector(){
	close(pipefds[0]);
	close(pipefds[1]);
	close(epfd);
	delete []pevs;
	delete []pchs;
	while(dnastk.size()){
		delete []dnastk.top();dnastk.pop();
	}
	while(cdastk.size()){
		delete []cdastk.top();cdastk.pop();
	}
	while(isnastk.size()){
		delete []isnastk.top();isnastk.pop();
	}
	//delete []ph;
	idbg("done");
}

void ConnectionSelector::prepare(){
}

void ConnectionSelector::unprepare(){
}

int ConnectionSelector::doIo(Channel &_rch, ulong _evs){
	int rv = 0;
	if(_evs & (EPOLLERR | EPOLLHUP)) return ERRDONE;
	if(_evs & EPOLLIN){
		rv = _rch.doRecv();
	}
	if((_evs & EPOLLOUT) && !(rv & ERRDONE)){
		rv |= _rch.doSend();
	}
	return rv;
}

inline int computePollWait(const TimeSpec &_rntimepos, const TimeSpec &_rctimepos){
	if(_rntimepos.seconds() == 0XFFFFFFFF) return 0x7FFFFFFF;
	int rv = (_rntimepos.seconds() - _rctimepos.seconds()) * 1000;
	rv += (_rntimepos.nanoSeconds() - _rctimepos.nanoSeconds()) / 1000000 ; 
	if(rv < 0) return 0x7FFFFFFF;
	return rv;
}

/*
	NOTE: TODO:
		Look at the note from core/objectselector.cpp
*/

void ConnectionSelector::run(){
	TimeSpec	crttout;
	epoll_event	ev;
	int 		state;
	int			evs;
	const int	maxnbcnt = 128;
	int			nbcnt = -1;//non blocking opperations count,
							//used to reduce the number of calls for the system time.
	int 		pollwait = 0;
	do{
		state = 0;
		if(nbcnt < 0){
			//ctimepos.set((uint64)time(NULL) - btimepos);
			clock_gettime(CLOCK_MONOTONIC, &ctimepos);
			nbcnt = maxnbcnt;
		}
		for(int i = 0; i < selcnt; ++i){
			SelChannel *pc = reinterpret_cast<SelChannel*>(pevs[i].data.ptr);
			if(pc != pchs){
				if(!pc->state && (evs = doIo(pc->conptr->channel(), pevs[i].events))){
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
			ntimepos.set(0XFFFFFFFF);
			for(uint i = 1; i < cp; ++i){
				SelChannel &pc = pchs[i];
				if(!pc.conptr) continue;
				evs = 0;
				if(ctimepos >= pc.timepos) evs |= TIMEOUT;
				else if(ntimepos > pc.timepos) ntimepos = pc.timepos;
				if(pc.conptr->signaled(S_RAISE)) evs |= SIGNALED;//should not be checked by objs
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
				SelChannel &rc = *chq.front();chq.pop(); --qsz;
				if(rc.state){
					rc.state = 0;
					crttout = ctimepos;
					doExecute(rc, OKDONE, crttout, ev);
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

int ConnectionSelector::doExecute(SelChannel &_rch, ulong _evs, TimeSpec &_crttout, epoll_event &_rev){
	//_crttout = MAXTIMEOUT;
	//assert(_rcrttout <= MAXTIMEOUT);;
	int rv = 0;
	Connection	&rcon = *_rch.conptr;
	switch(rcon.execute(_evs, _crttout)){
		case BAD://close
			idbg("BAD: removing the connection");
			fstk.push(&_rch);
			epoll_ctl(epfd, EPOLL_CTL_DEL, rcon.channel().descriptor(), NULL);
			rcon.channel().unprepare();
			_rch.conptr.clear();
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
				ulong t = (EPOLLET) | rcon.channel().ioRequest();
				if((_rch.evmsk & EPOLLMASK) != t){
					idbg("epollctl");
					if(t & EPOLLOUT){idbg("WTOUT");}
					if(t & EPOLLIN){idbg("RTOUT");}
					_rch.evmsk = _rev.events = t;
					_rev.data.ptr = &_rch;
					assert(!epoll_ctl(epfd, EPOLL_CTL_MOD, rcon.channel().descriptor(), &_rev));
				}
				if(_crttout != ctimepos){
					_rch.timepos = _crttout;
					if(ntimepos > _crttout) ntimepos = _crttout;
				}else{
					_rch.timepos.set(0xFFFFFFFF);
				}
			}
			break;
		case LEAVE:
			idbg("LEAVE: connection leave");
			fstk.push(&_rch);
			assert(!epoll_ctl(epfd, EPOLL_CTL_DEL, rcon.channel().descriptor(), NULL));
			--sz;
			_rch.conptr.release();
			_rch.state = 0;
			rcon.channel().unprepare();
			if(empty()) rv = EXIT_LOOP;
			break;
		case REGISTER:{//
			idbg("REGISTER: register connection with new descriptor");
			_rev.data.ptr = &_rch;
			uint ioreq = rcon.channel().ioRequest();
			_rch.evmsk = _rev.events = (EPOLLET) | ioreq;
			assert(!epoll_ctl(epfd, EPOLL_CTL_ADD, rcon.channel().descriptor(), &_rev));
			if(!ioreq){
				if(!_rch.state) {chq.push(&_rch); _rch.state = 1;}
			}
			}break;
		case UNREGISTER:
			idbg("UNREGISTER: unregister connection's descriptor");
			assert(!epoll_ctl(epfd, EPOLL_CTL_DEL, rcon.channel().descriptor(), NULL));
			if(!_rch.state) {chq.push(&_rch); _rch.state = 1;}
			break;
		default:
			assert(false);
	}
	idbg("doExecute return "<<rv);
	return rv;
}

/**
 * Returns true if we have to check for new SelChannels.
 */
int ConnectionSelector::doReadPipe(){
	enum {BUFSZ = 128, BUFLEN = BUFSZ * sizeof(uint)};
	uint32	buf[128];
	int rv = 0;//no
	int rsz = 0;int j = 0;int maxcnt = (cp / BUFSZ) + 1;
	idbg("maxcnt = "<<maxcnt);
	SelChannel	*pch = NULL;
	while((++j <= maxcnt) && ((rsz = read(pipefds[0], buf, BUFLEN)) == BUFLEN)){
		for(int i = 0; i < BUFSZ; ++i){
			idbg("buf["<<i<<"]="<<buf[i]);
			uint pos = buf[i];
			if(pos){
				if(pos < cp && (pch = pchs + pos)->conptr && !pch->state && pch->conptr->signaled(S_RAISE)){
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
				if(pos < cp && (pch = pchs + pos)->conptr && !pch->state && pch->conptr->signaled(S_RAISE)){
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

void ConnectionSelector::push(const ConnectionPtrTp &_conptr, uint _thid){
	assert(fstk.size());
	SelChannel *pc = fstk.top(); fstk.pop();
	//pc->fd = _conptr->descriptor();
	//pc->state = 0;
	_conptr->setThread(_thid, pc - pchs);
	//pc->param = _param;
	pc->timepos.set(0xffffffff);
	pc->evmsk = 0;
	//pc->phprev = pc->phnext = NULL;
	epoll_event ev;
	//ev.data.u64 = 0;
	ev.data.ptr = NULL;
	ev.events = 0;
	ev.data.ptr = pc;
	if(_conptr->channel().descriptor() >= 0 && 
		epoll_ctl(epfd, EPOLL_CTL_ADD, _conptr->channel().descriptor(), &ev)){
		idbg("error adding filedesc "<<_conptr->channel().descriptor());
		pc->conptr.clear();
		pc->reset(); fstk.push(pc);
		assert(false);
	}else{
		++sz;
	}
	_conptr->channel().prepare();
	pc->conptr = _conptr;
	idbg("pushing "<<&(*(pc->conptr))<<" on pos "<<(pc - pchs));
	pc->state = 1;
	//insertHash(pc);
	chq.push(pc);
}

void ConnectionSelector::signal(uint _objid){
	idbg("signal connection: "<<_objid);
	write(pipefds[1], &_objid, sizeof(uint32));
}

void ConnectionSelector::SelChannel::reset(){
	state = 0;
	assert(!conptr.release());
	timepos.set(0);
	evmsk = 0;
}

}//namespace tcp
}//namespace clientserver

#if 0
void ConnectionSelector::insertHash(SelChannel *_pch){
	ulong objid = _pch->conptr->id();
	ulong objh = objid % cp;
	SelChannel *pc = ph[objh];
	if(!pc){
		idbg("Add hash at "<<objh);
		ph[objh] = _pch;
		_pch->phnext = NULL;
		_pch->phprev = NULL;
	}else{
		//find the best position in list
		if(objid >= pc->conptr->id()){
			idbg("1) Add hash at "<<objh);
			ph[objh] = _pch;
			_pch->phnext = pc;
			_pch->phprev = NULL;
			pc->phprev = _pch;
		}else{
			idbg("2)Add hash at "<<objh);
			SelChannel *poldc;
			do{
				poldc = pc;
				pc = pc->phnext;
			}while(pc && objid < pc->conptr->id());
			poldc->phnext = _pch;
			_pch->phnext = pc;
			_pch->phprev = poldc;
			if(pc) pc->phprev = _pch;
		}
	}
}
void ConnectionSelector::removeHash(SelChannel *_pch){
	if(!_pch->phprev){
		ph[_pch->conptr->id() % cp] = _pch->phnext;
		if(_pch->phnext) _pch->phnext->phprev = NULL;
	}else{
		_pch->phprev->phnext = _pch->phnext;
		if(_pch->phnext)
			_pch->phnext->phprev = _pch->phprev;
	}
}

inline ConnectionSelector::SelChannel* ConnectionSelector::findHash(ulong _objid){
	SelChannel *pc = ph[_objid % cp];
	while(pc && _objid < pc->conptr->id()) pc = pc->phnext;
	if(pc && _objid == pc->conptr->id()) return pc;
	return NULL;
}
#endif

