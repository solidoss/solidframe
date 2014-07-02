// frame/src/objectselector.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <vector>
#include <stack>

#include "system/timespec.hpp"
#include "system/condition.hpp"
#include "system/mutex.hpp"

#include "utility/queue.hpp"
#include "utility/stack.hpp"


#include "system/debug.hpp"

#include "frame/manager.hpp"
#include "frame/object.hpp"
#include "frame/selector.hpp"
#include "frame/schedulerbase.hpp"

namespace solid{
namespace frame{

enum {MAXTIMEPOS = 0xffffffff};

struct Selector::Data{
	enum {EXIT_LOOP = 1, FULL_SCAN = 2, READ_PIPE = 4};
	struct ObjectStub{
		ObjectPointerT	objptr;
		TimeSpec		timepos;
		int				state;
	};
	typedef std::vector<ObjectStub>					ObjectStubVectorT;
	typedef Stack<ulong>							ULongStackT;
	typedef Queue<ulong>							ULongQueueT;
	Data():sz(0){}
	ulong 				sz;
	ObjectStubVectorT	sv;
	ULongQueueT			uiq;
	ULongStackT			fstk;
	Mutex				mtx;
	Condition			cnd;
	uint64				btimepos;//begin time pos
    TimeSpec			ntimepos;//next timepos == next timeout
    TimeSpec			ctimepos;//current time pos
    ULongQueueT			objq;
};



Selector::Selector(SchedulerBase &):d(*(new Data)){
}
Selector::~Selector(){
	delete &d;
}

/*virtual*/ void Selector::raise(uint32 _objidx){
	
}
/*virtual*/ void Selector::stop(){
	
}
/*virtual*/ void Selector::update(){
	
}

void Selector::run(){
	if(!prepareThread()){
		return;
	}
#if 0
	//ulong		crttout;
	int 		state;
	int 		pollwait = 0;
	const int	maxnbcnt = 256;
	int			nbcnt = -1;//non blocking opperations count
							//used to reduce the number of calls for the system time.
	do{
		state = 0;
		if(nbcnt < 0){
			d.ctimepos.currentRealTime();
			nbcnt = maxnbcnt;
		}
		
		if(d.ctimepos > d.ntimepos){
			state |= Data::FULL_SCAN;
		}
		if(state || d.objq.size()){
			pollwait = 0;
			--nbcnt;
		}else{ 
			if(d.ntimepos.seconds() != 0xffffffff) {
				pollwait = 1;
			}else pollwait = -1;
			nbcnt = -1;
        }
        
		state |= doWait(pollwait);
		
		if(state & Data::FULL_SCAN){
			idbgx(Debug::frame, "full_scan");
			ulong evs = 0;
			d.ntimepos.set(0xffffffff);
			for(Data::ObjectStubVectorT::iterator it(d.sv.begin()); it != d.sv.end(); it += 4){
				if(!it->objptr.empty()){
					//Data::ObjectStub &ro = *it;
					//evs = 0;
					//if(d.ctimepos >= ro.timepos) evs |= EventTimeout;
					//else if(d.ntimepos > ro.timepos) d.ntimepos = ro.timepos;
					//if(ro.objptr->notified(S_RAISE)) evs |= EventSignal;//should not be checked by objs
					//if(evs){
					//	state |= doExecute(it - d.sv.begin(), evs, d.ctimepos);
					//}
				}
				if(!(it + 1)->objptr.empty()){
					Data::ObjectStub &ro = *(it + 1);
					evs = 0;
					if(d.ctimepos >= ro.timepos) evs |= EventTimeout;
					else if(d.ntimepos > ro.timepos) d.ntimepos = ro.timepos;
					if(ro.objptr->notified(S_RAISE)) evs |= EventSignal;//should not be checked by objs
					if(evs){
						state |= doExecute(it - d.sv.begin() + 1, evs, d.ctimepos);
					}
				}
				if(!(it + 2)->objptr.empty()){
					Data::ObjectStub &ro = *(it + 2);
					evs = 0;
					if(d.ctimepos >= ro.timepos) evs |= EventTimeout;
					else if(d.ntimepos > ro.timepos) d.ntimepos = ro.timepos;
					if(ro.objptr->notified(S_RAISE)) evs |= EventSignal;//should not be checked by objs
					if(evs){
						state |= doExecute(it - d.sv.begin() + 2, evs, d.ctimepos);
					}
				}
				if(!(it + 3)->objptr.empty()){
					Data::ObjectStub &ro = *(it + 3);
					evs = 0;
					if(d.ctimepos >= ro.timepos) evs |= EventTimeout;
					else if(d.ntimepos > ro.timepos) d.ntimepos = ro.timepos;
					if(ro.objptr->notified(S_RAISE)) evs |= EventSignal;//should not be checked by objs
					if(evs){
						state |= doExecute(it - d.sv.begin() + 3, evs, d.ctimepos);
					}
				}
			}
		}
		
		{	
			int qsz = d.objq.size();
			while(qsz){//we only do a single scan:
				vdbgx(Debug::frame, "qsz = "<<qsz<<" queuesz "<<d.objq.size());
				int pos = d.objq.front();d.objq.pop(); --qsz;
				Data::ObjectStub &ro = d.sv[pos];
				if(ro.state){
					ro.state = 0;
					state |= doExecute(pos, 0, d.ctimepos);
				}
			}
		}
		vdbgx(Debug::frame, "sz = "<<d.sz);
		if(empty()) state |= Data::EXIT_LOOP;
	}while(state != Data::EXIT_LOOP);

#endif
	unprepareThread();
}


bool Selector::push(ObjectPointerT &_robj){
	cassert(d.fstk.size());
	uint pos = d.fstk.top();
	if(!this->setObjectThread(*_robj, pos)){
		return false;
	}
	d.fstk.pop();
	d.sv[pos].objptr = _robj;
	d.sv[pos].timepos = 0;
	d.sv[pos].state = 1;
	++d.sz;
	d.objq.push(pos);
	return true;
}

int Selector::doWait(int _wt){
	vdbgx(Debug::frame, "wt = "<<_wt);
	int rv = 0;
	Locker<Mutex> lock(d.mtx);
	if(_wt){
		if(_wt > 0){
			TimeSpec ts(d.ctimepos);
			ts.add(60);//1 min
			if(ts > d.ntimepos){
				ts = d.ntimepos;
			}
			vdbgx(Debug::frame, "uiq.size = "<<d.uiq.size());
			while(d.uiq.empty()){
				vdbgx(Debug::frame, "before cond wait");
				if(d.cnd.wait(lock, ts)){
					vdbgx(Debug::frame, "after 1 cond wait");
					d.ctimepos.currentRealTime();
					rv |= Data::FULL_SCAN;
					break;
				}
				vdbgx(Debug::frame, "after 2 cond wait");
				d.ctimepos.currentRealTime();
				if(d.ctimepos >= d.ntimepos){
					rv |= Data::FULL_SCAN;
					break;
				}
			}
		}else{
			while(d.uiq.empty()){
				d.cnd.wait(lock);
			}
			d.ctimepos.currentRealTime();
		}
	}
	if(d.uiq.size()){
		//we have something in the queue
		do{
#if 0
			ulong id = d.uiq.front(); d.uiq.pop();
			if(id){

				Data::ObjectStub *pobj;
				if(id < d.sv.size() && !(pobj = &d.sv[id])->objptr.empty() && !pobj->state && pobj->objptr->notified(S_RAISE)){
					vdbgx(Debug::frame, "signaling object id = "<<id);
					d.objq.push(id);
					pobj->state = 1;
				}
			}else rv |= Data::EXIT_LOOP;
#endif
		}while(d.uiq.size());
	}else{if(_wt) rv |= Data::FULL_SCAN;}
	vdbgx(Debug::frame, "rv = "<<rv);
	return rv;
}

int Selector::doExecute(unsigned _i, ulong _evs, TimeSpec _crttout){
#if 0
	this->associateObjectToCurrentThread(*d.sv[_i].objptr);
	
	int							rv = 0;
	Object::ExecuteController	exectl(_evs, _crttout);
	
	this->executeObject(*d.sv[_i].objptr, exectl);
	
	switch(exectl.returnValue()){
		case Object::ExecuteContext::RescheduleRequest:
			idbgx(Debug::frame, "OK: reentering object");
			if(!d.sv[_i].state) {d.objq.push(_i); d.sv[_i].state = 1;}
			d.sv[_i].timepos.set(0xffffffff);
			break;
		case Object::ExecuteContext::WaitRequest:
			idbgx(Debug::frame, "TOUT: object waits for signals");
			d.sv[_i].timepos.set(0xffffffff);
			break;
		case Object::ExecuteContext::WaitUntilRequest:
			idbgx(Debug::frame, "TOUT: object waits for signals");
			d.sv[_i].timepos = exectl.waitTime();
			if(d.ntimepos > exectl.waitTime()){
				d.ntimepos = exectl.waitTime();
			}
			break;
		case Object::ExecuteContext::LeaveRequest:
			idbgx(Debug::frame, "LEAVE: object leave");
			d.fstk.push(_i);
			d.sv[_i].objptr.release();
			d.sv[_i].state = 0;
			--d.sz;
			if(empty()) rv = Data::EXIT_LOOP;
			break;
		case Object::ExecuteContext::CloseRequest:
			d.fstk.push(_i);
			this->stopObject(*d.sv[_i].objptr);
			d.sv[_i].objptr.clear();
			d.sv[_i].state = 0;
			--d.sz;
			if(empty()) rv = Data::EXIT_LOOP;
			break;
		default: cassert(false);break;
	}
	return rv;
#endif
	return -1;
}

//=====================================================================
bool SelectorBase::setObjectThread(ObjectBase &_robj, const UidT &_uid){
	//we are sure that the method is called from within a Manager thread
	UidT	uid(Manager::specific().computeThreadId(mgridx, _uid.index), _uid.unique);
	if(uid.isValid()){
		_robj.runId(uid);
		return true;
	}else{
		return false;
	}
}
bool SelectorBase::prepareThread(){
	return scheduler().prepareThread(*this);
}
void SelectorBase::unprepareThread(){
	scheduler().unprepareThread(*this);
}

}//namespace frame
}//namespace solid


