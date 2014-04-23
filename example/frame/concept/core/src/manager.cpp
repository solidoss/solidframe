/////// manager.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
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
#include "frame/message.hpp"
#include "frame/requestuid.hpp"

#include "frame/ipc/ipcservice.hpp"

#include <iostream>

using namespace std;
using namespace solid;

namespace concept{

typedef solid::DynamicSharedPointer<FileStoreT>	FileStoreSharedPointerT;

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


struct AuthMessage: Dynamic<AuthMessage, DynamicShared<frame::ipc::Message> >{
	AuthMessage():authidx(0), authcnt(0){}
	~AuthMessage(){}
	
	template <class S>
	void serialize(S &_s, frame::ipc::ConnectionContext const &_rctx){
		_s.push(authidx, "authidx").push(authcnt, "authcnt");
		_s.push(msguidpeer.idx, "msguidpeer.idx").push(msguidpeer.uid,"msguidpeer.uid");
	}
//data:
	int							authidx;
	int							authcnt;
	frame::ipc::MessageUid		msguidpeer;
};

struct IpcServiceController: frame::ipc::Controller{
	IpcServiceController():frame::ipc::Controller(0, 400), authidx(0){
	}
	
	/*virtual*/ void scheduleTalker(frame::aio::Object *_po);
	/*virtual*/ void scheduleListener(frame::aio::Object *_po);
	/*virtual*/ void scheduleNode(frame::aio::Object *_po);
	
	
	/*virtual*/ bool compressPacket(
		frame::ipc::PacketContext &_rbc,
		const uint32 _bufsz,
		char* &_rpb,
		uint32 &_bl
	);
	/*virtual*/ bool decompressPacket(
		frame::ipc::PacketContext &_rbc,
		char* &_rpb,
		uint32 &_bl
	);
	/*virtual*/ AsyncE authenticate(
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
		ipcsvc(_rm, new IpcServiceController){
	}
	
	AioSchedulerT				mainaiosched;
	AioSchedulerT				scndaiosched;
	SchedulerT					objsched;
	frame::ipc::Service			ipcsvc;
	FileStoreSharedPointerT		filestoreptr;
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
	//just to be sure - stop the schedulers before delete:
	//It may happen that objects on schedulers to access deleted members of data
	d.mainaiosched.stop(false);
	d.scndaiosched.stop(false);
	d.objsched.stop(false);
	
	d.mainaiosched.stop(true);
	d.scndaiosched.stop(true);
	d.objsched.stop(true);
	delete &d;
}

void Manager::start(){
	d.ipcsvc.registerMessageType<AuthMessage>();
	
	registerService(d.ipcsvc);
	
	{
		frame::file::Utf8Configuration	utf8cfg;
		frame::file::TempConfiguration	tempcfg;
		
		system("[ -d /tmp/solidframe_concept ] || mkdir -p /tmp/solidframe_concept");
		
		const char *homedir = getenv("HOME");
		
		utf8cfg.storagevec.push_back(solid::frame::file::Utf8Configuration::Storage());
		utf8cfg.storagevec.back().globalprefix = "/";
		utf8cfg.storagevec.back().localprefix = homedir;
		if(utf8cfg.storagevec.back().localprefix.size() && *utf8cfg.storagevec.back().localprefix.rbegin() != '/'){
			utf8cfg.storagevec.back().localprefix.push_back('/');
		}
		
		tempcfg.storagevec.push_back(solid::frame::file::TempConfiguration::Storage());
		tempcfg.storagevec.back().level = frame::file::MemoryLevelFlag;
		tempcfg.storagevec.back().capacity = 1024 * 1024 * 10;//10MB
		tempcfg.storagevec.back().minsize = 0;
		tempcfg.storagevec.back().maxsize = 1024 * 10;
		
		tempcfg.storagevec.push_back(solid::frame::file::TempConfiguration::Storage());
		tempcfg.storagevec.back().path = "/tmp/solidframe_concept/";
		tempcfg.storagevec.back().level = frame::file::VeryFastLevelFlag;
		tempcfg.storagevec.back().capacity = 1024 * 1024 * 100;//100MB
		tempcfg.storagevec.back().minsize = 0;
		tempcfg.storagevec.back().maxsize = 1024 * 1024 * 10;
		tempcfg.storagevec.back().removemode = frame::file::RemoveNeverE;
	
		d.filestoreptr = new FileStoreT(*this, utf8cfg, tempcfg);
	}
	
	registerObject(*d.filestoreptr);
	d.objsched.schedule(d.filestoreptr);
}


frame::ipc::Service&	Manager::ipc()const{
	return d.ipcsvc;
}
FileStoreT&	Manager::fileStore()const{
	return *d.filestoreptr;
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

/*virtual*/ bool IpcServiceController::compressPacket(
	frame::ipc::PacketContext &_rbc,
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

/*virtual*/ bool IpcServiceController::decompressPacket(
	frame::ipc::PacketContext &_rbc,
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

/*virtual*/ AsyncE IpcServiceController::authenticate(
	DynamicPointer<frame::Message> &_msgptr,//the received signal
	frame::ipc::MessageUid &_rmsguid,
	uint32 &_rflags,
	frame::ipc::SerializationTypeIdT &_rtid
){
	if(!_msgptr.get()){
		if(authidx){
			idbg("");
			return AsyncError;
		}
		//initiate authentication
		_msgptr = new AuthMessage;
		++authidx;
		idbg("authidx = "<<authidx);
		return AsyncWait;
	}
	if(_msgptr->dynamicTypeId() != AuthMessage::staticTypeId()){
		cassert(false);
		return AsyncError;
	}
	AuthMessage &rmsg(static_cast<AuthMessage&>(*_msgptr));
	
	idbg("sig = "<<(void*)_msgptr.get()<<" auth("<<rmsg.authidx<<','<<rmsg.authcnt<<") authidx = "<<this->authidx);
	
	if(rmsg.authidx == 0){
		if(this->authidx == 2){
			idbg("");
			return solid::AsyncError;
		}
		++this->authidx;
		rmsg.authidx = this->authidx;
	}
	
	++rmsg.authcnt;
	
	if(rmsg.authidx == 2 && rmsg.authcnt >= 3){
		idbg("");
		return solid::AsyncError;
	}
	
	
	if(rmsg.authcnt == 4){
		idbg("");
		return solid::AsyncSuccess;
	}
	if(rmsg.authcnt == 5){
		_msgptr.clear();
		idbg("");
		return solid::AsyncSuccess;
	}
	idbg("");
	return solid::AsyncWait;
}


}//namespace concept

