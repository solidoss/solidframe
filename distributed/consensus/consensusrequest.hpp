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
 * Inherit RequestSignal if you want a distributed request to be 
 * used with a distributed/replicated consensus object.<br>
 * 
 * \see example/distributed/consensus for a proof-of-concept
 * 
 */ 
struct RequestSignal: Dynamic<RequestSignal, DynamicShared<foundation::Signal> >{
	enum{
		OnSender,
		OnPeer,
		BackOnSender
	};
	RequestSignal();
	RequestSignal(const RequestId &_rreqid);
	~RequestSignal();
	void ipcReceived(
		foundation::ipc::SignalUid &_rsiguid
	);
	//! Implement this to send "this" to a distributed::consensus::Object when on peer
	/*!
	 * While on peer (the process containing the needed distributed::consensus::Object)
	 * this RequestSignal will use this call to signal itself to the needed
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


}//namespace consensus
}//namespace distributed

#endif
