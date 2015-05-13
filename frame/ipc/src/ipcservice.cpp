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

struct SessionStub{
	SessionStub():uid(0), conn_pending(0), conn_active(0){}
	
	void clear(){
		name.clear();
		synch_conn_uid = ObjectUidT();
		uid = -1;
		conn_pending = 0;
		conn_active = 0;
		cassert(msgq.empty());
		while(conn_waitingq.size()){
			conn_waitingq.pop();
		}
	}
	
	uint32			uid;
	size_t			conn_pending;
	size_t			conn_active;
	std::string		name;
	ObjectUidT 		synch_conn_uid;
	MessageQueueT	msgq;
	ObjectUidQueueT	conn_waitingq;
};

typedef std::deque<SessionStub>							SessionDequeT;
typedef Stack<size_t>									SizeStackT;


struct Service::Data{
	Data():pmtxarr(nullptr), mtxsarrcp(0){}
	
	~Data(){
		delete []pmtxarr;
	}
	
	
	Mutex & sessionMutex(size_t _idx)const{
		return pmtxarr[_idx % mtxsarrcp];
	}
	
	Mutex			mtx;
	Mutex			*pmtxarr;
	size_t			mtxsarrcp;
	NameMapT		namemap;
	SessionDequeT	sessiondq;
	SizeStackT		cachestk;
	Configuration	config;
};
//=============================================================================

Service::Service(
	frame::Manager &_rm, frame::Event const &_revt
):BaseT(_rm, _revt), d(*(new Data)){}
	
//! Destructor
Service::~Service(){
	delete &d;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::reconfigure(Configuration const& _rcfg){
	this->stop(true);
	this->start();
	
	ErrorConditionT err;
	
	ServiceProxy	sp(*this);
	
	_rcfg.regfnc(sp);
	
	d.config = _rcfg;
	
	if(d.config.listen_addr_str.size()){
		std::string		tmp;
		const char 		*hst_name;
		const char		*svc_name;
		
		size_t off = d.config.listen_addr_str.rfind(':');
		if(off != std::string::npos){
			tmp = d.config.listen_addr_str.substr(0, off);
			hst_name = tmp.c_str();
			svc_name = d.config.listen_addr_str.c_str() + off + 1;
			if(!svc_name[0]){
				svc_name = d.config.default_listen_port_str.c_str();
			}
		}else{
			hst_name = d.config.listen_addr_str.c_str();
			svc_name = d.config.default_listen_port_str.c_str();
		}
		
		ResolveData		rd = synchronous_resolve(hst_name, svc_name, 0, -1, SocketInfo::Stream);
		SocketDevice	sd;
			
		sd.create(rd.begin());
		sd.prepareAccept(rd.begin(), 2000);
			
		if(sd.ok()){
			DynamicPointer<aio::Object>		objptr(new Listener(sd));
			
			ObjectUidT						conuid = d.config.scheduler().startObject(objptr, *this, d.config.event_start, err);
			
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
		if(static_cast<Connection&>(_robj).pushMessage(rmsgptr, msg_type_idx, flags)){
			_rreact.raise(_robj.runId(), raise_event);
		}
		return true;
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
	const char *_session_name,
	const ConnectionUid	&_rconuid_in,
	MessagePointerT &_rmsgptr,
	SessionUid *_psession_out,
	ulong _flags
){
	solid::ErrorConditionT		err;
	Locker<Mutex>				lock(d.mtx);
	size_t						idx;
	uint32						uid;
	bool						check_uid = false;
	size_t						msg_type_idx = tm.index(_rmsgptr.get());
	
	if(msg_type_idx == 0){
		err.assign(-1, err.category());//TODO:type not registered
		return err;
	}
	
	if(_session_name){
		NameMapT::const_iterator	it = d.namemap.find(_session_name);
		
		if(it != d.namemap.end()){
			idx = it->second;
		}else{
			if(d.cachestk.size()){
				idx = d.cachestk.top();
				d.cachestk.pop();
			}else{
				idx = d.sessiondq.size();
				d.sessiondq.push_back(SessionStub());
			}
			Locker<Mutex>					lock2(d.sessionMutex(idx));
			SessionStub 					&rss(d.sessiondq[idx]);
			
			rss.name = _session_name;
			
			DynamicPointer<aio::Object>		objptr(new Connection(SessionUid(idx, rss.uid)));
			
			ObjectUidT						conuid = d.config.scheduler().startObject(objptr, *this, d.config.event_start, err);
			
			if(err){
				idbgx(Debug::ipc, "Error starting Session object: "<<err.message());
				rss.clear();
				d.cachestk.push(idx);
				return err;
			}
			
			d.namemap[rss.name.c_str()] = idx;
			
			idbgx(Debug::ipc, "Success starting Session object: "<<conuid.index<<','<<conuid.unique);
			//resolve the name
			ResolveCompleteFunctionT		cbk(OnRelsolveF(manager(), conuid, d.config.event_raise));
			
			d.config.resolve_fnc(rss.name, cbk);
			
			rss.msgq.push(MessageStub(_rmsgptr, msg_type_idx, _flags));
			++rss.conn_pending;
			
			return err;
		}
	}else if(_rconuid_in.ssnidx < d.sessiondq.size()/* && _rconuid_in.ssnuid == d.sessiondq[_rconuid_in.ssnidx].uid*/){
		//we cannot check the uid right now because we need a lock on the session's mutex
		check_uid = true;
		idx = _rconuid_in.ssnidx;
		uid = _rconuid_in.ssnuid;
	}else if(_rconuid_in.isInvalidSession() && !_rconuid_in.isInvalidConnection()/* && d.config.isServerOnly()*/){
		return doSendMessage(_rconuid_in.conid, _rmsgptr, msg_type_idx, _rconuid_in, _psession_out, _flags);
	}else{
		err.assign(-1, err.category());//TODO: session does not exist
		return err;
	}
	
	Locker<Mutex>			lock2(d.sessionMutex(idx));
	SessionStub 			&rss(d.sessiondq[idx]);
	
	if(check_uid && rss.uid != uid){
		//failed uid check
		err.assign(-1, err.category());//TODO: session does not exist
		return err;
	}
	
	if(rss.conn_waitingq.size()){//there are connections waiting for something to send
		ObjectUidT objuid = rss.conn_waitingq.front();
		rss.conn_waitingq.pop();
		return doSendMessage(objuid, _rmsgptr, msg_type_idx, SessionUid(idx, rss.uid), _psession_out, _flags);
	}
	
	//All connections are busy
	//Check if we should create a new connection
	
	if((rss.conn_active + rss.conn_pending + 1) < d.config.max_per_session_connection_count){
		DynamicPointer<aio::Object>		objptr(new Connection(SessionUid(idx, rss.uid)));
			
		ObjectUidT						conuid = d.config.scheduler().startObject(objptr, *this, d.config.event_start, err);
		
		if(!err){
			ResolveCompleteFunctionT		cbk(OnRelsolveF(manager(), conuid, d.config.event_raise));
			
			d.config.resolve_fnc(rss.name, cbk);
			++rss.conn_pending;
		}else{
			cassert(rss.conn_pending + rss.conn_active);//there must be at least one connection to handle the message
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
	
	rss.msgq.push(MessageStub(_rmsgptr, msg_type_idx, _flags));
	
	return err;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doSendMessage(
	ObjectUidT const &_robjuid,
	MessagePointerT &_rmsgptr,
	const size_t _msg_type_idx,
	SessionUid const &_rsessionuid,
	SessionUid *_psession_out,
	ulong _flags
){
	solid::ErrorConditionT	err;
	PushMessageVisitorF		fnc(_rmsgptr, _msg_type_idx, _flags, d.config.event_raise);
	bool					rv = manager().visit(_robjuid, fnc);
	if(rv){
		//message successfully delivered
		if(_psession_out){
			*_psession_out = _rsessionuid;
		}
	}else{
		//connection does not exist
		err.assign(-1, err.category());//TODO
	}
	return err;
}
//-----------------------------------------------------------------------------
bool Service::isEventStart(Event const&_revent){
	return _revent.id == d.config.event_start.id;
}
//-----------------------------------------------------------------------------
bool Service::isEventStop(Event const&_revent){
	return _revent.id == this->stopEvent().id;
}
//-----------------------------------------------------------------------------
void Service::connectionReceive(SocketDevice &_rsd){
	DynamicPointer<aio::Object>		objptr(new Connection(_rsd));
	solid::ErrorConditionT			err;
	ObjectUidT						conuid = d.config.scheduler().startObject(objptr, *this, d.config.event_start, err);
	
	idbgx(Debug::ipc, "receive connection ["<<conuid<<"] err = "<<err.message());
}
//-----------------------------------------------------------------------------
void Service::forwardResolveMessage(SessionUid const &_rssnuid, Event const&_revent){
	ResolveMessage 	*presolvemsg = ResolveMessage::cast(_revent.msgptr.get());
	ErrorConditionT	err;
	++presolvemsg->crtidx;
	if(presolvemsg->crtidx < presolvemsg->addrvec.size()){
		DynamicPointer<aio::Object>		objptr(new Connection(_rssnuid));
		
		Locker<Mutex>					lock(d.sessionMutex(_rssnuid.ssnidx));
		SessionStub						&rssn(d.sessiondq[_rssnuid.ssnidx]);
		ObjectUidT						conuid = d.config.scheduler().startObject(objptr, *this, _revent, err);
		idbgx(Debug::ipc, conuid<<" "<<err.message());
		if(!err){
			++rssn.conn_pending;
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


