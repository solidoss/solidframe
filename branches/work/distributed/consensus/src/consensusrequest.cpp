#include "distributed/consensus/consensusrequest.hpp"

#include "foundation/ipc/ipcservice.hpp"
#include "foundation/manager.hpp"

#include "algorithm/serialization/binary.hpp"
#include "algorithm/serialization/idtypemap.hpp"

namespace fdt=foundation;

namespace distributed{
namespace consensus{

bool RequestId::operator<(const RequestId &_rcsi)const{
	if(this->sockaddr < _rcsi.sockaddr){
		return true;
	}else if(_rcsi.sockaddr < this->sockaddr){
		return false;
	}else if(this->senderuid < _rcsi.senderuid){
		return true;
	}else if(_rcsi.senderuid > this->senderuid){
		return false;
	}else return overflow_safe_less(this->requid, _rcsi.requid);
}
bool RequestId::operator==(const RequestId &_rcsi)const{
	return this->sockaddr == _rcsi.sockaddr && 
		this->senderuid == _rcsi.senderuid &&
		this->requid == _rcsi.requid;
}
size_t RequestId::hash()const{
	return sockaddr.hash() ^ this->senderuid.first ^ this->requid;
}
bool RequestId::senderEqual(const RequestId &_rcsi)const{
	return this->sockaddr == _rcsi.sockaddr && 
		this->senderuid == _rcsi.senderuid;
}
bool RequestId::senderLess(const RequestId &_rcsi)const{
	if(this->sockaddr < _rcsi.sockaddr){
		return true;
	}else if(_rcsi.sockaddr < this->sockaddr){
		return false;
	}else return this->senderuid < _rcsi.senderuid;
}
size_t RequestId::senderHash()const{
	return sockaddr.hash() ^ this->senderuid.first;
}
std::ostream &operator<<(std::ostream& _ros, const RequestId &_rreqid){
	_ros<<_rreqid.requid<<','<<' '<<_rreqid.senderuid.first<<','<<' '<<_rreqid.senderuid.second<<','<<' ';
	const SocketAddress4 &ra(_rreqid.sockaddr);
	char				host[SocketAddress::HostNameCapacity];
	char				port[SocketAddress::ServiceNameCapacity];
	ra.name(
		host,
		SocketAddress::HostNameCapacity,
		port,
		SocketAddress::ServiceNameCapacity
		,
		SocketAddress::NumericService | SocketAddress::NumericHost
	);
	_ros<<host<<':'<<port;
	return _ros;
}
//--------------------------------------------------------------
RequestSignal::RequestSignal():waitresponse(false), st(OnSender), sentcount(0){
	idbg("RequestSignal "<<(void*)this);
}
RequestSignal::RequestSignal(const RequestId &_rreqid):waitresponse(false), st(OnSender), sentcount(0),id(_rreqid){
	idbg("RequestSignal "<<(void*)this);
}

RequestSignal::~RequestSignal(){
	idbg("~RequestSignal "<<(void*)this);
	if(waitresponse && !sentcount){
		idbg("failed receiving response "/*<<sentcnt*/);
		fdt::m().signal(fdt::S_KILL | fdt::S_RAISE, id.senderuid);
	}
}

void RequestSignal::ipcReceived(
	foundation::ipc::SignalUid &_rsiguid
){
	//_rsiguid = this->ipcsiguid;
	ipcconid = fdt::ipc::SignalContext::the().connectionuid;
	
	char				host[SocketAddress::HostNameCapacity];
	char				port[SocketAddress::ServiceNameCapacity];
	
	id.sockaddr = fdt::ipc::SignalContext::the().pairaddr;
	
	id.sockaddr.name(
		host,
		SocketAddress::HostNameCapacity,
		port,
		SocketAddress::ServiceNameCapacity,
		SocketAddress::NumericService | SocketAddress::NumericHost
	);
	
	waitresponse = false;
	
	if(st == OnSender){
		st = OnPeer;
		idbg((void*)this<<" on peer: baseport = "<<fdt::ipc::SignalContext::the().baseport<<" host = "<<host<<":"<<port);
		id.sockaddr.port(fdt::ipc::SignalContext::the().baseport);
		//fdt::m().signal(sig, serverUid());
		this->sendThisToConsensusObject();
	}else if(st == OnPeer){
		st = BackOnSender;
		idbg((void*)this<<" back on sender: baseport = "<<fdt::ipc::SignalContext::the().baseport<<" host = "<<host<<":"<<port);
		
		DynamicPointer<fdt::Signal> sig(this);
		
		fdt::m().signal(sig, id.senderuid);
		_rsiguid = this->ipcsiguid;
	}else{
		cassert(false);
	}
}
// void RequestSignal::sendThisToConsensusObject(){
// }
uint32 RequestSignal::ipcPrepare(){
	uint32	rv(0);
	idbg((void*)this);
	if(st == OnSender){
		if(waitresponse){
			rv |= foundation::ipc::Service::WaitResponseFlag;
		}
		rv |= foundation::ipc::Service::SynchronousSendFlag;
		rv |= foundation::ipc::Service::SameConnectorFlag;
	}
	return rv;
}

void RequestSignal::ipcFail(int _err){
	idbg((void*)this<<" sentcount = "<<(int)sentcount<<" err = "<<_err);
}

void RequestSignal::ipcSuccess(){
	Mutex::Locker lock(mutex());
	++sentcount;
	idbg((void*)this<<" sentcount = "<<(int)sentcount);
}


void RequestSignal::use(){
	DynamicShared<fdt::Signal>::use();
	idbg((void*)this<<" usecount = "<<usecount);
}
int RequestSignal::release(){
	int rv = DynamicShared<fdt::Signal>::release();
	idbg((void*)this<<" usecount = "<<usecount);
	return rv;
}
//--------------------------------------------------------------
}//namespace consensus
}//namespace distributed