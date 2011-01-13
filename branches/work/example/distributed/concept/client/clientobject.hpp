#ifndef DISTRIBUTED_CONCEPT_CLIENTOBJECT_HPP
#define DISTRIBUTED_CONCEPT_CLIENTOBJECT_HPP

#include "foundation/object.hpp"
#include "foundation/signal.hpp"
#include <vector>



class ClientObject;

struct ClientSignal: Dynamic<ClientSignal, foundation::Signal>{
	ClientSignal();
	~ClientSignal();
	virtual void execute(ClientObject &) = 0;
};

class ClientObject: public Dynamic<ClientObject, foundation::Object>{
	typedef DynamicExecuter<void, ClientObject>	DynamicExecuterT;
public:
	ClientObject();
	~ClientObject();
	void dynamicExecute(DynamicPointer<> &_dp);
	void dynamicExecute(DynamicPointer<ClientSignal> &_rsig);
	
	int execute(ulong _sig, TimeSpec &_tout);
	
	uint32 newRequestId(int _pos = -1);
	bool   isRequestIdExpected(uint32 _v, int &_rpos)const;
	void   deleteRequestId(uint32 _v);
private:
	typedef std::vector<std::pair<uint32, int> >	RequestIdVectorT;
	
	uint32				crtreqid;
	
	DynamicExecuterT	exe;
	RequestIdVectorT	reqidvec;
};


#endif
