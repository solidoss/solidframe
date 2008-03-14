/* Implementation file service.cpp
	
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

#include <deque>
#include <vector>
#include <algorithm>

#include "system/debug.hpp"

#include "utility/queue.hpp"

#include "core/object.hpp"
#include "core/server.hpp"
#include "core/command.hpp"
#include "core/readwriteservice.hpp"
#include "core/common.hpp"

namespace clientserver{

typedef std::pair<Object*, ulong>	ObjPairTp;
class ObjectVector:public std::deque<ObjPairTp>{};
class IndexStack: public std::stack<ulong>{};

Service::Service(int _objpermutbts, int _mutrowsbts, int _mutcolsbts):
			objv(*(new ObjectVector)),
			inds(*(new IndexStack)),
			mut(NULL),
			objcnt(0),
			mutpool(_objpermutbts, _mutrowsbts, _mutcolsbts){
	state(Running);
}

Service::~Service(){
	//stop();
	idbg("here");
	cassert(!objcnt);
	delete &objv;
	delete &inds;
}

int Service::insert(Object &_robj, ulong _srvid){
	Mutex::Locker lock(*mut);
	return doInsert(_robj, _srvid);
}

int Service::doInsert(Object &_robj, ulong _srvid){
	if(state() != Running) return BAD;
	if(inds.size()){
		{
			Mutex &rmut = mutpool.object(inds.top());
			Mutex::Locker lock(rmut);
			_robj.mutex(&rmut);
			objv[inds.top()].first = &_robj;
			_robj.id(_srvid, inds.top());
		}
		inds.pop();
	}else{
		//worst case scenario
		Mutex &rmut = mutpool.safeObject(objv.size());
		Mutex::Locker lock(rmut);
		_robj.mutex(&rmut);
		_robj.id(_srvid, objv.size());
		objv.push_back(ObjPairTp(&_robj, 0));	
	}
	++objcnt;
	return OK;
}

void Service::mutex(Mutex *_pmut){
	mut = _pmut;
}
void Service::remove(Object &_robj){
	Mutex::Locker lock1(*mut);
	//obj's mutext should not be locked
	Mutex::Locker	lock2(mutpool.object(_robj.index()));
	idbg("removig object with index "<<_robj.index());
	objv[_robj.index()].first = NULL;
	++objv[_robj.index()].second;
	inds.push(_robj.index());
	--objcnt;
	cond.signal();
}
/**
 * Be very carefull when using this function as you can raise/kill
 * other object than you might want.
 */
int Service::signal(ulong _fullid, ulong _uid, Server &_rsrv, ulong _sigmask){
	Mutex::Locker	lock(mutpool.object(Object::computeIndex(_fullid)));
	if(_uid != objv[Object::computeIndex(_fullid)].second) return BAD;
	Object *pobj = objv[Object::computeIndex(_fullid)].first;
	if(!pobj) return BAD;
	if(pobj->signal(_sigmask)){
		_rsrv.raiseObject(*pobj);
	}
	return OK;
}

int Service::signal(Object &_robj, Server &_rsrv, ulong _sigmask){
	Mutex::Locker	lock(mutpool.object(_robj.index()));
	if(_robj.signal(_sigmask)){
		_rsrv.raiseObject(_robj);
	}
	return OK;
}

Mutex& Service::mutex(ulong _fullid, ulong){
	return mutpool.object(Object::computeIndex(_fullid));
}

Object* Service::object(ulong _fullid, ulong _uid){
	if(_uid != objv[Object::computeIndex(_fullid)].second) return NULL;
	return objv[Object::computeIndex(_fullid)].first;
}

int Service::signal(Object &_robj, Server &_rsrv, CmdPtr<Command> &_cmd){
	Mutex::Locker	lock(mutpool.object(_robj.index()));
	if(_robj.signal(_cmd)){
		_rsrv.raiseObject(_robj);
	}
	return OK;
}

int Service::signal(ulong _fullid, ulong _uid, Server &_rsrv, CmdPtr<Command> &_cmd){
	Mutex::Locker	lock(mutpool.object(Object::computeIndex(_fullid)));
	if(_uid != objv[Object::computeIndex(_fullid)].second) return BAD;
	Object *pobj = objv[Object::computeIndex(_fullid)].first;
	if(!pobj) return BAD;
	if(pobj->signal(_cmd)){
		_rsrv.raiseObject(*pobj);
	}
	return OK;
}

void Service::signalAll(Server &_rsrv, ulong _sigmask){
	Mutex::Locker	lock(*mut);
	doSignalAll(_rsrv, _sigmask);
}

void Service::signalAll(Server &_rsrv, CmdPtr<Command> &_cmd){
	Mutex::Locker	lock(*mut);
	doSignalAll(_rsrv, _cmd);
}

void Service::doSignalAll(Server &_rsrv, ulong _sigmask){
	int oc = objcnt;
	int i = 0;
	int mi = -1;
	idbg("signalling "<<oc<<" objects");
	for(ObjectVector::iterator it(objv.begin()); oc && it != objv.end(); ++it, ++i){
		if(it->first){
			//if(!(i & objpermutmsk)){
			if(mutpool.isRangeBegin(i)){
				if(mi >= 0)	mutpool[mi].unlock();
				++mi;
				mutpool[mi].lock();
			}
			if(it->first->signal(_sigmask)){
				_rsrv.raiseObject(*it->first);
			}
			--oc;
		}
	}
	if(mi >= 0)	mutpool[mi].unlock();
}

void Service::doSignalAll(Server &_rsrv, CmdPtr<Command> &_cmd){
	int oc = objcnt;
	int i = 0;
	int mi = -1;
	idbg("signalling "<<oc<<" objects");
	for(ObjectVector::iterator it(objv.begin()); oc && it != objv.end(); ++it, ++i){
		if(it->first){
			//if(!(i & objpermutmsk)){
			if(mutpool.isRangeBegin(i)){
				if(mi >= 0)	mutpool[mi].unlock();
				++mi;
				mutpool[mi].lock();
			}
			CmdPtr<Command> cmd(_cmd.ptr());
			if(it->first->signal(cmd)){
				_rsrv.raiseObject(*it->first);
			}
			--oc;
		}
	}
	if(mi >= 0)	mutpool[mi].unlock();
}

void Service::visit(Server &_rsrv, Visitor &_rov){
	Mutex::Locker	lock(*mut);
	int oc = objcnt;
	int i = 0;
	int mi = -1;
	for(ObjectVector::iterator it(objv.begin()); oc && it != objv.end(); ++it, ++i){
		if(it->first){
			if(mutpool.isRangeBegin(i)){
				if(mi >= 0)	mutpool[mi].unlock();
				++mi;
				mutpool[mi].lock();
			}
			if(it->first->accept(_rov)){
				_rsrv.raiseObject(*it->first);
			}
			--oc;
		}
	}
	if(mi >= 0)	mutpool[mi].unlock();
}

Mutex& Service::mutex(Object &_robj){
	return mutpool.object(_robj.index());
}

ulong Service::uid(Object &_robj)const{
	return objv[_robj.index()].second;
}

int Service::start(Server &_rsrv){
	Mutex::Locker	lock(*mut);
	if(state() != Stopped) return OK;
	state(Running);
	return OK;
}

int Service::stop(Server &_rsrv, bool _wait){
	Mutex::Locker	lock(*mut);
	if(state() == Running){
		doSignalAll(_rsrv, S_KILL | S_RAISE);
		state(Stopping);
	}
	if(!_wait) return OK;
	while(objcnt){
		cond.wait(*mut);
	}
	state(Stopped);
	return OK;
}
int Service::execute(){
	return BAD;
}
//***********************************************************************
ReadWriteService::~ReadWriteService(){
}
Condition* ReadWriteService::popCondition(Object &_robj){
	CondStackTp &rs = cndpool.object(_robj.index());
	Condition *pc = NULL;
	if(rs.size()){
		pc = rs.top();rs.pop();
	}else pc = new Condition;
	return pc;
}
void ReadWriteService:: pushCondition(Object &_robj, Condition *&_rpc){
	if(_rpc){
		cndpool.object(_robj.index()).push(_rpc);
		_rpc = NULL;
	}
}
ReadWriteService::ReadWriteService(int _objpermutbts,
									int _mutrowsbts,
									int _mutcolsbts
									):
									Service(_objpermutbts, _mutrowsbts, _mutcolsbts){}
int ReadWriteService::insert(Object &_robj, ulong _srvid){
	Mutex::Locker lock(*mut);
	if(state() != Running) return BAD;
	if(inds.size()){
		{
			Mutex::Locker lock(mutpool.object(inds.top()));
			objv[inds.top()].first = &_robj;
			_robj.id(_srvid, inds.top());
		}
		inds.pop();
	}else{
		//worst case scenario
		Mutex::Locker lock(mutpool.safeObject(objv.size()));
		//just ensure the resevation in cndpool
		cndpool.safeObject(objv.size());
		_robj.id(_srvid, objv.size());
		objv.push_back(ObjPairTp(&_robj, 0));	
	}
	++objcnt;
	return OK;
}
}//namespace

