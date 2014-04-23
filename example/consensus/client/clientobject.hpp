#ifndef EXAMPLE_CONSENSUS_CLIENTOBJECT_HPP
#define EXAMPLE_CONSENSUS_CLIENTOBJECT_HPP

#include "frame/common.hpp"
#include "frame/object.hpp"
#include "frame/message.hpp"
#include "system/socketaddress.hpp"
#include "system/timespec.hpp"
#include <vector>
#include <string>
#include <ostream>

class ClientObject;
struct StoreRequest;
struct FetchRequest;
struct EraseRequest;

namespace solid{
namespace consensus{
struct WriteRequestMessage;
}//namespace consensus
namespace frame{
namespace ipc{
class Service;
}//namespace ipc
}//namespace frame
}//namespace solid

struct ClientMessage: solid::Dynamic<ClientMessage, solid::frame::Message>{
	ClientMessage();
	~ClientMessage();
	virtual void execute(ClientObject &) = 0;
};

struct ClientParams{
	typedef std::vector<std::string>				StringVectorT;
	typedef std::vector<solid::SocketAddressInet4>	AddressVectorT;
	struct Request{
		Request(solid::uint8 _opp = 0):opp(_opp){
			u.u64 = 0;
		}
		
		solid::uint8		opp;
		union{
			solid::uint64	u64;
			struct{
				solid::uint32	u32_1;
				solid::uint32	u32_2;
			}	u32s;
		}					u;
	};
	typedef std::vector<Request>	RequestVectorT;
	typedef std::vector<solid::uint32>		UInt32VectorT;
	
	std::string		seqstr;
	StringVectorT	addrstrvec;
	solid::uint32	cnt;
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


class ClientObject: public solid::Dynamic<ClientObject, solid::frame::Object>{
	typedef solid::DynamicMapper<void, ClientObject>	DynamicMapperT;
	enum{
		Execute,
		Wait
	};
public:
	static void dynamicRegister();
	ClientObject(const ClientParams &_rcp, solid::frame::ipc::Service &_ripcsvc);
	~ClientObject();
	void dynamicHandle(solid::DynamicPointer<> &_dp);
	void dynamicHandle(solid::DynamicPointer<ClientMessage> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<StoreRequest> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<FetchRequest> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<EraseRequest> &_rmsgptr);
		
	solid::uint32 newRequestId(int _pos = -1);
	bool   isRequestIdExpected(solid::uint32 _v, int &_rpos)const;
	void   deleteRequestId(solid::uint32 _v);
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
	solid::uint32 sendMessage(solid::consensus::WriteRequestMessage *_pmsg);
	
	const std::string& getString(solid::uint32 _pos, solid::uint32 _crtpos);
	void expectStore(solid::uint32 _rid, const std::string &_rs, solid::uint32 _v, solid::uint32 _cnt);
	void expectFetch(solid::uint32 _rid, const std::string &_rs, solid::uint32 _cnt);
	void expectErase(solid::uint32 _rid, const std::string &_rs, solid::uint32 _cnt);
	void expectErase(solid::uint32 _rid, solid::uint32 _cnt);
	
	/*virtual*/ bool notify(solid::DynamicPointer<solid::frame::Message> &_rmsgptr);
	void state(int _st){
		st = _st;
	}
	int state()const{
		return st;
	}
private:
	static DynamicMapperT		dm;
	typedef std::vector<std::pair<solid::uint32, int> >		RequestIdVectorT;
	typedef std::vector<solid::DynamicPointer<> >			DynamicPointerVectorT;
	
	
	ClientParams				params;
	solid::frame::ipc::Service	&ripcsvc;
	solid::uint32				crtreqid;
	solid::uint32				crtreqpos;
	solid::uint32				crtpos;
	solid::uint32				waitresponsecount;
	int							st;
	
	solid::TimeSpec				nexttimepos;
	DynamicPointerVectorT		dv;
	RequestIdVectorT			reqidvec;
};


#endif
