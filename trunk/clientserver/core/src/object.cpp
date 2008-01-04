/* Implementation file object.cpp
	
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

#include <cassert>
#include "system/synchronization.h"

#include "system/debug.h"

#include "core/object.h"
#include "core/objptr.h"
#include "core/visitor.h"
#include "core/command.h"
#include "core/cmdptr.h"
#include "core/server.h"

namespace clientserver{
//---------------------------------------------------------------------
//----	ObjPtr	----
//---------------------------------------------------------------------

void ObjPtrBase::clear(Object *_pobj){
	assert(_pobj);
	int usecnt = 0;
	{
		Mutex::Locker lock(Server::the().mutex(*_pobj));
		usecnt = --_pobj->usecnt;
	}
	if(!usecnt) delete _pobj;
}

//NOTE: No locking so Be carefull!!
void ObjPtrBase::use(Object *_pobj){
	++_pobj->usecnt;
}
void ObjPtrBase::destroy(Object *_pobj){
	delete _pobj;
}
//---------------------------------------------------------------------
//----	CmdPtr	----
//---------------------------------------------------------------------

void CmdPtrBase::clear(Command *_pcmd){
	assert(_pcmd);
	if(_pcmd->release()) delete _pcmd;
}

void CmdPtrBase::use(Command *_pcmd){
	_pcmd->use();
}
//---------------------------------------------------------------------
//----	Object	----
//---------------------------------------------------------------------

Object::Object(ulong _fullid):	fullid(_fullid), smask(0), 
								thrid(0),thrpos(0),usecnt(0),crtstate(0){
}

Object::~Object(){
}
/*
void Object::threadid(ulong _thrid){
	thrid = _thrid;
}
*/
int Object::signal(CmdPtr<Command> &_cmd){
	return OK;
}
/**
 * Returns true if the object must be executed.
 */

int Object::signal(uint _smask){
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
//----	Command	----
//---------------------------------------------------------------------

Command::~Command(){
}

void Command::use(){
	idbg("Use command");
}
int Command::received(const ipc::ConnectorUid&){
	return false;
}
int Command::execute(Object &){
	idbg("Unhandled command");
	return 0;
}

int Command::release(){
	idbg("Release command");
	return BAD;
}

}//namespace

