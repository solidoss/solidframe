#ifndef DISTRIBUTED_CONCEPT_SERVEROBJECT_HPP
#define DISTRIBUTED_CONCEPT_SERVEROBJECT_HPP

#include "foundation/object.hpp"
#include "foundation/signal.hpp"

struct StoreSignal;
struct FetchSignal;
struct EraseSignal;

class ServerObject: public Dynamic<ServerObject, foundation::Object>{
	typedef DynamicExecuter<void, ServerObject>	DynamicExecuterT;
public:
	static void dynamicRegister();
	ServerObject();
	~ServerObject();
	void dynamicExecute(DynamicPointer<> &_dp);
	void dynamicExecute(DynamicPointer<StoreSignal> &_rsig);
	void dynamicExecute(DynamicPointer<FetchSignal> &_rsig);
	void dynamicExecute(DynamicPointer<EraseSignal> &_rsig);
	
	int execute(ulong _sig, TimeSpec &_tout);
private:
	/*virtual*/ bool signal(DynamicPointer<foundation::Signal> &_sig);
private:
	DynamicExecuterT	exe;
	uint32				crtval;
};

#endif

