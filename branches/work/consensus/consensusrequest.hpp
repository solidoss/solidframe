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


#include "frame/message.hpp"
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
struct WriteRequestMessage: Dynamic<WriteRequestMessage, DynamicShared<frame::Message> >{
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
	S& operator&(S &_s){
		_s.push(id.requid, "id.requid").push(id.senderuid, "sender");
		_s.push(st, "state");
		if(waitresponse || !S::IsSerializer){//on peer
			_s.push(ipcmsguid.idx, "msguid.idx").push(ipcmsguid.uid,"msguid.uid");
		}else{//on sender
			frame::ipc::MessageUid &rmsguid(
				const_cast<frame::ipc::MessageUid &>(frame::ipc::ConnectionContext::the().msgid)
			);
			_s.push(rmsguid.idx, "msguid.idx").push(rmsguid.uid,"msguid.uid");
		}
		return _s;
	}
	
	/*virtual*/ void ipcReceive(
		frame::ipc::MessageUid &_rmsguid
	);
	/*virtual*/ uint32 ipcPrepare();
	/*virtual*/ void ipcComplete(int _err);
	
	size_t use();
	size_t release();
	
	bool							waitresponse;
	uint8							st;
	int8							sentcount;
	frame::ipc::ConnectionUid		ipcconid;
	frame::ipc::MessageUid			ipcmsguid;
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
struct ReadRequestMessage: Dynamic<ReadRequestMessage, DynamicShared<frame::Message> >{
	enum{
		OnSender,
		OnPeer,
		BackOnSender
	};
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
	S& operator&(S &_s){
		_s.push(id.requid, "id.requid").push(id.senderuid, "sender");
		_s.push(st, "state");
		if(waitresponse || !S::IsSerializer){//on peer
			_s.push(ipcmsguid.idx, "msguid.idx").push(ipcmsguid.uid,"msguid.uid");
		}else{//on sender
			frame::ipc::MessageUid &rmsguid(
				const_cast<frame::ipc::MessageUid &>(frame::ipc::ConnectionContext::the().msgid)
			);
			_s.push(rmsguid.idx, "msguid.idx").push(rmsguid.uid,"msguid.uid");
		}
		return _s;
	}
	
	/*virtual*/ void ipcReceive(
		frame::ipc::MessageUid &_rmsguid
	);
	/*virtual*/ uint32 ipcPrepare();
	/*virtual*/ void ipcComplete(int _err);
	
	size_t use();
	size_t release();
	
	bool							waitresponse;
	uint8							st;
	int8							sentcount;
	frame::ipc::ConnectionUid		ipcconid;
	frame::ipc::MessageUid			ipcmsguid;
	RequestId						id;
};


}//namespace consensus
}//namespace solid

#endif
