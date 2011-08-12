#ifndef CONSENSUSOBJECT_HPP
#define CONSENSUSOBJECT_HPP

#include <vector>

#include "system/socketaddress.hpp"
#include "foundation/object.hpp"

namespace consensus{

struct RequestSignal;
struct RequestId;
template <uint16 Count>
struct OperationSignal;
struct OperationStub;

struct Parameters{
	typedef std::vector<SocketAddress4>	AddressVectorT;
	static const Parameters& the(Parameters *_p = NULL);
	Parameters();
	
	AddressVectorT	addrvec;
	uint8			idx;
	uint8			threshold;
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
		Init,
		Run,
		Update
	};
	bool isCoordinator()const;
	uint32 acceptId()const;
	uint32 proposeId()const;
private:
	/*virtual*/ int execute(ulong _sig, TimeSpec &_tout);
	/*virtual*/ bool signal(DynamicPointer<foundation::Signal> &_sig);
	virtual void doAccept(DynamicPointer<RequestSignal> &_rsig) = 0;
	int doInit(RunData &_rd);
	int doRun(RunData &_rd);
	int doUpdate(RunData &_rd);
    void doProcessRequest(RunData &_rd, size_t _pos);
	void doSendAccept(RunData &_rd, size_t _pos);
	void doSendPropose(RunData &_rd, size_t _pos);
	void doFlushOperations(RunData &_rd);
	void doScanPendingRequests(RunData &_rd);
	void doAcceptRequest(RunData &_rd, size_t _pos);
    void doEraseRequest(RunData &_rd, size_t _pos);
	void doExecuteOperation(RunData &_rd, uint8 _replicaidx, OperationStub &_rop);
	void doExecuteProposeOperation(RunData &_rd, size_t _reqidx, OperationStub &_rop);
	void doExecuteProposeAcceptOperation(RunData &_rd, size_t _reqidx, OperationStub &_rop);
	void doExecuteAcceptOperation(RunData &_rd, size_t _reqidx, OperationStub &_rop);
private:
	struct Data;
	Data	&d;
};


}//namespace consensus

#endif
