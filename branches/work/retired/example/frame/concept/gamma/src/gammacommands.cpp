#include <cstdio>

#include "system/debug.hpp"
#include "system/socketaddress.hpp"
#include "system/filedevice.hpp"
#include "system/thread.hpp"
#include "system/timespec.hpp"

#include "protocol/text/namematcher.hpp"
#include "serialization/binary.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/ipc/ipcservice.hpp"
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
void Basic::execute(const uint _sid){
	switch(tp){
		case Noop:	execNoop(_sid); break;
		case Logout: execLogout(_sid); break;
		case Capability: execCapability(_sid); break;
	}
}

void Basic::execNoop(const uint _sid){
	Connection::the().socketData(_sid).w.push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done NOOP@")));
}
void Basic::execLogout(const uint _sid){
	Connection	&rc(Connection::the());
	SocketData	&rsd(rc.socketData(_sid));
	rsd.w.push(&Writer::returnValue<true>, protocol::text::Parameter(Writer::Failure));
	rsd.w.push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done LOGOUT@")));
	rsd.w.push(&Writer::putAtom, protocol::text::Parameter(StrDef("* Alpha connection closing\r\n")));
}
void Basic::execCapability(const uint _sid){
	Connection	&rc(Connection::the());
	SocketData	&rsd(rc.socketData(_sid));
	rsd.w.push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done CAPABILITY@")));
	rsd.w.push(&Writer::putAtom, protocol::text::Parameter(StrDef("* CAPABILITIES noop logout login\r\n")));
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
void Login::execute(const uint _sid){
	Connection	&rc(Connection::the());
	if(ctx.empty()){
		ctx += " OK [";
		rc.appendContextString(ctx);
		ctx += "] Done LOGIN@";
		Connection::the().socketData(_sid).w.push(&Writer::putStatus, protocol::text::Parameter((void*)ctx.data(), ctx.size()));
	}else{
		SocketData	&rsd(rc.socketData(_sid));
		rsd.w.push(&Writer::returnValue<false>, protocol::text::Parameter(Writer::Leave));
	}
}

void Login::contextData(ObjectUidT &_robjuid){
	cassert(ctx.size());
	if(sizeof(_robjuid.first) == 8){
		unsigned long long v;
		sscanf(ctx.c_str(), "%llX-%X", &v, &_robjuid.second);
		_robjuid.first = v;
	}else{
		sscanf(ctx.c_str(), "%lX-%X", &_robjuid.first, &_robjuid.second);
	}
	ctx.clear();
}

}//namespace gamma
}//namespace concept
