#include "example/consensus/core/consensusrequests.hpp"
#include "example/consensus/core/consensusmanager.hpp"

#include "frame/ipc/ipcservice.hpp"

#include "system/debug.hpp"

using namespace solid;
//--------------------------------------------------------------
void mapSignals(){
	//TODO:
	//frame::ipc::Service::the().typeMapper().insert<StoreRequest>();
	//fdt::ipc::Service::the().typeMapper().insert<FetchRequest>();
	//fdt::ipc::Service::the().typeMapper().insert<EraseRequest>();
}
//--------------------------------------------------------------
StoreRequest::StoreRequest(const std::string&, uint32 _pos):v(0){
	idbg("");
}
StoreRequest::StoreRequest(){
	idbg("");
}
/*virtual*/ void StoreRequest::notifyConsensusObjectWithThis(){
	DynamicPointer<frame::Message> msgptr(this);
}
/*virtual*/ void StoreRequest::notifySenderObjectWithThis(){
	DynamicPointer<frame::Message> msgptr(this);
}
/*virtual*/ void StoreRequest::notifySenderObjectWithFail(){
	DynamicPointer<frame::Message> msgptr(this);
}
//--------------------------------------------------------------
FetchRequest::FetchRequest(const std::string&){
	idbg("");
}
FetchRequest::FetchRequest(){
	idbg("");
}
/*virtual*/ void FetchRequest::notifyConsensusObjectWithThis(){
	DynamicPointer<frame::Message> msgptr(this);
}
/*virtual*/ void FetchRequest::notifySenderObjectWithThis(){
	DynamicPointer<frame::Message> msgptr(this);
}
/*virtual*/ void FetchRequest::notifySenderObjectWithFail(){
	DynamicPointer<frame::Message> msgptr(this);
}
//--------------------------------------------------------------
EraseRequest::EraseRequest(const std::string&){
	idbg("");
}
EraseRequest::EraseRequest(){
	idbg("");
}
/*virtual*/ void EraseRequest::notifyConsensusObjectWithThis(){
	DynamicPointer<frame::Message> msgptr(this);
}
/*virtual*/ void EraseRequest::notifySenderObjectWithThis(){
	DynamicPointer<frame::Message> msgptr(this);
}
/*virtual*/ void EraseRequest::notifySenderObjectWithFail(){
	DynamicPointer<frame::Message> msgptr(this);
}
//--------------------------------------------------------------
