// frame/ipc/src/ipcservice.cpp
//
// Copyright (c) 2007, 2008, 2010 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <vector>
#include <cstring>
#include <utility>
#include <algorithm>

#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/socketdevice.hpp"
#include "system/specific.hpp"
#include "system/exception.hpp"

#include "utility/queue.hpp"
#include "utility/binaryseeker.hpp"

#include "frame/common.hpp"
#include "frame/manager.hpp"

#include "frame/aio/openssl/opensslsocket.hpp"

#include "frame/ipc2/ipcservice.hpp"
#include "frame/ipc2/ipcconnectionuid.hpp"
#include "frame/ipc2/ipcmessage.hpp"

#include "ipclistener.hpp"
#include "ipcconnection.hpp"

#ifdef HAS_CPP11
#define CPP11_NS std
#include <unordered_map>
#else
#include "boost/unordered_map.hpp"
#define CPP11_NS boost
#endif

namespace solid{
namespace frame{
namespace ipc{

//*******	Service::Data	******************************************************************

struct Service::Data{
	Data(
		const DynamicPointer<Controller> &_rctrlptr
	);
	
	~Data();
	
	DynamicPointer<Controller>	ctrlptr;
	Configuration				config;
	int							lsnport;
};

//=======	ServiceData		===========================================

Service::Data::Data(
	const DynamicPointer<Controller> &_rctrlptr
):
	ctrlptr(_rctrlptr),
	lsnport(-1)
{
}

Service::Data::~Data(){
}

//=======	Service		===============================================

/*static*/ const char* Service::errorText(int _err){
	switch(_err){
		case NoError:
			return "No Error";
		case GenericError:
			return "Generic Error";
		case NoGatewayError:
			return "No Gateway Error";
		case UnsupportedSocketFamilyError:
			return "Unsupported socket family Error";
		case NoConnectionError:
			return "No such connection error";
		default:
			return "Unknown Error";
	}
}

Service::Service(
	Manager &_rm,
	const DynamicPointer<Controller> &_rctrlptr
):BaseT(_rm), d(*(new Data(_rctrlptr))){
}
//---------------------------------------------------------------------
Service::~Service(){
	delete &d;
}
//---------------------------------------------------------------------

bool Service::reconfigure(const Configuration &_rcfg){
	Locker<Mutex>	lock(mutex());
	
	if(_rcfg == d.config){
		return true;
	}
	
	unsafeReset(lock);
	
	d.config = _rcfg;
	return false;
}
//---------------------------------------------------------------------
bool Service::sendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const ConnectionUid &_rconid,//the id of the process connector
	uint32	_flags
){
	return true;
}
//---------------------------------------------------------------------
bool Service::sendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SerializationTypeIdT &_rtid,
	const ConnectionUid &_rconid,//the id of the process connector
	uint32	_flags
){
	return true;
}
//---------------------------------------------------------------------
int Service::listenPort()const{
	return d.lsnport;
}
//---------------------------------------------------------------------
bool Service::doSendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SerializationTypeIdT &_rtid,
	ConnectionUid *_pconid,
	const SocketAddressStub &_rsa_dest,
	uint32	_flags
){
	return false;
}
//---------------------------------------------------------------------
void Service::insertConnection(
	SocketDevice &_rsd,
	aio::openssl::Context *_pctx
){
}
//---------------------------------------------------------------------
Controller& Service::controller(){
	return *d.ctrlptr;
}
//---------------------------------------------------------------------
Configuration const& Service::configuration()const{
	return d.config;
}
//---------------------------------------------------------------------
const Controller& Service::controller()const{
	return *d.ctrlptr;
}
//------------------------------------------------------------------
//		Configuration
//------------------------------------------------------------------

//------------------------------------------------------------------
//		Controller
//------------------------------------------------------------------
/*virtual*/ Controller::~Controller(){
	
}
/*virtual*/ bool Controller::compressPacket(
	PacketContext &_rpc,
	const uint32 _bufsz,
	char* &_rpb,
	uint32 &_bl
){
	return false;
}

/*virtual*/ bool Controller::decompressPacket(
	PacketContext &_rpc,
	char* &_rpb,
	uint32 &_bl
){
	return true;
}

/*virtual*/ bool Controller::onMessageReceive(
	DynamicPointer<Message> &_rmsgptr,
	ConnectionContext const &_rctx
){
	_rmsgptr->ipcOnReceive(_rctx, _rmsgptr);
	return true;
}

/*virtual*/ Message::UInt32PairT Controller::onMessagePrepare(
	Message &_rmsg,
	ConnectionContext const &_rctx
){
	return _rmsg.ipcOnPrepare(_rctx);
}

/*virtual*/ void Controller::onMessageComplete(
	Message &_rmsg,
	ConnectionContext const &_rctx,
	int _error
){
	_rmsg.ipcOnComplete(_rctx, _error);
}

AsyncE Controller::authenticate(
	DynamicPointer<Message> &_rmsgptr,
	ipc::MessageUid &_rmsguid,
	uint32 &_rflags,
	SerializationTypeIdT &_rtid
){
	//use: ConnectionContext::the().connectionuid!!
	return AsyncError;//by default no authentication
}


/*virtual*/ void Controller::onDisconnect(const SocketAddressInet &_raddr){
	vdbgx(Debug::ipc, ':'<<_raddr);
}

//------------------------------------------------------------------
//		BasicController
//------------------------------------------------------------------

BasicController::BasicController(
	AioSchedulerT &_rsched,
	const uint32 _flags,
	const uint32 _resdatasz
):Controller(_flags, _resdatasz), rsched_l(_rsched), rsched_c(_rsched){
	vdbgx(Debug::ipc, "");
}

BasicController::BasicController(
	AioSchedulerT &_rsched_l,
	AioSchedulerT &_rsched_c,
	const uint32 _flags,
	const uint32 _resdatasz
):Controller(_flags, _resdatasz), rsched_l(_rsched_l), rsched_c(_rsched_c){
	vdbgx(Debug::ipc, "");
}

BasicController::~BasicController(){
	vdbgx(Debug::ipc, "");
}

/*virtual*/ void BasicController::scheduleListener(frame::aio::Object *_plis){
	DynamicPointer<frame::aio::Object> op(_plis);
	rsched_l.schedule(op);
}

/*virtual*/ void BasicController::scheduleConnection(frame::aio::Object *_pcon){
	DynamicPointer<frame::aio::Object> op(_pcon);
	rsched_c.schedule(op);
}

//------------------------------------------------------------------
//		Message
//------------------------------------------------------------------

/*virtual*/ Message::~Message(){
	
}
/*virtual*/ void Message::ipcOnReceive(ConnectionContext const &_ripcctx, MessagePointerT &_rmsgptr){
}
/*virtual*/ Message::UInt32PairT Message::ipcOnPrepare(ConnectionContext const &_ripcctx){
	return UInt32PairT(0,0);
}
/*virtual*/ void Message::ipcOnComplete(ConnectionContext const &_ripcctx, int _error){
	
}

struct HandleMessage{
	void operator()(Service::SerializerT &_rs, Message &_rt, const ConnectionContext &_rctx){
		vdbgx(Debug::ipc, "reset message state");
		if(_rt.ipcState() >= 2){
			_rt.ipcResetState();
		}
		
	}
	void operator()(Service::DeserializerT &_rs, Message &_rt, const ConnectionContext &_rctx){
		vdbgx(Debug::ipc, "increment message state");
		++_rt.ipcState();
	}
};

void Service::Handle::beforeSerialization(Service::SerializerT &_rs, Message *_pt, const ConnectionContext &_rctx){
	MessageUid	&rmsguid = _pt->ipcRequestMessageUid();
	vdbgx(Debug::ipc, ""<<rmsguid.idx<<','<<rmsguid.uid);
	_rs.pushHandle<HandleMessage>(_pt, "handle_state");
	_rs.push(_pt->ipcState(), "state").push(rmsguid.idx, "msg_idx").push(rmsguid.uid, "msg_uid");
}
void Service::Handle::beforeSerialization(Service::DeserializerT &_rs, Message *_pt, const ConnectionContext &_rctx){
	vdbgx(Debug::ipc, "");
	MessageUid	&rmsguid = _pt->ipcRequestMessageUid();
	_rs.pushHandle<HandleMessage>(_pt, "handle_state");
	_rs.push(_pt->ipcState(), "state").push(rmsguid.idx, "msg_idx").push(rmsguid.uid, "msg_uid");
}

//------------------------------------------------------------------
//		ConnectionContext
//------------------------------------------------------------------

ConnectionContext::MessagePointerT& ConnectionContext::requestMessage(const Message &_rmsg)const{
	static MessagePointerT msgptr;
// 	if(psession){
// 		ConnectionContext::MessagePointerT *pmsgptr = psession->requestMessageSafe(_rmsg.ipcRequestMessageUid());
// 		if(pmsgptr){
// 			return *pmsgptr;
// 		}else{
// 			msgptr.clear();
// 			return msgptr;
// 		}
// 	}else{
// 		msgptr.clear();
// 		return msgptr;
// 	}
	return msgptr;
}

}//namespace ipc
}//namespace frame
}//namespace solid


