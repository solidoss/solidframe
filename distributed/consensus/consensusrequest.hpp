/* Declarations file consensusrequest.hpp
	
	Copyright 2011, 2012 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DISTRIBUTED_CONSENSUS_CONSENSUSREQUEST_HPP
#define DISTRIBUTED_CONSENSUS_CONSENSUSREQUEST_HPP


#include "foundation/signal.hpp"
#include "foundation/ipc/ipcconnectionuid.hpp"
#include "utility/dynamicpointer.hpp"
#include "system/socketaddress.hpp"

#include "distributed/consensus/consensusrequestid.hpp"

namespace distributed{
namespace consensus{
//! A base class for all write distributed consensus requests
/*!
 * Inherit WriteRequestSignal if you want a distributed request to be 
 * used with a distributed/replicated consensus object.<br>
 * 
 * \see example/distributed/consensus for a proof-of-concept
 * 
 */ 
struct WriteRequestSignal: Dynamic<WriteRequestSignal, DynamicShared<foundation::Signal> >{
	enum{
		OnSender,
		OnPeer,
		BackOnSender
	};
	WriteRequestSignal();
	WriteRequestSignal(const RequestId &_rreqid);
	~WriteRequestSignal();
	void ipcReceived(
		foundation::ipc::SignalUid &_rsiguid
	);
	//! Implement this to send "this" to a distributed::consensus::Object when on peer
	/*!
	 * While on peer (the process containing the needed distributed::consensus::Object)
	 * this WriteRequestSignal will use this call to signal itself to the needed
	 * distributed::consensus::Object:<br>
	 * <code><br>
	 * void StoreRequest::sendThisToConsensusObject(){<br>
	 *     DynamicPointer<fdt::Signal> sig(this);<br>
	 *     m().signal(sig, serverUid());<br>
	 * }<br>
	 * </code>
	 */
	virtual void sendThisToConsensusObject() = 0;
	
	template <class S>
	S& operator&(S &_s){
		_s.push(id.requid, "id.requid").push(id.senderuid, "sender");
		_s.push(st, "state");
		if(waitresponse || !S::IsSerializer){//on peer
			_s.push(ipcsiguid.idx, "siguid.idx").push(ipcsiguid.uid,"siguid.uid");
		}else{//on sender
			foundation::ipc::SignalUid &rsiguid(
				const_cast<foundation::ipc::SignalUid &>(foundation::ipc::SignalContext::the().signaluid)
			);
			_s.push(rsiguid.idx, "siguid.idx").push(rsiguid.uid,"siguid.uid");
		}
		return _s;
	}
	
	uint32 ipcPrepare();
	void ipcFail(int _err);
	void ipcSuccess();
	
	void use();
	int release();
	
	bool							waitresponse;
	uint8							st;
	int8							sentcount;
	foundation::ipc::ConnectionUid	ipcconid;
	foundation::ipc::SignalUid		ipcsiguid;
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
struct ReadRequestSignal: Dynamic<ReadRequestSignal, DynamicShared<foundation::Signal> >{
	enum{
		OnSender,
		OnPeer,
		BackOnSender
	};
	ReadRequestSignal();
	ReadRequestSignal(const RequestId &_rreqid);
	~ReadRequestSignal();
	void ipcReceived(
		foundation::ipc::SignalUid &_rsiguid
	);
	//! Implement this to send "this" to a distributed::consensus::Object when on peer
	/*!
	 * While on peer (the process containing the needed distributed::consensus::Object)
	 * this WriteRequestSignal will use this call to signal itself to the needed
	 * distributed::consensus::Object:<br>
	 * <code><br>
	 * void FindRequest::sendThisToConsensusObject(){<br>
	 *     DynamicPointer<fdt::Signal> sig(this);<br>
	 *     m().signal(sig, serverUid());<br>
	 * }<br>
	 * </code>
	 */
	virtual void sendThisToConsensusObject() = 0;
	
	template <class S>
	S& operator&(S &_s){
		_s.push(id.requid, "id.requid").push(id.senderuid, "sender");
		_s.push(st, "state");
		if(waitresponse || !S::IsSerializer){//on peer
			_s.push(ipcsiguid.idx, "siguid.idx").push(ipcsiguid.uid,"siguid.uid");
		}else{//on sender
			foundation::ipc::SignalUid &rsiguid(
				const_cast<foundation::ipc::SignalUid &>(foundation::ipc::SignalContext::the().signaluid)
			);
			_s.push(rsiguid.idx, "siguid.idx").push(rsiguid.uid,"siguid.uid");
		}
		return _s;
	}
	
	uint32 ipcPrepare();
	void ipcFail(int _err);
	void ipcSuccess();
	
	void use();
	int release();
	
	bool							waitresponse;
	uint8							st;
	int8							sentcount;
	foundation::ipc::ConnectionUid	ipcconid;
	foundation::ipc::SignalUid		ipcsiguid;
	RequestId						id;
};


}//namespace consensus
}//namespace distributed

#endif
