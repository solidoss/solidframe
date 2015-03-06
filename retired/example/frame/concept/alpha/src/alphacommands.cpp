// alphacommands.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <iostream>
#include "system/debug.hpp"
#include "system/socketaddress.hpp"
#include "system/filedevice.hpp"
#include "system/thread.hpp"
#include "system/timespec.hpp"

#include "utility/iostream.hpp"

#include "protocol/text/namematcher.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/ipc/ipcservice.hpp"
#include "frame/requestuid.hpp"

#include "core/common.hpp"
#include "core/tstring.hpp"
#include "core/manager.hpp"
#include "core/messages.hpp"


#include "alphaconnection.hpp"
#include "alphacommands.hpp"
#include "alphamessages.hpp"
#include "alphawriter.hpp"
#include "alphareader.hpp"
#include "alphaconnection.hpp"
#include "alphaprotocolfilters.hpp"


#define StrDef(x) (void*)x, sizeof(x) - 1

typedef std::pair<std::string, int64>		StringInt64PairT;

using namespace solid;

namespace solid{namespace serialization{namespace binary{
template <class S, class Ctx>
void serialize(S &_s, solid::frame::UidT &_t, Ctx &_ctx){
	_s.push(_t.first, "first").push(_t.second, "second");
}
template <class S, class Ctx>
void serialize(S &_s, StringInt64PairT &_t, Ctx &_ctx){
	_s.push(_t.first, "first").push(_t.second, "second");
}
}}}

namespace{
std::streampos stream_size(std::istream &_rios){
	std::streampos pos = _rios.tellg();
	_rios.seekg(0, _rios.end);
	std::streampos endpos = _rios.tellg();
	_rios.seekg(pos);
	return endpos;
}
}


namespace concept{
namespace alpha{

//The commands and the associated namemather
struct Cmd{
	enum CmdId{
		LoginCmd,
		LogoutCmd,
		CapabilityCmd,
		ListCmd,
		RemoteListCmd,
		NoopCmd,
		FetchCmd,
		StoreCmd,
		SendStreamCmd,
		SendStringCmd,
		IdleCmd,
		CmdCount
	};
	const char *name;
	CmdId		id;
} const cmds[] = {
	{"login", Cmd::LoginCmd},
	{"logout",Cmd::LogoutCmd},
	{"capability",Cmd::CapabilityCmd},
	{"list",Cmd::ListCmd},
	{"remotelist", Cmd::RemoteListCmd},
	{"noop",Cmd::NoopCmd},
	{"fetch",Cmd::FetchCmd},
	{"store",Cmd::StoreCmd},
	{"idle",Cmd::IdleCmd},
	{NULL,Cmd::CmdCount},
};
static const protocol::text::NameMatcher cmdm(cmds);
//---------------------------------------------------------------
/*
	The creator method called by frame::Reader::fetchKey when the 
	command name was parsed.
	All it does is to create the proper command, which in turn,
	will instruct the reader how to parse itself.
*/
Command* Connection::create(const String& _name, Reader &){
	cassert(!pcmd);
	idbg("create command "<<_name);
	switch(cmds[cmdm.match(_name.c_str())].id){
		case Cmd::LoginCmd:
			return pcmd = new Login;
		case Cmd::LogoutCmd:
			return pcmd = new Basic(Basic::Logout);
		case Cmd::NoopCmd:
			return pcmd = new Basic(Basic::Noop);
		case Cmd::CapabilityCmd:
			return pcmd = new Basic(Basic::Capability);
		case Cmd::ListCmd:
			return pcmd = new List;
		case Cmd::RemoteListCmd:
			return pcmd = new RemoteList;
		case Cmd::FetchCmd:
			return pcmd = new Fetch(*this);
		case Cmd::StoreCmd:
			return pcmd = new Store(*this);
		case Cmd::IdleCmd:
			return pcmd = new Idle(*this);
		default:return NULL;
	}
}
//---------------------------------------------------------------
// Basic commands
//---------------------------------------------------------------
Basic::Basic(Basic::Types _tp):tp(_tp){
}
Basic::~Basic(){
}
void Basic::initReader(Reader &_rr){
}
void Basic::execute(Connection &_rc){
	switch(tp){
		case Noop:	execNoop(_rc); break;
		case Logout: execLogout(_rc); break;
		case Capability: execCapability(_rc); break;
	}
}
void Basic::execNoop(Connection &_rc){
	_rc.writer().push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done NOOP@")));
}
void Basic::execLogout(Connection &_rc){
	_rc.writer().push(&Writer::returnValue<true>, protocol::text::Parameter(Writer::Failure));
	_rc.writer().push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done LOGOUT@")));
	_rc.writer().push(&Writer::putAtom, protocol::text::Parameter(StrDef("* Alpha connection closing\r\n")));
}
void Basic::execCapability(Connection &_rc){
	_rc.writer().push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done CAPABILITY@")));
	_rc.writer().push(&Writer::putAtom, protocol::text::Parameter(StrDef("* CAPABILITIES noop logout login\r\n")));
}
//---------------------------------------------------------------
// Login command
//---------------------------------------------------------------
Login::Login(){
}
Login::~Login(){
}
void Login::initReader(Reader &_rr){
	_rr.push(&Reader::manage, protocol::text::Parameter(Reader::ResetLogging));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&pass));
	_rr.push(&Reader::manage, protocol::text::Parameter(Reader::ClearLogging));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&user));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
}
void Login::execute(Connection &_rc){
	_rc.writer().push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done LOGIN@")));
}
//---------------------------------------------------------------
// List command
//---------------------------------------------------------------
List::List(){
}
List::~List(){
}
void List::initReader(Reader &_rr){
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&strpth));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
}
void List::execute(Connection &_rc){
	idbg("path: "<<strpth);
	fs::path pth(strpth.c_str()/*, fs::native*/);
	protocol::text::Parameter &rp = _rc.writer().push(&Writer::putStatus);
	rp = protocol::text::Parameter(StrDef(" OK Done LIST@"));
	if(!exists( pth ) || !is_directory(pth)){
		rp = protocol::text::Parameter(StrDef(" NO LIST: Not a directory@"));
		return;
	}
	try{
	it = fs::directory_iterator(pth);
	}catch ( const std::exception & ex ){
		idbg("dir_iterator exception :"<<ex.what());
		return;
	}
	_rc.writer().push(&Writer::reinit<List>, protocol::text::Parameter(this, Step));
	if(it != end){
		_rc.writer().push(&Writer::putCrlf);
		_rc.writer().push(&Writer::putAString, protocol::text::Parameter((void*)it->path().c_str(), strlen(it->path().c_str())));
		if(is_directory(*it)){
			_rc.writer()<<"* DIR ";
		}else{
			_rc.writer()<<"* FILE "<<(uint32)FileDevice::size(it->path().c_str())<<' ';
		}
	}
}

int List::reinitWriter(Writer &_rw, protocol::text::Parameter &_rp){
	++it;
	if(it != end){
		_rw.push(&Writer::putCrlf);
		_rw.push(&Writer::putAString, protocol::text::Parameter((void*)it->path().c_str(), strlen(it->path().c_str())));
		if(is_directory(*it)){
			_rw<<"* DIR ";
		}else{
			_rw<<"* FILE "<<(uint32)FileDevice::size(it->path().c_str())<<' ';
		}
		return Writer::Continue;
	}
	return Writer::Success;
}
//---------------------------------------------------------------
// RemoteList command
//---------------------------------------------------------------
RemoteList::PathListT::PathListT(){
	idbg(""<<(void*)this);
}
RemoteList::PathListT::~PathListT(){
	idbg(""<<(void*)this);
}

RemoteList::RemoteList():pausems(0), ppthlst(NULL),state(SendError){
}
RemoteList::~RemoteList(){
	delete ppthlst;
}
void RemoteList::initReader(Reader &_rr){
	typedef CharFilter<' '>				SpaceFilterT;
	typedef NotFilter<SpaceFilterT> 	NotSpaceFilterT;
	
	hostvec.push_back(HostAddr());
	
	_rr.push(&Reader::reinitExtended<RemoteList, 0>, protocol::text::Parameter(this));
	_rr.push(&Reader::fetchUInt32, protocol::text::Parameter(&hostvec.back().netid));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&hostvec.back().port));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&hostvec.back().addr));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&strpth));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
}

template <>
int RemoteList::reinitReader<0>(Reader &_rr, protocol::text::Parameter &){
	typedef CharFilter<' '>				SpaceFilterT;
	typedef NotFilter<SpaceFilterT> 	NotSpaceFilterT;
	
	hostvec.push_back(HostAddr());
	
	_rr.push(&Reader::returnValue<true>, protocol::text::Parameter(Reader::Success));
	_rr.push(&Reader::fetchUInt32, protocol::text::Parameter(&pausems));
	
	//the pause amount
	_rr.push(&Reader::pop, protocol::text::Parameter(2));
	_rr.push(&Reader::fetchUInt32, protocol::text::Parameter(&hostvec.back().netid));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&hostvec.back().port));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&hostvec.back().addr));
	
	//not a digit
	_rr.push(&Reader::checkIfCharThenPop<DigitFilter>, protocol::text::Parameter(6));
	_rr.push(&Reader::dropChar);//SPACE
	_rr.push(&Reader::checkIfCharThenPop<NotSpaceFilterT>, protocol::text::Parameter(9));
	return Reader::Continue;
}

void RemoteList::execute(Connection &_rc){
	pp = &_rc.writer().push(&Writer::putStatus);
	*pp = protocol::text::Parameter(StrDef(" OK Done REMOTELIST@"));
	
	if(hostvec.back().addr.empty()){
		hostvec.pop_back();
	}
	
	DynamicSharedPointer<RemoteListMessage>	sig_sp(new RemoteListMessage(pausems, hostvec.size()));
	
	sig_sp->strpth = strpth;
	sig_sp->requid = _rc.newRequestId();
	sig_sp->fromv.first = _rc.id();
	sig_sp->fromv.second = Manager::the().id(_rc).second;
	
	for(HostAddrVectorT::const_iterator it(hostvec.begin()); it != hostvec.end(); ++it){
		const String &straddr(it->addr);
		const String &port(it->port);
		idbg("addr"<<straddr<<" port = "<<port);
		ResolveData rd = synchronous_resolve(straddr.c_str(), port.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream);
		if(!rd.empty()){
			state = Wait;
			DynamicPointer<frame::ipc::Message> msgptr(sig_sp);
			Manager::the().ipc().sendMessage(msgptr, rd.begin(), it->netid/*, frame::ipc::Service::SameConnectorFlag*/);
			_rc.writer().push(&Writer::reinit<RemoteList>, protocol::text::Parameter(this));
		}else{
			*pp = protocol::text::Parameter(StrDef(" NO REMOTELIST: no such peer address@"));
		}
	}
}
int RemoteList::reinitWriter(Writer &_rw, protocol::text::Parameter &_rp){
	switch(state){
		case Wait:
			return Writer::Wait;
		case SendListContinue:
			++it;
		case SendList:
			if(it != ppthlst->end()){
				_rw.push(&Writer::putCrlf);
				_rw.push(&Writer::putAString, protocol::text::Parameter((void*)it->first.data(), it->first.size()));
				if(it->second < 0){
					_rw<<"* DIR ";
				}else{
					_rw<<"* FILE "<<(uint32)it->second<<' ';
				}
				state = SendListContinue;
				return Writer::Continue;
			}
			return Writer::Success;
		case SendError:
			*pp = protocol::text::Parameter(StrDef(" NO LIST: Not a directory@"));
			return Writer::Success;
	};
	cassert(false);
	return AsyncError;
}

/*virtual*/ int RemoteList::receiveMessage(solid::DynamicPointer<RemoteListMessage> &_rmsgptr){
	ppthlst = _rmsgptr->ppthlst;
	_rmsgptr->ppthlst = NULL;
	if(ppthlst && !_rmsgptr->err){
		it = ppthlst->begin();
		state = SendList;
	}else{
		state = SendError;
	}
	return AsyncSuccess;
}
//---------------------------------------------------------------
// Fetch command
//---------------------------------------------------------------

struct FetchOpenCbk{
	frame::ObjectUidT	uid;
	uint32				reqid;
	FetchOpenCbk(){}
	FetchOpenCbk(const frame::ObjectUidT &_robjuid, uint32 _reqid):uid(_robjuid), reqid(_reqid){}
	
	void operator()(
		frame::file::Store<> &,
		frame::file::FilePointerT &_rptr,
		ERROR_NS::error_code err
	){
		idbg("");
		frame::Manager 				&rm = frame::Manager::specific();
		frame::file::FilePointerT	emptyptr;
		
		frame::MessagePointerT		msgptr(new FilePointerMessage(err ? emptyptr :_rptr, reqid));
		rm.notify(msgptr, uid);
	}
};


Fetch::Fetch(Connection &_rc):rc(_rc), state(0), litsz(-1), ios(/*0*/){
}
Fetch::~Fetch(){
	idbg(""<<(void*)this);
}
void Fetch::initReader(Reader &_rr){
	typedef CharFilter<' '>				SpaceFilterT;
	typedef NotFilter<SpaceFilterT> 	NotSpaceFilterT;
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&port));
	_rr.push(&Reader::dropChar);
	_rr.push(&Reader::checkIfCharThenPop<NotSpaceFilterT>, protocol::text::Parameter(2));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&straddr));
	_rr.push(&Reader::dropChar);
	_rr.push(&Reader::checkIfCharThenPop<NotSpaceFilterT>, protocol::text::Parameter(5));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&strpth));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	
}
void Fetch::execute(Connection &_rc){
	idbg("path "<<strpth<<", address "<<straddr<<", port "<<port<<' '<<(void*)this);
	protocol::text::Parameter &rp = _rc.writer().push(&Writer::putStatus);
	pp = &rp;
	rp = protocol::text::Parameter(StrDef(" OK Done FETCH@"));
	if(port.empty()) port = "1222";//default ipc port
	if(straddr.empty()){
		state = InitLocal;
	}else{
		state = InitRemote;
	}
	_rc.writer().push(&Writer::reinit<Fetch>, protocol::text::Parameter(this));
}

int Fetch::doInitLocal(){
	idbg(""<<(void*)this);
	//try to open stream to localfile
	Manager &rm = Manager::the();
	rm.fileStore().requestOpenFile(FetchOpenCbk(rm.id(rc), rc.newRequestId()), strpth, FileDevice::ReadOnlyE);
	state = WaitLocalStream;
	return Writer::Wait;
}

int Fetch::doGetTempStream(uint32 _sz){
	idbg(""<<(void*)this<<" "<<_sz);
	Manager &rm = Manager::the();
	streamcp = _sz;
	rm.fileStore().requestCreateTemp(FetchOpenCbk(rm.id(rc), rc.newRequestId()), _sz, frame::file::VeryFastLevelFlag);
	state = WaitTempStream;
	return Writer::Wait;
}

void Fetch::doSendMaster(const solid::frame::UidT &_ruid){
	idbg(""<<(void*)this);
	ResolveData rd = synchronous_resolve(straddr.c_str(), port.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream);
	idbg("addr"<<straddr<<" port = "<<port);
	if(!rd.empty()){
		//send the master remote command
		FetchMasterMessage						*pmsg(new FetchMasterMessage);
		DynamicPointer<frame::ipc::Message>		msgptr(pmsg);
		
		pmsg->fname = strpth;
		pmsg->requid = rc.newRequestId();
		pmsg->fromv = Manager::the().id(rc);
		pmsg->tmpfuid = _ruid;
		pmsg->streamsz = streamcp;
		state = WaitFirstRemoteStream;
		
		Manager::the().ipc().sendMessage(msgptr, rd.begin());
	}else{
		*pp = protocol::text::Parameter(StrDef(" NO FETCH: no such peer address@"));
		state = ReturnOk;
	}
}

int Fetch::doSendFirstData(Writer &_rw){
	idbg(""<<(void*)this<<" streamsz = "<<streamsz);
	uint64 remainsz(litsz - streamsz);
	if(remainsz){
		cassert(streamsz == streamcp);
		streamsz /= 2;
		state = SendNextData;
	}else{
		cachedmsg.clear();
		state = ReturnCrlf;
	}
	//sp_out = sp_in;
	return doSendLiteral(_rw, false);
}

int Fetch::doSendNextData(Writer &_rw){
	idbg(""<<(void*)this);
	cassert(!cachedmsg.empty());
	if(cachedmsg->streamsz != streamcp){
		ios.seekg(cachedmsg->streampos);
		streamsz = cachedmsg->streamsz;
	}else{
		//we have the remaning side of the stream
		cachedmsg->streamsz /= 2;
		streamsz = cachedmsg->streamsz;
		ios.seekg(streamsz);
		cachedmsg->streampos = streamsz;
	}
	
	uint64 remainsz(litsz - streamsz);
	if(remainsz){
		//not the last chunk - request another one
		if(cachedmsg->streampos){
			cachedmsg->streampos = 0;
		}else{
			cachedmsg->streampos = streamcp/2;
		}
		state = WaitRemoteStream;
		//cachedmsg->fromv = Manager::the().id(rc);
		cachedmsg->requid = rc.newRequestId();
		cachedmsg->ios.device().clear();
		solid::frame::ipc::ConnectionUid	conid = cachedmsg->conid;
		DynamicPointer<frame::ipc::Message>	msgptr(cachedmsg);
		Manager::the().ipc().sendMessage(msgptr, conid);
	}else{
		state = ReturnCrlf;
	}
	litsz -= streamsz;
	_rw.push(&Writer::putStream, protocol::text::Parameter(&ios, &streamsz));
	return Writer::Continue;
}

int Fetch::doSendLiteral(Writer &_rw, bool _local){
	idbg("send literal "<<litsz<<" "<<streamsz);
	//send local stream
	cassert(!ios.device().empty());
	_rw<<"* DATA {"<<litsz<<"}\r\n";
	litsz -= streamsz;
	if(_local){
		_rw.replace(&Writer::putCrlf);
	}
	_rw.push(&Writer::putStream, protocol::text::Parameter(static_cast<std::iostream*>(&ios), &streamsz));
	return Writer::Continue;
}

int Fetch::reinitWriter(Writer &_rw, protocol::text::Parameter &_rp){
	switch(state){
		case InitLocal:
			return doInitLocal();
		case SendLocal:
			return doSendLiteral(_rw, true);
		case InitRemote:
			return doGetTempStream(2 * 1024);
		case SendFirstData:
			return doSendFirstData(_rw);
		case SendNextData:
			return doSendNextData(_rw);
		case WaitLocalStream:
		case WaitTempStream:
		case WaitFirstRemoteStream:
		case WaitRemoteStream:
			return Writer::Wait;
		case ReturnBad:
			return Writer::Failure;
		case ReturnOk:
			return Writer::Success;
		case ReturnCrlf:
			_rw.replace(&Writer::putCrlf);
			return Writer::Continue;
		case SendError:
			*pp = protocol::text::Parameter(StrDef(" NO FETCH: an error occured@"));
			return Writer::Success;
		case SendTempError:
			*pp = protocol::text::Parameter(StrDef(" NO FETCH: no temp stream@"));
			return Writer::Success;
		case SendRemoteError:
			*pp = protocol::text::Parameter(StrDef(" NO FETCH: no remote stream@"));
			return Writer::Success;
	}
	cassert(false);
	return AsyncError;
}

int Fetch::receiveFilePointer(FilePointerMessage &_rmsg){
	idbg("");
	ios.device(_rmsg.ptr);
	if(state == WaitLocalStream){
		if(!ios.device().empty()){
			streamsz = stream_size(ios);
			litsz = streamsz;
			state = SendLocal;
		}else{
			state = SendError;
		}
	}else if(state == WaitTempStream){
		if(!ios.device().empty()){
			Manager::the().fileStore().uniqueToShared(ios.device());
			state = WaitFirstRemoteStream;
			doSendMaster(ios.device().id());
		}else{
			state = SendTempError;
		}
	}
	return AsyncSuccess;
}
/*virtual*/ int Fetch::receiveMessage(solid::DynamicPointer<FetchSlaveMessage> &_rmsgptr){
	idbg("");
	cachedmsg = _rmsgptr;
	if(state == WaitFirstRemoteStream){
		if(cachedmsg->filesz >= 0){
			cachedmsg->ipcResetState();
			state = SendFirstData;
			litsz = cachedmsg->filesz;
			streamsz = cachedmsg->streamsz;
		}else{
			state = SendRemoteError;
		}
	}else if(state == WaitRemoteStream){
		if(cachedmsg->filesz >= 0){
			state = SendNextData;
			//litsz = cachedmsg->filesz;
			//streamsz = 
		}else{
			state = ReturnBad;
		}
	}else{
		state = ReturnBad;
	}
	return AsyncSuccess;
}
//---------------------------------------------------------------
// Store Command
//---------------------------------------------------------------
Store::Store(Connection &_rc):rc(_rc),st(AsyncSuccess){
}
Store::~Store(){
}
void Store::initReader(Reader &_rr){
	_rr.push(&Reader::reinit<Store>, protocol::text::Parameter(this, Init));
	_rr.push(&Reader::checkChar, protocol::text::Parameter('}'));
	_rr.push(&Reader::fetchUInt32, protocol::text::Parameter(&litsz));
	_rr.push(&Reader::checkChar, protocol::text::Parameter('{'));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&strpth));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
}
int Store::reinitReader(Reader &_rr, protocol::text::Parameter &_rp){
	Manager &rm = Manager::the();
	switch(_rp.b.i){
		case Init:{
			rm.fileStore().requestCreateFile(FetchOpenCbk(rm.id(rc), rc.newRequestId()), strpth.c_str(), FileDevice::WriteOnlyE);
			st = AsyncWait;//waiting
			_rp.b.i = SendWait;
			return Reader::Wait;
		}
		case SendWait:
			if(!os.device().empty()){
				idbg("sending wait and preparing fetch");
				rc.writer()<<"* Expecting "<<litsz<<" CHARs\r\n";
				litsz64 = litsz;
				_rr.replace(&Reader::fetchLiteralStream, protocol::text::Parameter(&os, &litsz64));
				_rr.push(&Reader::flushWriter);
				_rr.push(&Reader::checkChar, protocol::text::Parameter('\n'));
				_rr.push(&Reader::checkChar, protocol::text::Parameter('\r'));
				return Reader::Continue;
			}else{
				idbg("no stream");
				if(st == AsyncWait) return Reader::Wait;//still waiting
				idbg("we have a problem");
				return Reader::Success;
			}
			break;
	}
	cassert(false);
	return Reader::Failure;
}

void Store::execute(Connection &_rc){
	protocol::text::Parameter &rp = _rc.writer().push(&Writer::putStatus);
	if(!os.device().empty()){
		rp = protocol::text::Parameter(StrDef(" OK Done STORE@"));
	}else{
		rp = protocol::text::Parameter(StrDef(" NO STORE: Failed opening file@"));
	}
}

int Store::receiveFilePointer(FilePointerMessage &_rmsg){
	idbg("received stream");
	os.device(_rmsg.ptr);
	if(!os.device().empty()){
		st = AsyncSuccess;
		return AsyncSuccess;
	}else{
		st = AsyncError;
		return AsyncSuccess;
	}
}

int Store::reinitWriter(Writer &_rw, protocol::text::Parameter &_rp){
	return Writer::Failure;
}
//---------------------------------------------------------------
// Idle command
//---------------------------------------------------------------
Idle::~Idle(){
}
void Idle::initReader(Reader &_rr){
	_rr.push(&Reader::checkChar, protocol::text::Parameter('e'));
	_rr.push(&Reader::checkChar, protocol::text::Parameter('n'));
	_rr.push(&Reader::checkChar, protocol::text::Parameter('o'));
	_rr.push(&Reader::checkChar, protocol::text::Parameter('d'));
	_rr.push(&Reader::checkChar, protocol::text::Parameter('\n'));
	_rr.push(&Reader::checkChar, protocol::text::Parameter('\r'));
}
void Idle::execute(Connection &_rc){
	_rc.writer().push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done IDLE@")));
}
int Idle::reinitWriter(Writer &_rw, protocol::text::Parameter &_rp){
	if(_rp.b.i == 1){//prepare
		cassert(typeq.size());
		if(typeq.front() == PeerStringType){
			_rw<<"* RECEIVED STRING ("<<(uint32)conidq.front().tkridx<<' '<<(uint32)conidq.front().sesidx<<' '<<(uint32)conidq.front().sesuid;
			_rw<<") ("<<(uint32)fromq.front().first<<' '<<(uint32)fromq.front().second<<") ";
			_rp.b.i = 0;
			_rw.push(&Writer::flushAll);
			_rw.push(&Writer::putChar, protocol::text::Parameter('\n'));
			_rw.push(&Writer::putChar, protocol::text::Parameter('\r'));
			_rw.push(&Writer::putAString, protocol::text::Parameter((void*)stringq.front().data(), stringq.front().size()));
		}else if(typeq.front() == PeerStreamType){
			_rw<<"* RECEIVED STREAM ("<<(uint32)conidq.front().tkridx<<' '<<(uint32)conidq.front().sesidx<<' '<<(uint32)conidq.front().sesuid;
			_rw<<") ("<<(uint32)fromq.front().first<<' '<<(uint32)fromq.front().second<<") PATH ";
			_rp.b.i = 0;
			_rw.push(&Writer::flushAll);
			_rw.push(&Writer::putChar, protocol::text::Parameter('\n'));
			_rw.push(&Writer::putChar, protocol::text::Parameter('\r'));
			_rw.push(&Writer::reinit<Idle>, protocol::text::Parameter(this, 2));
			_rw.push(&Writer::flushAll);
			_rw.push(&Writer::putAString, protocol::text::Parameter((void*)stringq.front().data(), stringq.front().size()));
		}else{
			cassert(false);
		}
		return Writer::Continue;
	}else if(_rp.b.i == 2){
		//streamq.front()->seek(0);//go to begining
		is.device(fileptrq.front());
		is.clear();
		is.seekg(0);
		litsz64 = stream_size(is);
		_rw<<" DATA {"<<(uint32)litsz64<<"}\r\n";
		_rw.replace(&Writer::putStream, protocol::text::Parameter(&is, &litsz64));
		return Writer::Continue;
	}else{//unprepare
		if(typeq.front() == PeerStringType){
			typeq.pop();
			stringq.pop();
			fromq.pop();
			conidq.pop();
		}else if(typeq.front() == PeerStreamType){
			typeq.pop();
			stringq.pop();
			fromq.pop();
			conidq.pop();
			fileptrq.pop();
		}else{
			cassert(false);
		}
		if(typeq.size()){
			_rp.b.i = 1;
			return Writer::Continue;
		}
		return Writer::Success;
	}
}
int Idle::receiveFilePointer(
	FilePointerMessage &_rmsg
){
// 	if(_conid){
// 		typeq.push(PeerStreamType);
// 		conidq.push(*_conid);
// 	}else{
		typeq.push(LocalStreamType);
//	}
	fileptrq.push(_rmsg.ptr);
	fromq.push(ObjectUidT());
	rc.writer().push(&Writer::reinit<Idle>, protocol::text::Parameter(this, 1));
	return AsyncSuccess;
}
int Idle::receiveString(
	const String &_str,
	int			_which,
	const ObjectUidT&_from,
	const frame::ipc::ConnectionUid *_conid
){
	if(typeq.size() && typeq.back() == PeerStreamType){
		stringq.push(_str);
		return AsyncSuccess;
	}
	if(_conid){
		typeq.push(PeerStringType);
		conidq.push(*_conid);
	}else{
		typeq.push(LocalStringType);
	}
	stringq.push(_str);
	fromq.push(_from);
	rc.writer().push(&Writer::reinit<Idle>, protocol::text::Parameter(this, 1));
	return AsyncWait;
}
//---------------------------------------------------------------
// Command Base
//---------------------------------------------------------------

Command::Command(){}
void Command::initStatic(Manager &_rm){
	MessageTypeIds ids;
	Manager::the().ipc().registerMessageType<FetchMasterMessage>();
	Manager::the().ipc().registerMessageType<FetchSlaveMessage>();
	Manager::the().ipc().registerMessageType<RemoteListMessage>();
	MessageTypeIds::the(&ids);
}
/*virtual*/ Command::~Command(){}

int Command::receiveFilePointer(FilePointerMessage &_rmsg){
	return AsyncError;
}
int Command::receiveMessage(solid::DynamicPointer<FetchSlaveMessage> &_rmsgptr){
	return AsyncError;
}
int Command::receiveMessage(solid::DynamicPointer<RemoteListMessage> &_rmsgptr){
	return AsyncError;
}

}//namespace alpha
}//namespace concept

