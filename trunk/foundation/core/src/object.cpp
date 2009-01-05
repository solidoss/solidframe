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

#include "system/debug.hpp"

#include "core/object.hpp"
#include "core/objptr.hpp"
#include "core/visitor.hpp"
#include "core/command.hpp"
#include "core/cmdptr.hpp"
#include "core/server.hpp"

#include "utility/memory.hpp"
namespace clientserver{
//---------------------------------------------------------------------
//----	Visitor	----
//---------------------------------------------------------------------

Visitor::Visitor(){
}

Visitor::~Visitor(){
}
//---------------------------------------------------------------------
//----	ObjPtr	----
//---------------------------------------------------------------------

void ObjPtrBase::clear(Object *_pobj){
	cassert(_pobj);
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
	cassert(_pcmd);
	if(_pcmd->release()) delete _pcmd;
}

void CmdPtrBase::use(Command *_pcmd){
	_pcmd->use();
}
//---------------------------------------------------------------------
//----	Object	----
//---------------------------------------------------------------------

#ifndef UINLINES
#include "object.ipp"
#endif

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
Command::Command(){
	objectCheck<Command>(true, __FUNCTION__);
	idbgx(Dbg::cs, "memadd "<<(void*)this);
}
Command::~Command(){
	objectCheck<Command>(false, __FUNCTION__);
	idbgx(Dbg::cs, "memsub "<<(void*)this);
}

void Command::use(){
	idbgx(Dbg::cs, "Use command");
}
int Command::ipcReceived(ipc::CommandUid&, const ipc::ConnectorUid&){
	return BAD;
}
int Command::ipcPrepare(const ipc::CommandUid&){
	return OK;//do nothing - no wait for response
}
void Command::ipcFail(int _err){
}
int Command::execute(Object &){
	idbgx(Dbg::cs, "Unhandled command");
	return 0;
}

int Command::execute(uint32 _evs, CommandExecuter &, const CommandUidTp &, TimeSpec &_rts){
	idbgx(Dbg::cs, "Unhandled command");
	return BAD;
}

int Command::release(){
	idbgx(Dbg::cs, "Release command");
	return BAD;
}

int Command::receiveCommand(
	CmdPtr<Command> &_rcmd,
	int			_which,
	const ObjectUidTp&_from,
	const ipc::ConnectorUid *_conid
){
	cassert(false);
	return BAD;
}

int Command::receiveIStream(
	StreamPtr<IStream> &,
	const FileUidTp	&,
	int			_which,
	const ObjectUidTp&,
	const ipc::ConnectorUid *
){
	cassert(false);
	return BAD;
}
int Command::receiveOStream(
	StreamPtr<OStream> &,
	const FileUidTp	&,
	int			_which,
	const ObjectUidTp&,
	const ipc::ConnectorUid *
){
	cassert(false);
	return BAD;
}
int Command::receiveIOStream(
	StreamPtr<IOStream> &,
	const FileUidTp	&,
	int			_which,
	const ObjectUidTp&,
	const ipc::ConnectorUid *
){
	cassert(false);
	return BAD;
}
int Command::receiveString(
	const std::string &_str,
	int			_which,
	const ObjectUidTp&,
	const ipc::ConnectorUid *
){
	cassert(false);
	return BAD;
}
int Command::receiveNumber(
	const int64 &,
	int			_which,
	const ObjectUidTp&,
	const ipc::ConnectorUid *
){
	cassert(false);
	return BAD;
}
int Command::receiveError(
	int, 
	const ObjectUidTp&,
	const ipc::ConnectorUid *
){
	cassert(false);
	return BAD;
}

}//namespace

