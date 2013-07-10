#include "utility/istream.hpp"
#include "utility/ostream.hpp"
#include "utility/iostream.hpp"
#include "utility/dynamicpointer.hpp"
#include "utility/sharedmutex.hpp"

#include "system/timespec.hpp"
#include "system/filedevice.hpp"
#include "system/mutex.hpp"

#include "frame/messagesteward.hpp"
#include "frame/requestuid.hpp"
#include "frame/file/filemanager.hpp"
#include "frame/ipc/ipcservice.hpp"

#include "core/common.hpp"
#include "core/manager.hpp"
#include "core/messages.hpp"


#include "alphamessages.hpp"
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
): ppthlst(NULL),err(-1),tout(_tout), success(0), ipcstatus(IpcOnSender){
	idbg(""<<(void*)this);
	shared_mutex_safe(this);
}
RemoteListMessage::RemoteListMessage(
	const NumberType<1>&
):ppthlst(NULL), err(-1),tout(0), success(0), ipcstatus(IpcOnSender){
	idbg(""<<(void*)this);
	shared_mutex_safe(this);
}
RemoteListMessage::~RemoteListMessage(){
	idbg(""<<(void*)this<<" success = "<<(int)success<<" ipcstatus = "<<(int)ipcstatus);
	if(ipcstatus == IpcOnSender && success <= 1){
		idbg("failed receiving response");
		Manager::the().notify(frame::S_KILL | frame::S_RAISE, ObjectUidT(fromv.first, fromv.second));
	}
	delete ppthlst;
}
size_t RemoteListMessage::use(){
	size_t rv = DynamicShared<frame::Message>::use();
	idbg(""<<(void*)this<<" usecount = "<<usecount);
	return rv;
}
size_t RemoteListMessage::release(){
	size_t rv = DynamicShared<frame::Message>::release();
	idbg(""<<(void*)this<<" usecount = "<<usecount);
	return rv;
}
uint32 RemoteListMessage::ipcPrepare(){
	const frame::ipc::ConnectionContext	&rmsgctx(frame::ipc::ConnectionContext::the());
	Locker<Mutex>						lock(shared_mutex(this));
	
	if(success == 0) success = 1;//wait
	idbg(""<<(void*)this<<" msguid = "<<rmsgctx.msgid.idx<<' '<<rmsgctx.msgid.uid<<" ipcstatus = "<<(int)ipcstatus);
	if(ipcstatus == IpcOnSender){//on sender
		return frame::ipc::WaitResponseFlag /*| frame::ipc::SynchronousSendFlag*/;
	}else{
		return 0/*frame::ipc::Service::SynchronousSendFlag*/;// on peer
	}
}
void RemoteListMessage::ipcReceive(
	frame::ipc::MessageUid &_rmsguid
){
	DynamicPointer<frame::Message> msgptr(this);
	idbg(""<<(void*)this<<" msguid = "<<msguid.idx<<' '<<msguid.uid);
	conid = frame::ipc::ConnectionContext::the().connectionuid;
	++ipcstatus;
	if(ipcstatus == IpcOnPeer){//on peer
		idbg("Received RemoteListMessage on peer");
		Manager::the().notify(msgptr, Manager::the().readMessageStewardUid());
	}else{//on sender
		cassert(ipcstatus == IpcBackOnSender);
		idbg("Received RemoteListMessage back on sender");
		_rmsguid = msguid;
		Manager::the().notify(msgptr, ObjectUidT(fromv.first, fromv.second));
	}
}
void RemoteListMessage::ipcComplete(int _err){
	Locker<Mutex> lock(shared_mutex(this));
	err = _err;
	if(!_err){
		success = 2;
	}else{
		if(ipcstatus == IpcOnSender){
			idbg("failed on sender "<<(void*)this);
		}else{
			idbg("failed on peer");
		}
	}
}

int RemoteListMessage::execute(
	DynamicPointer<Message> &_rmsgptr,
	uint32 _evs,
	frame::MessageSteward&,
	const MessageUidT &,
	TimeSpec &_rts
){
	if(tout){
		idbg("sleep for "<<tout<<" mseconds");
		_rts += tout;
		tout = 0;
		return NOK;
	}
	idbg("done sleeping");
	
	fs::directory_iterator			it,end;
	fs::path						pth(strpth.c_str()/*, fs::native*/);
	
	ppthlst = new RemoteList::PathListT;
	strpth.clear();
	
	if(!exists( pth ) || !is_directory(pth)){
		err = -1;
		Manager::the().ipc().sendMessage(_rmsgptr, conid);
		return BAD;
	}
	
	try{
		it = fs::directory_iterator(pth);
	}catch ( const std::exception & ex ){
		idbg("dir_iterator exception :"<<ex.what());
		err = -1;
		strpth = ex.what();
		Manager::the().ipc().sendMessage(_rmsgptr, MessageTypeIds::the().remotelistresponse, conid);
		return BAD;
	}
	
	while(it != end){
		ppthlst->push_back(std::pair<String, int64>(it->path().c_str(), -1));
		if(is_directory(*it)){
		}else{
			ppthlst->back().second = FileDevice::size(it->path().c_str());
		}
		++it;
	}
	err = 0;
	//Thread::sleep(1000 * 20);
	Manager::the().ipc().sendMessage(_rmsgptr, MessageTypeIds::the().remotelistresponse, conid);
	return BAD;
}

//-----------------------------------------------------------------------------------
// FetchMasterMessage
//-----------------------------------------------------------------------------------

FetchMasterMessage::~FetchMasterMessage(){
	delete pmsg;
	idbg((void*)this<<"");
}

void FetchMasterMessage::ipcComplete(int _err){
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

uint32 FetchMasterMessage::ipcPrepare(){
	return 0;//frame::ipc::Service::SynchronousSendFlag;
}


void FetchMasterMessage::ipcReceive(
	frame::ipc::MessageUid &_rmsguid
){
	DynamicPointer<frame::Message> msgptr(this);
	conid = frame::ipc::ConnectionContext::the().connectionuid;;
	state = Received;
	idbg("received master signal");
	print();
	Manager::the().notify(msgptr, Manager::the().readMessageStewardUid());
}
/*
	The state machine running on peer
*/
int FetchMasterMessage::execute(
	DynamicPointer<Message> &_rmsgptr,
	uint32 _evs,
	frame::MessageSteward& _rce,
	const MessageUidT &_msguid,
	TimeSpec &_rts
){
	Manager &rm(Manager::the());
	cassert(!(_evs & frame::TIMEOUT));
	switch(state){
		case Received:{
			idbg((void*)this<<" try to open file "<<fname<<" _msguid = "<<_msguid.first<<","<<_msguid.second);
			//try to get a stream for the file:
			frame::RequestUid reqid(_rce.id(), rm.id(_rce).second, _msguid.first, _msguid.second);
			switch(rm.fileManager().stream(ins, fuid, reqid, fname.c_str())){
				case BAD://ouch
					state = SendError;
					idbg("open failed");
					return OK;
				case OK:
					idbg("open succeded");
					state = SendFirstStream;
					return OK;
				case NOK:
					idbg("open wait");
					return NOK;//wait the stream - no timeout
			}
		}break;
		case SendFirstStream:{
			idbg((void*)this<<" send first stream");
			FetchSlaveMessage				*pmsg(new FetchSlaveMessage);
			DynamicPointer<frame::Message>	msgptr(pmsg);
			
			this->filesz = ins->size();
			pmsg->tov = fromv;
			pmsg->filesz = this->filesz;
			pmsg->streamsz = this->streamsz;
			if(pmsg->streamsz > pmsg->filesz){
				pmsg->streamsz = pmsg->filesz;
			}
			pmsg->msguid = _msguid;
			pmsg->requid = requid;
			pmsg->fuid = tmpfuid;
			idbg("filesz = "<<this->filesz<<" inpos = "<<filepos);
			this->filesz -= pmsg->streamsz;
			this->filepos += pmsg->streamsz;
			frame::RequestUid reqid(_rce.id(), rm.id(_rce).second, _msguid.first, _msguid.second);
			rm.fileManager().stream(pmsg->ins, fuid, requid, frame::file::Manager::NoWait);
			pmsg = NULL;
			if(!this->filesz){
				idbg("connector was destroyed or filesz = "<<this->filesz);
				return BAD;
			}
			rm.ipc().sendMessage(msgptr, conid);
			
			idbg("wait for streams");
			//TODO: put here timeout! - wait for signals
			//_rts.add(30);
			return NOK;
		}
		case SendNextStream:{
			idbg((void*)this<<" send next stream");
			DynamicPointer<frame::Message> msgptr(pmsg);
			pmsg->tov = fromv;
			pmsg->filesz = this->filesz;
			//pmsg->sz = FetchChunkSize;
			if(pmsg->streamsz > this->filesz){
				pmsg->streamsz = this->filesz;
			}
			pmsg->msguid = _msguid;
			//pmsg->fuid = tmpfuid;
			idbg("filesz = "<<this->filesz<<" filepos = "<<this->filepos);
			this->filesz -= pmsg->streamsz;
			frame::RequestUid reqid(_rce.id(), rm.id(_rce).second, _msguid.first, _msguid.second);
			rm.fileManager().stream(pmsg->ins, fuid, requid, frame::file::Manager::NoWait);
			pmsg->ins->seek(this->filepos);
			this->filepos += pmsg->streamsz;
			cassert(pmsg->ins);
			pmsg = NULL;
			rm.ipc().sendMessage(msgptr, conid);
			if(!this->filesz){
				idbg("connector was destroyed or filesz = "<<this->filesz);
				return BAD;
			}
			idbg("wait for streams");
			//TODO: put here timeout! - wait for signals
			//_rts.add(30);
			return NOK;
		}
		case SendError:{
			idbg((void*)this<<" sending error");
			FetchSlaveMessage 				*pmsg = new FetchSlaveMessage;
			DynamicPointer<frame::Message>	msgptr(pmsg);
			pmsg->tov = fromv;
			pmsg->filesz = -1;
			rm.ipc().sendMessage(msgptr, conid);
			return BAD;
		}
	}
	return BAD;
}

int FetchMasterMessage::receiveMessage(
	DynamicPointer<Message> &_rmsgptr,
	const ObjectUidT& _from,
	const frame::ipc::ConnectionUid *_conid
){
	if(_rmsgptr->dynamicTypeId() == InputStreamMessage::staticTypeId()){
		idbg((void*)this<<" Received stream");
		InputStreamMessage &rsig(*static_cast<InputStreamMessage*>(_rmsgptr.get()));
		ins = rsig.sptr;
		fuid = rsig.fileuid;
		state = SendFirstStream;
	}else /*if(_rmsgptr->dynamicTypeId() == OutputStreamMessage::staticTypeId()){
		OutputStreamMessage &rsig(*static_cast<OutputStreamMessage*>(_rmsgptr.get()));
	}else if(_rmsgptr->dynamicTypeId() == InputOutputStreamMessage::staticTypeId()){
		InputOutputStreamMessage &rsig(*static_cast<InputOutputStreamMessage*>(_rmsgptr.get()));
	}else */if(_rmsgptr->dynamicTypeId() == StreamErrorMessage::staticTypeId()){
		//StreamErrorMessage &rsig(*static_cast<StreamErrorMessage*>(_rmsgptr.get()));
		state = SendError;
	}else if(_rmsgptr->dynamicTypeId() == FetchSlaveMessage::staticTypeId()){
		//FetchSlaveMessage &rsig(*static_cast<FetchSlaveMessage*>(_rmsgptr.get()));
		pmsg = static_cast<FetchSlaveMessage*>(_rmsgptr.release());
		idbg((void*)this<<" Received slavesignal");
		state = SendNextStream;
	}else return NOK;
	idbg("success");
	return OK;//success reschedule command for execution
}

// int FetchMasterMessage::receiveMessage(
// 	DynamicPointer<frame::Message> &_rmsgptr,
// 	int			_which,
// 	const ObjectUidT&_from,
// 	const frame::ipc::ConnectionUid *
// ){
// 	psig = static_cast<FetchSlaveMessage*>(_rmsgptr.release());
// 	idbg("");
// 	state = SendNextStream;
// 	return OK;
// }
// int FetchMasterMessage::receiveInputStream(
// 	StreamPointer<InputStream> &_rins,
// 	const FileUidT	& _fuid,
// 	int			_which,
// 	const ObjectUidT&,
// 	const frame::ipc::ConnectionUid *
// ){
// 	idbg("fuid = "<<_fuid.first<<","<<_fuid.second);
// 	ins = _rins;
// 	fuid = _fuid;
// 	state = SendFirstStream;
// 	return OK;
// }
// int FetchMasterMessage::receiveError(
// 	int _errid, 
// 	const ObjectUidT&_from,
// 	const frame::ipc::ConnectionUid *_conid
// ){
// 	idbg("");
// 	state = SendError;
// 	return OK;
// }

//-----------------------------------------------------------------------------------
// FetchSlaveMessage
//-----------------------------------------------------------------------------------

FetchSlaveMessage::FetchSlaveMessage(): fromv(0xffffffff, 0xffffffff), filesz(-10), streamsz(-1), requid(0){
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
	idbg("filesz = "<<this->filesz<<" streamsz = "<<this->streamsz<<" requid = "<<requid);
	idbg("fuid.first = "<<fuid.first<<" fuid.second = "<<fuid.second);
	idbg("msguid.first = "<<msguid.first<<" msguid.second = "<<msguid.second);
}
int FetchSlaveMessage::sent(const frame::ipc::ConnectionUid &_rconid){
	idbg((void*)this<<"");
	fromv.first = 0xffffffff;
	return BAD;
}
uint32 FetchSlaveMessage::ipcPrepare(){
	return 0;//frame::ipc::Service::SynchronousSendFlag;
}
void FetchSlaveMessage::ipcReceive(
	frame::ipc::MessageUid &_rmsguid
){
	DynamicPointer<frame::Message> psig(this);
	conid = frame::ipc::ConnectionContext::the().connectionuid;
	if(filesz == -10){
		idbg((void*)this<<" Received FetchSlaveMessage on peer");
		print();
		ObjectUidT	ttov;
		Manager::the().notify(psig, Manager::the().readMessageStewardUid());
	}else{
		idbg((void*)this<<" Received FetchSlaveMessage on sender");
		print();
		Manager::the().notify(psig, ObjectUidT(tov.first, tov.second));
	}
}
// Executed on peer within the signal executer
int FetchSlaveMessage::execute(
	DynamicPointer<Message> &_rmsgptr,
	uint32 _evs,
	frame::MessageSteward& _rce,
	const MessageUidT &,
	TimeSpec &
){
	idbg((void*)this<<"");
	_rce.sendMessage(_rmsgptr, msguid);
	return BAD;
}

void FetchSlaveMessage::initOutputStream(){
	frame::RequestUid					requid;
	Manager::the().fileManager().stream(outs, fuid, requid, frame::file::Manager::Forced);
	idbg((void*)this<<" Create deserialization streamptr "<<(void*)outs.get());
}

void FetchSlaveMessage::clearOutputStream(){
	outs.clear();
}

//-----------------------------------------------------------------------------------
// SendStringMessage
//-----------------------------------------------------------------------------------

void SendStringMessage::ipcReceive(
	frame::ipc::MessageUid &_rmsguid
){
	DynamicPointer<frame::Message> psig(this);
	conid = frame::ipc::ConnectionContext::the().connectionuid;;
	Manager::the().notify(psig, ObjectUidT(tov.first, tov.second));
}

// int SendStringMessage::execute(concept::Connection &_rcon){
// 	return _rcon.receiveString(str, concept::Connection::RequestUidT(0, 0), 0, fromv, &conid);
// }

//-----------------------------------------------------------------------------------
// SendStreamMessage
//-----------------------------------------------------------------------------------

void SendStreamMessage::ipcReceive(
	frame::ipc::MessageUid &_rmsguid
){
	DynamicPointer<frame::Message> psig(this);
	conid = frame::ipc::ConnectionContext::the().connectionuid;;
	Manager::the().notify(psig, ObjectUidT(tov.first, tov.second));
}

void SendStreamMessage::destroyDeserializationStream(
	OutputStream *&_rpos, int64 &_rsz, uint64 &_roff, int _id
){
	idbg("Destroy deserialization <"<<_id<<"> sz "<<_rsz);
}
int SendStreamMessage::createDeserializationStream(
	OutputStream *&_rpos, int64 &_rsz, uint64 &_roff, int _id
){
	if(_id) return NOK;
	idbg("Create deserialization <"<<_id<<"> sz "<<_rsz);
	if(dststr.empty()/* || _rps.second < 0*/) return NOK;
	idbg("File name: "<<this->dststr);
	//TODO:
	int rv = Manager::the().fileManager().stream(this->iosp, this->dststr.c_str(), frame::file::Manager::NoWait);
	if(rv){
		idbg("Oops, could not open file");
		return BAD;
	}else{
		_rpos = static_cast<OutputStream*>(this->iosp.get());
	}
	return OK;
}
void SendStreamMessage::destroySerializationStream(
	InputStream *&_rpis, int64 &_rsz, uint64 &_roff, int _id
){
	idbg("doing nothing as the stream will be destroied when the signal will be destroyed");
}
int SendStreamMessage::createSerializationStream(
	InputStream *&_rpis, int64 &_rsz, uint64 &_roff, int _id
){
	if(_id) return NOK;
	idbg("Create serialization <"<<_id<<"> sz "<<_rsz);
	//The stream is already opened
	_rpis = static_cast<InputStream*>(this->iosp.get());
	_rsz = this->iosp->size();
	return OK;
}

// int SendStreamMessage::execute(concept::Connection &_rcon){
// 	{
// 	StreamPointer<InputStream>	isp(static_cast<InputStream*>(iosp.release()));
// 	idbg("");
// 	_rcon.receiveInputStream(isp, concept::Connection::FileUidT(0,0), concept::Connection::RequestUidT(0, 0), 0, fromv, &conid);
// 	idbg("");
// 	}
// 	return _rcon.receiveString(dststr, concept::Connection::RequestUidT(0, 0), 0, fromv, &conid);
// }

//-----------------------------------------------------------------------------------

}//namespace alpha

}//namespace concept
