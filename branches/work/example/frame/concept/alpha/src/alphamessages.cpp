#include "utility/istream.hpp"
#include "utility/ostream.hpp"
#include "utility/iostream.hpp"
#include "utility/dynamicpointer.hpp"
#include "utility/sharedmutex.hpp"

#include "system/timespec.hpp"
#include "system/filedevice.hpp"
#include "system/mutex.hpp"

#include "frame/requestuid.hpp"
#include "frame/ipc/ipcservice.hpp"

#include "core/common.hpp"
#include "core/manager.hpp"
#include "core/messages.hpp"


#include "alphamessages.hpp"
#include "alphasteward.hpp"
#include "system/debug.hpp"

using namespace solid;

namespace concept{

namespace alpha{

const MessageTypeIds& MessageTypeIds::the(const MessageTypeIds *_pids){
	static const MessageTypeIds ids(*_pids);
	return ids;
}
	
//-----------------------------------------------------------------------------------
// RemoteListMessage
//-----------------------------------------------------------------------------------
RemoteListMessage::RemoteListMessage(
	uint32 _tout, uint16 _sentcnt
): ppthlst(NULL),err(-1),tout(_tout), success(0){
	idbg(""<<(void*)this);
	shared_mutex_safe(this);
}
RemoteListMessage::RemoteListMessage(
	const NumberType<1>&
):ppthlst(NULL), err(-1),tout(0), success(0){
	idbg(""<<(void*)this);
	shared_mutex_safe(this);
}
RemoteListMessage::~RemoteListMessage(){
	idbg(""<<(void*)this<<" success = "<<(int)success);
	if(ipcIsOnSender() && success <= 1){
		idbg("failed receiving response");
		Manager::the().notify(frame::S_KILL | frame::S_RAISE, ObjectUidT(fromv.first, fromv.second));
	}
	delete ppthlst;
}
size_t RemoteListMessage::use(){
	size_t rv = DynamicShared<frame::ipc::Message>::use();
	idbg(""<<(void*)this<<" usecount = "<<usecount);
	return rv;
}
size_t RemoteListMessage::release(){
	size_t rv = DynamicShared<frame::ipc::Message>::release();
	idbg(""<<(void*)this<<" usecount = "<<usecount);
	return rv;
}
uint32 RemoteListMessage::ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx){
	Locker<Mutex>						lock(shared_mutex(this));
	
	if(success == 0) success = 1;//wait
	idbg(""<<(void*)this);
	if(ipcIsOnSender()){//on sender
		return frame::ipc::WaitResponseFlag /*| frame::ipc::SynchronousSendFlag*/;
	}else{
		return 0/*frame::ipc::Service::SynchronousSendFlag*/;// on peer
	}
}
void RemoteListMessage::ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, frame::ipc::Message::MessagePointerT &_rmsgptr){
	idbg(""<<(void*)this);
	conid = frame::ipc::ConnectionContext::the().connectionuid;
	DynamicPointer<frame::Message>	msgptr(_rmsgptr);
	if(ipcIsOnReceiver()){//on peer
		idbg("Received RemoteListMessage on peer");
		Steward::the().sendMessage(msgptr);
	}else if(ipcIsBackOnSender()){//back on sender
		Manager::the().notify(msgptr, fromv);
	}
}
void RemoteListMessage::ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err){
	Locker<Mutex> lock(shared_mutex(this));
	err = _err;
	if(!_err){
		success = 2;
	}else{
		if(ipcIsOnSender()){
			idbg("failed on sender "<<(void*)this);
		}else{
			idbg("failed on peer");
		}
	}
}

//-----------------------------------------------------------------------------------
// FetchMasterMessage
//-----------------------------------------------------------------------------------

FetchMasterMessage::~FetchMasterMessage(){
	delete pmsg;
	idbg((void*)this<<"");
}

void FetchMasterMessage::ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err){
	idbg((void*)this<<"");
	if(_err){
		Manager::the().notify(frame::S_RAISE | frame::S_KILL, ObjectUidT(fromv.first, fromv.second));
	}
}
void FetchMasterMessage::print()const{
	idbg((void*)this<<" FetchMasterMessage:");
	idbg("state = "<<state<<" streamsz = "<<streamsz<<" requid = "<<requid<<" fname = "<<fname);
	idbg("fromv.first = "<<fromv.first<<" fromv.second = "<<fromv.second);
	idbg("fuid.first = "<<fuid.first<<" fuid.second = "<<fuid.second);
	idbg("tmpfuid.first = "<<tmpfuid.first<<" tmpfuid.second = "<<tmpfuid.second);
}


void FetchMasterMessage::ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, frame::ipc::Message::MessagePointerT &_rmsgptr){
	DynamicPointer<frame::Message> msgptr(_rmsgptr);
	conid = frame::ipc::ConnectionContext::the().connectionuid;;
	state = Received;
	idbg("received master signal");
	print();
	Steward::the().sendMessage(msgptr);
}
//-----------------------------------------------------------------------------------
// FetchSlaveMessage
//-----------------------------------------------------------------------------------

FetchSlaveMessage::FetchSlaveMessage(): fromv(0xffffffff, 0xffffffff), ios(/*0*/), filesz(-10), streamsz(-1), requid(0){
	idbg(""<<(void*)this);
	serialized = false;
}

FetchSlaveMessage::~FetchSlaveMessage(){
	idbg(""<<(void*)this);
	cassert(serialized);
	print();
// 	if(fromv.first != 0xffffffff){
// 		idbg("unsuccessfull sent");
// 		//signal fromv object to die
// 		Manager::the().signalObject(fromv.first, fromv.second, frame::S_RAISE | frame::S_KILL);
// 	}
}
void FetchSlaveMessage::print()const{
	idbg((void*)this<<" FetchSlaveMessage:");
	idbg("filesz = "<<this->filesz<<" filepos = "<<filepos<<" requid = "<<requid);
	idbg("streamsz = "<<this->streamsz<<" streampos = "<<this->streampos);
	idbg("fuid.first = "<<fuid.first<<" fuid.second = "<<fuid.second);
	idbg("msguid.first = "<<msguid.first<<" msguid.second = "<<msguid.second);
}
int FetchSlaveMessage::sent(const frame::ipc::ConnectionUid &_rconid){
	idbg((void*)this<<"");
	fromv.first = 0xffffffff;
	return AsyncError;
}
void FetchSlaveMessage::ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, frame::ipc::Message::MessagePointerT &_rmsgptr){
	DynamicPointer<frame::Message> msgptr(_rmsgptr);
	conid = frame::ipc::ConnectionContext::the().connectionuid;
	if(ipcIsOnReceiver()){
		idbg((void*)this<<" Received FetchSlaveMessage on peer");
		print();
		ObjectUidT	ttov;
		Steward::the().sendMessage(msgptr);
	}else if(ipcIsBackOnSender()){
		idbg((void*)this<<" Received FetchSlaveMessage on sender");
		print();
		ipcResetState();
		Manager::the().notify(msgptr, ObjectUidT(tov.first, tov.second));
	}else{
		cassert(false);
	}
}

bool FetchSlaveMessage::initOutputStream(){
	if(!ipcIsOnReceiver()){
		frame::RequestUid					requid;
		//Manager::the().fileManager().stream(outs, fuid, requid, frame::file::Manager::Forced);
		idbg((void*)this<<" Create deserialization streamptr");
		ERROR_NS::error_code		err;
		frame::file::FilePointerT	fptr = Manager::the().fileStore().shared(fuid, err);
		ios.device(fptr);
		return true;
	}else{
		return false;
	}
}

void FetchSlaveMessage::clearOutputStream(){
	ios.flush();
	//ios.device().clear();
}

}//namespace alpha

}//namespace concept
