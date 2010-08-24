/* Implementation file objectselector.cpp
	
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

#include "system/debug.hpp"

#include "foundation/object.hpp"
#include "foundation/objectselector.hpp"

namespace foundation{

enum {MAXTIMEPOS = 0xffffffff};

ObjectSelector::ObjectSelector():sz(0){
}
ObjectSelector::~ObjectSelector(){
}
int ObjectSelector::reserve(ulong _cp){
	sv.resize(_cp);
	//objq.reserve(_cp);
	for(unsigned i = _cp - 1; i; --i) fstk.push(i);//all but first pos (0)
	ctimepos.set(0);
	ntimepos.set(MAXTIMEPOS);
	sz = 0;
	return OK;
}

void ObjectSelector::signal(uint _pos){
	Mutex::Locker lock(mtx);
	uiq.push(_pos);
	cnd.signal();
}

/*
NOTE: TODO:
	See if this ideea will bring faster timeout scan (see it implemented in FileManager) 
	- add a new deque<unsigned> toutv
	- add to SelObject an toutidx int value 
	
	When an object waits for tout:
		- add its index at the end of toutv.
		- set toutidx to (toutv.size() - 1);
	When an object doesnt have timeout (waits forever) toutidx = -1;
	
	When we have an event on an object:
		- toutv[toutidx] = toutv.back();
		- toutv.pop_back();
		- toutidx = -1;
		
	When timeout:
		for(it=toutv.begin(); it != toutv.end(); ++it){
			if(timeout(sv[*it])){
				...
			}
		}
*/

void ObjectSelector::run(){
	//ulong		crttout;
	int 		state;
	int 		pollwait = 0;
	const int	maxnbcnt = 256;
	int			nbcnt = -1;//non blocking opperations count
							//used to reduce the number of calls for the system time.
	do{
		state = 0;
		if(nbcnt < 0){
			clock_gettime(CLOCK_REALTIME, &ctimepos);
			nbcnt = maxnbcnt;
		}
		
		if(ctimepos > ntimepos){
			state |= FULL_SCAN;
		}
		if(state || objq.size()){
			pollwait = 0;
			--nbcnt;
		}else{ 
			if(ntimepos.seconds() != 0xffffffff) {
				pollwait = 1;
			} else pollwait = -1;
			nbcnt = -1;
        }
		state |= doWait(pollwait);
		
		if(state & FULL_SCAN){
			idbgx(Dbg::fdt, "full_scan");
			ulong evs = 0;
			ntimepos.set(0xffffffff);
			for(SelVecT::iterator it(sv.begin()); it != sv.end(); ++it){
				SelObject &ro = *it;
				if(ro.objptr){
					evs = 0;
					if(ctimepos >= ro.timepos) evs |= TIMEOUT;
					else if(ntimepos > ro.timepos) ntimepos = ro.timepos;
					if(ro.objptr->signaled(S_RAISE)) evs |= SIGNALED;//should not be checked by objs
					if(evs){
						state |= doExecute(it - sv.begin(), evs, ctimepos);
					}
				}
			}
		}
		
		{	
			int qsz = objq.size();
			while(qsz){//we only do a single scan:
				vdbgx(Dbg::fdt, "qsz = "<<qsz<<" queuesz "<<objq.size());
				int pos = objq.front();objq.pop(); --qsz;
				SelObject &ro = sv[pos];
				if(ro.state){
					ro.state = 0;
					state |= doExecute(pos, 0, ctimepos);
				}
			}
		}
		vdbgx(Dbg::fdt, "sz = "<<sz);
		if(empty()) state |= EXIT_LOOP;
	}while(state != EXIT_LOOP);
}

void ObjectSelector::push(const ObjectPtrT &_robj, uint _thid){
	cassert(fstk.size());
	uint pos = fstk.top(); fstk.pop();
	_robj->setThread(_thid, pos);
	sv[pos].objptr = _robj;
	sv[pos].timepos = 0;
	sv[pos].state = 1;
	++sz;
	objq.push(pos);
}

int ObjectSelector::doWait(int _wt){
	vdbgx(Dbg::fdt, "wt = "<<_wt);
	int rv = 0;
	mtx.lock();
	if(_wt){
		if(_wt > 0){
			TimeSpec ts(ctimepos);
			ts.add(60);//1 min
			if(ts > ntimepos){
				ts = ntimepos;
			}
			vdbgx(Dbg::fdt, "uiq.size = "<<uiq.size());
			while(uiq.empty()){
				vdbgx(Dbg::fdt, "before cond wait");
				if(cnd.wait(mtx,ts)){
					vdbgx(Dbg::fdt, "after 1 cond wait");
					clock_gettime(CLOCK_REALTIME, &ctimepos);
					rv |= FULL_SCAN;
					break;
				}
				vdbgx(Dbg::fdt, "after 2 cond wait");
				clock_gettime(CLOCK_REALTIME, &ctimepos);
				if(ctimepos >= ntimepos){
					rv |= FULL_SCAN;
					break;
				}
			}
		}else{
			while(uiq.empty()) cnd.wait(mtx);
			clock_gettime(CLOCK_REALTIME, &ctimepos);
		}
	}
	if(uiq.size()){
		//we have something in the queue
		do{
			ulong id = uiq.front(); uiq.pop();
			if(id){
				SelObject *pobj;
				if(id < sv.size() && (pobj = &sv[id])->objptr && !pobj->state && pobj->objptr->signaled(S_RAISE)){
					vdbgx(Dbg::fdt, "signaling object id = "<<id);
					objq.push(id);
					pobj->state = 1;
				}
			}else rv |= EXIT_LOOP;
		}while(uiq.size());
	}else{if(_wt) rv |= FULL_SCAN;}
	mtx.unlock();
	vdbgx(Dbg::fdt, "rv = "<<rv);
	return rv;
}

int ObjectSelector::doExecute(unsigned _i, ulong _evs, TimeSpec _crttout){
	int rv = 0;
	sv[_i].objptr->associateToCurrentThread();
	switch(sv[_i].objptr->execute(_evs, _crttout)){
		case BAD:
			fstk.push(_i);
			sv[_i].objptr.clear();
			sv[_i].state = 0;
			--sz;
			if(empty()) rv = EXIT_LOOP;
			break;
		case OK:
			idbgx(Dbg::fdt, "OK: reentering object");
			if(!sv[_i].state) {objq.push(_i); sv[_i].state = 1;}
			sv[_i].timepos.set(0xffffffff);
			break;
		case NOK:
			idbgx(Dbg::fdt, "TOUT: object waits for signals");
			if(_crttout != ctimepos){
				sv[_i].timepos = _crttout;
				if(ntimepos > _crttout){
					ntimepos = _crttout;
				}
			}else{
				sv[_i].timepos.set(0xffffffff);
			}
			break;
		case LEAVE:
			idbgx(Dbg::fdt, "LEAVE: object leave");
			fstk.push(_i);
			sv[_i].objptr.release();
			sv[_i].state = 0;
			--sz;
			if(empty()) rv = EXIT_LOOP;
			break;
		default: cassert(false);
	}
	return rv;
}

}//namespace foundation


