#ifndef DISTRIBUTED_CONCEPT_SERVEROBJECT_HPP
#define DISTRIBUTED_CONCEPT_SERVEROBJECT_HPP

#include <deque>

#include "foundation/object.hpp"
#include "foundation/signal.hpp"

struct C;
struct StoreSignal;
struct FetchSignal;
struct EraseSignal;
struct ConceptSignal;

class ServerObject: public Dynamic<ServerObject, foundation::Object>{
	typedef DynamicExecuter<void, ServerObject>	DynamicExecuterT;
	typedef DynamicExecuter<void, ServerObject, int>	DynamicExecuterExT;
public:
	static void dynamicRegister();
	ServerObject();
	~ServerObject();
	void dynamicExecute(DynamicPointer<> &_dp);
	
	void dynamicExecute(DynamicPointer<ConceptSignal> &_rsig);
	
	void dynamicExecute(DynamicPointer<> &_dp, int);
	
	void dynamicExecute(DynamicPointer<StoreSignal> &_rsig, int);
	void dynamicExecute(DynamicPointer<FetchSignal> &_rsig, int);
	void dynamicExecute(DynamicPointer<EraseSignal> &_rsig, int);
	
	
	int execute(ulong _sig, TimeSpec &_tout);
private:
	/*virtual*/ bool signal(DynamicPointer<foundation::Signal> &_sig);
private:
	struct ClientRequest{
		DynamicPointer<ConceptSignal>	sig;
		uint32							state;
	};
	struct SigCmp{
		bool operator()(const ClientRequest* const & _req1, const ClientRequest* const & _req2)const;
	};
	typedef std::deque<ClientRequest>	ClientRequestVectorT;
private:
	DynamicExecuterT	exe;
	DynamicExecuterExT	exeex;
	uint32				consensusval;
	
	uint32				crtval;
	
	
};

#endif

