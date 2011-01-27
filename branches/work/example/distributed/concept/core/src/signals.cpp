#include "example/distributed/concept/core/signals.hpp"
#include "example/distributed/concept/core/manager.hpp"

#include "foundation/ipc/ipcservice.hpp"

#include "system/debug.hpp"

foundation::Manager& m(){
	return foundation::m();
}

const foundation::IndexT& serverIndex(){
	static const foundation::IndexT idx(10);
	return idx;
}


ConceptSignal::ConceptSignal():waitresponse(false), reqid(-1), sentcount(0){
	
}
ConceptSignal::~ConceptSignal(){
	if(waitresponse && !sentcount){
		idbg("failed receiving response "/*<<sentcnt*/);
		m().signal(fdt::S_KILL | fdt::S_RAISE, senderuid);
	}
}

int ConceptSignal::ipcReceived(
	foundation::ipc::SignalUid &_rsiguid,
	const foundation::ipc::ConnectionUid &_rconid,
	const SockAddrPair &_peeraddr, int _peerbaseport
){
	
}
uint32 ConceptSignal::ipcPrepare(){
	if(waitresponse){//on sendeer
		return foundation::ipc::Service::WaitResponseFlag | foundation::ipc::Service::SynchronousSendFlag;
	}else{//on peer
		return 0;
	}
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

InsertSignal::InsertSignal(const std::string&, uint32 _pos){
	
}

FetchSignal::FetchSignal(const std::string&){
	
}

EraseSignal::EraseSignal(const std::string&){
	
}

EraseSignal::EraseSignal(){
	
}
