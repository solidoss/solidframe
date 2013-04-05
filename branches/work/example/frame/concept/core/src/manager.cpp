/* Implementation file manager.cpp
	
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

#include <map>
#include <vector>

#include "system/debug.hpp"

#include "utility/iostream.hpp"
#include "utility/dynamictype.hpp"

#include "core/manager.hpp"
#include "core/service.hpp"
#include "core/messages.hpp"
#include "quicklz.h"

#include "frame/aio/aioselector.hpp"
#include "frame/aio/aioobject.hpp"

#include "frame/objectselector.hpp"
#include "frame/messagesteward.hpp"
#include "frame/message.hpp"
#include "frame/requestuid.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/file/filemanager.hpp"
#include "frame/file/filemappers.hpp"

#include <iostream>

using namespace std;
using namespace solid;


namespace concept{
//------------------------------------------------------
//		FileManagerController
//------------------------------------------------------

class FileManagerController: public solid::frame::file::Manager::Controller{
public:
	FileManagerController(){}
protected:
	/*virtual*/ void init(const frame::file::Manager::InitStub &_ris);
	 
	/*virtual*/ bool release();
	
	/*virtual*/ void sendStream(
		StreamPointer<InputStream> &_sptr,
		const FileUidT &_rfuid,
		const solid::frame::RequestUid& _rrequid
	);
	/*virtual*/ void sendStream(
		StreamPointer<OutputStream> &_sptr,
		const FileUidT &_rfuid,
		const solid::frame::RequestUid& _rrequid
	);
	/*virtual*/ void sendStream(
		StreamPointer<InputOutputStream> &_sptr,
		const FileUidT &_rfuid,
		const solid::frame::RequestUid& _rrequid
	);
	/*virtual*/ void sendError(
		int _error,
		const solid::frame::RequestUid& _rrequid
	);
};


//------------------------------------------------------
//		IpcServiceController
//------------------------------------------------------
/*
In this example we do:
* accept only one connection from one other concept application
* authenticate in X steps:
	> concept1 -> request auth step 1 -> concept2
	> concept2 -> auth step 2 -> concept1
	> concept1 -> auth step 3 -> concept2
	> concept2 accept authentication -> auth step 4 -> concept1 auth done
*/


struct AuthMessage: Dynamic<AuthMessage, DynamicShared<frame::Message> >{
	AuthMessage():authidx(0), authcnt(0){}
	~AuthMessage(){}
	
	template <class S>
	S& operator&(S &_s){
		_s.push(authidx, "authidx").push(authcnt, "authcnt");
		if(S::IsDeserializer){
			_s.push(msguid.idx, "msguid.idx").push(msguid.uid,"msguid.uid");
		}else{//on sender
			frame::ipc::MessageUid &rmsguid(
				const_cast<frame::ipc::MessageUid &>(frame::ipc::ConnectionContext::the().msgid)
			);
			_s.push(rmsguid.idx, "msguid.idx").push(rmsguid.uid,"msguid.uid");
		}
		_s.push(msguidpeer.idx, "msguidpeer.idx").push(msguidpeer.uid,"msguidpeer.uid");
		return _s;
	}
//data:
	int							authidx;
	int							authcnt;
	frame::ipc::MessageUid		msguid;
	frame::ipc::MessageUid		msguidpeer;
};


struct IpcServiceController: frame::ipc::Controller{
	IpcServiceController():frame::ipc::Controller(0, 400), authidx(0){
		use();
	}
	
	/*virtual*/ void scheduleTalker(frame::aio::Object *_po);
	/*virtual*/ void scheduleListener(frame::aio::Object *_po);
	/*virtual*/ void scheduleNode(frame::aio::Object *_po);
	
	
	/*virtual*/ bool compressBuffer(
		frame::ipc::BufferContext &_rbc,
		const uint32 _bufsz,
		char* &_rpb,
		uint32 &_bl
	);
	/*virtual*/ bool decompressBuffer(
		frame::ipc::BufferContext &_rbc,
		char* &_rpb,
		uint32 &_bl
	);
	/*virtual*/ int authenticate(
		DynamicPointer<frame::Message> &_msgptr,//the received signal
		frame::ipc::MessageUid &_rmsguid,
		uint32 &_rflags,
		frame::ipc::SerializationTypeIdT &_rtid
	);
	
private:
	qlz_state_compress		qlz_comp_ctx;
	qlz_state_decompress	qlz_decomp_ctx;
	int						authidx;
	uint32					netid;
};


//------------------------------------------------------
//		Manager::Data
//------------------------------------------------------

struct Manager::Data{
	Data(Manager &_rm):
		mainaiosched(_rm), scndaiosched(_rm), objsched(_rm),
		ipcsvc(_rm, &ipcctrl){
	}
	typedef DynamicSharedPointer<frame::file::Manager>	FileManagerSharedPointerT;
	
	FileManagerController		fmctrl;
	IpcServiceController		ipcctrl;
	AioSchedulerT				mainaiosched;
	AioSchedulerT				scndaiosched;
	SchedulerT					objsched;
	frame::ipc::Service			ipcsvc;
	FileManagerSharedPointerT	filemgrptr;
	ObjectUidT					readmsgstwuid;
	ObjectUidT					writemsgstwuid;
};

//--------------------------------------------------------------------------
/*static*/ Manager& Manager::the(Manager *_pm){
	static Manager *pm(_pm);
	return *pm;
}
Manager::Manager():frame::Manager(), d(*(new Data(*this))){
	the(this);
}

Manager::~Manager(){
	delete &d;
}

void Manager::start(){
	d.ipcsvc.typeMapper().insert<AuthMessage>();
	
	registerService(d.ipcsvc);
	
	d.filemgrptr = new frame::file::Manager(&d.fmctrl);
	
	DynamicPointer<frame::Object>	msgptr(d.filemgrptr);
	
	registerObject(*msgptr);
	d.objsched.schedule(msgptr);
	
	msgptr = new frame::MessageSteward;
	
	d.readmsgstwuid = registerObject(*msgptr);
	
	d.objsched.schedule(msgptr);
	
	msgptr = new frame::MessageSteward;
	d.writemsgstwuid = registerObject(*msgptr);
	
	d.objsched.schedule(msgptr);
}

ObjectUidT Manager::readMessageStewardUid()const{
	return d.readmsgstwuid;
}
	
ObjectUidT Manager::writeMessageStewardUid()const{
	return d.writemsgstwuid;
}

frame::ipc::Service&	Manager::ipc()const{
	return d.ipcsvc;
}
frame::file::Manager&	Manager::fileManager()const{
	return *d.filemgrptr;
}

void Manager::scheduleListener(solid::DynamicPointer<solid::frame::aio::Object> &_objptr){
	d.scndaiosched.schedule(_objptr);
}
void Manager::scheduleTalker(solid::DynamicPointer<solid::frame::aio::Object> &_objptr){
	d.scndaiosched.schedule(_objptr);
}
void Manager::scheduleAioObject(solid::DynamicPointer<solid::frame::aio::Object> &_objptr){
	d.mainaiosched.schedule(_objptr);
}

void Manager::scheduleObject(solid::DynamicPointer<solid::frame::Object> &_objptr){
	d.objsched.schedule(_objptr);
}

//------------------------------------------------------
//		IpcServiceController
//------------------------------------------------------
void IpcServiceController::scheduleTalker(frame::aio::Object *_po){
	idbg("");
	DynamicPointer<frame::aio::Object> objptr(_po);
	Manager::the().scheduleTalker(objptr);
}

void IpcServiceController::scheduleListener(frame::aio::Object *_po){
	idbg("");
	DynamicPointer<frame::aio::Object> objptr(_po);
	Manager::the().scheduleListener(objptr);
}
void IpcServiceController::scheduleNode(frame::aio::Object *_po){
	idbg("");
	DynamicPointer<frame::aio::Object> objptr(_po);
	Manager::the().scheduleAioObject(objptr);
}

/*virtual*/ bool IpcServiceController::compressBuffer(
	frame::ipc::BufferContext &_rbc,
	const uint32 _bufsz,
	char* &_rpb,
	uint32 &_bl
){
// 	if(_bufsz < 1024){
// 		return false;
// 	}
	uint32	destcp(0);
	char 	*pdest =  allocateBuffer(_rbc, destcp);
	size_t	len = qlz_compress(_rpb, pdest, _bl, &qlz_comp_ctx);
	_rpb = pdest;
	_bl = len;
	return true;
}

/*virtual*/ bool IpcServiceController::decompressBuffer(
	frame::ipc::BufferContext &_rbc,
	char* &_rpb,
	uint32 &_bl
){
	uint32	destcp(0);
	char 	*pdest =  allocateBuffer(_rbc, destcp);
	size_t	len = qlz_decompress(_rpb, pdest, &qlz_decomp_ctx);
	_rpb = pdest;
	_bl = len;
	return true;
}

/*virtual*/ int IpcServiceController::authenticate(
	DynamicPointer<frame::Message> &_msgptr,//the received signal
	frame::ipc::MessageUid &_rmsguid,
	uint32 &_rflags,
	frame::ipc::SerializationTypeIdT &_rtid
){
	if(!_msgptr.get()){
		if(authidx){
			idbg("");
			return BAD;
		}
		//initiate authentication
		_msgptr = new AuthMessage;
		++authidx;
		idbg("authidx = "<<authidx);
		return NOK;
	}
	if(_msgptr->dynamicTypeId() != AuthMessage::staticTypeId()){
		cassert(false);
		return BAD;
	}
	AuthMessage &rmsg(static_cast<AuthMessage&>(*_msgptr));
	
	_rmsguid = rmsg.msguidpeer;
	
	rmsg.msguidpeer = rmsg.msguid;
	
	idbg("sig = "<<(void*)_msgptr.get()<<" auth("<<rmsg.authidx<<','<<rmsg.authcnt<<") authidx = "<<this->authidx);
	
	if(rmsg.authidx == 0){
		if(this->authidx == 2){
			idbg("");
			return BAD;
		}
		++this->authidx;
		rmsg.authidx = this->authidx;
	}
	
	++rmsg.authcnt;
	
	if(rmsg.authidx == 2 && rmsg.authcnt >= 3){
		idbg("");
		return BAD;
	}
	
	
	if(rmsg.authcnt == 4){
		idbg("");
		return OK;
	}
	if(rmsg.authcnt == 5){
		_msgptr.clear();
		idbg("");
		return OK;
	}
	idbg("");
	return NOK;
}


//------------------------------------------------------
//		FileManagerController
//------------------------------------------------------
/*virtual*/ void FileManagerController::init(const frame::file::Manager::InitStub &_ris){
	_ris.registerMapper(new frame::file::NameMapper(10, 0));
	_ris.registerMapper(new frame::file::TempMapper(1024ULL * 1024 * 1024, "/tmp"));
	_ris.registerMapper(new frame::file::MemoryMapper(1024ULL * 1024 * 100));
}
/*virtual*/ bool FileManagerController::release(){
	return false;
}
void FileManagerController::sendStream(
	StreamPointer<InputStream> &_sptr,
	const FileUidT &_rfuid,
	const frame::RequestUid& _rrequid
){
	RequestUidT						ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<frame::Message>	cp(new InputStreamMessage(_sptr, _rfuid, ru));
	Manager::the().notify(cp, ObjectUidT(_rrequid.objidx, _rrequid.objuid));
}

void FileManagerController::sendStream(
	StreamPointer<OutputStream> &_sptr,
	const FileUidT &_rfuid,
	const frame::RequestUid& _rrequid
){
	RequestUidT						ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<frame::Message>	cp(new OutputStreamMessage(_sptr, _rfuid, ru));
	Manager::the().notify(cp, ObjectUidT(_rrequid.objidx, _rrequid.objuid));
}
void FileManagerController::sendStream(
	StreamPointer<InputOutputStream> &_sptr,
	const FileUidT &_rfuid,
	const frame::RequestUid& _rrequid
){
	RequestUidT						ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<frame::Message>	cp(new InputOutputStreamMessage(_sptr, _rfuid, ru));
	Manager::the().notify(cp, ObjectUidT(_rrequid.objidx, _rrequid.objuid));
}
void FileManagerController::sendError(
	int _error,
	const frame::RequestUid& _rrequid
){
	RequestUidT						ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<frame::Message>	cp(new StreamErrorMessage(_error, ru));
	Manager::the().notify(cp, ObjectUidT(_rrequid.objidx, _rrequid.objuid));
}



}//namespace concept

