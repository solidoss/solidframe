#include "example/distributed/consensus/core/signals.hpp"
#include "example/distributed/consensus/core/manager.hpp"

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

	TypeMapper::map<StoreSignal, BinSerializer, BinDeserializer>();
	TypeMapper::map<FetchSignal, BinSerializer, BinDeserializer>();
	TypeMapper::map<EraseSignal, BinSerializer, BinDeserializer>();
}

//--------------------------------------------------------------
ConceptSignal::ConceptSignal():waitresponse(false), st(OnSender), requid(-1), sentcount(0){
	idbg("ConceptSignal "<<(void*)this);
}
ConceptSignal::~ConceptSignal(){
	idbg("~ConceptSignal "<<(void*)this);
	if(waitresponse && !sentcount){
		idbg("failed receiving response "/*<<sentcnt*/);
		m().signal(fdt::S_KILL | fdt::S_RAISE, senderuid);
	}
}

void ConceptSignal::ipcReceived(
	foundation::ipc::SignalUid &_rsiguid
){
	DynamicPointer<fdt::Signal> sig(this);
	//_rsiguid = this->ipcsiguid;
	ipcconid = fdt::ipc::SignalContext::the().connectionuid;
	waitresponse = false;
	if(st == OnSender){
		st = OnPeer;
		idbg((void*)this<<" on peer");
		m().signal(sig, serverUid());
	}else if(st == OnPeer){
		st == BackOnSender;
		idbg((void*)this<<" back on sender");
		m().signal(sig, senderuid);
		_rsiguid = this->ipcsiguid;
	}else{
		cassert(false);
	}
}
uint32 ConceptSignal::ipcPrepare(){
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

void ConceptSignal::ipcFail(int _err){
	idbg((void*)this<<" sentcount = "<<(int)sentcount<<" err = "<<_err);
}

void ConceptSignal::ipcSuccess(){
	Mutex::Locker lock(mutex());
	++sentcount;
	idbg((void*)this<<" sentcount = "<<(int)sentcount);
}


void ConceptSignal::use(){
	DynamicShared<fdt::Signal>::use();
	idbg((void*)this<<" usecount = "<<usecount);
}
int ConceptSignal::release(){
	int rv = DynamicShared<fdt::Signal>::release();
	idbg((void*)this<<" usecount = "<<usecount);
	return rv;
}
//--------------------------------------------------------------
StoreSignal::StoreSignal(const std::string&, uint32 _pos):v(0){
	idbg("");
}
StoreSignal::StoreSignal(){
	idbg("");
}
//--------------------------------------------------------------
FetchSignal::FetchSignal(const std::string&){
	idbg("");
}
FetchSignal::FetchSignal(){
	idbg("");
}
//--------------------------------------------------------------
EraseSignal::EraseSignal(const std::string&){
	idbg("");
}
EraseSignal::EraseSignal(){
	idbg("");
}
//--------------------------------------------------------------