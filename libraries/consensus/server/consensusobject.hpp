// consensus/server/consensusobject.hpp
//
// Copyright (c) 2011, 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_CONSENSUS_CONSENSUSOBJECT_HPP
#define SOLID_CONSENSUS_CONSENSUSOBJECT_HPP

#include <vector>

#include "system/socketaddress.hpp"
#include "frame/object.hpp"

namespace solid{

namespace frame{
namespace ipc{
class Service;
struct Message;
}//namespace ipc
}//namespace frame

namespace consensus{

struct WriteRequestMessage;
struct ReadRequestMessage;
struct RequestId;

namespace server{

template <uint16 Count>
struct OperationMessage;
struct OperationStub;

struct Configuration: Dynamic<Configuration, DynamicShared<> >{
	typedef std::vector<SocketAddressInet4>	AddressVectorT;
	
	AddressVectorT	addrvec;
	uint8			crtidx;
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
	typedef DynamicMapper<void, Object, RunData>	DynamicMapperT;
public:
	static void dynamicRegister();
	static void registerMessages(frame::ipc::Service &_ripcsvc);
	Object(DynamicPointer<Configuration> &_rcfgptr);
	~Object();
	void serverIndex(const frame::IndexT &_ridx);
	frame::IndexT serverIndex()const;
	void dynamicHandle(DynamicPointer<> &_dp, RunData &_rrd);
	
	void dynamicHandle(DynamicPointer<WriteRequestMessage> &_rmsgptr, RunData &_rrd);
	void dynamicHandle(DynamicPointer<ReadRequestMessage> &_rmsgptr, RunData &_rrd);
	void dynamicHandle(DynamicPointer<OperationMessage<1> > &_rmsgptr, RunData &_rrd);
	void dynamicHandle(DynamicPointer<OperationMessage<2> > &_rmsgptr, RunData &_rrd);
	void dynamicHandle(DynamicPointer<OperationMessage<4> > &_rmsgptr, RunData &_rrd);
	void dynamicHandle(DynamicPointer<OperationMessage<8> > &_rmsgptr, RunData &_rrd);
	void dynamicHandle(DynamicPointer<OperationMessage<16> > &_rmsgptr, RunData &_rrd);
	void dynamicHandle(DynamicPointer<OperationMessage<32> > &_rmsgptr, RunData &_rrd);
protected:
	static DynamicMapperT	dm;
	
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
	virtual void doSendMessage(DynamicPointer<frame::ipc::Message> &_rmsgptr, const SocketAddressInet4 &_raddr) = 0;
private:
	void state(int _st);
	int state()const;
	/*virtual*/ void execute(ExecuteContext &_rexectx);
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
	virtual AsyncE recovery() = 0;
	AsyncE doInit(RunData &_rd);
	AsyncE doPrepareRun(RunData &_rd);
	AsyncE doRun(RunData &_rd);
	AsyncE doPrepareRecovery(RunData &_rd);
	AsyncE doRecovery(RunData &_rd);
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
