/* Implementation file object.cpp
	
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

#include "system/cassert.hpp"
#include "system/mutex.hpp"
#include "system/thread.hpp"
#include "system/debug.hpp"

#include "utility/dynamicpointer.hpp"

#include "foundation/object.hpp"
#include "foundation/objectpointer.hpp"
#include "foundation/visitor.hpp"
#include "foundation/signal.hpp"
#include "foundation/manager.hpp"

#include "utility/memory.hpp"

//--------------------------------------------------------------
namespace{
static const unsigned specificPosition(){
	//TODO: staticproblem
	static const unsigned	thrspecpos = Thread::specificId();
	return thrspecpos;
}
}


namespace foundation{
//---------------------------------------------------------------------
//----	Visitor	----
//---------------------------------------------------------------------

Visitor::Visitor(){
}

Visitor::~Visitor(){
}
//---------------------------------------------------------------------
//----	ObjectPointerBase	----
//---------------------------------------------------------------------

void ObjectPointerBase::clear(Object *_pobj){
	cassert(_pobj);
	int usecnt = 0;
	{
		Mutex::Locker lock(Manager::the().mutex(*_pobj));
		usecnt = --_pobj->usecnt;
	}
	if(!usecnt) delete _pobj;
}

//NOTE: No locking so Be carefull!!
void ObjectPointerBase::use(Object *_pobj){
	++_pobj->usecnt;
}
void ObjectPointerBase::destroy(Object *_pobj){
	delete _pobj;
}
//---------------------------------------------------------------------
//----	Object	----
//---------------------------------------------------------------------

#ifdef NINLINES
#include "object.ipp"
#endif

Object::Object(IndexT _fullid):
	fullid(_fullid), smask(0),
	thrid(0),thrpos(0),usecnt(0),crtstate(0){
}

Object::~Object(){
}

//--------------------------------------------------------------
/*static*/ Object& Object::the(){
	return *reinterpret_cast<Object*>(Thread::specific(specificPosition()));
}
//--------------------------------------------------------------
void Object::associateToCurrentThread(){
	Thread::specific(specificPosition(), this);
}
//--------------------------------------------------------------
Mutex& Object::mutex()const{
	return Manager::the().mutex(*this);
}
//--------------------------------------------------------------
/*
void Object::threadid(ulong _thrid){
	thrid = _thrid;
}
*/
uint32  Object::uid()const{
	return Manager::the().uid(*this);
}
int Object::signal(DynamicPointer<Signal> &_sig){
	return OK;
}
/**
 * Returns true if the object must be executed.
 */

ulong Object::signal(ulong _smask){
	ulong oldmask = smask;
	smask |= _smask;
	return (smask != oldmask) && signaled(S_RAISE);
}


int Object::accept(Visitor &_roi){
	return _roi.visit(*this);
}

int Object::execute(){
	return BAD;
}
int Object::execute(ulong _evs, TimeSpec &_rtout){
	return BAD;
}
//we do not need the keep a pointer to mutex
void Object::mutex(Mutex *){
}
//---------------------------------------------------------------------
//----	Signal	----
//---------------------------------------------------------------------
Signal::Signal(){
	objectCheck<Signal>(true, __FUNCTION__);
	vdbgx(Dbg::fdt, "memadd "<<(void*)this);
}
Signal::~Signal(){
	objectCheck<Signal>(false, __FUNCTION__);
	vdbgx(Dbg::fdt, "memsub "<<(void*)this);
}

int Signal::ipcReceived(
	ipc::SignalUid&,
	const ipc::ConnectionUid&,
	const SockAddrPair &_peeraddr,
	int _peerbaseport
){
	return BAD;
}
int Signal::ipcPrepare(const ipc::SignalUid&){
	return OK;//do nothing - no wait for response
}
void Signal::ipcFail(int _err){
}

int Signal::execute(
	DynamicPointer<Signal> &_rthis_ptr,
	uint32 _evs,
	SignalExecuter &,
	const SignalUidT &,
	TimeSpec &_rts
){
	wdbgx(Dbg::fdt, "Unhandled signal");
	return BAD;
}

int Signal::receiveSignal(
	DynamicPointer<Signal> &_rsig,
	const ObjectUidT& _from,
	const ipc::ConnectionUid *_conid
){
	wdbgx(Dbg::fdt, "Unhandled signal receive");
	return BAD;//no need for execution
}
}//namespace

