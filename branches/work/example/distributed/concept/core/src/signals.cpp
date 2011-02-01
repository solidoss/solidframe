#include "example/distributed/concept/core/signals.hpp"
#include "example/distributed/concept/core/manager.hpp"

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

	TypeMapper::map<InsertSignal, BinSerializer, BinDeserializer>();
	TypeMapper::map<FetchSignal, BinSerializer, BinDeserializer>();
	TypeMapper::map<EraseSignal, BinSerializer, BinDeserializer>();
}

//--------------------------------------------------------------
ConceptSignal::ConceptSignal():waitresponse(false), st(OnSender), requid(-1), sentcount(0){
	
}
ConceptSignal::~ConceptSignal(){
	if(waitresponse && !sentcount){
		idbg("failed receiving response "/*<<sentcnt*/);
		m().signal(fdt::S_KILL | fdt::S_RAISE, senderuid);
	}
}

bool ConceptSignal::ipcReceived(
	foundation::ipc::SignalUid &_rsiguid,
	const foundation::ipc::ConnectionUid &_rconid,
	const SockAddrPair &_peeraddr, int _peerbaseport
){
	_rsiguid = this->ipcsiguid;
	ipcconid = _rconid;
	DynamicPointer<fdt::Signal> sig(this);
	
	if(st == OnSender){
		st = OnPeer;
		m().signal(sig, serverUid());
	}else if(st == OnPeer){
		st == BackOnSender;
		m().signal(sig, senderuid);
	}else{
		cassert(false);
	}
	return false;
}
uint32 ConceptSignal::ipcPrepare(){
	uint32 	rv(0);
	if(st == OnSender){
		if(waitresponse){
			rv |= foundation::ipc::Service::WaitResponseFlag;
		}
		rv |= foundation::ipc::Service::SynchronousSendFlag;
	}
	
	return rv;
}

void ConceptSignal::ipcFail(int _err){
	
}

void ConceptSignal::use(){
	DynamicShared<fdt::Signal>::use();
	idbg(""<<(void*)this<<" usecount = "<<usecount);
}
int ConceptSignal::release(){
	int rv = DynamicShared<fdt::Signal>::release();
	idbg(""<<(void*)this<<" usecount = "<<usecount);
	return rv;
}
//--------------------------------------------------------------
InsertSignal::InsertSignal(const std::string&, uint32 _pos){
	
}
InsertSignal::InsertSignal(){
	
}
//--------------------------------------------------------------
FetchSignal::FetchSignal(const std::string&){
	
}
FetchSignal::FetchSignal(){
	
}
//--------------------------------------------------------------
EraseSignal::EraseSignal(const std::string&){
	
}
EraseSignal::EraseSignal(){
	
}
//--------------------------------------------------------------