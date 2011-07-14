#include "example/distributed/consensus/server/serverobject.hpp"
#include "example/distributed/consensus/core/consensusmanager.hpp"
#include "example/distributed/consensus/core/consensusrequests.hpp"

#include "foundation/ipc/ipcservice.hpp"

#include "foundation/common.hpp"

#include "system/thread.hpp"
#include "system/debug.hpp"
#include "utility/binaryseeker.hpp"

#include <ifaddrs.h>

#include <algorithm>

namespace fdt=foundation;
using namespace std;

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

//------------------------------------------------------------
namespace{
static const DynamicRegisterer<ServerObject>	dre;
}
/*static*/ void ServerObject::dynamicRegister(){
	DynamicExecuterExT::registerDynamic<StoreRequest, ServerObject>();
	DynamicExecuterExT::registerDynamic<FetchRequest, ServerObject>();
	DynamicExecuterExT::registerDynamic<EraseRequest, ServerObject>();
}
//------------------------------------------------------------
ServerObject::ServerObject(){
	
}
ServerObject::~ServerObject(){
	
}

/*virtual*/ void ServerObject::doAccept(DynamicPointer<consensus::RequestSignal> &_rsig){
	idbg("accepting consensus::RequestSignal request");
	DynamicPointer<>	dp(_rsig);
	exeex.execute(*this, dp, 1);
}

void ServerObject::dynamicExecute(DynamicPointer<> &_dp, int){
	
}

void ServerObject::dynamicExecute(DynamicPointer<StoreRequest> &_rsig, int){
	idbg("received InsertSignal request");
	const foundation::ipc::ConnectionUid	ipcconid(_rsig->ipcconid);
	
	_rsig->v = this->acceptId();
	
	DynamicPointer<foundation::Signal>		sigptr(_rsig);
	foundation::ipc::Service::the().sendSignal(sigptr, ipcconid);
}

void ServerObject::dynamicExecute(DynamicPointer<FetchRequest> &_rsig, int){
	idbg("received FetchSignal request");
	const foundation::ipc::ConnectionUid	ipcconid(_rsig->ipcconid);
	DynamicPointer<foundation::Signal>		sigptr(_rsig);
	
	foundation::ipc::Service::the().sendSignal(sigptr, ipcconid);
}

void ServerObject::dynamicExecute(DynamicPointer<EraseRequest> &_rsig, int){
	idbg("received EraseSignal request");
	const foundation::ipc::ConnectionUid	ipcconid(_rsig->ipcconid);
	DynamicPointer<foundation::Signal>		sigptr(_rsig);
	
	foundation::ipc::Service::the().sendSignal(sigptr, ipcconid);
}

