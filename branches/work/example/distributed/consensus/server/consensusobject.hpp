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

struct Parameters{
	typedef std::vector<SocketAddress4>	AddressVectorT;
	static Parameters& the(Parameters *_p = NULL);
	Parameters();
	
	AddressVectorT	addrvec;
	uint8			idx;
};

class Object: public Dynamic<Object, foundation::Object>{
	typedef DynamicExecuter<void, Object>			DynamicExecuterT;
public:
	static void dynamicRegister();
	Object();
	~Object();
	void dynamicExecute(DynamicPointer<> &_dp);
	
	void dynamicExecute(DynamicPointer<RequestSignal> &_rsig);
	void dynamicExecute(DynamicPointer<OperationSignal<1> > &_rsig);
	void dynamicExecute(DynamicPointer<OperationSignal<2> > &_rsig);
	void dynamicExecute(DynamicPointer<OperationSignal<4> > &_rsig);
	void dynamicExecute(DynamicPointer<OperationSignal<8> > &_rsig);
	void dynamicExecute(DynamicPointer<OperationSignal<16> > &_rsig);
	void dynamicExecute(DynamicPointer<OperationSignal<32> > &_rsig);
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
	struct RunData;
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
private:
	struct Data;
	Data	&d;
};


}//namespace consensus

#endif
