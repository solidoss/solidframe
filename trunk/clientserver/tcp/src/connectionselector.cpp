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
//-------------------------------------------------------------------
struct ConnectionSelector::ChannelStub{
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
	
	typedef Stack<ChannelStub*> 		FreeStackTp;
	typedef Queue<ChannelStub*>			ChQueueTp;
	
	typedef Stack<DataNode*>			DataNodeStackTp;
	typedef Stack<ChannelData*>			ChannelDataStackTp;
	
	Data();
	~Data();
	
	int computePollWait();
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

//-------------------------------------------------------------------
void ConnectionSelector::ChannelStub::reset(){
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

inline int ConnectionSelector::Data::computePollWait(){
	//TODO: think of a way to eliminate this if - e.g. MAXTIMEPOS==0?!
	if(ntimepos.seconds() == MAXTIMEPOS) return -1;//return MAXPOLLWAIT;
	int rv = (ntimepos.seconds() - ctimepos.seconds()) * 1000;
	rv += ((long)ntimepos.nanoSeconds() - ctimepos.nanoSeconds()) / 1000000;
	if(rv < 0) return MAXPOLLWAIT;
	return rv;
}
//-------------------------------------------------------------------
ConnectionSelector::ConnectionSelector():d(*(new Data)){
}

ConnectionSelector::~ConnectionSelector(){
	delete &d;
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
	
	d.pchs = new ChannelStub[d.cp];
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
			edbgx(Dbg::tcp, "epollctl");
			cassert(false);
			return -1;
		}
	}
	idbgx(Dbg::tcp, "pipe fds "<<d.pipefds[0]<<" "<<d.pipefds[1]);
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

void ConnectionSelector::push(const ConnectionPtrTp &_conptr, uint _thid){
	cassert(d.fstk.size());
	ChannelStub *pc = d.fstk.top(); d.fstk.pop();
	_conptr->setThread(_thid, pc - d.pchs);
	pc->timepos.set(Data::MAXTIMEPOS);
	pc->evmsk = 0;
	epoll_event ev;
	ev.data.ptr = NULL;
	ev.events = 0;
	ev.data.ptr = pc;
	if(_conptr->channel().descriptor() >= 0 && 
		epoll_ctl(d.epfd, EPOLL_CTL_ADD, _conptr->channel().descriptor(), &ev)){
		edbgx(Dbg::tcp, "epoll_ctl adding filedesc "<<_conptr->channel().descriptor());
		pc->conptr.clear();
		pc->reset();
		d.fstk.push(pc);
		cassert(false);
	}else{
		++d.sz;
	}
	_conptr->channel().prepare();
	pc->conptr = _conptr;
	idbgx(Dbg::tcp, "pushing connection "<<&(*(pc->conptr))<<" on position "<<(pc - d.pchs));
	pc->state = ChannelStub::InQExec;
	d.chq.push(pc);
}

void ConnectionSelector::signal(uint _objid){
	idbgx(Dbg::tcp, "signal connection: "<<_objid);
	write(d.pipefds[1], &_objid, sizeof(uint32));
}

uint ConnectionSelector::doIo(Channel &_rch, ulong _evs){
	if(_evs & (EPOLLERR | EPOLLHUP)){
		_rch.clear();
		return ERRDONE;
	}
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
void ConnectionSelector::run(){
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
		
		if(d.chq.size()){
			flags |= doExecuteQueue();
		}
		
		if(flags & Data::READ_PIPE){
			flags |= doReadPipe();
		}
		
		if(d.empty()) flags |= Data::EXIT_LOOP;
		
		if(flags || d.chq.size()){
			pollwait = 0;
			--nbcnt;
		}else{
			pollwait = d.computePollWait();
			idbgx(Dbg::tcp, "pollwait "<<pollwait<<" ntimepos.s = "<<d.ntimepos.seconds()<<" ntimepos.ns = "<<d.ntimepos.nanoSeconds());
			idbgx(Dbg::tcp, "ctimepos.s = "<<d.ctimepos.seconds()<<" ctimepos.ns = "<<d.ctimepos.nanoSeconds());
			nbcnt = -1;
        }
		
		d.selcnt = epoll_wait(d.epfd, d.pevs, d.sz, pollwait);
		idbgx(Dbg::tcp, "epollwait = "<<d.selcnt);
	}while(!(flags & Data::EXIT_LOOP));
}

uint ConnectionSelector::doAllIo(){
	uint		flags = 0;
	TimeSpec	crttout;
	epoll_event	ev;
	uint		evs;
	idbgx(Dbg::tcp, "selcnt = "<<d.selcnt);
	for(int i = 0; i < d.selcnt; ++i){
		ChannelStub *pc = reinterpret_cast<ChannelStub*>(d.pevs[i].data.ptr);
		if(pc != d.pchs){
			idbgx(Dbg::tcp, "state = "<<pc->state<<" empty "<<ChannelStub::Empty);
			if((evs = doIo(pc->conptr->channel(), d.pevs[i].events))){
				crttout = d.ctimepos;
				flags |= doExecute(*pc, evs, crttout, ev);
			}
		}else{
			flags |= Data::READ_PIPE;
		}
	}
	return flags;
}
uint ConnectionSelector::doFullScan(){
	TimeSpec	crttout;
	uint		flags = 0;
	uint		evs;
	epoll_event	ev;
	idbgx(Dbg::tcp, "fullscan");
	d.ntimepos.set(Data::MAXTIMEPOS);
	for(uint i = 1; i < d.cp; ++i){
		ChannelStub &rcs = d.pchs[i];
		if(!rcs.conptr) continue;
		evs = 0;
		if(d.ctimepos >= rcs.timepos){
			evs |= TIMEOUT;
		}else if(d.ntimepos > rcs.timepos){
			d.ntimepos = rcs.timepos;
		}
		if(rcs.conptr->signaled(S_RAISE)){
			evs |= SIGNALED;//should not be checked by objs
		}
		if(evs){
			crttout = d.ctimepos;
			doExecute(rcs, evs, crttout, ev);
		}
	}
	return flags;
}
uint ConnectionSelector::doExecuteQueue(){
	TimeSpec	crttout;
	uint		flags = 0;
	uint		evs;
	epoll_event	ev;
	uint		qsz(d.chq.size());
	while(qsz){//we only do a single scan:
		ChannelStub &rc = *d.chq.front();d.chq.pop(); --qsz;
		switch(rc.state){
			case ChannelStub::Empty:
			case ChannelStub::NotInQ:
				break;
			case ChannelStub::InQExec:
				rc.state &= ~ChannelStub::InQExec;
				evs = 0;
				crttout = d.ctimepos;
				flags |= doExecute(rc, evs, crttout, ev);
				break;
		}
	}
	return flags;
}

int ConnectionSelector::doExecute(ChannelStub &_rch, ulong _evs, TimeSpec &_crttout, epoll_event &_rev){
	int rv = 0;
	Connection	&rcon = *_rch.conptr;
	switch(rcon.execute(_evs, _crttout)){
		case BAD://close
			idbgx(Dbg::tcp, "BAD: removing the connection");
			d.fstk.push(&_rch);
			epoll_ctl(d.epfd, EPOLL_CTL_DEL, rcon.channel().descriptor(), NULL);
			rcon.channel().unprepare();
			_rch.conptr.clear();
			_rch.state = ChannelStub::Empty;
			--d.sz;
			//if(empty()) 
			rv = Data::EXIT_LOOP;
			break;
		case OK://
			//idbgx(Dbg::tcp, "OK: reentering connection "<<rcon.channel().ioRequest());
			if(!(_rch.state & ChannelStub::InQExec)){
				d.chq.push(&_rch);
				_rch.state |= ChannelStub::InQExec;
			}
			_rch.timepos.set(Data::MAXTIMEPOS);
			break;
		case NOK:
			idbgx(Dbg::tcp, "TOUT: connection waits for signals");
			{	
				ulong t = (EPOLLET) | rcon.channel().ioRequest();
				if((_rch.evmsk & Data::EPOLLMASK) != t){
					idbgx(Dbg::tcp, "epollctl");
					_rch.evmsk = _rev.events = t;
					_rev.data.ptr = &_rch;
					epoll_ctl(d.epfd, EPOLL_CTL_MOD, rcon.channel().descriptor(), &_rev);
				}
				if(_crttout != d.ctimepos){
					_rch.timepos = _crttout;
					if(d.ntimepos > _crttout) d.ntimepos = _crttout;
				}else{
					_rch.timepos.set(Data::MAXTIMEPOS);
				}
			}
			break;
		case LEAVE:
			idbgx(Dbg::tcp, "LEAVE: connection leave");
			d.fstk.push(&_rch);
			epoll_ctl(d.epfd, EPOLL_CTL_DEL, rcon.channel().descriptor(), NULL);
			--d.sz;
			_rch.conptr.release();
			_rch.state = ChannelStub::Empty;
			rcon.channel().unprepare();
			//if(empty()) 
			rv = Data::EXIT_LOOP;
			break;
		case REGISTER:{//
			idbgx(Dbg::tcp, "REGISTER: register connection with new descriptor");
			_rev.data.ptr = &_rch;
			uint ioreq = rcon.channel().ioRequest();
			_rch.evmsk = _rev.events = (EPOLLET) | ioreq;
			epoll_ctl(d.epfd, EPOLL_CTL_ADD, rcon.channel().descriptor(), &_rev);
			if(!ioreq && !(_rch.state & ChannelStub::InQExec)){
				d.chq.push(&_rch);
				_rch.state |= ChannelStub::InQExec;
			}
			}break;
		case UNREGISTER:
			idbgx(Dbg::tcp, "UNREGISTER: unregister connection's descriptor");
			epoll_ctl(d.epfd, EPOLL_CTL_DEL, rcon.channel().descriptor(), NULL);
			if(!(_rch.state & ChannelStub::InQExec)){
				d.chq.push(&_rch);
				_rch.state |= ChannelStub::InQExec;
			}
			break;
		default:
			cassert(false);
	}
	//idbgx(Dbg::tcp, "return "<<rv);
	return rv;
}

/**
 * Returns true if we have to check for new ChannelStubs.
 */
int ConnectionSelector::doReadPipe(){
	enum {BUFSZ = 128, BUFLEN = BUFSZ * sizeof(uint)};
	uint32		buf[128];
	int			rv(0);//no
	int			rsz(0);
	int			j(0);
	int			maxcnt((d.cp / BUFSZ) + 1);
	ChannelStub	*pch(NULL);
	
	while((++j <= maxcnt) && ((rsz = read(d.pipefds[0], buf, BUFLEN)) == BUFLEN)){
		for(int i = 0; i < BUFSZ; ++i){
			uint pos(buf[i]);
			if(pos){
				if(pos < d.cp && (pch = d.pchs + pos)->conptr && !pch->state && pch->conptr->signaled(S_RAISE)){
					d.chq.push(pch);
					pch->state = ChannelStub::InQExec;
				}
			}else rv = Data::EXIT_LOOP;
		}
	}
	if(rsz){
		rsz >>= 2;
		for(int i = 0; i < rsz; ++i){	
			uint pos(buf[i]);
			if(pos){
				if(pos < d.cp && (pch = d.pchs + pos)->conptr && !pch->state && pch->conptr->signaled(S_RAISE)){
					d.chq.push(pch);
					pch->state = ChannelStub::InQExec;
				}
			}else rv = Data::EXIT_LOOP;
		}
	}
	if(j > maxcnt){
		//dummy read:
		rv = Data::EXIT_LOOP | Data::FULL_SCAN;//scan all filedescriptors for events
		idbgx(Dbg::tcp, "reading pipe dummy");
		while((rsz = read(d.pipefds[0], buf, BUFSZ)) > 0);
	}
	return rv;
}

}//namespace tcp
}//namespace clientserver

