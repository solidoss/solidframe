#ifndef DISTRIBUTED_CONCEPT_CLIENTOBJECT_HPP
#define DISTRIBUTED_CONCEPT_CLIENTOBJECT_HPP

#include "foundation/common.hpp"
#include "foundation/object.hpp"
#include "foundation/signal.hpp"
#include "system/socketaddress.hpp"
#include <vector>
#include <string>
#include <ostream>

class ClientObject;

struct ClientSignal: Dynamic<ClientSignal, foundation::Signal>{
	ClientSignal();
	~ClientSignal();
	virtual void execute(ClientObject &) = 0;
};

struct ClientParams{
	typedef std::vector<std::string>	StringVectorT;
	typedef std::vector<SocketAddress>	AddressVectorT;
	typedef std::pair<uint8, uint32>	RequestPairT;
	typedef std::vector<RequestPairT>	RequestVectorT;
	typedef std::vector<uint32>			UInt32VectorT;
	
	std::string		seqstr;
	StringVectorT	addrstrvec;
	uint32			cnt;
	UInt32VectorT	strszvec;
	
	AddressVectorT	addrvec;
	RequestVectorT	reqvec;
	StringVectorT	strvec;
	ClientParams():cnt(1){}
	ClientParams(const ClientParams &_rcp);
	void print(std::ostream &_ros);
};


class ClientObject: public Dynamic<ClientObject, foundation::Object>{
	typedef DynamicExecuter<void, ClientObject>	DynamicExecuterT;
public:
	ClientObject(const ClientParams &_rcp);
	~ClientObject();
	void dynamicExecute(DynamicPointer<> &_dp);
	void dynamicExecute(DynamicPointer<ClientSignal> &_rsig);
	
	int execute(ulong _sig, TimeSpec &_tout);
	
	uint32 newRequestId(int _pos = -1);
	bool   isRequestIdExpected(uint32 _v, int &_rpos)const;
	void   deleteRequestId(uint32 _v);
private:
	typedef std::vector<std::pair<uint32, int> >	RequestIdVectorT;
	
	ClientParams		params;
	uint32				crtreqid;
	
	DynamicExecuterT	exe;
	RequestIdVectorT	reqidvec;
};


#endif
