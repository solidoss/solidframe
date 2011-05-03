/* Implementation file object.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "system/cassert.hpp"
#include "system/mutex.hpp"
#include "system/thread.hpp"
#include "system/debug.hpp"

#include "foundation/object.hpp"
#include "foundation/objectpointer.hpp"
#include "foundation/signal.hpp"
#include "foundation/manager.hpp"
#include "foundation/service.hpp"

#include "utility/memory.hpp"
#include "utility/dynamicpointer.hpp"

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
//----	ObjectPointerBase	----
//---------------------------------------------------------------------

void ObjectPointerBase::clear(Object *_pobj){
	cassert(_pobj);
	int usecnt = 0;
	{
		Mutex::Locker lock(Manager::the().mutex(*_pobj));
		usecnt = --_pobj->usecnt;
	}
	if(!usecnt){
		m().erase(*_pobj);
		delete _pobj;
	}
}

//NOTE: No locking so Be carefull!!
void ObjectPointerBase::use(Object *_pobj){
	//NOTE: the first mutex will be the first mutex from the first service
	//which is a valid mutex. The valid mutex will be received only
	//after objects registration within a service.
	Mutex::Locker lock(Manager::the().mutex(*_pobj));
	++_pobj->usecnt;
}
void ObjectPointerBase::destroy(Object *_pobj){
	delete _pobj;
}
//---------------------------------------------------------------------
//----	Object	----
//---------------------------------------------------------------------

#ifdef NINLINES
#include "foundation/object.ipp"
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
	return m().mutex(*this);
}
//--------------------------------------------------------------
ObjectUidT  Object::uid()const{
	return ObjectUidT(id(), m().uid(*this));
}

bool Object::signal(DynamicPointer<Signal> &_sig){
	return false;//by default do not raise the object
}
/**
 * Returns true if the object must be executed.
 */

bool Object::signal(ulong _smask){
	ulong oldmask = smask;
	smask |= _smask;
	return (smask != oldmask) && signaled(S_RAISE);
}

int Object::execute(ulong _evs, TimeSpec &_rtout){
	return BAD;
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

void Signal::ipcReceived(
	ipc::SignalUid&
){
}
uint32 Signal::ipcPrepare(){
	return 0;//do nothing - no wait for response
}
void Signal::ipcFail(int _err){
	wdbgx(Dbg::fdt,"");
}
void Signal::ipcSuccess(){
	wdbgx(Dbg::fdt,"");
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

/*virtual*/ void Object::init(Mutex*){
}


}//namespace

