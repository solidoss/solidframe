#ifndef DISTRIBUTED_CONCEPT_SERVEROBJECT_HPP
#define DISTRIBUTED_CONCEPT_SERVEROBJECT_HPP

#include <deque>
#include <queue>
#include <vector>
#include <string>

#include "system/common.hpp"

#ifdef HAVE_UNORDERED_MAP
#include <unordered_map>
#include <unordered_set>
#else
#include <map>
#endif

#include "system/socketaddress.hpp"
#include "system/timespec.hpp"

#include "foundation/object.hpp"
#include "foundation/signal.hpp"
#include "utility/stack.hpp"
#include "example/distributed/consensus/core/consensusrequestid.hpp"
#include "src/timerqueue.hpp"

//struct C;
struct StoreRequest;
struct FetchRequest;
struct EraseRequest;

namespace consensus{
struct RequestSignal;
struct RequestId;
}

struct ServerParams{
	typedef std::vector<std::string>	StringVectorT;
	typedef std::vector<SocketAddress4>	AddressVectorT;
	static ServerParams& the();
	ServerParams():idx(0xff){}
	bool init(int _ipc_port);
	const std::string& errorString()const{
		return err;
	}
	std::ostream& print(std::ostream &_ros)const;
	
	StringVectorT	addrstrvec;
	AddressVectorT	addrvec;
	uint8			idx;
	
private:
	std::string		err;
};

std::ostream& operator<<(std::ostream &_ros, const ServerParams &_rsp);


class ServerObject: public Dynamic<ServerObject, foundation::Object>{
	typedef DynamicExecuter<void, ServerObject>			DynamicExecuterT;
	typedef DynamicExecuter<void, ServerObject, int>	DynamicExecuterExT;
public:
	static void dynamicRegister();
	ServerObject();
	~ServerObject();
	void dynamicExecute(DynamicPointer<> &_dp);
	
	void dynamicExecute(DynamicPointer<consensus::RequestSignal> &_rsig);
	
	void dynamicExecute(DynamicPointer<> &_dp, int);
	
	void dynamicExecute(DynamicPointer<StoreRequest> &_rsig, int);
	void dynamicExecute(DynamicPointer<FetchRequest> &_rsig, int);
	void dynamicExecute(DynamicPointer<EraseRequest> &_rsig, int);
	
	
	int execute(ulong _sig, TimeSpec &_tout);
private:
	struct ClientRequest;
	/*virtual*/ bool signal(DynamicPointer<foundation::Signal> &_sig);
	size_t insertClientRequest(DynamicPointer<consensus::RequestSignal> &_rsig);
	void eraseClientRequest(size_t _idx);
	ClientRequest& clientRequest(size_t _idx);
	const ClientRequest& clientRequest(size_t _idx)const;
	bool checkAlreadyReceived(DynamicPointer<consensus::RequestSignal> &_rsig);
	void registerClientRequestTimer(const TimeSpec &_rts, size_t _idx);
	size_t popClientRequestTimeout(const TimeSpec &_rts);
	void scheduleNextClientRequestTimer(TimeSpec &_rts);
private:
	struct ClientRequest{
		ClientRequest():uid(0), state(0){}
		ClientRequest(DynamicPointer<consensus::RequestSignal>	&_rsig):sig(_rsig), uid(0), state(0){}
		DynamicPointer<consensus::RequestSignal>	sig;
		uint16										uid;
		uint16										timerid;
		uint16										state;
	};
	struct ReqCmpEqual{
		bool operator()(const consensus::RequestId* const & _req1, const consensus::RequestId* const & _req2)const;
	};
	struct ReqCmpLess{
		bool operator()(const consensus::RequestId* const & _req1, const consensus::RequestId* const & _req2)const;
	};
	struct ReqHash{
		size_t operator()(const consensus::RequestId* const & _req1)const;
	};
	struct SenderCmpEqual{
		bool operator()(const consensus::RequestId & _req1, const consensus::RequestId& _req2)const;
	};
	struct SenderCmpLess{
		bool operator()(const consensus::RequestId& _req1, const consensus::RequestId& _req2)const;
	};
	struct SenderHash{
		size_t operator()(const consensus::RequestId& _req1)const;
	};
	typedef std::deque<ClientRequest>															ClientRequestVectorT;
#ifdef HAVE_UNORDERED_MAP
	typedef std::unordered_map<const consensus::RequestId*, size_t, ReqHash, ReqCmpEqual>		ClientRequestMapT;
	typedef std::unordered_set<consensus::RequestId, SenderHash, SenderCmpEqual>				SenderSetT;
#else
	typedef std::map<const consensus::RequestId*, size_t, ReqCmpLess>							ClientRequestMapT;
	typedef std::set<consensus::RequestId, SenderCmpLess>										SenderSetT;
#endif
	typedef Stack<size_t>																		SizeTStackT;
private:
	DynamicExecuterT		exe;
	DynamicExecuterExT		exeex;
	
	uint32					consensusval;
		
	uint32					crtval;
	
	ClientRequestMapT		reqmap;
	ClientRequestVectorT	reqvec;
	SizeTStackT				freeposstk;
	SenderSetT				senderset;
	TimerQueue				timerq;
};

#endif

