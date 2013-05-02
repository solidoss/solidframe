#include <cstdio>

#include "system/debug.hpp"
#include "system/socketaddress.hpp"
#include "system/filedevice.hpp"
#include "system/thread.hpp"
#include "system/timespec.hpp"

#include "utility/iostream.hpp"

#include "protocol/text/namematcher.hpp"
#include "serialization/binary.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/ipc/ipcservice.hpp"
#include "frame/file/filemanager.hpp"
#include "frame/messagesteward.hpp"
#include "frame/requestuid.hpp"

#include "core/common.hpp"
#include "core/tstring.hpp"
#include "core/manager.hpp"
#include "core/messages.hpp"


#include "gammaconnection.hpp"
#include "gammacommands.hpp"
#include "gammawriter.hpp"
#include "gammareader.hpp"
#include "gammafilters.hpp"


#define StrDef(x) (void*)x, sizeof(x) - 1

using namespace solid;

namespace concept{
namespace gamma{

//The commands and the associated namemather
struct Cmd{
	enum CmdId{
		LoginCmd,
		LogoutCmd,
		CapabilityCmd,
		OpenCmd,
		NoopCmd,
		CmdCount
	};
	const char *name;
	CmdId		id;
} const cmds[] = {
	{"login", Cmd::LoginCmd},
	{"logout",Cmd::LogoutCmd},
	{"capability",Cmd::CapabilityCmd},
	{"open", Cmd::OpenCmd},
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
Command* Connection::doCreateSlave(const String& _name, Reader &_rr){
	SocketData	&rsd(socketData(_rr.socketId()));
	cassert(!rsd.pcmd);
	idbg("create command "<<_name);
	switch(cmds[cmdm.match(_name.c_str())].id){
		case Cmd::LoginCmd:
			return rsd.pcmd = new Login;
		case Cmd::LogoutCmd:
			return rsd.pcmd = new Basic(Basic::Logout);
		case Cmd::NoopCmd:
			return rsd.pcmd = new Basic(Basic::Noop);
		case Cmd::CapabilityCmd:
			return rsd.pcmd = new Basic(Basic::Capability);
		default:return NULL;
	}
}
/*static*/ void Connection::doInitStaticSlave(Manager &_rm){
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
int Basic::execute(const uint _sid){
	switch(tp){
		case Noop:	return execNoop(_sid);
		case Logout: return execLogout(_sid);
		case Capability: return execCapability(_sid);
	}
	return BAD;
}
int Basic::execNoop(const uint _sid){
	Connection::the().socketData(_sid).w.push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done NOOP@")));
	return OK;
}
int Basic::execLogout(const uint _sid){
	Connection	&rc(Connection::the());
	SocketData	&rsd(rc.socketData(_sid));
	rsd.w.push(&Writer::returnValue<true>, protocol::text::Parameter(Writer::Bad));
	rsd.w.push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done LOGOUT@")));
	rsd.w.push(&Writer::putAtom, protocol::text::Parameter(StrDef("* Alpha connection closing\r\n")));
	return NOK;
}
int Basic::execCapability(const uint _sid){
	Connection	&rc(Connection::the());
	SocketData	&rsd(rc.socketData(_sid));
	rsd.w.push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done CAPABILITY@")));
	rsd.w.push(&Writer::putAtom, protocol::text::Parameter(StrDef("* CAPABILITIES noop logout login\r\n")));
	return OK;
}
//---------------------------------------------------------------
// Login command
//---------------------------------------------------------------
Login::Login(){
}
Login::~Login(){
}
void Login::initReader(Reader &_rr){
	typedef CharFilter<' '>				SpaceFilterT;
	typedef NotFilter<SpaceFilterT> 	NotSpaceFilterT;
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&ctx));
	_rr.push(&Reader::dropChar);
	_rr.push(&Reader::checkIfCharThenPop<NotSpaceFilterT>, protocol::text::Parameter(2));
	_rr.push(&Reader::manage, protocol::text::Parameter(Reader::ResetLogging));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&pass));
	_rr.push(&Reader::manage, protocol::text::Parameter(Reader::ClearLogging));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&user));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
}
int Login::execute(const uint _sid){
	Connection	&rc(Connection::the());
	if(ctx.empty()){
		ctx += " OK [";
		rc.appendContextString(ctx);
		ctx += "] Done LOGIN@";
		Connection::the().socketData(_sid).w.push(&Writer::putStatus, protocol::text::Parameter((void*)ctx.data(), ctx.size()));
	}else{
		SocketData	&rsd(rc.socketData(_sid));
		rsd.w.push(&Writer::returnValue<false>, protocol::text::Parameter(Writer::Leave));
// 		ctx += " OK [";
// 		rc.appendContextString(ctx);
// 		ctx += "] Done LOGIN2@";
// 		Connection::the().socketData(_sid).w.push(&Writer::putStatus, protocol::text::Parameter((void*)ctx.data(), ctx.size()));
		//first return unregister
		//rsd.w.push(&Writer::returnValue, protocol::text::Parameter(Writer::Unregister));
	}
	return OK;
}

void Login::contextData(ObjectUidT &_robjuid){
	cassert(ctx.size());
	if(sizeof(_robjuid.first) == 8){
		sscanf(ctx.c_str(), "%llX-%X", &_robjuid.first, &_robjuid.second);
	}else{
		sscanf(ctx.c_str(), "%X-%X", &_robjuid.first, &_robjuid.second);
	}
	ctx.clear();
}
//---------------------------------------------------------------
// Open command
//---------------------------------------------------------------

Open::Open(){
}
Open::~Open(){
	Connection	&rc(Connection::the());
	rc.deleteRequestId(reqid);
}
void Open::initReader(Reader &_rr){
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&flags));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&path));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
}
int Open::execute(const uint _sid){
	Connection	&rc(Connection::the());
	SocketData	&rsd(rc.socketData(_sid));
	
	protocol::text::Parameter &rp(rsd.w.push(&Writer::putStatus));
	pp = &rp;
	
	if(flags.empty() || (tolower(flags[0]) != 'r')){
		rp =  protocol::text::Parameter(StrDef(" NO Fail [\"Only READ is allowed for now\"] OPEN@"));
		return OK;
	}
	state = InitLocal;
	rsd.w.push(&Writer::reinit<Open>, protocol::text::Parameter(this));
	return OK;
}
int Open::reinitWriter(Writer &_rw, protocol::text::Parameter &_rp){
	switch(state){
		case InitLocal:
			return doInitLocal(_rw.socketId());
		case DoneLocal:
			return doDoneLocal(_rw);
		case WaitLocal:
			return Writer::No;
		case SendError:
			*pp = protocol::text::Parameter(StrDef(" NO Fail [\"Unable to open file\"] OPEN@"));
			return Writer::Ok;
	}
	cassert(false);
	return Writer::Bad;
}

int Open::doInitLocal(const uint _sid){
	
	idbg(""<<(void*)this);
	
	Connection	&rc(Connection::the());
	
	//TODO:
// 	uint32 r = rc.checkIfAlreadyOpen(path);
// 	if(r == (uint32)-1){
// 		
// 	}
	
	//try to open stream to localfile
	reqid = rc.newRequestId(_sid);
	frame::RequestUid rqid(rc.id(), Manager::the().id(rc).second, reqid);
	int rv = Manager::the().fileManager().stream(isp, reqid, path.c_str());
	switch(rv){
		case BAD: 
			*pp = protocol::text::Parameter(StrDef(" NO Fail [\"Unable to open file\"] OPEN@"));
			return Writer::Ok;
		case OK: 
			state = DoneLocal;
			return Writer::Continue;
		case NOK:
			state = WaitLocal;
			return Writer::No;
	}
	cassert(false);
	return BAD;
}


int Open::doDoneLocal(Writer &_rw){
	idbg(""<<(void*)this);
	
	uint32	pos(-1);
	uint64	sz(-1);
	
	//TODO:
	
	if(isp){
		sz = isp->size();
		//pos = rc.insertStream(this->path, isp);
	}else if(iosp){
		sz = iosp->size();
		//pos = rc.insertStream(this->path, iosp);
	}else if(osp){
		sz = osp->size();
		//pos = rc.insertStream(this->path, osp);
	}else{
		cassert(false);
	}
	if(pos == -1) {
		state = SendError;
		return Writer::Continue;
	}
	
	
	
	return Writer::Bad;
}


int Open::receiveInputStream(
	StreamPointer<InputStream> &_sptr,
	const FileUidT &_fuid,
	int _which,
	const ObjectUidT&,
	const solid::frame::ipc::ConnectionUid *
){
	//sp_out =_sptr;
	//fuid = _fuid;
	if(state == WaitLocal){
		isp = _sptr;
		reqid = 0;
		state = DoneLocal;
	}else{
		cassert(false);
	}
	return OK;
}

int Open::receiveError(
	int _errid,
	const ObjectUidT&_from,
	const solid::frame::ipc::ConnectionUid *
){
	state = SendError;
	return OK;
}



}//namespace gamma
}//namespace concept
