#include "example/consensus/core/consensusrequests.hpp"
#include "example/consensus/core/consensusmanager.hpp"

#include "consensus/consensusregistrar.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/manager.hpp"

#include "system/debug.hpp"

using namespace solid;

namespace solid{namespace serialization{namespace binary{
template <class S, class Ctx>
void serialize(S &_s, frame::ObjectUidT &_t, Ctx &_ctx){
	_s.push(_t.first, "first").push(_t.second, "second");
}
}}}

//--------------------------------------------------------------
void mapSignals(frame::ipc::Service &_rsvc){
	_rsvc.registerMessageType<StoreRequest>();
	_rsvc.registerMessageType<FetchRequest>();
	_rsvc.registerMessageType<EraseRequest>();
}
//--------------------------------------------------------------
StoreRequest::StoreRequest(const std::string&, uint32 _pos):v(0){
	idbg("");
}
StoreRequest::StoreRequest(){
	idbg("");
}
/*virtual*/ void StoreRequest::consensusNotifyServerWithThis(){
	DynamicPointer<frame::Message> msgptr(this);
	frame::Manager::specific().notify(msgptr, consensus::Registrar::the().objectUid(0));
}
/*virtual*/ void StoreRequest::consensusNotifyClientWithThis(){
	DynamicPointer<frame::Message> msgptr(this);
	frame::Manager::specific().notify(msgptr, this->id.senderuid);
}
/*virtual*/ void StoreRequest::consensusNotifyClientWithFail(){
	frame::Manager::specific().notify(frame::S_KILL | frame::S_RAISE, this->id.senderuid);
}
//--------------------------------------------------------------
FetchRequest::FetchRequest(const std::string&){
	idbg("");
}
FetchRequest::FetchRequest(){
	idbg("");
}
/*virtual*/ void FetchRequest::consensusNotifyServerWithThis(){
	DynamicPointer<frame::Message> msgptr(this);
	frame::Manager::specific().notify(msgptr, consensus::Registrar::the().objectUid(0));
}
/*virtual*/ void FetchRequest::consensusNotifyClientWithThis(){
	DynamicPointer<frame::Message> msgptr(this);
	frame::Manager::specific().notify(msgptr, this->id.senderuid);
}
/*virtual*/ void FetchRequest::consensusNotifyClientWithFail(){
	frame::Manager::specific().notify(frame::S_KILL | frame::S_RAISE, this->id.senderuid);
}
//--------------------------------------------------------------
EraseRequest::EraseRequest(const std::string&){
	idbg("");
}
EraseRequest::EraseRequest(){
	idbg("");
}
/*virtual*/ void EraseRequest::consensusNotifyServerWithThis(){
	DynamicPointer<frame::Message> msgptr(this);
	frame::Manager::specific().notify(msgptr, consensus::Registrar::the().objectUid(0));
}
/*virtual*/ void EraseRequest::consensusNotifyClientWithThis(){
	DynamicPointer<frame::Message> msgptr(this);
	frame::Manager::specific().notify(msgptr, this->id.senderuid);
}
/*virtual*/ void EraseRequest::consensusNotifyClientWithFail(){
	frame::Manager::specific().notify(frame::S_KILL | frame::S_RAISE, this->id.senderuid);
}
//--------------------------------------------------------------
