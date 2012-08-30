#ifndef DISTRIBUTED_CONCEPT_CLIENTOBJECT_HPP
#define DISTRIBUTED_CONCEPT_CLIENTOBJECT_HPP

#include "foundation/common.hpp"
#include "foundation/object.hpp"
#include "foundation/signal.hpp"
#include "system/socketaddress.hpp"
#include "system/timespec.hpp"
#include <vector>
#include <string>
#include <ostream>

class ClientObject;
struct StoreRequest;
struct FetchRequest;
struct EraseRequest;

namespace distributed{
namespace consensus{
struct WriteRequestSignal;
}
}

struct ClientSignal: Dynamic<ClientSignal, foundation::Signal>{
	ClientSignal();
	~ClientSignal();
	virtual void execute(ClientObject &) = 0;
};

struct ClientParams{
	typedef std::vector<std::string>		StringVectorT;
	typedef std::vector<SocketAddressInet4>	AddressVectorT;
	struct Request{
		Request(uint8 _opp = 0):opp(_opp){
			u.u64 = 0;
		}
		
		uint8	opp;
		union{
			uint64	u64;
			struct{
				uint32	u32_1;
				uint32	u32_2;
			}		u32s;
		}		u;
	};
	typedef std::vector<Request>	RequestVectorT;
	typedef std::vector<uint32>		UInt32VectorT;
	
	std::string		seqstr;
	StringVectorT	addrstrvec;
	uint32			cnt;
	UInt32VectorT	strszvec;
	
	AddressVectorT	addrvec;
	RequestVectorT	reqvec;
	StringVectorT	strvec;
	
	ClientParams():cnt(0){}
	ClientParams(const ClientParams &_rcp);
	bool init();
	const std::string& errorString()const{
		return err;
	}
	void print(std::ostream &_ros);
private:
	bool parseRequests();
private:
	std::string		err;
};


class ClientObject: public Dynamic<ClientObject, foundation::Object>{
	typedef DynamicExecuter<void, ClientObject>	DynamicExecuterT;
	enum{
		Execute,
		Wait
	};
public:
	static void dynamicRegister();
	ClientObject(const ClientParams &_rcp);
	~ClientObject();
	void dynamicExecute(DynamicPointer<> &_dp);
	void dynamicExecute(DynamicPointer<ClientSignal> &_rsig);
	void dynamicExecute(DynamicPointer<StoreRequest> &_rsig);
	void dynamicExecute(DynamicPointer<FetchRequest> &_rsig);
	void dynamicExecute(DynamicPointer<EraseRequest> &_rsig);
	
	int execute(ulong _sig, TimeSpec &_tout);
	
	uint32 newRequestId(int _pos = -1);
	bool   isRequestIdExpected(uint32 _v, int &_rpos)const;
	void   deleteRequestId(uint32 _v);
private:
	uint32 sendSignal(distributed::consensus::WriteRequestSignal *_psig);
	const std::string& getString(uint32 _pos, uint32 _crtpos);
	void expectStore(uint32 _rid, const std::string &_rs, uint32 _v, uint32 _cnt);
	void expectFetch(uint32 _rid, const std::string &_rs, uint32 _cnt);
	void expectErase(uint32 _rid, const std::string &_rs, uint32 _cnt);
	void expectErase(uint32 _rid, uint32 _cnt);
	
	/*virtual*/ bool signal(DynamicPointer<foundation::Signal> &_sig);
private:
	typedef std::vector<std::pair<uint32, int> >	RequestIdVectorT;
	
	ClientParams		params;
	uint32				crtreqid;
	uint32				crtreqpos;
	uint32				crtpos;
	uint32				waitresponsecount;
	
	TimeSpec			nexttimepos;
	DynamicExecuterT	exe;
	RequestIdVectorT	reqidvec;
};


#endif
