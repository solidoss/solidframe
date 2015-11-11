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
#include "utility/string.hpp"

#include "ipcutility.hpp"
#include "ipcconnection.hpp"
#include "ipclistener.hpp"


#include <vector>
#include <deque>

#ifdef SOLID_USE_CPP11
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
typedef CPP11_NS::unordered_map<
	const char*, size_t, CStringHash, CStringEqual
>														NameMapT;
typedef Queue<ObjectUidT>								ObjectUidQueueT;

struct MessageStub{
	MessageStub(
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ResponseHandlerFunctionT &_rresponse_fnc,
		ulong _flags
	): msgptr(std::move(_rmsgptr)), msg_type_idx(_msg_type_idx), response_fnc(std::move(_rresponse_fnc)), flags(_flags){}
	
	MessageStub(
		MessageStub && _rrmsg
	): msgptr(std::move(_rrmsg.msgptr)), msg_type_idx(_rrmsg.msg_type_idx), response_fnc(std::move(_rrmsg.response_fnc)), flags(_rrmsg.flags){}
	
	MessagePointerT 			msgptr;
	const size_t				msg_type_idx;
	ResponseHandlerFunctionT 	response_fnc;
	ulong						flags;
};

typedef Queue<MessageStub>								MessageQueueT;

struct ConnectionPoolStub{
	ConnectionPoolStub():uid(0), pending_connection_count(0), active_connection_count(0){}
	
	void clear(){
		name.clear();
		synchronous_connection_uid = ObjectUidT();
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
	ObjectUidT 		synchronous_connection_uid;
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
	
	void lockAllConnectionPoolMutexes(){
		for(size_t i = 0; i < mtxsarrcp; ++i){
			pmtxarr[i].lock();
		}
	}
	
	void unlockAllConnectionPoolMutexes(){
		for(size_t i = 0; i < mtxsarrcp; ++i){
			pmtxarr[i].unlock();
		}
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
size_t Service::doPushNewConnectionPool(){
	d.lockAllConnectionPoolMutexes();
	for(size_t i = 0; i < d.mtxsarrcp; ++i){
		size_t idx = d.conpooldq.size();
		d.conpooldq.push_back(ConnectionPoolStub());
		d.conpoolcachestk.push(d.mtxsarrcp - idx - 1);
	}
	d.unlockAllConnectionPoolMutexes();
	size_t	idx = d.conpoolcachestk.top();
	d.conpoolcachestk.pop();
	return idx;
}
//-----------------------------------------------------------------------------
struct PushMessageConnectionVisitorF{
	PushMessageConnectionVisitorF(
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ResponseHandlerFunctionT &_rresponse_fnc,
		ulong _flags,
		MessageUid  *_pmsguid = nullptr
	):	rmsgptr(_rmsgptr), msg_type_idx(_msg_type_idx),
		rresponse_fnc(_rresponse_fnc), flags(_flags),
		pmsguid(_pmsguid){}
	
	
	bool operator()(ObjectBase &_robj, ReactorBase &_rreact){
		Connection *pcon = Connection::cast(&_robj);
		
		if(pcon){
			Event		raise_event;
			const bool	retval = pcon->pushMessage(rmsgptr, msg_type_idx, rresponse_fnc, flags, pmsguid, raise_event);
			
			if(retval){
				if(!raise_event.empty()){
					_rreact.raise(_robj.runId(), raise_event);
				}
			}
			return retval;
		}else{
			return false;
		}
	}
	
	MessagePointerT 			&rmsgptr;
	const size_t 				msg_type_idx;
	ResponseHandlerFunctionT	&rresponse_fnc;
	ulong 						flags;
	MessageUid 					*pmsguid;
};


struct ActivateConnectionVisitorF{
	ActivateConnectionVisitorF(
		ConnectionPoolUid const &_rconpoolid
	):conpoolid(_rconpoolid){}
	
	bool operator()(ObjectBase &_robj, ReactorBase &_rreact){
		Connection *pcon = Connection::cast(&_robj);
		
		if(pcon){
			Event		raise_event;
			const bool	retval = pcon->prepareActivate(conpoolid, raise_event);
			
			if(retval){
				if(!raise_event.empty()){
					_rreact.raise(_robj.runId(), raise_event);
				}
			}
			return retval;
		}else{
			return false;
		}
	}
	
	ConnectionPoolUid conpoolid;
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
		idbgx(Debug::ipc, "OnResolveF(addrvec of size "<<_raddrvec.size()<<")");
		event.msgptr = new ResolveMessage(_raddrvec);
		rm.notify(objuid, event);
	}
};

ErrorConditionT Service::doSendMessage(
	const char *_recipient_name,
	const ConnectionUid	&_rconuid_in,
	MessagePointerT &_rmsgptr,
	ResponseHandlerFunctionT &_rresponse_fnc,
	ConnectionPoolUid *_pconpoolid_out,
	MessageUid *_pmsguid_out,
	ulong _flags
){
	solid::ErrorConditionT		err;
	Locker<Mutex>				lock(d.mtx);
	size_t						idx;
	uint32						uid;
	bool						check_uid = false;
	const size_t				msg_type_idx = tm.index(_rmsgptr.get());
	
	if(msg_type_idx == 0){
		edbgx(Debug::ipc, this<<" type not registered");
		err.assign(-1, err.category());//TODO:type not registered
		return err;
	}
	
	if(_recipient_name){
		NameMapT::const_iterator	it = d.namemap.find(_recipient_name);
		
		if(it != d.namemap.end()){
			idx = it->second;
		}else{
			
			if(d.config.isServerOnly()){
				edbgx(Debug::ipc, this<<" request for name resolve for a server only configuration");
				err.assign(-1, err.category());//TODO: server only
				return err;
			}
			
			if(d.conpoolcachestk.size()){
				idx = d.conpoolcachestk.top();
				d.conpoolcachestk.pop();
			}else{
				idx = doPushNewConnectionPool();
			}
			
			Locker<Mutex>					lock2(d.connectionPoolMutex(idx));
			ConnectionPoolStub 				&rconpool(d.conpooldq[idx]);
			
			rconpool.name = _recipient_name;
			
			DynamicPointer<aio::Object>		objptr(new Connection(ConnectionPoolUid(idx, rconpool.uid)));
			
			ObjectUidT						conuid = d.config.scheduler().startObject(objptr, *this, EventCategory::createStart(), err);
			
			if(err){
				edbgx(Debug::ipc, this<<" Starting Session: "<<err.message());
				rconpool.clear();
				d.conpoolcachestk.push(idx);
				return err;
			}
			
			d.namemap[rconpool.name.c_str()] = idx;
			
			idbgx(Debug::ipc, this<<" Success starting Connection Pool object: "<<conuid.index<<','<<conuid.unique);
			//resolve the name
			ResolveCompleteFunctionT		cbk(OnRelsolveF(manager(), conuid, Connection::resolveEvent()));
			
			d.config.name_resolve_fnc(rconpool.name, cbk);
			
			rconpool.msgq.push(MessageStub(_rmsgptr, msg_type_idx, _rresponse_fnc, _flags));
			++rconpool.pending_connection_count;
			
			return err;
		}
	}else if(not _rconuid_in.isInvalidConnection()){
		return doNotifyConnectionPushMessage(_rconuid_in.connectionid, _rmsgptr, msg_type_idx, _rresponse_fnc, _rconuid_in.poolid, _pconpoolid_out, _flags);
	}else if(_rconuid_in.poolid.index < d.conpooldq.size()/* && _rconuid_in.ssnuid == d.conpooldq[_rconuid_in.ssnidx].uid*/){
		//we cannot check the uid right now because we need a lock on the session's mutex
		check_uid = true;
		idx = _rconuid_in.poolid.index;
		uid = _rconuid_in.poolid.unique;
	}else{
		edbgx(Debug::ipc, this<<" session does not exist");
		err.assign(-1, err.category());//TODO: session does not exist
		return err;
	}
	
	Locker<Mutex>			lock2(d.connectionPoolMutex(idx));
	ConnectionPoolStub 		&rconpool(d.conpooldq[idx]);
	
	if(check_uid && rconpool.uid != uid){
		//failed uid check
		edbgx(Debug::ipc, this<<" session does not exist");
		err.assign(-1, err.category());//TODO: session does not exist
		return err;
	}
	
	if(
		Message::is_synchronous(_flags) and
		rconpool.synchronous_connection_uid.isValid()
	){
		ErrorConditionT	tmperr = doNotifyConnectionPushMessage(
			rconpool.synchronous_connection_uid,
			_rmsgptr,
			msg_type_idx,
			_rresponse_fnc,
			ConnectionPoolUid(idx, rconpool.uid),
			_pconpoolid_out, _flags
		);
		if(!tmperr){
			return err;
		}
		edbgx(Debug::ipc, this<<" synchronous connection is dead and pool's synchronous_connection_uid is valid");
		cassert(false);
		rconpool.synchronous_connection_uid = UniqueId::invalid();
	}
	
	while(rconpool.conn_waitingq.size()){//there are connections waiting for something to send
		ObjectUidT 		objuid = rconpool.conn_waitingq.front();
		
		rconpool.conn_waitingq.pop();
		
		ErrorConditionT	tmperr = doNotifyConnectionPushMessage(
			objuid, _rmsgptr, msg_type_idx, _rresponse_fnc, ConnectionPoolUid(idx, rconpool.uid), _pconpoolid_out, _flags
		);
		
		if(!tmperr){
			if(Message::is_synchronous(_flags)){
				rconpool.synchronous_connection_uid = objuid;
			}
			return err;
		}
		wdbgx(Debug::ipc, this<<" failed sending message to connection "<<objuid<<" error: "<<tmperr.message());
	}
	
	idbgx(Debug::ipc, this<<" all connections are busy. active connections "<<rconpool.active_connection_count<<" pending connections "<< rconpool.pending_connection_count);
	//All connections are busy
	//Check if we should create a new connection
	
	if((rconpool.active_connection_count + rconpool.pending_connection_count + 1) <= d.config.max_per_pool_connection_count){
		DynamicPointer<aio::Object>		objptr(new Connection(ConnectionPoolUid(idx, rconpool.uid)));
			
		ObjectUidT						conuid = d.config.scheduler().startObject(objptr, *this, EventCategory::createStart(), err);
		
		if(!err){
			ResolveCompleteFunctionT		cbk(OnRelsolveF(manager(), conuid, Connection::resolveEvent()));
			
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
	
	rconpool.msgq.push(MessageStub(_rmsgptr, msg_type_idx, _rresponse_fnc, _flags));
	
	return err;
}
//-----------------------------------------------------------------------------
void Service::tryFetchNewMessage(Connection &_rcon, aio::ReactorContext &_rctx, const bool _connection_has_no_message_to_send){
	Locker<Mutex>			lock2(d.connectionPoolMutex(_rcon.poolUid().index));
	ConnectionPoolStub 		&rconpool(d.conpooldq[_rcon.poolUid().index]);
	
	cassert(rconpool.uid == _rcon.poolUid().unique);
	if(rconpool.uid != _rcon.poolUid().unique) return;
	
	idbgx(Debug::ipc, this<<' '<<&_rcon<<" msg_q_size "<<rconpool.msgq.size());
	if(rconpool.msgq.size()){
		//we have something to send
		if(
			Message::is_asynchronous(rconpool.msgq.front().flags) or 
			this->manager().id(_rcon) == rconpool.synchronous_connection_uid
		){
			MessageStub		&rmsgstub = rconpool.msgq.front();
			MessageBundle	msgbundle(rmsgstub.msgptr, rmsgstub.msg_type_idx, rmsgstub.flags, rmsgstub.response_fnc);
			_rcon.directPushMessage(_rctx, msgbundle, nullptr);
			rconpool.msgq.pop();
		}else if(rconpool.synchronous_connection_uid.isInvalid()){
			MessageStub	&rmsgstub = rconpool.msgq.front();
			MessageBundle	msgbundle(rmsgstub.msgptr, rmsgstub.msg_type_idx, rmsgstub.flags, rmsgstub.response_fnc);
			_rcon.directPushMessage(_rctx, msgbundle, nullptr);
			rconpool.synchronous_connection_uid = this->manager().id(_rcon);
			rconpool.msgq.pop();
		}else{
			//worse case scenario - we must skip the current synchronous message
			//and find an asynchronous one
			size_t		qsz = rconpool.msgq.size();
			bool		found = false;
			
			while(qsz--){
				MessageStub		msgstub(std::move(rconpool.msgq.front()));
				
				rconpool.msgq.pop();
				
				if(not found and Message::is_asynchronous(msgstub.flags)){
					MessageBundle	msgbundle(msgstub.msgptr, msgstub.msg_type_idx, msgstub.flags, msgstub.response_fnc);
					_rcon.directPushMessage(_rctx, msgbundle, nullptr);
					found = true;
				}else{
					rconpool.msgq.push(std::move(msgstub));
				}
			}
			
			if(not found and _connection_has_no_message_to_send){
				rconpool.conn_waitingq.push(this->manager().id(_rcon));
			}
		}
		
	}else if(_connection_has_no_message_to_send){
		rconpool.conn_waitingq.push(this->manager().id(_rcon));
	}
}

//-----------------------------------------------------------------------------
ErrorConditionT Service::doNotifyConnectionPushMessage(
	ObjectUidT const &_robjuid,
	MessagePointerT &_rmsgptr,
	const size_t _msg_type_idx,
	ResponseHandlerFunctionT &_rresponse_fnc,
	ConnectionPoolUid const &_rconpooluid,
	ConnectionPoolUid *_pconpoolid_out,
	ulong _flags
){
	solid::ErrorConditionT			err;
	PushMessageConnectionVisitorF	fnc(_rmsgptr, _msg_type_idx, _rresponse_fnc, _flags);
	bool							rv = manager().visit(_robjuid, fnc);
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
ErrorConditionT Service::doNotifyConnectionActivate(
	ObjectUidT const &_robjuid,
	ConnectionPoolUid const &_rconpooluid
){
	solid::ErrorConditionT		err;
	ActivateConnectionVisitorF	fnc(_rconpooluid);
	bool						rv = manager().visit(_robjuid, fnc);
	if(rv){
		//message successfully delivered
	}else{
		//connection does not exist
		err.assign(-1, err.category());//TODO
	}
	return err;
}
//-----------------------------------------------------------------------------
struct DelayedCloseConnectionVisitorF{
	DelayedCloseConnectionVisitorF(
	){}
	
	bool operator()(ObjectBase &_robj, ReactorBase &_rreact){
		Connection *pcon = Connection::cast(&_robj);
		
		if(pcon){
			Event		raise_event;
			const bool	retval = pcon->pushDelayedClose(raise_event);
			
			if(retval){
				if(!raise_event.empty()){
					_rreact.raise(_robj.runId(), raise_event);
				}
			}
			return retval;
		}else{
			return false;
		}
	}
};
bool Service::doNotifyConnectionDelayedClose(
	ObjectUidT const &_robjuid
){
	DelayedCloseConnectionVisitorF		fnc;
	return manager().visit(_robjuid, fnc);
}
//-----------------------------------------------------------------------------
bool Service::delayedConnectionClose(
	ConnectionUid const &_rconnection_uid
){
	return doNotifyConnectionDelayedClose(_rconnection_uid.connectionid);
}
//-----------------------------------------------------------------------------
bool Service::forcedConnectionClose(
	ConnectionUid const &_rconnection_uid
){
	
	return manager().notify(_rconnection_uid.connectionid, EventCategory::createKill());
}
//-----------------------------------------------------------------------------
// Three situations in doActivateConnection is called:
// 1. for a new server connection - in a client-server scenario
// 2. for a new server connection - in a peer2peer scenario - given a valid _recipient_name
// 3. for a new client connection - in any scenario - given a valid _rconnection_uid.poolid
//
// 

ErrorConditionT Service::doActivateConnection(
	ConnectionUid const &_rconnection_uid,
	const char *_recipient_name,
	ActivateConnectionMessageFactoryFunctionT const &_rmsgfactory,
	const bool _may_quit
){
	solid::ErrorConditionT					err;
	std::pair<MessagePointerT, uint32>		msgpair;
	ConnectionPoolUid						poolid;
	
	if(_recipient_name == nullptr and not _rconnection_uid.poolid.isValid()){//situation 1
		err = doNotifyConnectionActivate(_rconnection_uid.connectionid, poolid);
		return err;
	}
	
	if(_recipient_name != nullptr and _rconnection_uid.poolid.isValid()){
		edbgx(Debug::ipc, this<<" only accepted connections allowed");
		err.assign(-1, err.category());//TODO: only accepted connections allowed
		return err;
	}
	
	Locker<Mutex>				lock(d.mtx);
	SmartLocker<Mutex>			lock2;
	
	if(_rconnection_uid.poolid.isValid()){
		poolid = _rconnection_uid.poolid;//situation 3
	}else{
		//situation 2
		NameMapT::const_iterator	it = d.namemap.find(_recipient_name);
		
		if(it != d.namemap.end()){//connection pool exist
			poolid.index = it->second;
			SmartLocker<Mutex>		tmplock(d.connectionPoolMutex(poolid.index));
			ConnectionPoolStub 		&rconpool(d.conpooldq[poolid.index]);
			
			poolid.unique = rconpool.uid;
			
			lock2 = std::move(tmplock);
		}else{//connection pool does not exist
			
			if(d.config.isServerOnly()){
				edbgx(Debug::ipc, this<<" request for name resolve for a server only configuration");
				err.assign(-1, err.category());//TODO: server only
				return err;
			}
			
			if(d.conpoolcachestk.size()){
				poolid.index = d.conpoolcachestk.top();
				d.conpoolcachestk.pop();
			}else{
				poolid.index = doPushNewConnectionPool();
			}
			
			SmartLocker<Mutex>				tmplock(d.connectionPoolMutex(poolid.index));
			ConnectionPoolStub 				&rconpool(d.conpooldq[poolid.index]);
			
			rconpool.name = _recipient_name;
			poolid.unique = rconpool.uid;
		
			d.namemap[rconpool.name.c_str()] = poolid.index;
			
			lock2 = std::move(tmplock);
		}
	}
	
	ConnectionPoolStub			&rconpool(d.conpooldq[poolid.index]);
	const size_t				wouldbe_active_connection_count = rconpool.active_connection_count + 1;
	ResponseHandlerFunctionT	response_handler;
	
	poolid.unique = rconpool.uid;
	
	
	if(
		(wouldbe_active_connection_count < d.config.max_per_pool_connection_count) or
		(
			(
				wouldbe_active_connection_count == d.config.max_per_pool_connection_count
			) and not rconpool.pending_connection_count
		) or
		(
			(
				wouldbe_active_connection_count == d.config.max_per_pool_connection_count
			) and rconpool.pending_connection_count and not _may_quit
		)
	){
		std::pair<MessagePointerT, uint32>			msgpair;
		bool										success = false;
		
		idbgx(Debug::ipc, this<<" connection count limit not reached on connection-pool: "<<poolid);
		
		if(not FUNCTION_EMPTY(_rmsgfactory)){
			msgpair = _rmsgfactory(err);
		}
		
		if(not msgpair.first.empty()){
			const size_t		msg_type_idx = tm.index(msgpair.first.get());
			
			if(msg_type_idx != 0){
				//first send the Init message
				err = doNotifyConnectionPushMessage(_rconnection_uid.connectionid, msgpair.first, msg_type_idx, response_handler, poolid, nullptr, msgpair.second);
				if(err){
					success = false;
				}else{
					//then send the activate event
					err = doNotifyConnectionActivate(_rconnection_uid.connectionid, poolid);
					success = !err;
				}
			}else{
				err.assign(-1, err.category());//TODO: Unknown message type
				success = false;
			}
		}else if(FUNCTION_EMPTY(_rmsgfactory)){
			err = doNotifyConnectionActivate(_rconnection_uid.connectionid, poolid);
			success = !err;
		}else{
			//close connection
			doNotifyConnectionDelayedClose(_rconnection_uid.connectionid);
			success = false;
		}
		
		if(success){
			++rconpool.active_connection_count;
			++rconpool.pending_connection_count;//activateConnectionComplete will decrement it
			idbgx(Debug::ipc, this<<' '<<poolid<<" active_connection_count "<<rconpool.active_connection_count<<" pending_connection_count "<<rconpool.pending_connection_count);
		}
	}else{
		idbgx(Debug::ipc, this<<" connection count limit reached on connection-pool: "<<poolid);
		
		err.assign(-1, err.category());//TODO: Connection count limit
		
		std::pair<MessagePointerT, uint32>			msgpair;
		
		
		if(not FUNCTION_EMPTY(_rmsgfactory)){
			msgpair = _rmsgfactory(err);
		}
		
		if(not msgpair.first.empty()){
			const size_t		msg_type_idx = tm.index(msgpair.first.get());
			
			if(msg_type_idx != 0){
				doNotifyConnectionPushMessage(_rconnection_uid.connectionid, msgpair.first, msg_type_idx, response_handler, poolid, nullptr, msgpair.second);
			}else{
				err.assign(-1, err.category());//TODO: Unknown message type
			}
		}
		
		msgpair.first.clear();
		msgpair.second = 0;
		//close connection
		doNotifyConnectionDelayedClose(_rconnection_uid.connectionid);
		cassert(rconpool.active_connection_count or rconpool.pending_connection_count);
	}
	
	return err;
}
//-----------------------------------------------------------------------------
void Service::activateConnectionComplete(Connection &_rcon){
	if(_rcon.conpoolid.isValid()){
	
		Locker<Mutex>		lock2(d.connectionPoolMutex(_rcon.conpoolid.index));
		ConnectionPoolStub	&rconpool(d.conpooldq[_rcon.conpoolid.index]);
		
		--rconpool.pending_connection_count;
		
		idbgx(Debug::ipc, this<<' '<<_rcon.conpoolid<<" active_connection_count "<<rconpool.active_connection_count<<" pending_connection_count "<<rconpool.pending_connection_count);
	}
}
//-----------------------------------------------------------------------------
void Service::onConnectionClose(Connection &_rcon, aio::ReactorContext &_rctx, ObjectUidT const &_robjuid){
	
	idbgx(Debug::ipc, this<<' '<<&_rcon<<" "<<_robjuid);
	
	if(_rcon.conpoolid.isValid()){
		ConnectionPoolUid	conpoolid = _rcon.conpoolid;
		Locker<Mutex>		lock(d.mtx);
		Locker<Mutex>		lock2(d.connectionPoolMutex(_rcon.conpoolid.index));
		ConnectionPoolStub	&rconpool(d.conpooldq[_rcon.conpoolid.index]);
		
		_rcon.conpoolid = ConnectionPoolUid();
		
		cassert(rconpool.uid == conpoolid.unique);
		
		idbgx(Debug::ipc, this<<' '<<_robjuid);
		
		if(_rcon.isAtomicActive()){//we do not care if the connection is aware if it is active
			--rconpool.active_connection_count;
			
			//we do not actually need to pop the connection from conn_waitingq
			//doPushMessageToConnection will fail anyway
#if 0
			if(_robjuid.isValid()){
				//pop the connection from waitingq
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
#endif
		}else{
			--rconpool.pending_connection_count;
			
			
			if(_robjuid.isInvalid()){
				//called on Connection::doActivate, after Connection::postStop
				--rconpool.active_connection_count;
			}
			idbgx(Debug::ipc, this<<' '<<conpoolid<<" active_connection_count "<<rconpool.active_connection_count<<" pending_connection_count "<<rconpool.pending_connection_count);
		}
		
		if(rconpool.synchronous_connection_uid == _robjuid){
			rconpool.synchronous_connection_uid = ObjectUidT::invalid();
		}
		
		if(!rconpool.active_connection_count and !rconpool.pending_connection_count){
			//_rcon is the last dying connection
			//so, move all pending messages to _rcon for completion
			while(rconpool.msgq.size()){
				MessageStub 	&rmsgstub = rconpool.msgq.front();
				MessageBundle	msgbundle(rmsgstub.msgptr, rmsgstub.msg_type_idx, rmsgstub.flags, rmsgstub.response_fnc);
				_rcon.directPushMessage(_rctx, msgbundle, nullptr);
				rconpool.msgq.pop();
			}
			
			d.conpoolcachestk.push(conpoolid.index);
		
			rconpool.clear();
		}else{
			size_t		qsz = rconpool.msgq.size();
			
			_rcon.fetchUnsentMessages(
				[this](
					ConnectionPoolUid &_rconpoolid,
					MessagePointerT &_rmsgptr,
					const size_t _msg_type_idx,
					ResponseHandlerFunctionT &_rresponse_fnc,
					const ulong _flags
				){
					this->pushBackMessageToConnectionPool(_rconpoolid, _rmsgptr, _msg_type_idx, _rresponse_fnc, _flags);
				}
			);
			
			if(rconpool.msgq.size() > qsz){
				//move the newly pushed messages up-front of msgq.
				//rotate msgq by qsz
				while(qsz--){
					MessageStub msgstub(std::move(rconpool.msgq.front()));
					rconpool.msgq.pop();
					rconpool.msgq.push(std::move(msgstub));
				}
			}
		}
	}
}
//-----------------------------------------------------------------------------
void Service::pushBackMessageToConnectionPool(
	ConnectionPoolUid &_rconpoolid,
	MessagePointerT &_rmsgptr,
	const size_t _msg_type_idx,
	ResponseHandlerFunctionT &_rresponse_fnc,
	ulong _flags
){
	ConnectionPoolStub	&rconpool(d.conpooldq[_rconpoolid.index]);
	
	cassert(rconpool.uid == _rconpoolid.unique);
	
	if(Message::is_idempotent(_flags) or (/*not Message::is_started_send(_flags) and*/ not Message::is_done_send(_flags))){
		_flags &= ~(Message::DoneSendFlagE | Message::StartedSendFlagE);
		rconpool.msgq.push(MessageStub(_rmsgptr, _msg_type_idx, _rresponse_fnc, _flags));
	}
}
//-----------------------------------------------------------------------------
void Service::acceptIncomingConnection(SocketDevice &_rsd){
	DynamicPointer<aio::Object>		objptr(new Connection(_rsd));
	solid::ErrorConditionT			err;
	ObjectUidT						conuid = d.config.scheduler().startObject(objptr, *this, EventCategory::createStart(), err);
	
	idbgx(Debug::ipc, this<<" receive connection ["<<conuid<<"] err = "<<err.message());
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
		idbgx(Debug::ipc, this<<' '<<conuid<<" "<<err.message());
		if(!err){
			++rconpool.pending_connection_count;
			idbgx(Debug::ipc, this<<' '<<_rconpoolid<<" active_connection_count "<<rconpool.active_connection_count<<" pending_connection_count "<<rconpool.pending_connection_count);
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
				idbgx(Debug::ipc, "add resolved address: "<<addrvec.back());
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
	
	rresolver.requestResolve(fnc, hst_name, svc_name, 0, this->family, SocketInfo::Stream);
}
//=============================================================================

std::ostream& operator<<(std::ostream &_ros, ConnectionUid const &_con_id){
	_ros<<'{'<<_con_id.connectionId()<<"}{"<<_con_id.poolId()<<'}';
	return _ros;
}
//-----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream &_ros, RequestUid const &_msguid){
	_ros<<'{'<<_msguid.index<<','<<_msguid.unique<<'}';
	return _ros;
}
//-----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream &_ros, MessageUid const &_msguid){
	_ros<<'{'<<_msguid.index<<','<<_msguid.unique<<'}';
	return _ros;
}
//=============================================================================
}//namespace ipc
}//namespace frame
}//namespace solid


