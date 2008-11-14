/* Implementation file multiconnectionselector.cpp
	
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

#include "tcp/multiconnectionselector.hpp"
#include "tcp/channel.hpp"
#include "tcp/multiconnection.hpp"
#include "tcp/visitor.hpp"

namespace clientserver{
namespace tcp{
//-------------------------------------------------------------------
struct MultiConnectionSelector::Stub{
	enum State{
		OutExecQueue = 0,
		InExecQueue = 1,
	};
	
	Stub():timepos(0),events(0),state(OutExecQueue){}
	void reset();
	ObjectTp				objptr;
	TimeSpec				timepos;
	ulong					events;
	uint					state;
};

struct MultiConnectionSelector::Data{
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
	
	typedef Stack<Stub*> 				FreeStackTp;
	typedef Queue<Stub*>				StubQueueTp;
	
	Data();
	~Data();
	
	int computePollWait();
	bool empty()const{return sz == 1;}
	void addNewChannel();
	epoll_event* eventPrepare(epoll_event &_ev, unsigned _objpos, unsigned _pos);
	std::pair<Stub*, unsigned> stub(const epoll_event &_ev);
	uint				cp;
	uint				sz;
	uint				chncp;
	uint				chnsz;
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
void MultiConnectionSelector::Stub::reset(){
	state = OutExecQueue;
	cassert(!objptr.release());
	timepos.set(0);
	events = 0;
}
//-------------------------------------------------------------------
MultiConnectionSelector::Data::Data():
	cp(0),sz(0),chncp(0),chnsz(0),selcnt(0),epfd(-1), pevs(NULL),
	pstubs(NULL),	ntimepos(0), ctimepos(0),rep_fullscancount(0)
{
	pipefds[0] = pipefds[1] = -1;
}
MultiConnectionSelector::Data::~Data(){
	close(pipefds[0]);
	close(pipefds[1]);
	close(epfd);
	delete []pevs;
	delete []pstubs;
}

inline int MultiConnectionSelector::Data::computePollWait(){
	//TODO: think of a way to eliminate this if - e.g. MAXTIMEPOS==0?!
	if(ntimepos.seconds() == MAXTIMEPOS) return -1;//return MAXPOLLWAIT;
	int rv = (ntimepos.seconds() - ctimepos.seconds()) * 1000;
	rv += ((long)ntimepos.nanoSeconds() - ctimepos.nanoSeconds()) / 1000000;
	if(rv < 0) return MAXPOLLWAIT;
	return rv;
}
void MultiConnectionSelector::Data::addNewChannel(){
	++chnsz;
	if(chnsz > chncp){
		chncp += 64;//TODO: improve!!
		delete []pevs;
		pevs = new epoll_event[chncp];
		for(uint i = 0; i < chncp; ++i){
			pevs[i].events = 0;
			pevs[i].data.u64 = 0;
		}
	}
}
epoll_event* MultiConnectionSelector::Data::eventPrepare(
	epoll_event &_ev, unsigned _objpos, unsigned _pos
){
	_ev.data.u64 = _objpos;
	_ev.data.u64 <<= 32;
	_ev.data.u64 |= _pos;
	return &_ev;
}

inline std::pair<MultiConnectionSelector::Stub*, unsigned> 
MultiConnectionSelector::Data::stub(const epoll_event &_ev){
	uint32	objpos = (_ev.data.u64 >> 32);
	uint32	pos = (_ev.data.u64 & 0xffffffff);
	cassert(objpos < sz);
	return std::pair<MultiConnectionSelector::Stub*, unsigned>(pstubs + objpos, pos);
}
//-------------------------------------------------------------------
MultiConnectionSelector::MultiConnectionSelector():d(*(new Data)){
}

MultiConnectionSelector::~MultiConnectionSelector(){
	delete &d;
}

int MultiConnectionSelector::reserve(ulong _cp){
	cassert(_cp);
	cassert(!d.sz);
	if(_cp < d.cp) return OK;
	if(d.cp){
		cassert(false);
		delete []d.pevs;delete []d.pstubs;
	}
	d.cp = _cp;
	
	//The epoll_event table
	d.chncp = d.cp;
	d.pevs = new epoll_event[d.chncp];
	for(uint i = 0; i < d.chncp; ++i){
		d.pevs[i].events = 0;
		d.pevs[i].data.ptr = NULL;
	}
	
	//the Stub table:
	
	d.pstubs = new Stub[d.cp];
	//no need for initialization
	
	//Init the free channel stack:
	for(int i = d.cp - 1; i; --i){
		d.fstk.push(&d.pstubs[i]);
	}
	
	//init the channel queue
	//execq.reserve(cp);
	//zero is reserved for pipe
	if(d.epfd < 0 && (d.epfd = epoll_create(d.cp)) < 0){
		edbgx(Dbg::tcp, "epoll_create "<<strerror(errno));
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
		ev.data.u64 = 0;
		ev.events = EPOLLIN | EPOLLPRI;//must be LevelTriggered
		if(epoll_ctl(d.epfd, EPOLL_CTL_ADD, d.pipefds[0], &ev)){
			edbgx(Dbg::tcp, "epoll_ctl "<<strerror(errno));
			cassert(false);
			return BAD;
		}
	}
	idbgx(Dbg::tcp, "pipe fds "<<d.pipefds[0]<<" "<<d.pipefds[1]);
	d.ctimepos.set(0);
	d.ntimepos.set(Data::MAXTIMEPOS);
	d.selcnt = 0;
	d.sz = 1;
	return OK;
}

uint MultiConnectionSelector::capacity()const{
	return d.cp - 1;
}

uint MultiConnectionSelector::size() const{
	return d.sz;
}
int  MultiConnectionSelector::empty()const{
	return d.sz == 1;
}

int  MultiConnectionSelector::full()const{
	return d.sz == d.cp;
}

void MultiConnectionSelector::prepare(){
}

void MultiConnectionSelector::unprepare(){
}

void MultiConnectionSelector::push(const ObjectTp &_objptr, uint _thid){
	cassert(d.fstk.size());
	Stub *pstub = d.fstk.top(); d.fstk.pop();
	_objptr->setThread(_thid, pstub - d.pstubs);
	pstub->timepos.set(Data::MAXTIMEPOS);
	pstub->events = 0;
	epoll_event ev;
	ev.data.ptr = NULL;
	ev.events = 0;
	bool fail = false;
	for(unsigned i = 0; i < _objptr->channelCount(); ++i){
		Channel *p(_objptr->channel(i));
		if(p && p->descriptor() >= 0){
			if(
				epoll_ctl(d.epfd, EPOLL_CTL_ADD, p->descriptor(), d.eventPrepare(ev, pstub - d.pstubs, i))
			){
				edbgx(Dbg::tcp, "epoll_ctl adding filedesc "<<p->descriptor()<<" err = "<<strerror(errno));
				fail = true;
			}else{
				d.addNewChannel();
				p->prepare();
			}
		}
	}
	
	if(fail){
		doUnregisterObject(*_objptr);
		pstub->reset();
		d.fstk.push(pstub);
	}else{
		++d.sz;
		pstub->objptr = _objptr;
		idbgx(Dbg::tcp, "pushing connection "<<&(*(pstub->objptr))<<" on position "<<(pstub - d.pstubs));
		pstub->state = Stub::InExecQueue;
		d.execq.push(pstub);
	}
}

void MultiConnectionSelector::signal(uint _objid){
	idbgx(Dbg::tcp, "signal connection: "<<_objid);
	write(d.pipefds[1], &_objid, sizeof(uint32));
}

uint MultiConnectionSelector::doIo(Channel &_r, ulong _evs){
	if(_evs & (EPOLLERR | EPOLLHUP)){
		_r.clear();
		return ERRDONE;
	}
	int rv = 0;
	if(_evs & Channel::INTOUT){
		rv = _r.doRecv();
	}
	if(!(rv & ERRDONE) && (_evs & Channel::OUTTOUT)){
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
void MultiConnectionSelector::run(){
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
			idbgx(Dbg::tcp, "pollwait "<<pollwait<<" ntimepos.s = "<<d.ntimepos.seconds()<<" ntimepos.ns = "<<d.ntimepos.nanoSeconds());
			idbgx(Dbg::tcp, "ctimepos.s = "<<d.ctimepos.seconds()<<" ctimepos.ns = "<<d.ctimepos.nanoSeconds());
			nbcnt = -1;
        }
		
		d.selcnt = epoll_wait(d.epfd, d.pevs, d.sz, pollwait);
		if(d.selcnt < 0){
			edbgx(Dbg::tcp, "epoll_create "<<strerror(errno));
			//cassert(false);
		}
		idbgx(Dbg::tcp, "epollwait = "<<d.selcnt);
	}while(!(flags & Data::EXIT_LOOP));
}

uint MultiConnectionSelector::doAllIo(){
	uint		flags = 0;
	TimeSpec	crttout;
// 	epoll_event	ev;
	uint		evs;
	idbgx(Dbg::tcp, "selcnt = "<<d.selcnt);
	for(int i = 0; i < d.selcnt; ++i){
		std::pair<Stub*, uint32> stubpair = d.stub(d.pevs[i]);//reinterpret_cast<Stub*>(d.pevs[i].data.ptr);
		if(stubpair.first != d.pstubs){
			idbgx(Dbg::tcp, "state = "<<stubpair.first->state);
			cassert(stubpair.first->objptr->channel(stubpair.second));
			if((evs = doIo(*stubpair.first->objptr->channel(stubpair.second), d.pevs[i].events))){
				//first mark the channel in connection
				idbgx(Dbg::tcp, "evs = "<<evs<<" indone = "<<INDONE);
				stubpair.first->objptr->addDoneChannelNext(stubpair.second, evs);
				//push channel execqueue
				if(stubpair.first->state == Stub::OutExecQueue){
					d.execq.push(stubpair.first);
					stubpair.first->state = Stub::InExecQueue;
				}
			}
		}else{
			flags |= Data::READ_PIPE;
		}
	}
	return flags;
}
uint MultiConnectionSelector::doFullScan(){
	TimeSpec	crttout;
	uint		flags = 0;
	uint		evs;
	epoll_event	ev;
	++d.rep_fullscancount;
	idbgx(Dbg::tcp, "fullscan count "<<d.rep_fullscancount);
	d.ntimepos.set(Data::MAXTIMEPOS);
	for(uint i = 1; i < d.cp; ++i){
		Stub &rcs = d.pstubs[i];
		if(!rcs.objptr) continue;
		evs = 0;
		if(d.ctimepos >= rcs.timepos){
			evs |= TIMEOUT;
			rcs.objptr->addTimeoutChannels(d.ctimepos);
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
uint MultiConnectionSelector::doExecuteQueue(){
	TimeSpec	crttout;
	uint		flags = 0;
	uint		evs;
	epoll_event	ev;
	uint		qsz(d.execq.size());
	while(qsz){//we only do a single scan:
		Stub &rc = *d.execq.front();d.execq.pop(); --qsz;
		if(rc.state == Stub::InExecQueue){
			//rc.state &= ~Stub::InQExec;//moved to doExecute
			evs = 0;
			crttout = d.ctimepos;
			flags |= doExecute(rc, evs, crttout, ev);
		}
	}
	return flags;
}

int MultiConnectionSelector::doExecute(Stub &_rstub, ulong _evs, TimeSpec &_crttout, epoll_event &_rev){
	int rv = 0;
	MultiConnection	&robj = *_rstub.objptr;
	_rstub.state = Stub::OutExecQueue;
	robj.nextchntout.set(0xffffffff, 0xffffffff);
	switch(robj.execute(_evs, _crttout)){
		case BAD://close
			idbgx(Dbg::tcp, "BAD: removing the connection");
			d.fstk.push(&_rstub);
			//epoll_ctl(d.epfd, EPOLL_CTL_DEL, robj.channel().descriptor(), NULL);
			//unregister all channels
			doUnregisterObject(robj);
			_rstub.objptr.clear();
			--d.sz;
			rv = Data::EXIT_LOOP;
			break;
		case OK://
			idbgx(Dbg::tcp, "OK: reentering connection");
			d.execq.push(&_rstub);
			//TODO: do you have to uncomment the following line?
			//_rstub.objptr->clearResponseVector();
			_rstub.state = Stub::InExecQueue;
			_rstub.timepos.set(Data::MAXTIMEPOS);
			break;
		case NOK:
			idbgx(Dbg::tcp, "TOUT: connection waits for signals");
			doPrepareObjectWait(_rstub, _crttout);
// 			if(0){	
// 				ulong t = (EPOLLET) | robj.channel().ioRequest();
// 				if((_rstub.events & Data::EPOLLMASK) != t){
// 					idbgx(Dbg::tcp, "epollctl");
// 					_rstub.events = _rev.events = t;
// 					_rev.data.ptr = &_rstub;
// 					epoll_ctl(d.epfd, EPOLL_CTL_MOD, robj.channel().descriptor(), &_rev);
// 				}
// 				if(_crttout != d.ctimepos){
// 					_rstub.timepos = _crttout;
// 					if(d.ntimepos > _crttout) d.ntimepos = _crttout;
// 				}else{
// 					_rstub.timepos.set(Data::MAXTIMEPOS);
// 				}
//			}
			break;
		case LEAVE:
			idbgx(Dbg::tcp, "LEAVE: connection leave");
			d.fstk.push(&_rstub);
			doUnregisterObject(*_rstub.objptr);
			--d.sz;
			_rstub.objptr.release();
			//robj.channel().unprepare();
			rv = Data::EXIT_LOOP;
			break;
// 		case REGISTER:{//
// 			idbgx(Dbg::tcp, "REGISTER: register connection with new descriptor");
// 			_rev.data.ptr = &_rstub;
// 			uint ioreq = robj.channel().ioRequest();
// 			_rstub.events = _rev.events = (EPOLLET) | ioreq;
// 			epoll_ctl(d.epfd, EPOLL_CTL_ADD, robj.channel().descriptor(), &_rev);
// 			if(!ioreq){
// 				d.execq.push(&_rstub);
// 				_rstub.state = Stub::InExecQueue;
// 			}
// 			}break;
// 		case UNREGISTER:
// 			idbgx(Dbg::tcp, "UNREGISTER: unregister connection's descriptor");
// 			epoll_ctl(d.epfd, EPOLL_CTL_DEL, robj.channel().descriptor(), NULL);
// 			d.execq.push(&_rstub);
// 			_rstub.state = Stub::InExecQueue;
// 			break;
		default:
			cassert(false);
	}
	//idbgx(Dbg::tcp, "return "<<rv);
	return rv;
}

/**
 * Returns true if we have to check for new Stubs.
 */
int MultiConnectionSelector::doReadPipe(){
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
		idbgx(Dbg::tcp, "reading pipe dummy");
		while((rsz = read(d.pipefds[0], buf, BUFSZ)) > 0);
	}
	return rv;
}

void MultiConnectionSelector::doUnregisterObject(MultiConnection &_robj){
	for(MultiConnection::ChannelVectorTp::iterator it(_robj.chnvec.begin()); it != _robj.chnvec.end(); ++it){
		if(it->pchannel && it->pchannel->ok()){
			if(!epoll_ctl(d.epfd, EPOLL_CTL_DEL, it->pchannel->descriptor(), NULL)){
				--d.chnsz;
				it->pchannel->unprepare();
			}else{
				edbgx(Dbg::tcp, "epoll_ctl "<<strerror(errno));
			}
		}
	}
}
/*
	this is a difficult method as it does lots of things:
	- clear the response vector (whith responses from previous run)
	- iterate through all requesting channels and execute the requested opperation:
*/
void MultiConnectionSelector::doPrepareObjectWait(Stub &_rstub, const TimeSpec &_crttout){
	_rstub.objptr->clearResponseVector();
	MultiConnection &ro = *_rstub.objptr;
	bool mustwait = true;
	for(MultiConnection::UIntVectorTp::iterator it(ro.reqvec.begin()); it != ro.reqvec.end(); ++it){
		cassert(ro.chnvec[*it].pchannel);
		Channel &rs = *ro.chnvec[*it].pchannel;
		int flags = ro.chnvec[*it].flags & ~(MultiConnection::ChannelStub::Request);
		switch(flags){
			case MultiConnection::ChannelStub::IORequest:{
				epoll_event ev;
				uint t = (EPOLLET) | rs.ioRequest();
				if((ro.chnvec[*it].selevents & Data::EPOLLMASK) != t){
					idbgx(Dbg::tcp, "epollctl");
					ro.chnvec[*it].selevents = ev.events = t;
					if(epoll_ctl(d.epfd, EPOLL_CTL_MOD, rs.descriptor(), d.eventPrepare(ev, &_rstub - d.pstubs,*it))){
						edbgx(Dbg::tcp, "epoll_ctl "<<strerror(errno)<<" desc = "<<rs.descriptor());
						cassert(false);
					}
				}
				}break;
			case MultiConnection::ChannelStub::RegisterRequest:{
				epoll_event ev;
				rs.prepare();
				uint ioreq = rs.ioRequest();
				 ro.chnvec[*it].selevents = ev.events = (EPOLLET) | ioreq;
				if(epoll_ctl(d.epfd, EPOLL_CTL_ADD, rs.descriptor(), d.eventPrepare(ev, &_rstub - d.pstubs,*it))){
					edbgx(Dbg::tcp, "epoll_ctl "<<strerror(errno));
					cassert(false);
				}else{
					ro.addDoneChannelFirst(*it, OKDONE);
					if(!ioreq){
						d.execq.push(&_rstub);
						_rstub.state = Stub::InExecQueue;
					}else{
						mustwait = false;
					}
				}
				}break;
			case MultiConnection::ChannelStub::UnregisterRequest:
				if(rs.ok()){
					if(!epoll_ctl(d.epfd, EPOLL_CTL_DEL, rs.descriptor(), NULL)){
						--d.chnsz;
						rs.unprepare();
						ro.addDoneChannelFirst(*it, OKDONE);
					}else{
						edbgx(Dbg::tcp, "epoll_ctl: desc "<<rs.descriptor()<<" err "<<strerror(errno));
					}
				}
				mustwait = false;
				break;
			default:
				cassert(false);
		}//switch
	}//for
	//ok done with all channels
	ro.clearRequestVector();//done with the request
	if(mustwait){
		//we must set the timeout - which will be min(the returned one, channel's or the internal one)
		const TimeSpec *pts = &_crttout;//the returned time
		if(*pts > ro.nextchntout){
			pts = &ro.nextchntout;
		}
		if(*pts != d.ctimepos){
			_rstub.timepos = *pts;
			if(d.ntimepos > *pts) d.ntimepos = *pts;
		}else{
			_rstub.timepos.set(Data::MAXTIMEPOS);
		}
	}else{
		d.execq.push(&_rstub);
		_rstub.state = Stub::InExecQueue;
	}
}

}//namespace tcp
}//namespace clientserver

