// frame/ipc/src/ipcservice.cpp
//
// Copyright (c) 2007, 2008, 2010, 2014 Valentin Palade (vipalade @ gmail . com) 
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
#include "frame/ipc2/ipcsessionuid.hpp"
#include "frame/ipc2/ipcmessage.hpp"

#include "system/mutualstore.hpp"
#include "system/atomic.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "ipclistener.hpp"
#include "ipcconnection.hpp"
#include "ipcutility.hpp"

#include <vector>
#include <deque>

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

//*******	Service::Data	*******************************************

struct MessageStub{
	MessageStub():tid(SERIALIZATION_INVALIDID),flags(0){}
	MessageStub(
		const DynamicPointer<Message>	&_rmsgptr,
		const SerializationTypeIdT &_rtid,
		uint32 _flags
	):msgptr(_rmsgptr), tid(_rtid), flags(_flags){}

	DynamicPointer<Message>	msgptr;
	SerializationTypeIdT	tid;
	uint32					flags;
	uint32					tag;
};

struct SendMessageStub{
	SendMessageStub():tid(SERIALIZATION_INVALIDID),flags(0), uid(0){}
	SendMessageStub(
		const DynamicPointer<Message>	&_rmsgptr,
		const SerializationTypeIdT &_rtid,
		uint32 _flags
	):msgptr(_rmsgptr), tid(_rtid), flags(_flags), uid(0){}

	DynamicPointer<Message>	msgptr;
	SerializationTypeIdT	tid;
	uint32					flags;
	uint16					uid;
};

struct ConnectionStub{
	size_t			use;//==-1 connection dormant, == 0 connection will poll for new message soon, >0 connection busy and will poll from time to time
	ObjectUidT		objid;
	uint16			uid;
};

typedef Queue<MessageStub>							MessageQueueT;
typedef std::vector<ConnectionStub>					ConnectionVectorT;
typedef std::vector<SendMessageStub>				SendMessageVectorT;
typedef Stack<size_t>								SizeStackT;
typedef std::auto_ptr<frame::aio::openssl::Context> SslContextPtrT;

struct SessionStub{
	enum {
		StartedState,
		StoppingState,
		StoppedState,
	};

	SessionStub(){}
	SessionStub(
		const SocketAddressStub &_rsa_dest
	):addr(_rsa_dest), crtmsgtag(0), uid(0),
	state(StoppedState){}
	
	bool isStarted()const{
		return state == StartedState;
	}
	
	
	SocketAddressInet	addr;
	uint32				crtmsgtag;
	uint16				uid;
	uint8				state;
	
	MessageQueueT		plnmsgq;
	MessageQueueT		scrmsgq;
	ConnectionVectorT	plnconvec;
	ConnectionVectorT	scrconvec;
	SendMessageVectorT	sndmsgvec;
	SizeStackT			sndmsgidxcache;
};

typedef ATOMIC_NS::atomic<size_t>			AtomicSizeT;
typedef MutualStore<Mutex>					MutexMutualStoreT;
typedef std::deque<SessionStub>				SessionDequeT;
typedef CPP11_NS::unordered_map<
	const SocketAddressInet*,
	size_t,
	SocketAddressHash,
	SocketAddressEqual	
>											SocketAddressMapT;

struct Service::Data{
	Data(
		const DynamicPointer<Controller> &_rctrlptr
	);
	
	~Data();
	
	DynamicPointer<Controller>	ctrlptr;
	Configuration				config;
	int							lsnport;
	AtomicSizeT					ssnmaxcnt;
	SslContextPtrT				sslctxptr;
	
	MutexMutualStoreT			mtxstore;
	SocketAddressMapT			ssnmap;
	SessionDequeT				ssndq;
	SizeStackT					cachessnidxstk;
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
	
	d.sslctxptr.reset(frame::aio::openssl::Context::create());
#if 0
	if(d.sslctxptr){
		const char *pcertpath(OSSL_SOURCE_PATH"ssl_/certs/A-server.pem");
		
		pctx->loadCertificateFile(pcertpath);
		pctx->loadPrivateKeyFile(pcertpath);
	}
#endif
}
//---------------------------------------------------------------------
Service::~Service(){
	//the held object must be stopped before deleting data
	stop(true);
	
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
	const SessionUid &_rssnid,//the id of the process connector
	uint32	_flags
){
	if(_rssnid.ssnidx < d.ssnmaxcnt){
		Locker<Mutex>	lock(d.mtxstore.at(_rssnid.ssnidx));
		SessionStub		&rssn(d.ssndq[_rssnid.ssnidx]);
		if(rssn.uid == _rssnid.ssnuid){
			return doUnsafeSendMessage(_rssnid.ssnidx, _rmsgptr, SERIALIZATION_INVALIDID, _flags);
		}
	}
	return false;
}
//---------------------------------------------------------------------
bool Service::sendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SerializationTypeIdT &_rtid,
	const SessionUid &_rssnid,//the id of the process connector
	uint32	_flags
){
	if(_rssnid.ssnidx < d.ssnmaxcnt){
		Locker<Mutex>	lock(d.mtxstore.at(_rssnid.ssnidx));
		SessionStub		&rssn(d.ssndq[_rssnid.ssnidx]);
		if(rssn.uid == _rssnid.ssnuid){
			return doUnsafeSendMessage(_rssnid.ssnidx, _rmsgptr, _rtid, _flags);
		}
	}
	return false;
}
//---------------------------------------------------------------------
int Service::listenPort()const{
	return d.lsnport;
}
//---------------------------------------------------------------------
bool Service::doSendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SerializationTypeIdT &_rtid,
	SessionUid *_psesid,
	const SocketAddressStub &_rsa_dest,
	uint32	_flags
){
	return false;
}
//---------------------------------------------------------------------
size_t find_available_connection(ConnectionVectorT const &_rconvec, bool &_shouldcreatenew){
	return -1;
}
bool Service::doUnsafeSendMessage(
	const size_t _idx,
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SerializationTypeIdT &_rtid,
	uint32	_flags
){
	SessionStub		&rssn(d.ssndq[_idx]);
	
	if(!rssn.isStarted()){
		return false;
	}
	
	bool			sendsecure = d.config.mustSendSecure(_flags);
	
	if(sendsecure){
		rssn.scrmsgq.push(MessageStub(_rmsgptr, _rtid, _flags));
		//now we need to notify a connection
		bool	shouldcreatenew = rssn.scrconvec.empty();
		size_t	conidx = find_available_connection(rssn.scrconvec, shouldcreatenew);
		if(conidx < rssn.scrconvec.size()){
			doNotifyConnection(rssn.scrconvec[conidx].objid);
		}else if(shouldcreatenew){
			
		}
	}else{
		rssn.plnmsgq.push(MessageStub(_rmsgptr, _rtid, _flags));
		//now we need to notify a connection
		bool	shouldcreatenew = rssn.plnconvec.empty();
		size_t	conidx = find_available_connection(rssn.plnconvec, shouldcreatenew);
		if(conidx < rssn.plnconvec.size()){
			doNotifyConnection(rssn.plnconvec[conidx].objid);
		}else if(shouldcreatenew){
			
		}
	}
	return true;
}
//---------------------------------------------------------------------
void Service::doNotifyConnection(ObjectUidT const &_objid){
	
}
//---------------------------------------------------------------------
void Service::insertConnection(
	SocketDevice &_rsd
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


