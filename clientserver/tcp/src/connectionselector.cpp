/* Implementation file connectionselector.cpp
	
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
#include "system/thread.hpp"
#include "system/timespec.hpp"

#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "core/object.hpp"
#include "core/common.hpp"

#include "tcp/connectionselector.hpp"
#include "tcp/channel.hpp"
#include "tcp/connection.hpp"
#include "tcp/visitor.hpp"

#include "channeldata.hpp"

//#include <iostream>
//using namespace std;

namespace clientserver{
namespace tcp{

struct ConnectionSelector::Data{
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
	
	struct ChannelStub{
		enum State{
			Empty = -1,
			NotInQ = 0,
			InQExec = 1,
		};
		
		ChannelStub():timepos(0),evmsk(0),state(Empty){}
		void reset();
		ConnectionPtrTp	conptr;
		TimeSpec		timepos;
		ulong			evmsk;
		uint			state;
	};

	
	typedef Stack<ChannelStub*> 		FreeStackTp;
	typedef Queue<ChannelStub*>			ChQueueTp;
	
	typedef Stack<DataNode*>			DataNodeStackTp;
	typedef Stack<ChannelData*>			ChannelDataStackTp;
	
	Data();
	~Data();
	
	int computePollWait(const TimeSpec &_rntimepos, const TimeSpec &_rctimepos);
	int doReadPipe();
	int doExecute(ChannelStub &_rch, ulong _evs, TimeSpec &_crttout, epoll_event &_rev);
	uint doIo(Channel &_rch, ulong _evs);
	void run();
	void push(const ConnectionPtrTp &_conptr, uint _thid);
	uint doAllIo();
	uint doFullScan();
	uint doExecuteQueue();
	bool empty()const{return sz == 1;}
	
	uint				cp;
	uint				sz;
	int					selcnt;
	int					epfd;
	epoll_event 		*pevs;
	ChannelStub			*pchs;
	ChQueueTp			chq;
	FreeStackTp			fstk;
	int					pipefds[2];
	TimeSpec			ntimepos;//next timepos == next timeout
	TimeSpec			ctimepos;//current time pos
	DataNodeStackTp		dnstk, dnastk;
	ChannelDataStackTp	cdstk, cdastk;
};
//===================================================================
ConnectionSelector::ConnectionSelector():d(*(new Data)){
}

ConnectionSelector::~ConnectionSelector(){
	delete &d;
	idbg("done");
}

int ConnectionSelector::reserve(ulong _cp){
	cassert(_cp);
	cassert(!d.sz);
	if(_cp < d.cp) return OK;
	if(d.cp){
		cassert(false);
		delete []d.pevs;delete []d.pchs;
	}
	d.cp = _cp;
	
	//The epoll_event table
	
	d.pevs = new epoll_event[d.cp];
	for(uint i = 0; i < d.cp; ++i){
		d.pevs[i].events = 0;
		d.pevs[i].data.ptr = NULL;
	}
	
	//the ChannelStub table:
	
	d.pchs = new Data::ChannelStub[d.cp];
	//no need for initialization
	
	//Init the free channel stack:
	for(int i = d.cp - 1; i; --i){
		d.fstk.push(&d.pchs[i]);
	}
	
	//init the channel queue
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
		fcntl(d.pipefds[1], F_SETFL, O_NONBLOCK);
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

uint ConnectionSelector::capacity()const{
	return d.cp - 1;
}

uint ConnectionSelector::size() const{
	return d.sz;
}
int  ConnectionSelector::empty()const{
	return d.sz == 1;
}

int  ConnectionSelector::full()const{
	return d.sz == d.cp;
}

void ConnectionSelector::prepare(){
}

void ConnectionSelector::unprepare(){
}

void ConnectionSelector::run(){
	d.run();
}

void ConnectionSelector::push(const ConnectionPtrTp &_conptr, uint _thid){
	return d.push(_conptr, _thid);
}

void ConnectionSelector::signal(uint _objid){
	idbg("signal connection: "<<_objid);
	write(d.pipefds[1], &_objid, sizeof(uint32));
}

void ConnectionSelector::Data::ChannelStub::reset(){
	state = Empty;
	cassert(!conptr.release());
	timepos.set(0);
	evmsk = 0;
}
//-------------------------------------------------------------------
ConnectionSelector::Data::Data():
	cp(0),sz(0),selcnt(0),epfd(-1), pevs(NULL),
	pchs(NULL),	ntimepos(0), ctimepos(0)
{
	pipefds[0] = pipefds[1] = -1;
}
ConnectionSelector::Data::~Data(){
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
}

inline int ConnectionSelector::Data::computePollWait(const TimeSpec &_rntimepos, const TimeSpec &_rctimepos){
	//TODO: think of a way to eliminate this if - e.g. MAXTIMEPOS==0?!
	if(_rntimepos.seconds() == MAXTIMEPOS) return -1;//return MAXPOLLWAIT;
	int rv = (_rntimepos.seconds() - _rctimepos.seconds()) * 1000;
	rv += ((long)_rntimepos.nanoSeconds() - _rctimepos.nanoSeconds()) / 1000000;
	if(rv < 0) return MAXPOLLWAIT;
	return rv;
}


void ConnectionSelector::Data::push(const ConnectionPtrTp &_conptr, uint _thid){
	cassert(fstk.size());
	ChannelStub *pc = fstk.top(); fstk.pop();
	_conptr->setThread(_thid, pc - pchs);
	pc->timepos.set(MAXTIMEPOS);
	pc->evmsk = 0;
	epoll_event ev;
	ev.data.ptr = NULL;
	ev.events = 0;
	ev.data.ptr = pc;
	if(_conptr->channel().descriptor() >= 0 && 
		epoll_ctl(epfd, EPOLL_CTL_ADD, _conptr->channel().descriptor(), &ev)){
		idbg("error adding filedesc "<<_conptr->channel().descriptor());
		pc->conptr.clear();
		pc->reset(); fstk.push(pc);
		cassert(false);
	}else{
		++sz;
	}
	_conptr->channel().prepare();
	pc->conptr = _conptr;
	idbg("pushing "<<&(*(pc->conptr))<<" on pos "<<(pc - pchs));
	pc->state = ChannelStub::InQExec;
	chq.push(pc);
}


uint ConnectionSelector::Data::doIo(Channel &_rch, ulong _evs){
	if(_evs & (EPOLLERR | EPOLLHUP)) return ERRDONE;
	int rv = 0;
	if(_evs & Channel::INTOUT){
		rv = _rch.doRecv();
	}
	if(!(rv & ERRDONE) && (_evs & Channel::OUTTOUT)){
		rv |= _rch.doSend();
	}
	return rv;
}
/*
NOTE:
1) Because connections can only be added while exiting the loop, and because
the execqueue is executed at the end of the loop, a connection that has left
the selector, and set the state to "not in queue", will surely be taken out
of the queue, before its associated ChannelStub will be reused by a newly added
connection.
*/
void ConnectionSelector::Data::run(){
	uint 		flags;
	const int	maxnbcnt = 64;
	int			nbcnt = 0;	//non blocking opperations count,
							//used to reduce the number of calls for the system time.
	int 		pollwait = 0;
	//int			ioqsz = 0;
	
	do{
		flags = 0;
		if(!nbcnt){
			clock_gettime(CLOCK_MONOTONIC, &ctimepos);
			nbcnt = maxnbcnt;
		}
		
		if(selcnt){
			flags |= doAllIo();
		}
		
		if((flags & FULL_SCAN) || ctimepos >= ntimepos){
			flags |= doFullScan();
		}
		
		if(chq.size()){
			flags |= doExecuteQueue();
		}
		
		if(flags & READ_PIPE){
			flags |= doReadPipe();
		}
		
		if(empty()) flags |= EXIT_LOOP;
		
		if(flags || chq.size()){
			pollwait = 0;
			--nbcnt;
		}else{
			pollwait = computePollWait(ntimepos, ctimepos);
			idbg("pollwait "<<pollwait<<" ntimepos.s = "<<ntimepos.seconds()<<" ntimepos.ns = "<<ntimepos.nanoSeconds());
			idbg("ctimepos.s = "<<ctimepos.seconds()<<" ctimepos.ns = "<<ctimepos.nanoSeconds());
			nbcnt = -1;
        }
		
		selcnt = epoll_wait(epfd, pevs, sz, pollwait);
		idbg("epollwait = "<<selcnt);
	}while(!(flags & EXIT_LOOP));
}

uint ConnectionSelector::Data::doAllIo(){
	uint		flags = 0;
	TimeSpec	crttout;
	epoll_event	ev;
	uint		evs;
	idbg("selcnt = "<<selcnt);
	for(int i = 0; i < selcnt; ++i){
		ChannelStub *pc = reinterpret_cast<ChannelStub*>(pevs[i].data.ptr);
		if(pc != pchs){
			idbg("state = "<<pc->state<<" empty "<<ChannelStub::Empty);
			if((evs = doIo(pc->conptr->channel(), pevs[i].events))){
				crttout = ctimepos;
				flags |= doExecute(*pc, evs, crttout, ev);
			}
		}else{
			flags |= READ_PIPE;
		}
	}
	return flags;
}
uint ConnectionSelector::Data::doFullScan(){
	TimeSpec	crttout;
	uint		flags = 0;
	uint		evs;
	epoll_event	ev;
	idbg("fullscan");
	ntimepos.set(MAXTIMEPOS);
	for(uint i = 1; i < cp; ++i){
		ChannelStub &rcs = pchs[i];
		if(!rcs.conptr) continue;
		evs = 0;
		if(ctimepos >= rcs.timepos){
			evs |= TIMEOUT;
		}else if(ntimepos > rcs.timepos){
			ntimepos = rcs.timepos;
		}
		if(rcs.conptr->signaled(S_RAISE)){
			evs |= SIGNALED;//should not be checked by objs
		}
		if(evs){
			crttout = ctimepos;
			doExecute(rcs, evs, crttout, ev);
		}
	}
	return flags;
}
uint ConnectionSelector::Data::doExecuteQueue(){
	TimeSpec	crttout;
	uint		flags = 0;
	uint		evs;
	epoll_event	ev;
	uint		qsz(chq.size());
	while(qsz){//we only do a single scan:
		idbg("qsz = "<<qsz<<" queuesz "<<chq.size());
		ChannelStub &rc = *chq.front();chq.pop(); --qsz;
		idbg("qsz = "<<qsz<<" queuesz "<<chq.size()<<" state = "<<rc.state);
		switch(rc.state){
			case ChannelStub::Empty:
				idbg("empty");
			case ChannelStub::NotInQ:
				idbg("notinq");
				break;
			case ChannelStub::InQExec:
				idbg("InQExec");
				rc.state &= ~ChannelStub::InQExec;
				evs = 0;
				crttout = ctimepos;
				flags |= doExecute(rc, evs, crttout, ev);
				break;
		}
	}
	return flags;
}

int ConnectionSelector::Data::doExecute(ChannelStub &_rch, ulong _evs, TimeSpec &_crttout, epoll_event &_rev){
	int rv = 0;
	Connection	&rcon = *_rch.conptr;
	switch(rcon.execute(_evs, _crttout)){
		case BAD://close
			idbg("BAD: removing the connection");
			fstk.push(&_rch);
			epoll_ctl(epfd, EPOLL_CTL_DEL, rcon.channel().descriptor(), NULL);
			rcon.channel().unprepare();
			_rch.conptr.clear();
			_rch.state = ChannelStub::Empty;
			--sz;
			if(empty()) rv = EXIT_LOOP;
			break;
		case OK://
			idbg("OK: reentering connection "<<rcon.channel().ioRequest());
			if(!(_rch.state & ChannelStub::InQExec)){
				chq.push(&_rch);
				_rch.state |= ChannelStub::InQExec;
			}
			_rch.timepos.set(MAXTIMEPOS);
			break;
		case NOK:
			idbg("TOUT: connection waits for signals");
			{	
				ulong t = (EPOLLET) | rcon.channel().ioRequest();
				if((_rch.evmsk & EPOLLMASK) != t){
					idbg("epollctl");
					//TODO: remove
					if(t & EPOLLOUT){idbg("WTOUT");}
					if(t & EPOLLIN){idbg("RTOUT");}
					_rch.evmsk = _rev.events = t;
					_rev.data.ptr = &_rch;
					int rz = epoll_ctl(epfd, EPOLL_CTL_MOD, rcon.channel().descriptor(), &_rev);
					//TODO: remove
					if(rz) idbg("epollctl "<<strerror(errno));
				}
				if(_crttout != ctimepos){
					_rch.timepos = _crttout;
					if(ntimepos > _crttout) ntimepos = _crttout;
				}else{
					_rch.timepos.set(MAXTIMEPOS);
				}
			}
			break;
		case LEAVE:
			idbg("LEAVE: connection leave");
			fstk.push(&_rch);
			epoll_ctl(epfd, EPOLL_CTL_DEL, rcon.channel().descriptor(), NULL);
			--sz;
			_rch.conptr.release();
			_rch.state = ChannelStub::Empty;
			rcon.channel().unprepare();
			if(empty()) rv = EXIT_LOOP;
			break;
		case REGISTER:{//
			idbg("REGISTER: register connection with new descriptor");
			_rev.data.ptr = &_rch;
			uint ioreq = rcon.channel().ioRequest();
			_rch.evmsk = _rev.events = (EPOLLET) | ioreq;
			epoll_ctl(epfd, EPOLL_CTL_ADD, rcon.channel().descriptor(), &_rev);
			if(!ioreq && !(_rch.state & ChannelStub::InQExec)){
				chq.push(&_rch);
				_rch.state |= ChannelStub::InQExec;
			}
			}break;
		case UNREGISTER:
			idbg("UNREGISTER: unregister connection's descriptor");
			epoll_ctl(epfd, EPOLL_CTL_DEL, rcon.channel().descriptor(), NULL);
			if(!(_rch.state & ChannelStub::InQExec)){
				chq.push(&_rch);
				_rch.state |= ChannelStub::InQExec;
			}
			break;
		default:
			cassert(false);
	}
	idbg("doExecute return "<<rv);
	return rv;
}

/**
 * Returns true if we have to check for new ChannelStubs.
 */
int ConnectionSelector::Data::doReadPipe(){
	enum {BUFSZ = 128, BUFLEN = BUFSZ * sizeof(uint)};
	uint32	buf[128];
	int rv = 0;//no
	int rsz = 0;int j = 0;int maxcnt = (cp / BUFSZ) + 1;
	idbg("maxcnt = "<<maxcnt);
	ChannelStub	*pch = NULL;
	while((++j <= maxcnt) && ((rsz = read(pipefds[0], buf, BUFLEN)) == BUFLEN)){
		for(int i = 0; i < BUFSZ; ++i){
			idbg("buf["<<i<<"]="<<buf[i]);
			uint pos = buf[i];
			if(pos){
				if(pos < cp && (pch = pchs + pos)->conptr && !pch->state && pch->conptr->signaled(S_RAISE)){
					chq.push(pch);
					pch->state = ChannelStub::InQExec;
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
					pch->state = ChannelStub::InQExec;
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

}//namespace tcp
}//namespace clientserver

