// frame/ipc/src/ipcservice.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
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

#include "frame/aio/aioresolver.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/ipc/ipccontext.hpp"
#include "frame/ipc/ipcmessage.hpp"
#include "frame/ipc/ipcconfiguration.hpp"

#include "system/mutualstore.hpp"
#include "system/atomic.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "ipcutility.hpp"
#include "ipcconnection.hpp"
#include "ipclistener.hpp"


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
//=============================================================================
typedef CPP11_NS::unordered_map<const char*, size_t>	NameMapT;
typedef Queue<ObjectUidT>								ObjectUidQueueT;

struct MessageStub{
	MessageStub(
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ulong _flags
	): msgptr(std::move(_rmsgptr)), msg_type_idx(_msg_type_idx), flags(_flags){}
	
	MessagePointerT msgptr;
	const size_t	msg_type_idx;
	ulong			flags;
};

typedef Queue<MessageStub>								MessageQueueT;

struct ConnectionPoolStub{
	ConnectionPoolStub():uid(0), pending_connection_count(0), active_connection_count(0){}
	
	void clear(){
		name.clear();
		synch_conn_uid = ObjectUidT();
		++uid;
		pending_connection_count = 0;
		active_connection_count = 0;
		cassert(msgq.empty());
		while(conn_waitingq.size()){
			conn_waitingq.pop();
		}
	}
	
	uint32			uid;
	size_t			pending_connection_count;
	size_t			active_connection_count;
	std::string		name;
	ObjectUidT 		synch_conn_uid;
	MessageQueueT	msgq;
	ObjectUidQueueT	conn_waitingq;
};

typedef std::deque<ConnectionPoolStub>					ConnectionPoolDequeT;
typedef Stack<size_t>									SizeStackT;


struct Service::Data{
	Data():pmtxarr(nullptr), mtxsarrcp(0){}
	
	~Data(){
		delete []pmtxarr;
	}
	
	
	Mutex & connectionPoolMutex(size_t _idx)const{
		return pmtxarr[_idx % mtxsarrcp];
	}
	
	Mutex					mtx;
	Mutex					*pmtxarr;
	size_t					mtxsarrcp;
	NameMapT				namemap;
	ConnectionPoolDequeT	conpooldq;
	SizeStackT				conpoolcachestk;
	Configuration			config;
};
//=============================================================================

Service::Service(
	frame::Manager &_rm
):BaseT(_rm), d(*(new Data)){}
	
//! Destructor
Service::~Service(){
	delete &d;
}
//-----------------------------------------------------------------------------
Configuration const & Service::configuration()const{
	return d.config;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::reconfigure(Configuration const& _rcfg){
	this->stop(true);
	this->start();
	
	ErrorConditionT err;
	
	ServiceProxy	sp(*this);
	
	_rcfg.message_register_fnc(sp);
	
	d.config = _rcfg;
	
	if(d.config.listen_address_str.size()){
		std::string		tmp;
		const char 		*hst_name;
		const char		*svc_name;
		
		size_t off = d.config.listen_address_str.rfind(':');
		if(off != std::string::npos){
			tmp = d.config.listen_address_str.substr(0, off);
			hst_name = tmp.c_str();
			svc_name = d.config.listen_address_str.c_str() + off + 1;
			if(!svc_name[0]){
				svc_name = d.config.default_listen_port_str.c_str();
			}
		}else{
			hst_name = d.config.listen_address_str.c_str();
			svc_name = d.config.default_listen_port_str.c_str();
		}
		
		ResolveData		rd = synchronous_resolve(hst_name, svc_name, 0, -1, SocketInfo::Stream);
		SocketDevice	sd;
			
		sd.create(rd.begin());
		sd.prepareAccept(rd.begin(), 2000);
			
		if(sd.ok()){
			DynamicPointer<aio::Object>		objptr(new Listener(sd));
			
			ObjectUidT						conuid = d.config.scheduler().startObject(objptr, *this, EventCategory::createStart(), err);
			
			if(err){
				return err;
			}
		}else{
			err.assign(-1, err.category());
			return err;
		}
	}
	
	if(d.config.session_mutex_count > d.mtxsarrcp){
		delete []d.pmtxarr;
		d.pmtxarr = new Mutex[d.config.session_mutex_count];
		d.mtxsarrcp = d.config.session_mutex_count;
	}
	
	return ErrorConditionT();
}
//-----------------------------------------------------------------------------
struct PushMessageVisitorF{
	PushMessageVisitorF(
		MessagePointerT	&_rmsgptr,
		const size_t	_msg_type_idx,
		ulong			_flags,
		Event const &	_revt
	):rmsgptr(_rmsgptr), flags(_flags), raise_event(_revt), msg_type_idx(_msg_type_idx){}
	
	
	bool operator()(ObjectBase &_robj, ReactorBase &_rreact){
		Connection *pcon = Connection::cast(&_robj);
		
		if(pcon){
			if(pcon->pushMessage(rmsgptr, msg_type_idx, flags)){
				_rreact.raise(_robj.runId(), raise_event);
			}
			return true;
		}else{
			return false;
		}
	}
	
	MessagePointerT		&rmsgptr;
	ulong				flags;
	Event				raise_event;
	const size_t		msg_type_idx;
};



struct OnRelsolveF{
	Manager		&rm;
	ObjectUidT	objuid;
	Event 		event;
	
	OnRelsolveF(
		Manager &_rm,
		const ObjectUidT &_robjuid,
		const Event &_revent
	):rm(_rm), objuid(_robjuid), event(_revent){}
	
	void operator()(AddressVectorT &_raddrvec){
		idbgx(Debug::ipc, "OnRelsolveF(addrvec of size "<<_raddrvec.size()<<")");
		event.msgptr = new ResolveMessage(_raddrvec);
		rm.notify(objuid, event);
	}
};

ErrorConditionT Service::doSendMessage(
	const char *_recipient_name,
	const ConnectionUid	&_rconuid_in,
	MessagePointerT &_rmsgptr,
	ConnectionPoolUid *_pconpoolid_out,
	ulong _flags
){
	solid::ErrorConditionT		err;
	Locker<Mutex>				lock(d.mtx);
	size_t						idx;
	uint32						uid;
	bool						check_uid = false;
	const size_t				msg_type_idx = tm.index(_rmsgptr.get());
	
	if(msg_type_idx == 0){
		edbgx(Debug::ipc, "type not registered");
		err.assign(-1, err.category());//TODO:type not registered
		return err;
	}
	
	if(_recipient_name){
		NameMapT::const_iterator	it = d.namemap.find(_recipient_name);
		
		if(it != d.namemap.end()){
			idx = it->second;
		}else{
			
			if(d.config.isServerOnly()){
				edbgx(Debug::ipc, "request for name resolve for a server only configuration");
				err.assign(-1, err.category());//TODO: server only
				return err;
			}
			
			if(d.conpoolcachestk.size()){
				idx = d.conpoolcachestk.top();
				d.conpoolcachestk.pop();
			}else{
				idx = d.conpooldq.size();
				d.conpooldq.push_back(ConnectionPoolStub());
			}
			Locker<Mutex>					lock2(d.connectionPoolMutex(idx));
			ConnectionPoolStub 				&rconpool(d.conpooldq[idx]);
			
			rconpool.name = _recipient_name;
			
			DynamicPointer<aio::Object>		objptr(new Connection(ConnectionPoolUid(idx, rconpool.uid)));
			
			ObjectUidT						conuid = d.config.scheduler().startObject(objptr, *this, EventCategory::createStart(), err);
			
			if(err){
				edbgx(Debug::ipc, "starting Session object: "<<err.message());
				rconpool.clear();
				d.conpoolcachestk.push(idx);
				return err;
			}
			
			d.namemap[rconpool.name.c_str()] = idx;
			
			idbgx(Debug::ipc, "Success starting Connection Pool object: "<<conuid.index<<','<<conuid.unique);
			//resolve the name
			ResolveCompleteFunctionT		cbk(OnRelsolveF(manager(), conuid, EventCategory::createRaise()));
			
			d.config.name_resolve_fnc(rconpool.name, cbk);
			
			rconpool.msgq.push(MessageStub(_rmsgptr, msg_type_idx, _flags));
			++rconpool.pending_connection_count;
			
			return err;
		}
	}else if(_rconuid_in.poolid.index < d.conpooldq.size()/* && _rconuid_in.ssnuid == d.conpooldq[_rconuid_in.ssnidx].uid*/){
		//we cannot check the uid right now because we need a lock on the session's mutex
		check_uid = true;
		idx = _rconuid_in.poolid.index;
		uid = _rconuid_in.poolid.unique;
	}else if(_rconuid_in.isInvalidPool() && !_rconuid_in.isInvalidConnection()/* && d.config.isServerOnly()*/){
		return doSendMessage(_rconuid_in.connectionid, _rmsgptr, msg_type_idx, _rconuid_in.poolid, _pconpoolid_out, _flags);
	}else{
		edbgx(Debug::ipc, "session does not exist");
		err.assign(-1, err.category());//TODO: session does not exist
		return err;
	}
	
	Locker<Mutex>			lock2(d.connectionPoolMutex(idx));
	ConnectionPoolStub 		&rconpool(d.conpooldq[idx]);
	
	if(check_uid && rconpool.uid != uid){
		//failed uid check
		edbgx(Debug::ipc, "session does not exist");
		err.assign(-1, err.category());//TODO: session does not exist
		return err;
	}
	
	if(rconpool.conn_waitingq.size()){//there are connections waiting for something to send
		ObjectUidT objuid = rconpool.conn_waitingq.front();
		rconpool.conn_waitingq.pop();
		return doSendMessage(objuid, _rmsgptr, msg_type_idx, ConnectionPoolUid(idx, rconpool.uid), _pconpoolid_out, _flags);
	}
	
	//All connections are busy
	//Check if we should create a new connection
	
	if((rconpool.active_connection_count + rconpool.pending_connection_count + 1) < d.config.max_per_pool_connection_count){
		DynamicPointer<aio::Object>		objptr(new Connection(ConnectionPoolUid(idx, rconpool.uid)));
			
		ObjectUidT						conuid = d.config.scheduler().startObject(objptr, *this, EventCategory::createStart(), err);
		
		if(!err){
			ResolveCompleteFunctionT		cbk(OnRelsolveF(manager(), conuid, EventCategory::createRaise()));
			
			d.config.name_resolve_fnc(rconpool.name, cbk);
			++rconpool.pending_connection_count;
		}else{
			cassert(rconpool.pending_connection_count + rconpool.active_connection_count);//there must be at least one connection to handle the message
			//NOTE:
			//The last connection before dying MUST:
			//	1. Lock service.d.mtx
			//	2. Lock SessionStub.mtx
			//	3. Fetch all remaining messages in the SessionStub.msgq
			//	4. Destroy the SessionStub and unregister it from namemap
			//	5. call complete function for every fetched message
			//	6. Die
		}
	}
	
	rconpool.msgq.push(MessageStub(_rmsgptr, msg_type_idx, _flags));
	
	return err;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doSendMessage(
	ObjectUidT const &_robjuid,
	MessagePointerT &_rmsgptr,
	const size_t _msg_type_idx,
	ConnectionPoolUid const &_rconpooluid,
	ConnectionPoolUid *_pconpoolid_out,
	ulong _flags
){
	solid::ErrorConditionT	err;
	PushMessageVisitorF		fnc(_rmsgptr, _msg_type_idx, _flags, EventCategory::createRaise());
	bool					rv = manager().visit(_robjuid, fnc);
	if(rv){
		//message successfully delivered
		if(_pconpoolid_out){
			*_pconpoolid_out = _rconpooluid;
		}
	}else{
		//connection does not exist
		err.assign(-1, err.category());//TODO
	}
	return err;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::scheduleConnectionClose(
	ConnectionUid const &_rconnection_uid
){
	ConnectionPoolUid	fakeuid;
	MessagePointerT		msgptr;
	return doSendMessage(_rconnection_uid.connectionid, msgptr, -1, fakeuid, nullptr, 0);
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doActivateConnection(
	ConnectionUid const &_rconnection_uid,
	const char *_recipient_name,
	ActivateConnectionMessageFactoryFunctionT const &_rmsgfactory,
	const bool _may_quit
){
	solid::ErrorConditionT					err;
	Event									evt;
	std::pair<MessagePointerT, uint32>		msgpair;
	
	if(_recipient_name == nullptr){
		evt = Connection::activateEvent();
		if(manager().notify(_rconnection_uid.connectionid, evt)){
		}else{
			edbgx(Debug::ipc, "connection does not exist");
			err.assign(-1, err.category());//TODO: server only
		}
		return err;
	}
	
	if(_rconnection_uid.poolid.isValid()){
		edbgx(Debug::ipc, "only accepted connections allowed");
		err.assign(-1, err.category());//TODO: only accepted connections allowed
		return err;
	}
	
	ConnectionPoolUid			poolid;
	Locker<Mutex>				lock(d.mtx);
	NameMapT::const_iterator	it = d.namemap.find(_recipient_name);
	
	SmartLocker<Mutex>			lock2;
	
	if(it != d.namemap.end()){//connection pool exists
		poolid.index = it->second;
		SmartLocker<Mutex>		tmplock(d.connectionPoolMutex(poolid.index));
		
		lock2 = std::move(tmplock);
	}else{//connection pool does not exist
		
		if(d.config.isServerOnly()){
			edbgx(Debug::ipc, "request for name resolve for a server only configuration");
			err.assign(-1, err.category());//TODO: server only
			return err;
		}
		
		if(d.conpoolcachestk.size()){
			poolid.index = d.conpoolcachestk.top();
			d.conpoolcachestk.pop();
		}else{
			poolid.index = d.conpooldq.size();
			d.conpooldq.push_back(ConnectionPoolStub());
		}
		
		SmartLocker<Mutex>				tmplock(d.connectionPoolMutex(poolid.index));
		ConnectionPoolStub 				&rconpool(d.conpooldq[poolid.index]);
		
		rconpool.name = _recipient_name;
	
		d.namemap[rconpool.name.c_str()] = poolid.index;
		
		lock2 = std::move(tmplock);
	}
	
	evt = Connection::activateEvent(poolid);
	
	ConnectionPoolStub		&rconpool(d.conpooldq[poolid.index]);
		
	poolid.unique = rconpool.uid;
	
	const size_t			wouldbe_active_connection_count = rconpool.active_connection_count + 1;
	
	if(
		(wouldbe_active_connection_count < d.config.max_per_pool_connection_count) or
		(
			(wouldbe_active_connection_count == d.config.max_per_pool_connection_count) and
			rconpool.pending_connection_count and not _may_quit
		)
	){
		std::pair<MessagePointerT, uint32>			msgpair = _rmsgfactory(err);
		bool										success = false;
		
		if(not msgpair.first.empty()){
			const size_t		msg_type_idx = tm.index(msgpair.first.get());
			
			if(msg_type_idx != 0){
				//first send the Init message
				err = doSendMessage(_rconnection_uid.connectionid, msgpair.first, msg_type_idx, poolid, nullptr, msgpair.second);
				if(err){
					success = false;
				}else{
					//then send the activate event
					success = manager().notify(_rconnection_uid.connectionid, evt);
				}
			}else{
				err.assign(-1, err.category());//TODO: Unknown message type
				success = false;
			}
		}else{
			success = false;
		}
		
		if(success){
			++rconpool.active_connection_count;
			++rconpool.pending_connection_count;
		}
	}else{
		edbgx(Debug::ipc, "Connection count limit reached on connection-pool: "<<_recipient_name);
		
		err.assign(-1, err.category());//TODO: Connection count limit
		
		std::pair<MessagePointerT, uint32>			msgpair = _rmsgfactory(err);
		
		if(not msgpair.first.empty()){
			const size_t		msg_type_idx = tm.index(msgpair.first.get());
			
			if(msg_type_idx != 0){
				doSendMessage(_rconnection_uid.connectionid, msgpair.first, msg_type_idx, poolid, nullptr, msgpair.second);
			}else{
				err.assign(-1, err.category());//TODO: Unknown message type
			}
		}
		msgpair.first.clear();
		msgpair.second = 0;
		//close connection
		doSendMessage(_rconnection_uid.connectionid, msgpair.first, -1, poolid, nullptr, msgpair.second);
		cassert(rconpool.active_connection_count or rconpool.pending_connection_count);
	}
	
	return err;
}
//-----------------------------------------------------------------------------
void Service::activateConnectionComplete(Connection &_rcon){
	cassert(_rcon.conpoolid.isValid());
	
	Locker<Mutex>		lock2(d.connectionPoolMutex(_rcon.conpoolid.index));
	ConnectionPoolStub	&rconpool(d.conpooldq[_rcon.conpoolid.index]);
	
	--rconpool.pending_connection_count;
}
//-----------------------------------------------------------------------------
void Service::onConnectionClose(Connection &_rcon, ObjectUidT const &_robjuid){
	if(_rcon.conpoolid.isValid()){
		Locker<Mutex>		lock(d.mtx);
		Locker<Mutex>		lock2(d.connectionPoolMutex(_rcon.conpoolid.index));
		ConnectionPoolStub	&rconpool(d.conpooldq[_rcon.conpoolid.index]);
		
		cassert(rconpool.uid == _rcon.conpoolid.unique);
		
		if(_rcon.isActive()){
			--rconpool.active_connection_count;
			if(_robjuid.isValid()){
				//pop the connection from 
				size_t	sz = rconpool.conn_waitingq.size();
				while(sz){
					if(
						rconpool.conn_waitingq.front().index != _robjuid.index /*and
						rconpool.conn_waitingq.front().unique == _robjuid.unique*/
					){
						rconpool.conn_waitingq.push(rconpool.conn_waitingq.front());
					}else{
						cassert(rconpool.conn_waitingq.front().unique == _robjuid.unique);
					}
					
					rconpool.conn_waitingq.pop();
					
					--sz;
				}
			}
		}else{
			--rconpool.pending_connection_count;
			if(_robjuid.isInvalid()){
				//called on Connection::doActivate, after Connection::postStop
				--rconpool.active_connection_count;
			}
		}
		
		if(!rconpool.active_connection_count and !rconpool.pending_connection_count){
			//move all pending messages to _rcon for completion
			while(rconpool.msgq.size()){
				MessageStub &rms = rconpool.msgq.front();
				_rcon.directPushMessage(rms.msgptr, rms.msg_type_idx, rms.flags);
				rconpool.msgq.pop();
			}
		}
		
		d.conpoolcachestk.push(_rcon.conpoolid.index);
		
		rconpool.clear();
	}
}
//-----------------------------------------------------------------------------
void Service::acceptIncomingConnection(SocketDevice &_rsd){
	DynamicPointer<aio::Object>		objptr(new Connection(_rsd));
	solid::ErrorConditionT			err;
	ObjectUidT						conuid = d.config.scheduler().startObject(objptr, *this, EventCategory::createStart(), err);
	
	idbgx(Debug::ipc, "receive connection ["<<conuid<<"] err = "<<err.message());
}
//-----------------------------------------------------------------------------
void Service::onIncomingConnectionStart(ConnectionContext &_rconctx){
	configuration().incoming_connection_start_fnc(_rconctx);
}
//-----------------------------------------------------------------------------
void Service::onOutgoingConnectionStart(ConnectionContext &_rconctx){
	configuration().outgoing_connection_start_fnc(_rconctx);
}
//-----------------------------------------------------------------------------
void Service::onConnectionStop(ConnectionContext &_rconctx, ErrorConditionT const &_err){
	configuration().connection_stop_fnc(_rconctx, _err);
}
//-----------------------------------------------------------------------------
void Service::forwardResolveMessage(ConnectionPoolUid const &_rconpoolid, Event const&_revent){
	ResolveMessage 	*presolvemsg = ResolveMessage::cast(_revent.msgptr.get());
	ErrorConditionT	err;
	++presolvemsg->crtidx;
	if(presolvemsg->crtidx < presolvemsg->addrvec.size()){
		DynamicPointer<aio::Object>		objptr(new Connection(_rconpoolid));
		
		Locker<Mutex>					lock(d.connectionPoolMutex(_rconpoolid.index));
		ConnectionPoolStub				&rconpool(d.conpooldq[_rconpoolid.index]);
		ObjectUidT						conuid = d.config.scheduler().startObject(objptr, *this, _revent, err);
		idbgx(Debug::ipc, conuid<<" "<<err.message());
		if(!err){
			++rconpool.pending_connection_count;
		}
	}
}
//=============================================================================
//-----------------------------------------------------------------------------
/*virtual*/ Message::~Message(){
	
}
//-----------------------------------------------------------------------------
//=============================================================================
struct ResolveF{
	ResolveCompleteFunctionT	cbk;
	
	void operator()(ResolveData &_rrd, ERROR_NS::error_code const &_rerr){
		AddressVectorT		addrvec;
		if(!_rerr){
			for(auto it = _rrd.begin(); it != _rrd.end(); ++it){
				addrvec.push_back(SocketAddressStub(it));
				idbgx(Debug::ipc, "Add resolved address: "<<addrvec.back());
			}
		}
		cbk(addrvec);
	}
};

void ResolverF::operator()(const std::string&_name, ResolveCompleteFunctionT& _cbk){
	std::string		tmp;
	const char 		*hst_name;
	const char		*svc_name;
	
	size_t off = _name.rfind(':');
	if(off != std::string::npos){
		tmp = _name.substr(0, off);
		hst_name = tmp.c_str();
		svc_name = _name.c_str() + off + 1;
		if(!svc_name[0]){
			svc_name = default_service.c_str();
		}
	}else{
		hst_name = _name.c_str();
		svc_name = default_service.c_str();
	}
	
	ResolveF		fnc;
	
	fnc.cbk = std::move(_cbk);
	
	rresolver.requestResolve(fnc, hst_name, svc_name, 0, -1, SocketInfo::Stream);
}
//=============================================================================
}//namespace ipc
}//namespace frame
}//namespace solid


