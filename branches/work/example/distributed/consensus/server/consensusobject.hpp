#ifndef CONSENSUSOBJECT_HPP
#define CONSENSUSOBJECT_HPP

#include <vector>

#include "system/socketaddress.hpp"
#include "foundation/object.hpp"

namespace consensus{

struct RequestSignal;
struct RequestId;

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
	virtual int doInit(ulong _sig, TimeSpec &_tout);
	virtual int doRun(ulong _sig, TimeSpec &_tout);
	virtual int doUpdate(ulong _sig, TimeSpec &_tout);
	virtual void doAccept(DynamicPointer<RequestSignal> &_rsig) = 0;
private:
	struct Data;
	Data	&d;
};


}//namespace consensus

#endif
