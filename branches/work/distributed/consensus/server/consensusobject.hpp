#ifndef DISTRIBUTED_CONSENSUS_CONSENSUSOBJECT_HPP
#define DISTRIBUTED_CONSENSUS_CONSENSUSOBJECT_HPP

#include <vector>

#include "system/socketaddress.hpp"
#include "foundation/object.hpp"

namespace distributed{
namespace consensus{

struct RequestSignal;
struct RequestId;

namespace server{

template <uint16 Count>
struct OperationSignal;
struct OperationStub;

struct Parameters{
	typedef std::vector<SocketAddress4>	AddressVectorT;
	static const Parameters& the(Parameters *_p = NULL);
	Parameters();
	
	AddressVectorT	addrvec;
	uint8			idx;
	uint8			quorum;
};

class Object: public Dynamic<Object, foundation::Object>{
	struct RunData;
	typedef DynamicExecuter<void, Object, RunData&>			DynamicExecuterT;
public:
	static void dynamicRegister();
	static void registerSignals();
	Object();
	~Object();
	void dynamicExecute(DynamicPointer<> &_dp, RunData &_rrd);
	
	void dynamicExecute(DynamicPointer<RequestSignal> &_rsig, RunData &_rrd);
	void dynamicExecute(DynamicPointer<OperationSignal<1> > &_rsig, RunData &_rrd);
	void dynamicExecute(DynamicPointer<OperationSignal<2> > &_rsig, RunData &_rrd);
	void dynamicExecute(DynamicPointer<OperationSignal<4> > &_rsig, RunData &_rrd);
	void dynamicExecute(DynamicPointer<OperationSignal<8> > &_rsig, RunData &_rrd);
	void dynamicExecute(DynamicPointer<OperationSignal<16> > &_rsig, RunData &_rrd);
	void dynamicExecute(DynamicPointer<OperationSignal<32> > &_rsig, RunData &_rrd);
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
	/*virtual*/ int execute(ulong _sig, TimeSpec &_tout);
	/*virtual*/ bool signal(DynamicPointer<foundation::Signal> &_sig);
	virtual void accept(DynamicPointer<RequestSignal> &_rsig) = 0;
	virtual void init();
	virtual void prepareRun();
	virtual void prepareRecovery();
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
