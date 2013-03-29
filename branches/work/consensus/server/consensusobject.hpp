/* Declarations file consensusobject.hpp
	
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

#ifndef SOLID_CONSENSUS_CONSENSUSOBJECT_HPP
#define SOLID_CONSENSUS_CONSENSUSOBJECT_HPP

#include <vector>

#include "system/socketaddress.hpp"
#include "frame/object.hpp"

namespace solid{
namespace consensus{

struct WriteRequestMessage;
struct ReadRequestMessage;
struct RequestId;

namespace server{

template <uint16 Count>
struct OperationMessage;
struct OperationStub;

struct Parameters{
	typedef std::vector<SocketAddressInet4>	AddressVectorT;
	static const Parameters& the(Parameters *_p = NULL);
	Parameters();
	
	AddressVectorT	addrvec;
	uint8			idx;
	uint8			quorum;
};
//! The base class for distributed objects needing consensus on processing requests
/*!
 * Inherit this class and implement accept, [init,] [prepareRun,] [prepareRecovery,] and
 * recovery to have a replicated object which needs consensus on the order it(they) process
 * the incomming requests from clients.<br>
 * The distributed::consensus::Object implements fast multi Paxos algorithm.<br>
 * 
 * \see example/distributed/consensus for a proof-of-concept
 * 
 */
class Object: public Dynamic<Object, frame::Object>{
	struct RunData;
	typedef DynamicExecuter<void, Object, DynamicDefaultPointerStore, RunData&>	DynamicExecuterT;
public:
	static void dynamicRegister();
	static void registerMessages();
	Object();
	~Object();
	void dynamicExecute(DynamicPointer<> &_dp, RunData &_rrd);
	
	void dynamicExecute(DynamicPointer<WriteRequestMessage> &_rmsgptr, RunData &_rrd);
	void dynamicExecute(DynamicPointer<ReadRequestMessage> &_rmsgptr, RunData &_rrd);
	void dynamicExecute(DynamicPointer<OperationMessage<1> > &_rmsgptr, RunData &_rrd);
	void dynamicExecute(DynamicPointer<OperationMessage<2> > &_rmsgptr, RunData &_rrd);
	void dynamicExecute(DynamicPointer<OperationMessage<4> > &_rmsgptr, RunData &_rrd);
	void dynamicExecute(DynamicPointer<OperationMessage<8> > &_rmsgptr, RunData &_rrd);
	void dynamicExecute(DynamicPointer<OperationMessage<16> > &_rmsgptr, RunData &_rrd);
	void dynamicExecute(DynamicPointer<OperationMessage<32> > &_rmsgptr, RunData &_rrd);
protected:
	enum State{
		InitState,
		PrepareRunState,
		RunState,
		PrepareRecoveryState,
		FirstRecoveryState,
		LastRecoveryState = FirstRecoveryState + 32,
	};
	bool isCoordinator()const;
	uint32 acceptId()const;
	uint32 proposeId()const;
	void enterRunState();
	bool isRecoveryState()const;
private:
	void state(int _st);
	int state()const;
	/*virtual*/ int execute(ulong _sig, TimeSpec &_tout);
	/*virtual*/ bool notify(DynamicPointer<frame::Message> &_rmsgptr);
	//! It should dynamically cast the signal to an accepted Request and process it.
	virtual void accept(DynamicPointer<WriteRequestMessage> &_rmsgptr) = 0;
	//! Called once while initializing the Object
	virtual void init();
	//! Called once before entering RunState
	virtual void prepareRun();
	//! Called once before entering RecoveryState
	virtual void prepareRecovery();
	//! Called continuously by execute method until exiting RecoveryState
	virtual int recovery() = 0;
	int doInit(RunData &_rd);
	int doPrepareRun(RunData &_rd);
	int doRun(RunData &_rd);
	int doPrepareRecovery(RunData &_rd);
	int doRecovery(RunData &_rd);
    void doProcessRequest(RunData &_rd, const size_t _reqidx);
	void doSendAccept(RunData &_rd, const size_t _reqidx);
	void doSendFastAccept(RunData &_rd, const size_t _reqidx);
	void doSendPropose(RunData &_rd, const size_t _reqidx);
	void doSendConfirmPropose(RunData &_rd, const uint8 _replicaidx, const size_t _reqidx);
	void doSendDeclinePropose(RunData &_rd, const uint8 _replicaidx, const OperationStub &_rop);
	void doSendConfirmAccept(RunData &_rd, const uint8 _replicaidx, const size_t _reqidx);
	void doSendDeclineAccept(RunData &_rd, const uint8 _replicaidx, const OperationStub &_rop);
	void doFlushOperations(RunData &_rd);
	void doScanPendingRequests(RunData &_rd);
	void doAcceptRequest(RunData &_rd, const size_t _reqidx);
    void doEraseRequest(RunData &_rd, const size_t _reqidx);
	void doExecuteOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop);
	void doExecuteProposeOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop);
	void doExecuteProposeConfirmOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop);
	void doExecuteProposeDeclineOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop);
	void doExecuteAcceptOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop);
	void doExecuteFastAcceptOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop);
	void doExecuteAcceptConfirmOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop);
	void doExecuteAcceptDeclineOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop);
    void doStartCoordinate(RunData &_rd, const size_t _reqidx);
    void doEnterRecoveryState();
private:
	struct Data;
	Data	&d;
};

}//namespace server
}//namespace consensus
}//namespace distributed
#endif