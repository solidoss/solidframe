#include "example/distributed/consensus/server/serverobject.hpp"
#include "example/distributed/consensus/core/manager.hpp"
#include "example/distributed/consensus/core/signals.hpp"

#include "foundation/ipc/ipcservice.hpp"

#include "foundation/common.hpp"

#include "system/thread.hpp"
#include "system/debug.hpp"
#include "utility/binaryseeker.hpp"

#include <ifaddrs.h>

#include <algorithm>

namespace fdt=foundation;
using namespace std;

/*static*/ ServerParams& ServerParams::the(){
	//TODO: static_problem
	static ServerParams s;
	return s;
}

bool ServerParams::init(int _ipc_port){
	for(StringVectorT::const_iterator it(addrstrvec.begin()); it != addrstrvec.end(); ++it){
		string s(*it);
		size_t pos = s.find_last_of(':');
		if(pos == string::npos){
			err = "An address must be of host:port or host:service";
			return false;
		}
		s[pos] = 0;
		AddrInfo ai(s.c_str(), s.c_str() + pos + 1, 0, AddrInfo::Inet4, AddrInfo::Datagram);
		if(!ai.empty()){
			addrvec.push_back(SocketAddress4(ai.begin()));
		}else{
			err = "No such address [";
			err += s.c_str();
			err += ':';
			err += s.c_str() + pos + 1;
			err += ']';
			return false;
		}
	}
	std::sort(addrvec.begin(), addrvec.end());
	BinarySeeker<> bs;
	
	struct ifaddrs* ifap;
	if(::getifaddrs(&ifap)){
		err = "getifaddrs did not work";
		return false;
	}
	
	struct ifaddrs *it(ifap);
	char host[512];
	char srvc[128];
	
	while(it){
		if(it->ifa_addr && it->ifa_addr->sa_family == AF_INET)
        {
            struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(it->ifa_addr);
            if(addr->sin_addr.s_addr != 0){
            	getnameinfo(it->ifa_addr, sizeof(sockaddr_in), host, 512, srvc, 128, NI_NUMERICHOST | NI_NUMERICSERV);
				idbg("inaddr: name = "<<it->ifa_name<<", addr = "<<host<<":"<<srvc);
				
            }
            SocketAddress4 sa(SockAddrPair(it->ifa_addr, sizeof(sockaddr_in)));
			sa.port(_ipc_port);
			int pos = bs(addrvec.begin(), addrvec.end(), sa);
			if(pos >= 0){
				idx = pos;
				break;
			}
		}
		it = it->ifa_next;
	}
	freeifaddrs(ifap);
	
	return true;
}

std::ostream& ServerParams::print(std::ostream &_ros)const{
	_ros<<"Addresses: ";
	for(AddressVectorT::const_iterator it(addrvec.begin()); it != addrvec.end(); ++it){
		const SocketAddress4 &ra(*it);
		char				host[SocketAddress::MaxSockHostSz];
		char				port[SocketAddress::MaxSockServSz];
		ra.name(
			host,
			SocketAddress::MaxSockHostSz,
			port,
			SocketAddress::MaxSockServSz,
			SocketAddress::NumericService | SocketAddress::NumericHost
		);
		_ros<<host<<':'<<port<<' ';
	}
	_ros<<endl;
	_ros<<"Index: "<<(int)idx<<endl;
	return _ros;
}
std::ostream& operator<<(std::ostream &_ros, const ServerParams &_rsp){
	return _rsp.print(_ros);
}

inline bool ServerObject::ReqCmpEqual::operator()(
	const ConceptSignalIdetifier* const & _req1,
	const ConceptSignalIdetifier* const & _req2
)const{
	return *_req1 == *_req2;
}
inline bool ServerObject::ReqCmpLess::operator()(
	const ConceptSignalIdetifier* const & _req1,
	const ConceptSignalIdetifier* const & _req2
)const{
	return *_req1 < *_req2;
}
inline size_t ServerObject::ReqHash::operator()(
		const ConceptSignalIdetifier* const & _req1
)const{
	return _req1->hash();
}
inline bool ServerObject::SenderCmpEqual::operator()(
	const ConceptSignalIdetifier & _req1,
	const ConceptSignalIdetifier& _req2
)const{
	return _req1.senderEqual(_req2);
}
inline bool ServerObject::SenderCmpLess::operator()(
	const ConceptSignalIdetifier& _req1,
	const ConceptSignalIdetifier& _req2
)const{
	return _req1.senderLess(_req2);
}
inline size_t ServerObject::SenderHash::operator()(
	const ConceptSignalIdetifier& _req1
)const{
	return _req1.senderHash();
}
inline bool ServerObject::TimerDataCmp::operator()(
	const TimerData &_rtd1, const TimerData &_rtd2
)const{
	return _rtd1.timepos > _rtd2.timepos;
}

ServerObject::ServerObject():crtval(1){
	
}
ServerObject::~ServerObject(){
	
}
inline size_t ServerObject::insertClientRequest(DynamicPointer<ConceptSignal> &_rsig){
	if(freeposstk.size()){
		size_t idx(freeposstk.top());
		freeposstk.pop();
		ClientRequest &rcr(clientRequest(idx));
		rcr.sig = _rsig;
		return idx;
	}else{
		size_t idx(reqvec.size());
		reqvec.push_back(ClientRequest(_rsig));
		return idx;
	}
}
inline void ServerObject::eraseClientRequest(size_t _idx){
	ClientRequest &rcr(clientRequest(_idx));
	cassert(rcr.sig.ptr());
	rcr.sig.clear();
	rcr.state = 0;
	++rcr.uid;
	freeposstk.push(_idx);
}
inline ServerObject::ClientRequest& ServerObject::clientRequest(size_t _idx){
	cassert(_idx < reqvec.size());
	return reqvec[_idx];
}
inline const ServerObject::ClientRequest& ServerObject::clientRequest(size_t _idx)const{
	cassert(_idx < reqvec.size());
	return reqvec[_idx];
}
//------------------------------------------------------------
namespace{
static const DynamicRegisterer<ServerObject>	dre;
}
/*static*/ void ServerObject::dynamicRegister(){
	DynamicExecuterT::registerDynamic<ConceptSignal, ServerObject>();
	
	DynamicExecuterExT::registerDynamic<StoreSignal, ServerObject>();
	DynamicExecuterExT::registerDynamic<FetchSignal, ServerObject>();
	DynamicExecuterExT::registerDynamic<EraseSignal, ServerObject>();
}

int ServerObject::execute(ulong _sig, TimeSpec &_tout){
	foundation::Manager &rm(m());
	
	if(signaled()){//we've received a signal
		ulong sm(0);
		{
			Mutex::Locker	lock(rm.mutex(*this));
			sm = grabSignalMask(0);//grab all bits of the signal mask
			if(sm & fdt::S_KILL) return BAD;
			if(sm & fdt::S_SIG){//we have signals
				exe.prepareExecute();
			}
		}
		if(sm & fdt::S_SIG){//we've grabed signals, execute them
			while(exe.hasCurrent()){
				exe.executeCurrent(*this);
				exe.next();
			}
		}
		//now we determine if we return with NOK or we continue
		if(!_sig) return NOK;
	}
	
	return NOK;
}

//------------------------------------------------------------
/*virtual*/ bool ServerObject::signal(DynamicPointer<foundation::Signal> &_sig){
	if(this->state() < 0){
		_sig.clear();
		return false;//no reason to raise the pool thread!!
	}
	exe.push(DynamicPointer<>(_sig));
	return Object::signal(fdt::S_SIG | fdt::S_RAISE);
}


void ServerObject::dynamicExecute(DynamicPointer<> &_dp){
	
}

void ServerObject::dynamicExecute(DynamicPointer<ConceptSignal> &_rsig){
	idbg("received ConceptSignal request");
	if(checkAlreadyReceived(_rsig)) return;
	DynamicPointer<>	dp(_rsig);
	exeex.execute(*this, dp, 1);
}


void ServerObject::dynamicExecute(DynamicPointer<> &_dp, int){
	
}


void ServerObject::dynamicExecute(DynamicPointer<StoreSignal> &_rsig, int){
	idbg("received InsertSignal request");
	const foundation::ipc::ConnectionUid	ipcconid(_rsig->ipcconid);
	
	++crtval;
	_rsig->v = crtval;
	
	DynamicPointer<foundation::Signal>		sigptr(_rsig);
	foundation::ipc::Service::the().sendSignal(sigptr, ipcconid);
}

void ServerObject::dynamicExecute(DynamicPointer<FetchSignal> &_rsig, int){
	idbg("received FetchSignal request");
	const foundation::ipc::ConnectionUid	ipcconid(_rsig->ipcconid);
	DynamicPointer<foundation::Signal>		sigptr(_rsig);
	
	foundation::ipc::Service::the().sendSignal(sigptr, ipcconid);
}

void ServerObject::dynamicExecute(DynamicPointer<EraseSignal> &_rsig, int){
	idbg("received EraseSignal request");
	const foundation::ipc::ConnectionUid	ipcconid(_rsig->ipcconid);
	DynamicPointer<foundation::Signal>		sigptr(_rsig);
	
	foundation::ipc::Service::the().sendSignal(sigptr, ipcconid);
}

bool ServerObject::checkAlreadyReceived(DynamicPointer<ConceptSignal> &_rsig){
	SenderSetT::iterator it(senderset.find(_rsig->id));
	if(it != senderset.end()){
		if(overflowSafeLess(_rsig->id.requid, it->requid)){
			return true;
		}
		senderset.erase(it);
		senderset.insert(_rsig->id);
	}else{
		senderset.insert(_rsig->id);
	}
	return false;
}

void ServerObject::registerClientRequestTimer(const TimeSpec &_rts, size_t _idx){
	
}
size_t ServerObject::popClientRequestTimeout(const TimeSpec &_rts){
	return 0;
}
void ServerObject::scheduleNextClientRequestTimer(TimeSpec &_rts){
	
}
