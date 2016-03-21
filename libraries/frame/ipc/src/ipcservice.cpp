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
#include "frame/ipc/ipcerror.hpp"

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
typedef Queue<ObjectIdT>								ObjectIdQueueT;

enum{
	InnerLinkOrder = 0,
	InnerLinkAsync,
	InnerLinkCount
};

//-----------------------------------------------------------------------------

struct MessageStub: InnerNode<InnerLinkCount>{
	
	enum{
		CancelableFlag = 1
	};
	
	MessageStub(
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ResponseHandlerFunctionT &_rresponse_fnc,
		ulong _msgflags
	):	msgbundle(_rmsgptr, _msg_type_idx, _msgflags, _rresponse_fnc),
		unique(0), flags(0){}
	
	MessageStub(
		MessageStub && _rmsg
	):	InnerNode<InnerLinkCount>(std::move(_rmsg)),
		msgbundle(std::move(_rmsg.msgbundle)),
		msgid(_rmsg.msgid), objid(_rmsg.objid),
		unique(_rmsg.unique), flags(_rmsg.flags){}
	
	MessageStub():unique(0), flags(0){}
	
	bool isCancelable()const{
		return (flags & CancelableFlag) != 0;
	}
	
	void makeCancelable(){
		flags |= CancelableFlag;
	}
	
	void clear(){
		msgbundle.clear();
		msgid = MessageId();
		objid = ObjectIdT();
		++unique;
		flags = 0;
	}
	
	MessageBundle	msgbundle;
	MessageId		msgid;
	ObjectIdT		objid;
	uint32			unique;
	uint			flags;
};

//-----------------------------------------------------------------------------

using MessageVectorT = std::vector<MessageStub>;
using MessageOrderInnerListT = InnerList<MessageVectorT, InnerLinkOrder>;
using MessageCacheInnerListT = InnerList<MessageVectorT, InnerLinkOrder>;
using MessageAsyncInnerListT = InnerList<MessageVectorT, InnerLinkAsync>;

//-----------------------------------------------------------------------------

std::ostream& operator<<(std::ostream &_ros, const MessageOrderInnerListT &_rlst){
	size_t cnt = 0;
	_rlst.forEach(
		[&_ros, &cnt](size_t _idx, const MessageStub& _rmsg){
			_ros<<_idx<<' ';
			++cnt;
		}
	);
	cassert(cnt == _rlst.size());
	return _ros;
}

std::ostream& operator<<(std::ostream &_ros, const MessageAsyncInnerListT &_rlst){
	size_t cnt = 0;
	_rlst.forEach(
		[&_ros, &cnt](size_t _idx, const MessageStub& _rmsg){
			_ros<<_idx<<' ';
			++cnt;
		}
	);
	cassert(cnt == _rlst.size());
	return _ros;
}

//-----------------------------------------------------------------------------

struct ConnectionPoolStub{
	enum{
		ClosingFlag 					= 1,
		FastClosingFlag 				= 2,
		MainConnectionStoppingFlag		= 4,
		CleanOneShotMessagesFlag		= 8,
		CleanAllMessagesFlag			= 16,
		RestartFlag						= 32,
		MainConnectionActiveFlag		= 64,
	};
	
	uint32					unique;
	uint16					pending_connection_count;
	uint16					active_connection_count;
	uint16					stopping_connection_count;
	std::string				name;
	ObjectIdT 				main_connection_id;
	MessageVectorT			msgvec;
	MessageOrderInnerListT	msgorder_inner_list;
	MessageCacheInnerListT	msgcache_inner_list;
	MessageAsyncInnerListT	msgasync_inner_list;
	
	ObjectIdQueueT			conn_waitingq;
	
	uint8					flags;
	
	
	
	ConnectionPoolStub(
	):	unique(0), pending_connection_count(0), active_connection_count(0), stopping_connection_count(0),
		msgorder_inner_list(msgvec), msgcache_inner_list(msgvec), msgasync_inner_list(msgvec),
		flags(0){}
	
	ConnectionPoolStub(
		ConnectionPoolStub && _rconpool
	):	unique(_rconpool.unique), pending_connection_count(_rconpool.pending_connection_count),
		active_connection_count(_rconpool.active_connection_count),
		stopping_connection_count(_rconpool.stopping_connection_count),
		name(std::move(_rconpool.name)),
		main_connection_id(_rconpool.main_connection_id),
		msgvec(std::move(_rconpool.msgvec)),
		msgorder_inner_list(msgvec, _rconpool.msgorder_inner_list),
		msgcache_inner_list(msgvec, _rconpool.msgcache_inner_list),
		msgasync_inner_list(msgvec, _rconpool.msgasync_inner_list),
		conn_waitingq(std::move(_rconpool.conn_waitingq)),
		flags(_rconpool.flags){}
	
	void clear(){
		name.clear();
		main_connection_id = ObjectIdT();
		++unique;
		pending_connection_count = 0;
		active_connection_count = 0;
		stopping_connection_count = 0;
		while(conn_waitingq.size()){
			conn_waitingq.pop();
		}
		cassert(msgorder_inner_list.empty());
		cassert(msgasync_inner_list.empty());
		msgcache_inner_list.clear();
		msgorder_inner_list.clear();
		msgasync_inner_list.clear();
		msgvec.clear();
		msgvec.shrink_to_fit();
		flags = 0;
		cassert(msgorder_inner_list.check());
	}
	
	MessageId insertMessage(
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ResponseHandlerFunctionT &_rresponse_fnc,
		ulong _flags
	){
		size_t		idx;
		
		if(msgcache_inner_list.size()){
			idx = msgcache_inner_list.frontIndex();
			msgcache_inner_list.popFront();
		}else{
			idx = msgvec.size();
			msgvec.push_back(MessageStub{});
		}
		
		MessageStub	&rmsgstub{msgvec[idx]};
		
		rmsgstub.msgbundle = MessageBundle(_rmsgptr, _msg_type_idx, _flags, _rresponse_fnc);
		return MessageId(idx, rmsgstub.unique);
	}
	
	MessageId pushBackMessage(
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ResponseHandlerFunctionT &_rresponse_fnc,
		ulong _flags
	){
		const MessageId msgid = insertMessage(_rmsgptr, _msg_type_idx, _rresponse_fnc, _flags);
		msgorder_inner_list.pushBack(msgid.index);
		idbgx(Debug::ipc, "msgorder_inner_list "<<msgorder_inner_list);
		if(Message::is_asynchronous(_flags)){
			msgasync_inner_list.pushBack(msgid.index);
			idbgx(Debug::ipc, "msgasync_inner_list "<<msgasync_inner_list);
		}
		cassert(msgorder_inner_list.check());
		return msgid;
	}
	
	MessageId pushFrontMessage(
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ResponseHandlerFunctionT &_rresponse_fnc,
		ulong _flags
	){
		const MessageId msgid = insertMessage(_rmsgptr, _msg_type_idx, _rresponse_fnc, _flags);
		msgorder_inner_list.pushFront(msgid.index);
		idbgx(Debug::ipc, "msgorder_inner_list "<<msgorder_inner_list);
		if(Message::is_asynchronous(_flags)){
			msgasync_inner_list.pushFront(msgid.index);
			idbgx(Debug::ipc, "msgasync_inner_list "<<msgasync_inner_list);
		}
		cassert(msgorder_inner_list.check());
		return msgid;
	}
	
	MessageId reinsertFrontMessage(
		MessageId const &_rmsgid,
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ResponseHandlerFunctionT &_rresponse_fnc,
		ulong _flags
	){
		MessageStub	&rmsgstub{msgvec[_rmsgid.index]};
		
		cassert(rmsgstub.msgbundle.message_ptr.empty() and rmsgstub.unique == _rmsgid.unique);
		
		rmsgstub.msgbundle = MessageBundle(_rmsgptr, _msg_type_idx, _flags, _rresponse_fnc);
		
		msgorder_inner_list.pushFront(_rmsgid.index);
		
		idbgx(Debug::ipc, "msgorder_inner_list "<<msgorder_inner_list);
		
		if(Message::is_asynchronous(_flags)){
			msgasync_inner_list.pushFront(_rmsgid.index);
			idbgx(Debug::ipc, "msgasync_inner_list "<<msgasync_inner_list);
		}
		cassert(msgorder_inner_list.check());
		return _rmsgid;
	}
	
	
	void clearAndCacheMessage(const size_t _msg_idx){
		MessageStub	&rmsgstub{msgvec[_msg_idx]};
		rmsgstub.clear();
		msgcache_inner_list.pushBack(_msg_idx);
	}
	
	void cacheFrontMessage(){
		idbgx(Debug::ipc, "msgorder_inner_list "<<msgorder_inner_list);
		msgcache_inner_list.pushBack(msgorder_inner_list.popFront());
		
		cassert(msgorder_inner_list.check());
	}
	
	void popFrontMessage(){
		idbgx(Debug::ipc, "msgorder_inner_list "<<msgorder_inner_list);
		msgorder_inner_list.popFront();
		
		cassert(msgorder_inner_list.check());
	}
	
	void eraseMessage(const size_t _msg_idx){
		idbgx(Debug::ipc, "msgorder_inner_list "<<msgorder_inner_list);
		msgorder_inner_list.erase(_msg_idx);
		cassert(msgorder_inner_list.check());
	}
	void clearPopAndCacheMessage(const size_t _msg_idx){
		idbgx(Debug::ipc, "msgorder_inner_list "<<msgorder_inner_list);
		MessageStub	&rmsgstub{msgvec[_msg_idx]};
		
		msgorder_inner_list.erase(_msg_idx);
		
		if(Message::is_asynchronous(rmsgstub.msgbundle.message_flags)){
			msgasync_inner_list.erase(_msg_idx);
		}
		
		msgcache_inner_list.pushBack(_msg_idx);
		
		rmsgstub.clear();
		cassert(msgorder_inner_list.check());
	}
	
	void cacheMessageId(const size_t _msg_idx, const size_t _msg_uid){
		cassert(_msg_idx < msgvec.size());
		cassert(msgvec[_msg_idx].unique == _msg_uid);
		if(_msg_idx < msgvec.size() and msgvec[_msg_idx].unique == _msg_uid){
			msgvec[_msg_idx].clear();
			msgcache_inner_list.pushBack(_msg_idx);
		}
	}
	
	bool isMainConnectionStopping()const{
		return flags & MainConnectionStoppingFlag;
	}
	
	void setMainConnectionStopping(){
		flags |= MainConnectionStoppingFlag;
	}
	void resetMainConnectionStopping(){
		flags &= ~MainConnectionStoppingFlag;
	}
	
	bool isMainConnectionActive()const{
		return flags & MainConnectionActiveFlag;
	}
	
	void setMainConnectionActive(){
		flags |= MainConnectionActiveFlag;
	}
	void resetMainConnectionActive(){
		flags &= ~MainConnectionActiveFlag;
	}
	
	bool hasNoConnection()const{
		return pending_connection_count == 0 and active_connection_count == 0 and stopping_connection_count == 0;
	}
	
	bool isClosing()const{
		return flags & ClosingFlag;
	}
	
	void setClosing(){
		flags |= ClosingFlag;
	}
	bool isFastClosing()const{
		return flags & FastClosingFlag;
	}
	
	void setFastClosing(){
		flags |= FastClosingFlag;
	}
	
	bool isServerSide()const{
		return name.empty();
	}
	
	bool isCleaningOneShotMessages()const{
		return flags & CleanOneShotMessagesFlag;
	}
	
	void setCleaningOneShotMessages(){
		flags |= CleanOneShotMessagesFlag;
	}
	
	void resetCleaningOneShotMessages(){
		flags &= ~CleanOneShotMessagesFlag;
	}
	
	bool isCleaningAllMessages()const{
		return flags & CleanAllMessagesFlag;
	}
	
	void setCleaningAllMessages(){
		flags |= CleanAllMessagesFlag;
	}
	
	void resetCleaningAllMessages(){
		flags &= ~CleanAllMessagesFlag;
	}
	
	bool isRestarting()const{
		return flags & RestartFlag;
	}
	
	void setRestarting(){
		flags |= RestartFlag;
	}
	
	bool hasNoMessage()const{
		if(msgorder_inner_list.empty()){
			return true;
		}else{
			return msgorder_inner_list.front().msgbundle.message_ptr.empty();
		}
	}
	
	bool hasAnyMessage()const{
		return not hasNoMessage();
	}
	
	bool isMainConnection(ObjectIdT const &_robjuid)const{
		return main_connection_id == _robjuid;
	}
	
	bool isLastConnection()const{
		return (static_cast<size_t>(pending_connection_count) + active_connection_count) == 1;
	}
};

//-----------------------------------------------------------------------------

typedef std::deque<ConnectionPoolStub>					ConnectionPoolDequeT;
typedef Stack<size_t>									SizeStackT;

//-----------------------------------------------------------------------------

struct Service::Data{
	Data(Service &_rsvc): pmtxarr(nullptr), mtxsarrcp(0), config(ServiceProxy(_rsvc)){}
	
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
):BaseT(_rm), d(*(new Data(*this))){}
	
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
	
	ErrorConditionT error;
	
	ServiceProxy	sp(*this);
	
	error = _rcfg.check();
	
	if(error) return error;
	
	_rcfg.message_register_fnc(sp);
	
	d.config.reset(_rcfg);
	
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
			
			ObjectIdT						conuid = d.config.scheduler().startObject(objptr, *this, generic_event_category.event(GenericEvents::Start), error);
			
			if(error){
				return error;
			}
		}else{
			error.assign(-1, error.category());
			return error;
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

struct OnRelsolveF{
	Manager		&rm;
	ObjectIdT	objuid;
	Event 		event;
	
	OnRelsolveF(
		Manager &_rm,
		const ObjectIdT &_robjuid,
		const Event &_revent
	):rm(_rm), objuid(_robjuid), event(_revent){}
	
	void operator()(AddressVectorT &_raddrvec){
		idbgx(Debug::ipc, "OnResolveF(addrvec of size "<<_raddrvec.size()<<")");
		event.any().reset(ResolveMessage(_raddrvec));
		rm.notify(objuid, std::move(event));
	}
};

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
ErrorConditionT Service::doSendMessage(
	const char *_recipient_name,
	const RecipientId	&_rrecipient_id_in,
	MessagePointerT &_rmsgptr,
	ResponseHandlerFunctionT &_rresponse_fnc,
	RecipientId *_precipient_id_out,
	MessageId *_pmsgid_out,
	ulong _flags
){
	solid::ErrorConditionT		error;
	size_t						pool_idx;
	uint32						unique;
	bool						check_uid = false;
	const size_t				msg_type_idx = tm.index(_rmsgptr.get());
	
	
	if(msg_type_idx == 0){
		edbgx(Debug::ipc, this<<" message type not registered");
		error.assign(-1, error.category());//TODO:type not registered
		return error;
	}
	
	Locker<Mutex>				lock(d.mtx);
	
	if(_rrecipient_id_in.isValidConnection()){
		cassert(_precipient_id_out == nullptr);
		//directly send the message to a connection object
		return doNotifyConnectionPushMessage(
			_rrecipient_id_in,
			_rmsgptr,
			msg_type_idx,
			_rresponse_fnc,
			_pmsgid_out,
			_flags
		);
	}else if(_recipient_name){
		NameMapT::const_iterator	it = d.namemap.find(_recipient_name);
		
		if(it != d.namemap.end()){
			pool_idx = it->second;
		}else{
			if(d.config.isServerOnly()){
				edbgx(Debug::ipc, this<<" request for name resolve for a server only configuration");
				error.assign(-1, error.category());//TODO: server only
				return error;
			}
			
			return this->doSendMessageToNewPool(
				_recipient_name, _rmsgptr, msg_type_idx,
				_rresponse_fnc, _precipient_id_out, _pmsgid_out, _flags
			);
		}
	}else if(
		_rrecipient_id_in.poolid.index < d.conpooldq.size()
	){
		//we cannot check the uid right now because we need a lock on the session's mutex
		check_uid = true;
		pool_idx = _rrecipient_id_in.poolid.index;
		unique = _rrecipient_id_in.poolid.unique;
	}else{
		edbgx(Debug::ipc, this<<" recipient does not exist");
		error.assign(-1, error.category());//TODO: session does not exist
		return error;
	}
	
	Locker<Mutex>			lock2(d.connectionPoolMutex(pool_idx));
	ConnectionPoolStub 		&rconpool(d.conpooldq[pool_idx]);
	
	if(check_uid && rconpool.unique != unique){
		//failed uid check
		edbgx(Debug::ipc, this<<" connection pool does not exist");
		error.assign(-1, error.category());//TODO: connection pool does not exist
		return error;
	}
	
	if(rconpool.isClosing()){
		edbgx(Debug::ipc, this<<" connection pool is stopping");
		error.assign(-1, error.category());//TODO: connection pool is stopping
		return error;
	}
	
	if(_precipient_id_out){
		_precipient_id_out->poolid = ConnectionPoolId(pool_idx, rconpool.unique);
	}
	
	//At this point we can fetch the message from user's pointer
	//because from now on we can call complete on the message
	const MessageId msgid = rconpool.pushBackMessage(_rmsgptr, msg_type_idx, _rresponse_fnc, _flags);
	
	if(_pmsgid_out){
		
		MessageStub			&rmsgstub(rconpool.msgvec[msgid.index]);
		
		rmsgstub.makeCancelable();
		
		*_pmsgid_out = msgid;
		idbgx(Debug::ipc, this<<" set message id to "<<*_pmsgid_out);
	}
	
	bool success = false;
	
	if(
		Message::is_synchronous(_flags) and
		rconpool.main_connection_id.isValid()
	){
		success = manager().notify(
			rconpool.main_connection_id,
			Connection::eventNewMessage()
		);
		if(not success){
			wdbgx(Debug::ipc, this<<" failed sending message to synch connection "<<rconpool.main_connection_id);
			rconpool.main_connection_id = ObjectIdT();
		}
	}
	
	if(not success){
		success = doTryNotifyPoolWaitingConnection(pool_idx);
	}
	
	if(not success){
		doTryCreateNewConnectionForPool(pool_idx, error);
		error.clear();
	}
	
	if(not success){
		wdbgx(Debug::ipc, this<<" no connection notified about the new message");
	}
	
	return error;
}

//-----------------------------------------------------------------------------

ErrorConditionT Service::doSendMessageToNewPool(
	const char *_recipient_name,
	MessagePointerT &_rmsgptr,
	const size_t _msg_type_idx,
	ResponseHandlerFunctionT &_rresponse_fnc,
	RecipientId *_precipient_id_out,
	MessageId *_pmsgid_out,
	ulong _flags
){
	solid::ErrorConditionT		error;
	size_t						pool_idx;
	
	if(d.conpoolcachestk.size()){
		pool_idx = d.conpoolcachestk.top();
		d.conpoolcachestk.pop();
	}else{
		pool_idx = this->doPushNewConnectionPool();
	}
	
	Locker<Mutex>					lock2(d.connectionPoolMutex(pool_idx));
	ConnectionPoolStub 				&rconpool(d.conpooldq[pool_idx]);
	
	rconpool.name = _recipient_name;
	
	if(not doTryCreateNewConnectionForPool(pool_idx, error)){
		edbgx(Debug::ipc, this<<" Starting Session: "<<error.message());
		rconpool.clear();
		d.conpoolcachestk.push(pool_idx);
		return error;
	}
	
	d.namemap[rconpool.name.c_str()] = pool_idx;
	
	MessageId msgid = rconpool.pushBackMessage(_rmsgptr, _msg_type_idx, _rresponse_fnc, _flags);
	
	if(_precipient_id_out){
		_precipient_id_out->poolid = ConnectionPoolId(pool_idx, rconpool.unique);
	}
	
	if(_pmsgid_out){
		MessageStub	&rmsgstub = rconpool.msgvec[msgid.index];
		
		*_pmsgid_out = MessageId(msgid.index, rmsgstub.unique);
		rmsgstub.makeCancelable();
	}
	
	return error;
}
//-----------------------------------------------------------------------------
bool Service::doTryPushMessageToConnection(
	Connection &_rcon,
	ObjectIdT const &_robjuid,
	const size_t _pool_idx,
	const size_t msg_idx,
	bool &_rpushed_synchronous_message
){
	ConnectionPoolStub 	&rconpool(d.conpooldq[_pool_idx]);
	MessageStub			&rmsgstub = rconpool.msgvec[msg_idx];
	const bool			message_is_synchronous = Message::is_asynchronous(rmsgstub.msgbundle.message_flags);
	const bool			message_is_null = rmsgstub.msgbundle.message_ptr.empty();
	
	cassert(Message::is_canceled(rmsgstub.msgbundle.message_flags));
	
	
	if(rmsgstub.isCancelable()){
		rmsgstub.objid = _robjuid;
		if(
			_rcon.tryPushMessage(rmsgstub.msgbundle, rmsgstub.msgid, MessageId(msg_idx, rmsgstub.unique))
			and not message_is_null
		){
			rconpool.msgorder_inner_list.erase(msg_idx);
			
			if(message_is_synchronous){
				_rpushed_synchronous_message = true;
			}else{
				rconpool.msgasync_inner_list.erase(msg_idx);
			}
		}else{
			return false;
		}
	}else{
		if(_rcon.tryPushMessage(rmsgstub.msgbundle) and not message_is_null){
			rconpool.msgorder_inner_list.erase(msg_idx);
			
			if(message_is_synchronous){
				_rpushed_synchronous_message = true;
			}else{
				rconpool.msgasync_inner_list.erase(msg_idx);
			}
			
			rconpool.msgcache_inner_list.pushBack(msg_idx);
			rmsgstub.clear();
		}else{
			return false;
		}
	}
	return true;
}
//-----------------------------------------------------------------------------
void Service::pollPoolForUpdates(
	Connection &_rcon,
	ObjectIdT const &_robjuid,
	PoolStatus &_rpool_status
){
	const size_t			pool_idx = _rcon.poolId().index;
	Locker<Mutex>			lock2(d.connectionPoolMutex(pool_idx));
	ConnectionPoolStub 		&rconpool(d.conpooldq[pool_idx]);
	
	cassert(rconpool.unique == _rcon.poolId().unique);
	if(rconpool.unique != _rcon.poolId().unique) return;
	
	if(rconpool.isMainConnectionStopping() and rconpool.main_connection_id != _robjuid){
		idbgx(Debug::ipc, this<<' '<<&_rcon<<" switch message main connection from "<<rconpool.main_connection_id<<" to "<<_robjuid);
		rconpool.main_connection_id = _robjuid;
		rconpool.resetMainConnectionStopping();
	}
	
	if(_rcon.hasCompletingMessages()){
		//free-up positions of the completed messages
		auto free_up_fnc = [&rconpool](MessageId const &_rmsgid){
			rconpool.cacheMessageId(_rmsgid.index, _rmsgid.unique);
		};
		_rcon.visitCompletingMessages(free_up_fnc);
	}
	
	if(rconpool.isFastClosing()){
		idbgx(Debug::ipc, this<<' '<<&_rcon<<" pool is FastClosing");
		_rpool_status = PoolStatus::FastClosing;
		return;
	}
	_rpool_status = PoolStatus::Open;
	
	idbgx(Debug::ipc, this<<' '<<&_rcon<<" messages in pool: "<<rconpool.msgorder_inner_list.size());
	
	const bool 				connection_can_handle_synchronous_messages{
		_robjuid == rconpool.main_connection_id
	};
	
	bool					connection_may_handle_more_messages = true;
	bool					pushed_synchronous_message = false;
	
	bool					is_message_queue_empty = true;
	
	if(connection_can_handle_synchronous_messages){
		//use the order inner queue
		is_message_queue_empty = rconpool.msgorder_inner_list.size() == 0;
		if(not is_message_queue_empty and connection_may_handle_more_messages){
			connection_may_handle_more_messages = doTryPushMessageToConnection(
				_rcon,
				_robjuid,
				pool_idx,
				rconpool.msgorder_inner_list.frontIndex(),
				pushed_synchronous_message
			);
		}
	}else{
		//use the async inner queue
		is_message_queue_empty = rconpool.msgasync_inner_list.size() == 0;
		if(not is_message_queue_empty and connection_may_handle_more_messages){
			connection_may_handle_more_messages = connection_may_handle_more_messages = doTryPushMessageToConnection(
				_rcon,
				_robjuid,
				pool_idx,
				rconpool.msgasync_inner_list.frontIndex(),
				pushed_synchronous_message
			);
		}
	}
	
	if(pushed_synchronous_message and rconpool.synchronous_connection_id.isInvalid()){
		rconpool.synchronous_connection_id = _robjuid;
	}
	
	//connection WILL not check for new message if it is full
	//connection WILL check for new messages when become not full
	//connection may not check for new messages if (isInPoolWaitingQueue)
	
	if(connection_may_handle_more_messages and not _rcon.isInPoolWaitingQueue() and is_message_queue_empty){
		rconpool.conn_waitingq.push(_robjuid);
		_rcon.setInPoolWaitingQueue();
	}
	
	if(
		not connection_may_handle_more_messages and 
		not is_message_queue_empty
	){
		doTryNotifyPoolWaitingConnection(_rcon.poolId().index);
	}
}
//-----------------------------------------------------------------------------
bool Service::doTryNotifyPoolWaitingConnection(const size_t _pool_index){
	ConnectionPoolStub 	&rconpool(d.conpooldq[_pool_index]);
	bool 				success = false;
	
	//we were not able to handle the message, try notify another connection
	while(not success and rconpool.conn_waitingq.size()){
		//a connection is waiting for something to send
		ObjectIdT 		objuid = rconpool.conn_waitingq.front();
		
		rconpool.conn_waitingq.pop();
		
		
		success = manager().notify(
			objuid,
			Connection::eventNewMessage()
		);
	}
	return success;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doNotifyConnectionPushMessage(
	const RecipientId	&_rrecipient_id_in,
	MessagePointerT &_rmsgptr,
	const size_t _msg_type_idx,
	ResponseHandlerFunctionT &_rresponse_fnc,
	MessageId *_pmsgid_out,
	ulong _flags
){
	//d.mtx must be locked
	
	if(_rrecipient_id_in.isValidPool()){
		return error_connection_inexistent;//TODO: more explicit error
	}
	
	const size_t					pool_idx = _rrecipient_id_in.poolId().index;
	
	if(pool_idx >= d.conpooldq.size() or d.conpooldq[pool_idx].unique != _rrecipient_id_in.poolId().unique){
		return error_connection_inexistent;//TODO: more explicit error
	}
	
	Locker<Mutex>					lock2(d.connectionPoolMutex(pool_idx));
	ConnectionPoolStub				&rconpool = d.conpooldq[pool_idx];
	solid::ErrorConditionT			error;
	const bool						is_server_side_pool = rconpool.isServerSide();//unnamed pool has a single connection
	
	MessageId 						msgid;
	
	bool							success = false;
	
	if(is_server_side_pool){
		//for a server pool we want to enque messages in the pool
		//
		msgid = rconpool.pushBackMessage(_rmsgptr, _msg_type_idx, _rresponse_fnc, _flags | Message::OneShotSendFlagE);
		success = manager().notify(
			_rrecipient_id_in.connectionId(),
			Connection::eventNewMessage()
		);
	}else{
		msgid = rconpool.insertMessage(_rmsgptr, _msg_type_idx, _rresponse_fnc, _flags | Message::OneShotSendFlagE);
		success = manager().notify(
			_rrecipient_id_in.connectionId(),
			Connection::eventNewMessage(msgid)
		);
	}
	
	if(success){
		if(_pmsgid_out){
			*_pmsgid_out = msgid;
			MessageStub	&rmsgstub = rconpool.msgvec[msgid.index];
			rmsgstub.makeCancelable();
		}
	}else if(is_server_side_pool){
		rconpool.clearPopAndCacheMessage(msgid.index);
		error = error_connection_inexistent;//TODO: more explicit error
	}else{
		rconpool.clearAndCacheMessage(msgid.index);
		error = error_connection_inexistent;//TODO: more explicit error
	}
	
	return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doDelayCloseConnectionPool(
	RecipientId const &_rrecipient_id, 
	ResponseHandlerFunctionT &_rresponse_fnc
){
	ErrorConditionT			error;
	const size_t			pool_idx = _rrecipient_id.poolId().index;
	Locker<Mutex>			lock(d.mtx);
	Locker<Mutex>			lock2(d.connectionPoolMutex(pool_idx));
	ConnectionPoolStub 		&rconpool(d.conpooldq[pool_idx]);
	
	if(rconpool.unique != _rrecipient_id.poolId().unique){
		return error_connection_inexistent;
	}
	
	rconpool.setClosing();
	
	MessagePointerT		empty_msg_ptr;
	
	const MessageId		msgid = rconpool.pushBackMessage(empty_msg_ptr, 0, _rresponse_fnc, 0);
	(void)msgid;
	
	//notify all waiting connections about the new message
	while(rconpool.conn_waitingq.size()){
		ObjectIdT 		objuid = rconpool.conn_waitingq.front();
		
		rconpool.conn_waitingq.pop();
		
		manager().notify(
			objuid,
			Connection::eventNewMessage()
		);
	}
	
	return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doForceCloseConnectionPool(
	RecipientId const &_rrecipient_id, 
	ResponseHandlerFunctionT &_rresponse_fnc
){
	ErrorConditionT			error;
	const size_t			pool_idx = _rrecipient_id.poolId().index;
	Locker<Mutex>			lock(d.mtx);
	Locker<Mutex>			lock2(d.connectionPoolMutex(pool_idx));
	ConnectionPoolStub 		&rconpool(d.conpooldq[pool_idx]);
	
	if(rconpool.unique != _rrecipient_id.poolId().unique){
		return error_connection_inexistent;
	}
	
	rconpool.setClosing();
	rconpool.setFastClosing();
	
	MessagePointerT		empty_msg_ptr;
	
	const MessageId		msgid = rconpool.pushBackMessage(empty_msg_ptr, 0, _rresponse_fnc, 0);
	(void)msgid;
	//no reason to cancel all messages - they'll be handled on connection stop.
	//notify all waiting connections about the new message
	while(rconpool.conn_waitingq.size()){
		ObjectIdT 		objuid = rconpool.conn_waitingq.front();
		
		rconpool.conn_waitingq.pop();
		
		manager().notify(
			objuid,
			Connection::eventNewMessage()
		);
	}
	
	return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::cancelMessage(RecipientId const &_rrecipient_id, MessageId const &_rmsguid){
	ErrorConditionT			error;
	const size_t			pool_idx = _rrecipient_id.poolId().index;
	//Locker<Mutex>			lock(d.mtx);
	Locker<Mutex>			lock2(d.connectionPoolMutex(pool_idx));
	ConnectionPoolStub 		&rconpool(d.conpooldq[pool_idx]);
	
	if(rconpool.unique != _rrecipient_id.poolId().unique){
		return error_connection_inexistent;
	}
	
	if(_rmsguid.index < rconpool.msgvec.size() and rconpool.msgvec[_rmsguid.index].unique == _rmsguid.unique){
		MessageStub	&rmsgstub = rconpool.msgvec[_rmsguid.index];
		bool 		success = false;
		
		if(Message::is_canceled(rmsgstub.msgbundle.message_flags)){
			error.assign(-1, error.category());//message already canceled
		}else{
			
			if(rmsgstub.objid.isValid()){//message handled by a connection
				
				cassert(rmsgstub.msgbundle.message_ptr.empty());
				
				rmsgstub.msgbundle.message_flags |= Message::CanceledFlagE;
				
				success = manager().notify(
					rmsgstub.objid,
					Connection::eventCancelLocalMessage(rmsgstub.msgid)
				);
				
				if(not success){
					rmsgstub.msgid = MessageId();
					rmsgstub.objid = ObjectIdT();
					THROW_EXCEPTION("Lost message");
					error.assign(-1, error.category());//message lost
				}
			}
			
			if(not rmsgstub.msgbundle.message_ptr.empty()){
				rmsgstub.msgbundle.message_flags |= Message::CanceledFlagE;
				
				success = manager().notify(
					rconpool.main_connection_id,
					Connection::eventCancelPoolMessage(_rmsguid)
				);
				
				if(success){
					//erase/unlink the message from any list 
					if(rconpool.msgorder_inner_list.isLinked(_rmsguid.index)){
						rconpool.msgorder_inner_list.erase(_rmsguid.index);
						if(Message::is_asynchronous(rmsgstub.msgbundle.message_flags)){
							cassert(rconpool.msgasync_inner_list.isLinked(_rmsguid.index));
							rconpool.msgasync_inner_list.erase(_rmsguid.index);
						}
					}
				}else{
					THROW_EXCEPTION("Message Cancel connection not available");
				}
			}
		}
	}else{
		error.assign(-1, error.category());//message does not exist
	}
	return error;
	
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doPostActivateConnection(
	RecipientId const &_rrecipient_id,
	ActivateConnectionMessageFactoryFunctionT &&_rmsgfactory
){
	solid::ErrorConditionT	error;
	bool 					success = manager().notify(
		_rrecipient_id.connectionId(),
		Connection::eventActivate(std::move(_rmsgfactory))
	);
	
	if(not success){
		wdbgx(Debug::ipc, this<<" failed sending activate event to "<<_rrecipient_id.connectionId());
		error = error_connection_inexistent;
	}
	
	return error;
}
//-----------------------------------------------------------------------------
bool Service::fetchMessage(Connection &_rcon, ObjectIdT const &_robjuid, MessageId const &_rmsg_id){
	Locker<Mutex>			lock2(d.connectionPoolMutex(_rcon.poolId().index));
	const size_t 			pool_index = _rcon.poolId().index;
	ConnectionPoolStub 		&rconpool(d.conpooldq[pool_index]);
	
	if(
		_rmsg_id.index < rconpool.msgvec.size() and
		rconpool.msgvec[_rmsg_id.index].unique == _rmsg_id.unique
	){
		bool pushed_synchronous_message = false;
		return (_rcon, _robjuid, pool_index, _rmsg_id.index, pushed_synchronous_message);
	}
	return false;
}
//-----------------------------------------------------------------------------
bool Service::fetchCanceledMessage(Connection const &_rcon, MessageId const &_rmsg_id, MessageBundle &_rmsg_bundle){
	Locker<Mutex>			lock2(d.connectionPoolMutex(_rcon.poolId().index));
	const size_t 			pool_index = _rcon.poolId().index;
	ConnectionPoolStub 		&rconpool(d.conpooldq[pool_index]);
	if(
		_rmsg_id.index < rconpool.msgvec.size() and
		rconpool.msgvec[_rmsg_id.index].unique == _rmsg_id.unique
	){
		MessageStub			&rmsgstub = rconpool.msgvec[_rmsg_id.index];
		cassert(Message::is_canceled(rmsgstub.msgbundle.message_flags));
		cassert(rconpool.msgorder_inner_list.isLinked(_rmsg_id.index));
		_rmsg_bundle = std::move(rmsgstub.msgbundle);
		rmsgstub.clear();
		rconpool.msgcache_inner_list.pushBack(_rmsg_id.index);
		return true;
	}
	return false;
}
//-----------------------------------------------------------------------------
bool Service::connectionStopping(
	Connection &_rcon, ObjectIdT const &_robjuid,
	ulong &_rseconds_to_wait,
	MessageId &_rmsg_id, MessageBundle &_rmsg_bundle
){
	const size_t 			pool_index = _rcon.poolId().index;
	Locker<Mutex>			lock2(d.connectionPoolMutex(pool_index));
	ConnectionPoolStub 		&rconpool(d.conpooldq[pool_index]);
	
	_rseconds_to_wait = 0;
	_rmsg_bundle.clear();
	
	cassert(rconpool.unique == _rcon.poolId().unique);
	if(rconpool.unique != _rcon.poolId().unique) return false;
	
	idbgx(Debug::ipc, this<<' '<<pool_index<<" active_connection_count "<<rconpool.active_connection_count<<" pending_connection_count "<<rconpool.pending_connection_count);
	
	if(not rconpool.isMainConnection(_robjuid)){
		return doConnectionStoppingNotMain(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle);
	}else if(not rconpool.isLastConnection()){
		return doConnectionStoppingNotLast(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle);
	}else if(rconpool.isCleaningOneShotMessages()){
		return doConnectionStoppingCleanOneShot(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle);
	}else if(rconpool.isCleaningAllMessages()){
		return doConnectionStoppingCleanAll(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle);
	}else if(not rconpool.isFastClosing() and not rconpool.isServerSide()){
		return doConnectionStoppingPrepareCleanOneShot(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle);
	}else{
		return doConnectionStoppingPrepareCleanAll(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle);
	}
}
//-----------------------------------------------------------------------------
bool Service::doConnectionStoppingNotMain(
	Connection &_rcon, ObjectIdT const &_robjuid,
	ulong &_rseconds_to_wait,
	MessageId &_rmsg_id, MessageBundle &_rmsg_bundle
){
	const size_t 			pool_index = _rcon.poolId().index;
	ConnectionPoolStub 		&rconpool(d.conpooldq[pool_index]);
	
	if(_rcon.isActive()){
		--rconpool.active_connection_count;
	}else{
		cassert(not _rcon.isServer());
		--rconpool.pending_connection_count;
	}
	
	++rconpool.stopping_connection_count;
	
	if(rconpool.isLastConnection() and rconpool.isMainConnectionStopping()){
		manager().notify(
			rconpool.main_connection_id,
			Connection::eventStopping()
		);
	}
	
	if(not rconpool.isFastClosing()){
		doFetchResendableMessagesFromConnection(_rcon);
		doTryCreateNewConnectionForPool(pool_index);
	}
	
	return true;//the connection can call connectionStop asap
}
//-----------------------------------------------------------------------------
bool Service::doConnectionStoppingNotLast(
	Connection &_rcon, ObjectIdT const &/*_robjuid*/,
	ulong &_rseconds_to_wait,
	MessageId &/*_rmsg_id*/, MessageBundle &/*_rmsg_bundle*/
){
	const size_t 			pool_index = _rcon.poolId().index;
	ConnectionPoolStub 		&rconpool(d.conpooldq[pool_index]);
	
	//is main connection but is not the last one
	_rseconds_to_wait = -1;
	rconpool.setMainConnectionStopping();
		
	if(not rconpool.isFastClosing()){
		doFetchResendableMessagesFromConnection(_rcon);
	}
	
	return false;//connection will wait indefinitely for stoppingEvent
}
//-----------------------------------------------------------------------------
bool Service::doConnectionStoppingCleanOneShot(
	Connection &_rcon, ObjectIdT const &_robjuid,
	ulong &_rseconds_to_wait,
	MessageId &_rmsg_id, MessageBundle &_rmsg_bundle
){
	return false;
}
//-----------------------------------------------------------------------------
bool Service::doConnectionStoppingCleanAll(
	Connection &_rcon, ObjectIdT const &_robjuid,
	ulong &_rseconds_to_wait,
	MessageId &_rmsg_id, MessageBundle &_rmsg_bundle
){
	const size_t 			pool_index = _rcon.poolId().index;
	ConnectionPoolStub 		&rconpool(d.conpooldq[pool_index]);
	
	if(rconpool.msgcache_inner_list.size()){
		const size_t msgidx = rconpool.msgorder_inner_list.frontIndex();
		{
			MessageStub &rmsgstub = rconpool.msgorder_inner_list.front();
			_rmsg_bundle = std::move(rmsgstub.msgbundle);
			_rmsg_id = MessageId(msgidx, rmsgstub.unique);
		}
		rconpool.clearPopAndCacheMessage(msgidx);
		return false;
	}else{
		rconpool.resetCleaningAllMessages();
		return true;//TODO: maybe we can return false
	}
}
//-----------------------------------------------------------------------------
bool Service::doConnectionStoppingPrepareCleanOneShot(
	Connection &_rcon, ObjectIdT const &/*_robjuid*/,
	ulong &/*_rseconds_to_wait*/,
	MessageId &/*_rmsg_id*/, MessageBundle &/*_rmsg_bundle*/
){
	//the last connection
	const size_t 			pool_index = _rcon.poolId().index;
	ConnectionPoolStub 		&rconpool(d.conpooldq[pool_index]);
	
	doFetchResendableMessagesFromConnection(_rcon);
	
	if(rconpool.hasAnyMessage()){
		MessagePointerT				msg_ptr;
		ResponseHandlerFunctionT	response_fnc;
		
		rconpool.pushBackMessage(msg_ptr, -1, response_fnc, 0);//a sentinel
		rconpool.setCleaningOneShotMessages();
		
		return false;
	}else{
		return true;//the connection can call connectionStop asap
	}
}
//-----------------------------------------------------------------------------
bool Service::doConnectionStoppingPrepareCleanAll(
	Connection &_rcon, ObjectIdT const &/*_robjuid*/,
	ulong &/*_rseconds_to_wait*/,
	MessageId &/*_rmsg_id*/, MessageBundle &/*_rmsg_bundle*/
){
	//the last connection - fast closing or server side
	const size_t 			pool_index = _rcon.poolId().index;
	ConnectionPoolStub 		&rconpool(d.conpooldq[pool_index]);
	
	rconpool.setCleaningAllMessages();
	return false;
}
//-----------------------------------------------------------------------------
void Service::connectionStop(Connection const &_rcon){
	idbgx(Debug::ipc, this<<' '<<&_rcon<<" "<<_robjuid);
	const size_t 			pool_index = _rcon.poolId().index;
	Locker<Mutex>			lock(d.mtx);
	Locker<Mutex>			lock2(d.connectionPoolMutex(pool_index));
	ConnectionPoolStub 		&rconpool(d.conpooldq[pool_index]);
	
	cassert(rconpool.unique == _rcon.poolId().unique);
	if(rconpool.unique != _rcon.poolId().unique) return;
	
	--rconpool.stopping_connection_count;
	
	if(rconpool.hasNoConnection()){
		
		cassert(rconpool.msgorder_inner_list.empty());
		d.conpoolcachestk.push(pool_index);
		
		rconpool.clear();
	}
}
//-----------------------------------------------------------------------------
bool Service::doTryCreateNewConnectionForPool(const size_t _pool_index, ErrorConditionT &_rerror){
	ConnectionPoolStub	&rconpool(d.conpooldq[_pool_index]);
	
	if(
		rconpool.active_connection_count < d.config.pool_max_connection_count /*and
		rconpool.pending_resolve_count < d.config.pool_max_connection_count*///TODO
	){
		
		idbgx(Debug::ipc, this<<" try create new connection in pool "<<rconpool.active_connection_count<<" pending connections "<< rconpool.pending_connection_count);
		
		DynamicPointer<aio::Object>		objptr(new Connection(ConnectionPoolId(_pool_index, rconpool.unique)));
		ObjectIdT						conuid = d.config.scheduler().startObject(objptr, *this, generic_event_category.event(GenericEvents::Start), _rerror);
		
		if(!_rerror){
			
			idbgx(Debug::ipc, this<<" Success starting Connection Pool object: "<<conuid.index<<','<<conuid.unique);

			ResolveCompleteFunctionT		cbk(OnRelsolveF(manager(), conuid, Connection::eventResolve));
			
			d.config.name_resolve_fnc(rconpool.name, cbk);
			
			if(rconpool.main_connection_id.isInvalid()){
				rconpool.main_connection_id = conuid;
			}
			
			++rconpool.pending_connection_count;
		}else{
			//there must be at least one connection to handle the message:
			cassert(rconpool.pending_connection_count + rconpool.active_connection_count);
		}
	}
	return false;
}
//-----------------------------------------------------------------------------
void Service::doFetchResendableMessagesFromConnection(
	Connection &_rcon
){
	const size_t 		pool_index = _rcon.poolId().index;
	ConnectionPoolStub	&rconpool(d.conpooldq[pool_index]);

	//the final front message in msgorder_inner_list should be the oldest one from connection
	_rcon.forEveryMessagesNewerToOlder(
		[this](
			ConnectionPoolId &_rconpoolid,
			MessageBundle &_rmsgbundle,
			MessageId const &_rmsgid
		){
			this->pushFrontMessageToPool(_rconpoolid, _rmsgbundle, _rmsgid);
		}
	);
}
//-----------------------------------------------------------------------------
void Service::pushFrontMessageToPool(
	ConnectionPoolId &_rconpoolid,
	MessageBundle &_rmsgbundle,
	MessageId const &_rmsgid
){
	ConnectionPoolStub	&rconpool(d.conpooldq[_rconpoolid.index]);
	
	cassert(rconpool.unique == _rconpoolid.unique);
	
	if(
		Message::is_idempotent(_rmsgbundle.message_flags) or 
		(/*not Message::is_started_send(_flags) and*/ not Message::is_done_send(_rmsgbundle.message_flags))
	){
		_rmsgbundle.message_flags &= ~(Message::DoneSendFlagE | Message::StartedSendFlagE);
		if(_rmsgid.isInvalid()){
			rconpool.pushFrontMessage(
				_rmsgbundle.message_ptr,
				_rmsgbundle.message_type_id,
				_rmsgbundle.response_fnc,
				_rmsgbundle.message_flags
			);
		}else{
			rconpool.reinsertFrontMessage(
				_rmsgid,
				_rmsgbundle.message_ptr,
				_rmsgbundle.message_type_id,
				_rmsgbundle.response_fnc,
				_rmsgbundle.message_flags
			);
		}
	}
}
//-----------------------------------------------------------------------------
void Service::acceptIncomingConnection(SocketDevice &_rsd){
	size_t	pool_idx;
	
	Locker<Mutex>				lock(d.mtx);
	
	if(d.conpoolcachestk.size()){
		pool_idx = d.conpoolcachestk.top();
		d.conpoolcachestk.pop();
	}else{
		pool_idx = this->doPushNewConnectionPool();
	}
	
	{
		Locker<Mutex>					lock2(d.connectionPoolMutex(pool_idx));
		
		ConnectionPoolStub 				&rconpool(d.conpooldq[pool_idx]);
		
		DynamicPointer<aio::Object>		objptr(new Connection(_rsd, ConnectionPoolId(pool_idx, rconpool.unique)));
		solid::ErrorConditionT			error;
		
		ObjectIdT						con_id = d.config.scheduler().startObject(
			objptr, *this, generic_event_category.event(GenericEvents::Start), error
		);
		
		idbgx(Debug::ipc, this<<" receive connection ["<<con_id<<"] error = "<<error.message());
		
		if(error){
			cassert(con_id.isInvalid());
			rconpool.clear();
			d.conpoolcachestk.push(pool_idx);
		}else{
			cassert(con_id.isValid());
			++rconpool.pending_connection_count;
			rconpool.main_connection_id = con_id;
		}
	}
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
void Service::onConnectionStop(ConnectionContext &_rconctx, ErrorConditionT const &_rerror){
	configuration().connection_stop_fnc(_rconctx, _rerror);
}
//-----------------------------------------------------------------------------
void Service::forwardResolveMessage(ConnectionPoolId const &_rconpoolid, Event &_revent){

	ResolveMessage 	*presolvemsg = _revent.any().cast<ResolveMessage>();
	ErrorConditionT	error;
	
	++presolvemsg->crtidx;
	
	if(presolvemsg->crtidx < presolvemsg->addrvec.size()){
		DynamicPointer<aio::Object>		objptr(new Connection(_rconpoolid));
		
		Locker<Mutex>					lock(d.connectionPoolMutex(_rconpoolid.index));
		ConnectionPoolStub				&rconpool(d.conpooldq[_rconpoolid.index]);
		
		ObjectIdT	conuid = d.config.scheduler().startObject(objptr, *this, std::move(_revent), error);
		
		if(!error){
			++rconpool.pending_connection_count;
			++rconpool.pending_resolve_count;
			
			idbgx(Debug::ipc, this<<' '<<_rconpoolid<<" new connection "<<conuid<<" - active_connection_count "<<rconpool.active_connection_count<<" pending_connection_count "<<rconpool.pending_connection_count);
		}else{
			idbgx(Debug::ipc, this<<' '<<conuid<<" "<<error.message());
		}
	}else{
		Locker<Mutex>					lock(d.connectionPoolMutex(_rconpoolid.index));
		ConnectionPoolStub				&rconpool(d.conpooldq[_rconpoolid.index]);
		--rconpool.pending_resolve_count;
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
	
	void operator()(ResolveData &_rrd, ErrorCodeT const &_rerror){
		AddressVectorT		addrvec;
		if(!_rerror){
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

std::ostream& operator<<(std::ostream &_ros, RecipientId const &_con_id){
	_ros<<'{'<<_con_id.connectionId()<<"}{"<<_con_id.poolId()<<'}';
	return _ros;
}
//-----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream &_ros, RequestId const &_msguid){
	_ros<<'{'<<_msguid.index<<','<<_msguid.unique<<'}';
	return _ros;
}
//-----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream &_ros, MessageId const &_msguid){
	_ros<<'{'<<_msguid.index<<','<<_msguid.unique<<'}';
	return _ros;
}
//=============================================================================
}//namespace ipc
}//namespace frame
}//namespace solid


