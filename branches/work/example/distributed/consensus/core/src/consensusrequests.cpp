#include "example/distributed/consensus/core/consensusrequests.hpp"
#include "example/distributed/consensus/core/consensusmanager.hpp"

#include "foundation/ipc/ipcservice.hpp"

#include "algorithm/serialization/binary.hpp"
#include "algorithm/serialization/idtypemap.hpp"


#include "system/debug.hpp"

//--------------------------------------------------------------
foundation::Manager& m(){
	return foundation::m();
}
//--------------------------------------------------------------
const foundation::ObjectUidT& serverUid(){
	static const foundation::ObjectUidT uid(fdt::make_object_uid(11, 10, 0));
	return uid;
}
//--------------------------------------------------------------
void mapSignals(){
	typedef serialization::TypeMapper					TypeMapper;
	typedef serialization::bin::Serializer				BinSerializer;
	typedef serialization::bin::Deserializer			BinDeserializer;

	TypeMapper::map<StoreRequest, BinSerializer, BinDeserializer>();
	TypeMapper::map<FetchRequest, BinSerializer, BinDeserializer>();
	TypeMapper::map<EraseRequest, BinSerializer, BinDeserializer>();
}
//--------------------------------------------------------------
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
		m().signal(fdt::S_KILL | fdt::S_RAISE, id.senderuid);
	}
}

void RequestSignal::ipcReceived(
	foundation::ipc::SignalUid &_rsiguid
){
	DynamicPointer<fdt::Signal> sig(this);
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
		m().signal(sig, serverUid());
	}else if(st == OnPeer){
		st == BackOnSender;
		idbg((void*)this<<" back on sender: baseport = "<<fdt::ipc::SignalContext::the().baseport<<" host = "<<host<<":"<<port);
		m().signal(sig, id.senderuid);
		_rsiguid = this->ipcsiguid;
	}else{
		cassert(false);
	}
}
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

//--------------------------------------------------------------
StoreRequest::StoreRequest(const std::string&, uint32 _pos):v(0){
	idbg("");
}
StoreRequest::StoreRequest(){
	idbg("");
}
//--------------------------------------------------------------
FetchRequest::FetchRequest(const std::string&){
	idbg("");
}
FetchRequest::FetchRequest(){
	idbg("");
}
//--------------------------------------------------------------
EraseRequest::EraseRequest(const std::string&){
	idbg("");
}
EraseRequest::EraseRequest(){
	idbg("");
}
//--------------------------------------------------------------