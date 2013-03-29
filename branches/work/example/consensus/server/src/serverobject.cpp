#include "example/consensus/server/serverobject.hpp"
#include "example/consensus/core/consensusmanager.hpp"
#include "example/consensus/core/consensusrequests.hpp"

#include "frame/ipc/ipcservice.hpp"

#include "frame/common.hpp"

#include "system/thread.hpp"
#include "system/debug.hpp"
#include "utility/binaryseeker.hpp"

#include <ifaddrs.h>

#include <algorithm>

using namespace std;
using namespace solid;

bool ServerParams::init(int _ipc_port){
	for(StringVectorT::const_iterator it(addrstrvec.begin()); it != addrstrvec.end(); ++it){
		string s(*it);
		size_t pos = s.find_last_of(':');
		if(pos == string::npos){
			err = "An address must be of host:port or host:service";
			return false;
		}
		s[pos] = 0;
		ResolveData rd = synchronous_resolve(
			s.c_str(), s.c_str() + pos + 1, 0,
			SocketInfo::Inet4, SocketInfo::Datagram
		);
		if(!rd.empty()){
			addrvec.push_back(SocketAddressInet4(rd.begin()));
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
	
	struct ifaddrs* ifap(NULL);
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
            SocketAddressInet4 sa(SocketAddressStub(it->ifa_addr, sizeof(sockaddr_in)));
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
	quorum = addrvec.size()/2 + 1;
	
	return true;
}

std::ostream& ServerParams::print(std::ostream &_ros)const{
	_ros<<"Addresses: [";
	for(AddressVectorT::const_iterator it(addrvec.begin()); it != addrvec.end(); ++it){
		const SocketAddressInet4	&ra(*it);
		char						host[SocketInfo::HostStringCapacity];
		char						port[SocketInfo::ServiceStringCapacity];
		ra.toString(
			host,
			SocketInfo::HostStringCapacity,
			port,
			SocketInfo::ServiceStringCapacity
			,
			SocketInfo::NumericService | SocketInfo::NumericHost
		);
		_ros<<host<<':'<<port<<' ';
	}
	_ros<<"] Index: "<<(int)idx<<" Qorum: "<<(int)quorum;
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
/*static*/void ServerObject::registerMessages(){
	Object::registerMessages();
}
ServerObject::ServerObject(){
	
}
ServerObject::~ServerObject(){
	
}

/*virtual*/ void ServerObject::accept(DynamicPointer<solid::consensus::WriteRequestMessage> &_rmsgptr){
	idbg("accepting consensus::WriteRequestMessage request");
	DynamicPointer<>	dp(_rmsgptr);
	exeex.execute(*this, dp, 1);
}

/*virtual*/ int ServerObject::recovery(){
	//use enterRunState() and return OK when done recovery
	this->enterRunState();
	return OK;
}

void ServerObject::dynamicExecute(DynamicPointer<> &_dp, int){
	
}

void ServerObject::dynamicExecute(DynamicPointer<StoreRequest> &_rmsgptr, int){
	const frame::ipc::ConnectionUid	ipcconid(_rmsgptr->ipcconid);
	
	_rmsgptr->v = this->acceptId();

	idbg("StoreRequest: v = "<<_rmsgptr->v<<" for request "<<_rmsgptr->id);
	
	DynamicPointer<frame::Message>		msgptr(_rmsgptr);
	//TODO
	//foundation::ipc::Service::the().sendSignal(msgptr, ipcconid);
}

void ServerObject::dynamicExecute(DynamicPointer<FetchRequest> &_rmsgptr, int){
	idbg("received FetchRequest");
	const frame::ipc::ConnectionUid	ipcconid(_rmsgptr->ipcconid);
	DynamicPointer<frame::Message>	msgptr(_rmsgptr);
	//TODO:
	//foundation::ipc::Service::the().sendSignal(sigptr, ipcconid);
}

void ServerObject::dynamicExecute(DynamicPointer<EraseRequest> &_rmsgptr, int){
	idbg("received EraseRequest");
	const frame::ipc::ConnectionUid	ipcconid(_rmsgptr->ipcconid);
	DynamicPointer<frame::Message>	msgptr(_rmsgptr);
	
	//TODO:
	//foundation::ipc::Service::the().sendSignal(sigptr, ipcconid);
}
