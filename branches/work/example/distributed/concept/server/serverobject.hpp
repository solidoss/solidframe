#ifndef DISTRIBUTED_CONCEPT_SERVEROBJECT_HPP
#define DISTRIBUTED_CONCEPT_SERVEROBJECT_HPP

#include "foundation/object.hpp"
#include "foundation/signal.hpp"


class ServerObject: public Dynamic<ServerObject, foundation::Object>{
	typedef DynamicExecuter<void, ServerObject>	DynamicExecuterT;
public:
	ServerObject();
	~ServerObject();
	void dynamicExecute(DynamicPointer<> &_dp);
	
	int execute(ulong _sig, TimeSpec &_tout);
	
private:
	DynamicExecuterT	exe;
};

#endif

