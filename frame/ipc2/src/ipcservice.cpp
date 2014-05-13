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
//Some implementation flags
enum{
	FirstImplemetationFlag = (LastFlag << 1)
};

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
	size_t			use;//==-1 connection dormant, ,>0 connection busy and will poll from time to time
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
	state(StoppedState), plnconcnt(0), scrconcnt(0){}
	
	bool isStarted()const{
		return state == StartedState;
	}
	
	
	SocketAddressInet	addr;
	uint32				crtmsgtag;
	uint16				uid;
	uint8				state;
	
	uint8				plnconcnt;
	uint8				scrconcnt;
	
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
	SizeStackT					ssnidxcache;
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
	return true;
}
//---------------------------------------------------------------------
bool Service::sendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SessionUid &_rssnid,//the id of the process connector
	uint32	_flags
){
	Locker<Mutex>	lock(mutex());
	if(_rssnid.ssnidx < d.ssndq.size()){
		Locker<Mutex>	lock2(d.mtxstore.at(_rssnid.ssnidx));
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
	Locker<Mutex>	lock(mutex());
	if(_rssnid.ssnidx < d.ssndq.size()){
		Locker<Mutex>	lock2(d.mtxstore.at(_rssnid.ssnidx));
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
	const SocketAddressInet 			sa(_rsa_dest);
	Locker<Mutex>						lock(mutex());
	SocketAddressMapT::const_iterator	it = d.ssnmap.find(&sa);
	size_t								ssnidx = -1;
	
	if(it != d.ssnmap.end()){
		ssnidx = it->second;
	}else{
		if(d.ssnidxcache.size()){
			ssnidx = d.ssnidxcache.top();
			d.ssnidxcache.pop();
			d.ssndq[ssnidx].addr = _rsa_dest;
		}else{
			ssnidx = d.ssndq.size();
			d.ssndq.push_back(SessionStub(_rsa_dest));
		}
		d.ssnmap[&d.ssndq[ssnidx].addr] = ssnidx;
		d.ssndq[ssnidx].state = SessionStub::StartedState;
	}
	
	if(_psesid){
		_psesid->ssnidx = ssnidx;
		_psesid->ssnuid = d.ssndq[ssnidx].uid;
	}
	
	return doUnsafeSendMessage(ssnidx, _rmsgptr, _rtid, _flags);
}
//---------------------------------------------------------------------
size_t find_available_connection(ConnectionVectorT const &_rconvec, bool &_shouldcreatenew, const size_t _usethreshold){
	size_t cnt = 0;
	size_t rv = -1;
	switch(_rconvec.size()){
		case 0:
			_shouldcreatenew = true;
			break;
		case 1:
			if(_rconvec[0].use == static_cast<size_t>(-1)){
				return 0;
			}else if(_rconvec[0].use < _usethreshold){
			}else{
				++cnt;
			}
			break;
		case 2:
			if(_rconvec[0].use == static_cast<size_t>(-1)){
				return 0;
			}else if(_rconvec[0].use < _usethreshold){
				++cnt;
			}
			if(_rconvec[1].use == static_cast<size_t>(-1)){
				return 1;
			}else if(_rconvec[1].use < _usethreshold){
				++cnt;
			}
			break;
		case 3:
			if(_rconvec[0].use == static_cast<size_t>(-1)){
				return 0;
			}else if(_rconvec[0].use < _usethreshold){
				++cnt;
			}
			if(_rconvec[1].use == static_cast<size_t>(-1)){
				return 1;
			}else if(_rconvec[1].use < _usethreshold){
				++cnt;
			}
			if(_rconvec[2].use == static_cast<size_t>(-1)){
				return 2;
			}else if(_rconvec[2].use < _usethreshold){
				++cnt;
			}
			break;
		case 4:
			if(_rconvec[0].use == static_cast<size_t>(-1)){
				return 0;
			}else if(_rconvec[0].use < _usethreshold){
				++cnt;
			}
			if(_rconvec[1].use == static_cast<size_t>(-1)){
				return 1;
			}else if(_rconvec[1].use < _usethreshold){
				++cnt;
			}
			if(_rconvec[2].use == static_cast<size_t>(-1)){
				return 2;
			}else if(_rconvec[2].use < _usethreshold){
				++cnt;
			}
			if(_rconvec[3].use == static_cast<size_t>(-1)){
				return 3;
			}else if(_rconvec[3].use < _usethreshold){
				++cnt;
			}
			break;
		default:
			cassert(false);
			break;
	}
	_shouldcreatenew = (cnt == 0);
	return rv;
}

size_t find_available_connection_stub(ConnectionVectorT const &_rconvec){
	if(!_rconvec.empty()){
		if(is_invalid_uid(_rconvec[0].objid)){
			return 0;
		}
		if(_rconvec.size() == 1) goto Done;
		if(is_invalid_uid(_rconvec[1].objid)){
			return 1;
		}
		if(_rconvec.size() == 2) goto Done;
		if(is_invalid_uid(_rconvec[2].objid)){
			return 2;
		}
		if(_rconvec.size() == 3) goto Done;
		if(is_invalid_uid(_rconvec[3].objid)){
			return 3;
		}
		if(_rconvec.size() == 4) goto Done;
		if(is_invalid_uid(_rconvec[4].objid)){
			return 4;
		}
		if(_rconvec.size() == 5) goto Done;
		if(is_invalid_uid(_rconvec[5].objid)){
			return 5;
		}
		if(_rconvec.size() == 6) goto Done;
		if(is_invalid_uid(_rconvec[6].objid)){
			return 6;
		}
		if(_rconvec.size() == 7) goto Done;
		if(is_invalid_uid(_rconvec[7].objid)){
			return 7;
		}
		return -1;
	}
Done:
	return _rconvec.size();
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
		size_t	conidx = find_available_connection(rssn.scrconvec, shouldcreatenew, 3);
		if(conidx < rssn.scrconvec.size()){
			doNotifyConnection(rssn.scrconvec[conidx].objid);
		}else if(shouldcreatenew && rssn.scrconcnt < d.config.maxsecureconcnt){
			
			size_t								conidx = find_available_connection_stub(rssn.scrconvec);
			if(conidx == rssn.scrconvec.size()){
				rssn.scrconvec.push_back(ConnectionStub());
			}
			cassert(conidx != static_cast<size_t>(-1));
			const SessionUid 					ssnid(_idx, rssn.uid);
			const ConnectionUid					conid(conidx, rssn.scrconvec[conidx].uid);
			DynamicPointer<frame::aio::Object>	objptr(new Connection(*this, ssnid, conid, d.sslctxptr.get()));
			
			
			rssn.scrconvec[conidx].objid = this->unsafeRegisterObject(*objptr);
			controller().scheduleConnection(objptr);
		}
	}else{
		rssn.plnmsgq.push(MessageStub(_rmsgptr, _rtid, _flags));
		//now we need to notify a connection
		bool	shouldcreatenew = rssn.plnconvec.empty();
		size_t	conidx = find_available_connection(rssn.plnconvec, shouldcreatenew, 3);
		if(conidx < rssn.plnconvec.size()){
			doNotifyConnection(rssn.plnconvec[conidx].objid);
		}else if(shouldcreatenew && rssn.plnconcnt < d.config.maxplainconcnt){
			size_t								conidx = find_available_connection_stub(rssn.plnconvec);
			if(conidx == rssn.plnconvec.size()){
				rssn.plnconvec.push_back(ConnectionStub());
			}
			cassert(conidx != static_cast<size_t>(-1));
			const SessionUid 					ssnid(_idx, rssn.uid);
			const ConnectionUid					conid(conidx, rssn.plnconvec[conidx].uid);
			DynamicPointer<frame::aio::Object>	objptr(new Connection(*this, ssnid, conid));
			
			
			rssn.scrconvec[conidx].objid = this->unsafeRegisterObject(*objptr);
			controller().scheduleConnection(objptr);
		}
	}
	return true;
}
//---------------------------------------------------------------------
void Service::doNotifyConnection(ObjectUidT const &_objid){
	manager().notify(frame::S_RAISE, _objid);
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

/*virtual*/ void BasicController::scheduleListener(DynamicPointer<frame::aio::Object> &_objptr){
	rsched_l.schedule(_objptr);
}

/*virtual*/ void BasicController::scheduleConnection(DynamicPointer<frame::aio::Object> &_objptr){
	rsched_c.schedule(_objptr);
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


