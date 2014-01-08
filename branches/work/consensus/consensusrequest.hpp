// consensus/consensusrequest.hpp
//
// Copyright (c) 2011, 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_CONSENSUS_REQUEST_HPP
#define SOLID_CONSENSUS_REQUEST_HPP


#include "frame/ipc/ipcmessage.hpp"
#include "frame/ipc/ipcconnectionuid.hpp"
#include "utility/dynamicpointer.hpp"
#include "system/socketaddress.hpp"

#include "consensus/consensusrequestid.hpp"

namespace solid{
namespace consensus{
//! A base class for all write distributed consensus requests
/*!
 * Inherit WriteRequestSignal if you want a distributed request to be 
 * used with a distributed/replicated consensus object.<br>
 * 
 * \see example/distributed/consensus for a proof-of-concept
 * 
 */ 
struct WriteRequestMessage: Dynamic<WriteRequestMessage, DynamicShared<frame::ipc::Message> >{
	enum{
		OnSender,
		OnPeer,
		BackOnSender
	};
	WriteRequestMessage();
	WriteRequestMessage(const RequestId &_rreqid);
	~WriteRequestMessage();
	//! Implement to send "this" to a consensus::Object when on peer
	/*!
	 * While on peer (the process containing the needed consensus::Object)
	 * this WriteRequestMessage will use this call to signal itself to the needed
	 * consensus::Object:<br>
	 * <code><br>
	 * void StoreRequest::sendThisToConsensusObject(){<br>
	 *     DynamicPointer<frame::Message> msgptr(this);<br>
	 *     m().notify(msgptr, serverUid());<br>
	 * }<br>
	 * </code>
	 */
	virtual void notifyConsensusObjectWithThis() = 0;
	virtual void notifySenderObjectWithThis() = 0;
	virtual void notifySenderObjectWithFail() = 0;
	
	template <class S>
	S& serialize(S &_s, frame::ipc::ConnectionContext const &/*_rctx*/){
		_s.push(id.requid, "id.requid").push(id.senderuid, "sender");
		return _s;
	}
	
	/*virtual*/ void ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, MessagePointerT &_rmsgptr);
	/*virtual*/ uint32 ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx);
	/*virtual*/ void ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err);
	
	size_t use();
	size_t release();
	
	int8							sentcount;
	frame::ipc::ConnectionUid		ipcconid;
	RequestId						id;
};

//! A base class for all read-only distributed consensus requests
/*!
 * Inherit WriteRequestSignal if you want a distributed request to be 
 * used with a distributed/replicated consensus object.<br>
 * 
 * \see example/distributed/consensus for a proof-of-concept
 * 
 */ 
struct ReadRequestMessage: Dynamic<ReadRequestMessage, DynamicShared<frame::ipc::Message> >{
	ReadRequestMessage();
	ReadRequestMessage(const RequestId &_rreqid);
	~ReadRequestMessage();
	//! Implement this to send "this" to a distributed::consensus::Object when on peer
	/*!
	 * While on peer (the process containing the needed distributed::consensus::Object)
	 * this WriteRequestSignal will use this call to signal itself to the needed
	 * distributed::consensus::Object:<br>
	 * <code><br>
	 * void FindRequest::sendThisToConsensusObject(){<br>
	 *     DynamicPointer<frame::Message> msg(this);<br>
	 *     m().signal(sig, serverUid());<br>
	 * }<br>
	 * </code>
	 */
	virtual void notifyConsensusObjectWithThis() = 0;
	virtual void notifySenderObjectWithThis() = 0;
	virtual void notifySenderObjectWithFail() = 0;
	
	template <class S>
	S& serialize(S &_s, frame::ipc::ConnectionContext const &/*_rctx*/){
		_s.push(id.requid, "id.requid").push(id.senderuid, "sender");
		return _s;
	}
	
	/*virtual*/ void ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, frame::ipc::Message::MessagePointerT &_rmsgptr);
	/*virtual*/ uint32 ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx);
	/*virtual*/ void ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err);
	
	size_t use();
	size_t release();
	
	int8							sentcount;
	frame::ipc::ConnectionUid		ipcconid;
	RequestId						id;
};


}//namespace consensus
}//namespace solid

#endif
