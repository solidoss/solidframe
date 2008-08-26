/* Implementation file alphacommands.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "system/debug.hpp"
#include "system/socketaddress.hpp"
#include "system/filedevice.hpp"

#include "utility/iostream.hpp"

#include "algorithm/protocol/namematcher.hpp"
#include "algorithm/serialization/binary.hpp"

#include "clientserver/ipc/ipcservice.hpp"
#include "clientserver/ipc/ipcservice.hpp"
#include "clientserver/core/filemanager.hpp"
#include "clientserver/core/commandexecuter.hpp"
#include "clientserver/core/requestuid.hpp"

#include "core/common.hpp"
#include "core/tstring.hpp"
#include "core/server.hpp"
#include "core/command.hpp"


#include "alphaconnection.hpp"
#include "alphacommands.hpp"
#include "alphawriter.hpp"
#include "alphareader.hpp"
#include "alphaconnection.hpp"
#include "alphaprotocolfilters.hpp"


#define StrDef(x) (void*)x, sizeof(x) - 1

namespace cs=clientserver;


namespace std{

template <class S>
S& operator&(pair<String, int64> &_v, S &_s){
	idbg("stringint64 "<<(void*)&_v.second);
	return _s.push(_v.first, "first").push(_v.second, "second");
}
template <class S>
S& operator&(pair<unsigned int, unsigned int> &_v, S &_s){
	return _s.push(_v.first, "first").push(_v.second, "second");
}
template <class S>
S& operator&(pair<long unsigned int, unsigned int> &_v, S &_s){
	return _s.push(_v.first, "first").push(_v.second, "second");
}
}

namespace test{
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
	{"sendstream",Cmd::SendStreamCmd},
	{"sendstring",Cmd::SendStringCmd},
	{"idle",Cmd::IdleCmd},
	{NULL,Cmd::CmdCount},
};
static const protocol::NameMatcher cmdm(cmds);
//---------------------------------------------------------------
/*
	The creator method called by cs::Reader::fetchKey when the 
	command name was parsed.
	All it does is to create the proper command, which in turn,
	will instruct the reader how to parse itself.
*/
Command* Connection::create(const String& _name){
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
		case Cmd::SendStreamCmd:
			return pcmd = new SendStream;
		case Cmd::SendStringCmd:
			return pcmd = new SendString;
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
int Basic::execute(Connection &_rc){
	switch(tp){
		case Noop:	return execNoop(_rc);
		case Logout: return execLogout(_rc);
		case Capability: return execCapability(_rc);
	}
	return BAD;
}
int Basic::execNoop(Connection &_rc){
	_rc.writer().push(&Writer::putStatus, protocol::Parameter(StrDef(" OK Done NOOP@")));
	return OK;
}
int Basic::execLogout(Connection &_rc){
	_rc.writer().push(&Writer::returnValue, protocol::Parameter(Writer::Bad));
	_rc.writer().push(&Writer::putStatus, protocol::Parameter(StrDef(" OK Done LOGOUT@")));
	_rc.writer().push(&Writer::putAtom, protocol::Parameter(StrDef("* Alpha connection closing\r\n")));
	return NOK;
}
int Basic::execCapability(Connection &_rc){
	_rc.writer().push(&Writer::putStatus, protocol::Parameter(StrDef(" OK Done CAPABILITY@")));
	_rc.writer().push(&Writer::putAtom, protocol::Parameter(StrDef("* CAPABILITIES noop logout login\r\n")));
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
	_rr.push(&Reader::manage, protocol::Parameter(Reader::ResetLogging));
	_rr.push(&Reader::fetchAString, protocol::Parameter(&pass));
	_rr.push(&Reader::manage, protocol::Parameter(Reader::ClearLogging));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::Parameter(&user));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
}
int Login::execute(Connection &_rc){
	_rc.writer().push(&Writer::putStatus, protocol::Parameter(StrDef(" OK Done LOGIN@")));
	return OK;
}
//---------------------------------------------------------------
// List command
//---------------------------------------------------------------
List::List(){
}
List::~List(){
}
void List::initReader(Reader &_rr){
	_rr.push(&Reader::fetchAString, protocol::Parameter(&strpth));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
}
int List::execute(Connection &_rc){
	idbg("path: "<<strpth);
	fs::path pth(strpth.c_str(), fs::native);
	protocol::Parameter &rp = _rc.writer().push(&Writer::putStatus);
	rp = protocol::Parameter(StrDef(" OK Done LIST@"));
	if(!exists( pth ) || !is_directory(pth)){
		rp = protocol::Parameter(StrDef(" NO LIST: Not a directory@"));
		return OK;
	}
	try{
	it = fs::directory_iterator(pth);
	}catch ( const std::exception & ex ){
		idbg("dir_iterator exception :"<<ex.what());
		return OK;
	}
	_rc.writer().push(&Writer::reinit<List>, protocol::Parameter(this, Step));
	if(it != end){
		_rc.writer().push(&Writer::putCrlf);
		_rc.writer().push(&Writer::putAString, protocol::Parameter((void*)it->string().data(), it->string().size()));
		if(is_directory(*it)){
			_rc.writer()<<"* DIR ";
		}else{
			_rc.writer()<<"* FILE "<<(uint32)FileDevice::size(it->string().c_str())<<' ';
		}
	}
	return OK;
}

int List::reinitWriter(Writer &_rw, protocol::Parameter &_rp){
	++it;
	if(it != end){
		_rw.push(&Writer::putCrlf);
		_rw.push(&Writer::putAString, protocol::Parameter((void*)it->string().data(), it->string().size()));
		if(is_directory(*it)){
			_rw<<"* DIR ";
		}else{
			_rw<<"* FILE "<<(uint32)FileDevice::size(it->string().c_str())<<' ';
		}
		return Writer::Continue;
	}
	return Writer::Ok;
}
//---------------------------------------------------------------
// RemoteList command
//---------------------------------------------------------------
struct RemoteListCommand: test::Command{
	RemoteListCommand(): ppthlst(NULL),err(-1){
		idbg(""<<(void*)this);
	}
	~RemoteListCommand(){
		idbg(""<<(void*)this);
		//delete ppthlst;
	}
	int received(const cs::ipc::ConnectorUid &_rconid);
	int execute(test::Connection &);
	int execute(cs::CommandExecuter&, const CommandUidTp &, TimeSpec &_rts);
	template <class S>
	S& operator&(S &_s){
		return _s.pushContainer(ppthlst, "strlst").push(err, "error").push(requid, "requid").push(strpth, "strpth").push(fromv, "from");
	}
//data:
	RemoteList::PathListTp		*ppthlst;
	String						strpth;
	int							err;
	cs::ipc::ConnectorUid		conid;
	uint32						requid;
	ObjectUidTp					fromv;
};

int RemoteListCommand::received(const cs::ipc::ConnectorUid &_rconid){
	cs::CmdPtr<cs::Command> pcmd(this);
	conid = _rconid;
	if(!ppthlst){
		idbg("Received RemoteListCommand on peer");
		//print();
		ObjectUidTp	ttov;
		Server::the().readCommandExecuterUid(ttov);
		Server::the().signalObject(ttov.first, ttov.second, pcmd);
	}else{
		idbg("Received RemoteListCommand back on sender");
		//print();
		Server::the().signalObject(fromv.first, fromv.second, pcmd);
	}
	return false;
}
int RemoteListCommand::execute(test::Connection &_rcon){
	if(!err){
		idbg("");
		_rcon.receiveData((void*)ppthlst, -1, RequestUidTp(requid, 0), 0, CommandUidTp(), &conid);
		ppthlst = NULL;
	}else{
		idbg("");
		_rcon.receiveError(err, RequestUidTp(requid, 0), CommandUidTp(), &conid);
	}
	return NOK;
}
int RemoteListCommand::execute(cs::CommandExecuter&, const CommandUidTp &, TimeSpec &_rts){
	fs::directory_iterator	it,end;
	fs::path				pth(strpth.c_str(), fs::native);
	cs::CmdPtr<cs::Command> pcmd(this);
	ppthlst = new RemoteList::PathListTp;
	strpth.clear();
	if(!exists( pth ) || !is_directory(pth)){
		err = -1;
		if(Server::the().ipc().sendCommand(conid, pcmd)){
			idbg("connector was destroyed");
			return BAD;
		}
		return cs::LEAVE;
	}
	try{
	it = fs::directory_iterator(pth);
	}catch ( const std::exception & ex ){
		idbg("dir_iterator exception :"<<ex.what());
		err = -1;
		strpth = ex.what();
		if(Server::the().ipc().sendCommand(conid, pcmd)){
			idbg("connector was destroyed");
			return BAD;
		}
		return cs::LEAVE;
	}
	while(it != end){
		ppthlst->push_back(std::pair<String, int64>(it->string(), -1));
		if(is_directory(*it)){
		}else{
			ppthlst->back().second = FileDevice::size(it->string().c_str());
		}
		++it;
	}
	err = 0;
	if(Server::the().ipc().sendCommand(conid, pcmd)){
		idbg("connector was destroyed "<<conid.tkrid<<' '<<conid.procid<<' '<<conid.procuid);
		return BAD;
	}else{
		idbg("command sent "<<conid.tkrid<<' '<<conid.procid<<' '<<conid.procuid);
	}
	return cs::LEAVE;
}

//--------------------------------------------------------------
RemoteList::PathListTp::PathListTp(){
	idbg(""<<(void*)this);
}
RemoteList::PathListTp::~PathListTp(){
	idbg(""<<(void*)this);
}

RemoteList::RemoteList():ppthlst(NULL),state(SendError){
}
RemoteList::~RemoteList(){
	delete ppthlst;
}
void RemoteList::initReader(Reader &_rr){
	_rr.push(&Reader::fetchUint32, protocol::Parameter(&port));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::Parameter(&straddr));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::Parameter(&strpth));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
}
int RemoteList::execute(Connection &_rc){
	pp = &_rc.writer().push(&Writer::putStatus);
	*pp = protocol::Parameter(StrDef(" OK Done REMOTELIST@"));
	AddrInfo ai(straddr.c_str(), port, 0, AddrInfo::Inet4, AddrInfo::Stream);
	idbg("addr"<<straddr<<" port = "<<port);
	if(!ai.empty()){
		RemoteListCommand *pcmd(new RemoteListCommand);
		pcmd->strpth = strpth;
		pcmd->requid = _rc.newRequestId();
		pcmd->fromv.first = _rc.id();
		pcmd->fromv.second = Server::the().uid(_rc);
		state = Wait;
		cs::CmdPtr<cs::Command> cmdptr(pcmd);
		Server::the().ipc().sendCommand(ai.begin(), cmdptr);
		_rc.writer().push(&Writer::reinit<RemoteList>, protocol::Parameter(this));
	}else{
		*pp = protocol::Parameter(StrDef(" NO REMOTELIST: no such peer address@"));
	}
	return OK;
}
int RemoteList::reinitWriter(Writer &_rw, protocol::Parameter &_rp){
	switch(state){
		case Wait:
			return Writer::No;
		case SendListContinue:
			++it;
		case SendList:
			if(it != ppthlst->end()){
				_rw.push(&Writer::putCrlf);
				_rw.push(&Writer::putAString, protocol::Parameter((void*)it->first.data(), it->first.size()));
				if(it->second < 0){
					_rw<<"* DIR ";
				}else{
					_rw<<"* FILE "<<(uint32)it->second<<' ';
				}
				state = SendListContinue;
				return Writer::Continue;
			}
			return Writer::Ok;
		case SendError:
			*pp = protocol::Parameter(StrDef(" NO LIST: Not a directory@"));
			return Writer::Ok;
	};
	cassert(false);
	return BAD;
}
int RemoteList::receiveData(
	void *_pdata,
	int _datasz,
	int	_which, 
	const ObjectUidTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	ppthlst = reinterpret_cast<PathListTp*>(_pdata);
	if(ppthlst){
		it = ppthlst->begin();
		state = SendList;
	}else{
		state = SendError;
	}
	return OK;
}
int RemoteList::receiveError(
	int _errid, 
	const ObjectUidTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	idbg("");
	state = SendError;
	return OK;
}
//---------------------------------------------------------------
// Fetch command
//---------------------------------------------------------------

struct FetchSlaveCommand;
enum{
	FetchChunkSize = 1024*1024
};
/*
	This request is first sent to a peer's command executer - where 
	# it tries to get a stream,
	# sends a respose (FetchSlaveCommand) with the stream size and at most 1MB from the stream
	# waits for FetchSlaveCommand(s) to give em the rest of stream chunks
	# when the last stream chunk was sent it dies.
*/

struct FetchMasterCommand: test::Command{
	enum{
		NotReceived,
		Received,
		SendFirstStream,
		SendNextStream,
		SendError,
	};
	FetchMasterCommand():pcmd(NULL), state(NotReceived), insz(-1), inpos(0), requid(0){
		idbg("");
	}
	~FetchMasterCommand();
	int received(const cs::ipc::ConnectorUid &_rconid);
	int execute(cs::CommandExecuter&, const CommandUidTp &, TimeSpec &_rts);
	int receiveCommand(
		cs::CmdPtr<cs::Command> &_rcmd,
		int			_which,
		const ObjectUidTp&_from,
		const cs::ipc::ConnectorUid *
	);
	int receiveIStream(
		StreamPtr<IStream> &,
		const FileUidTp	&,
		int			_which,
		const ObjectUidTp&_from,
		const cs::ipc::ConnectorUid *
	);
	int receiveError(
		int _errid, 
		const ObjectUidTp&_from,
		const cs::ipc::ConnectorUid *_conid
	);
	template <class S>
	S& operator&(S &_s){
		_s.push(fname, "filename");
		_s.push(tmpfuid.first, "tmpfileuid_first").push(tmpfuid.second, "tmpfileuid_second");
		return _s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid").push(requid, "requestuid");
	}
	void print()const;
//data:
	String					fname;
	FetchSlaveCommand		*pcmd;
	ObjectUidTp				fromv;
	FileUidTp				fuid;
	FileUidTp				tmpfuid;
	cs::ipc::ConnectorUid	conid;
	StreamPtr<IStream>		ins;
	int						state;
	int64					insz;
	int64					inpos;
	uint32					requid;
};

/*
	The commands sent from the alpha connection to the remote FetchMasterCommand
	to request new file chunks, and from FetchMasterCommand to the alpha connection
	as reponse containing the requested file chunk.
*/
struct FetchSlaveCommand: test::Command{
	FetchSlaveCommand(): insz(-1), sz(-10), requid(0){
		idbg("");
	}
	~FetchSlaveCommand(){
		idbg("");
	}
	int received(const cs::ipc::ConnectorUid &_rconid);
	int execute(test::Connection &);
	int execute(cs::CommandExecuter&, const CommandUidTp &, TimeSpec &_rts);
	int createDeserializationStream(std::pair<OStream *, int64> &_rps, int _id);
	void destroyDeserializationStream(const std::pair<OStream *, int64> &_rps, int _id);
	int createSerializationStream(std::pair<IStream *, int64> &_rps, int _id);
	void destroySerializationStream(const std::pair<IStream *, int64> &_rps, int _id);
	
	template <class S>
	S& operator&(S &_s){
		_s.template pushStreammer<FetchSlaveCommand>(this, "FetchStreamResponse::isp");
		_s.push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		_s.push(insz, "inputstreamsize").push(requid, "requestuid");
		_s.push(sz, "inputsize").push(cmduid.first, "commanduid_first").push(cmduid.second, "commanduid_second");
		_s.push(fuid.first,"fileuid_first").push(fuid.second, "fileuid_second");
		return _s;
	}
	void print()const;
//data:	
	ObjectUidTp					tov;
	FileUidTp					fuid;
	cs::ipc::ConnectorUid		conid;
	CommandUidTp				cmduid;
	StreamPtr<IStream>			ins;
	//if insz >= 0 -> [0->1M) else -> [1M->2M)
	int64						insz;
	int							sz;
	uint32						requid;
};
//-------------------------------------------------------------------------------
FetchMasterCommand::~FetchMasterCommand(){
	delete pcmd;
	idbg("");
}

void FetchMasterCommand::print()const{
	idbg("FetchMasterCommand:");
	idbg("state = "<<state<<" insz = "<<insz<<" requid = "<<requid<<" fname = "<<fname);
	idbg("fromv.first = "<<fromv.first<<" fromv.second = "<<fromv.second);
	idbg("fuid.first = "<<fuid.first<<" fuid.second = "<<fuid.second);
	idbg("tmpfuid.first = "<<tmpfuid.first<<" tmpfuid.second = "<<tmpfuid.second);
}

int FetchMasterCommand::received(const cs::ipc::ConnectorUid &_rconid){
	cs::CmdPtr<cs::Command> cmd(this);
	conid = _rconid;
	state = Received;
	ObjectUidTp	tov;
	idbg("received master command");
	print();
	Server::the().readCommandExecuterUid(tov);
	Server::the().signalObject(tov.first, tov.second, cmd);
	return false;
}
/*
	The state machine running on peer
*/
int FetchMasterCommand::execute(cs::CommandExecuter& _rce, const CommandUidTp &_cmduid, TimeSpec &_rts){
	Server &rs(Server::the());
	switch(state){
		case Received:{
			idbg("try to open file "<<fname<<" _cmduid = "<<_cmduid.first<<","<<_cmduid.second);
			//try to get a stream for the file:
			cs::RequestUid reqid(_rce.id(), rs.uid(_rce), _cmduid.first, _cmduid.second);
			switch(rs.fileManager().stream(ins, fuid, reqid, fname.c_str())){
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
			idbg("send first stream");
			FetchSlaveCommand	*pcmd = new FetchSlaveCommand;
			cs::CmdPtr<cs::Command> cmdptr(pcmd);
			insz = ins->size();
			pcmd->tov = fromv;
			pcmd->insz = insz;
			pcmd->sz = FetchChunkSize;
			if(pcmd->sz > pcmd->insz){
				pcmd->sz = pcmd->insz;
			}
			pcmd->cmduid = _cmduid;
			pcmd->requid = requid;
			pcmd->fuid = tmpfuid;
			idbg("insz = "<<insz<<" inpos = "<<inpos);
			insz -= pcmd->sz;
			inpos += pcmd->sz;
			cs::RequestUid reqid(_rce.id(), rs.uid(_rce), _cmduid.first, _cmduid.second); 
			rs.fileManager().stream(pcmd->ins, fuid, requid, cs::FileManager::NoWait);
			pcmd = NULL;
			if(rs.ipc().sendCommand(conid, cmdptr) || !insz){
				idbg("connector was destroyed or insz "<<insz);
				return BAD;
			}
			idbg("wait for streams");
			//TODO: put here timeout! - wait for commands
			//_rts.add(30);
			return NOK;
		}
		case SendNextStream:{
			idbg("send next stream");
			cs::CmdPtr<cs::Command> cmdptr(pcmd);
			pcmd->tov = fromv;
			pcmd->sz = FetchChunkSize;
			if(pcmd->sz > insz){
				pcmd->sz = insz;
			}
			pcmd->cmduid = _cmduid;
			pcmd->fuid = tmpfuid;
			idbg("insz = "<<insz<<" inpos = "<<inpos);
			insz -= pcmd->sz;
			cs::RequestUid reqid(_rce.id(), rs.uid(_rce), _cmduid.first, _cmduid.second); 
			rs.fileManager().stream(pcmd->ins, fuid, requid, cs::FileManager::NoWait);
			pcmd->ins->seek(inpos);
			inpos += pcmd->sz;
			cassert(pcmd->ins);
			pcmd = NULL;
			if(rs.ipc().sendCommand(conid, cmdptr) || !insz){
				idbg("connector was destroyed or insz "<<insz);
				return BAD;
			}
			idbg("wait for streams");
			//TODO: put here timeout! - wait for commands
			//_rts.add(30);
			return NOK;
		}
		case SendError:{
			idbg("sending error");
			FetchSlaveCommand *pcmd = new FetchSlaveCommand;
			cs::CmdPtr<cs::Command> cmdptr(pcmd);
			pcmd->tov = fromv;
			pcmd->sz = -2;
			rs.ipc().sendCommand(conid, cmdptr);
			return BAD;
		}
	}
	return BAD;
}
int FetchMasterCommand::receiveCommand(
	cs::CmdPtr<cs::Command> &_rcmd,
	int			_which,
	const ObjectUidTp&_from,
	const cs::ipc::ConnectorUid *
){
	pcmd = static_cast<FetchSlaveCommand*>(_rcmd.release());
	idbg("");
	state = SendNextStream;
	return OK;
}
int FetchMasterCommand::receiveIStream(
	StreamPtr<IStream> &_rins,
	const FileUidTp	& _fuid,
	int			_which,
	const ObjectUidTp&,
	const cs::ipc::ConnectorUid *
){
	idbg("fuid = "<<_fuid.first<<","<<_fuid.second);
	ins = _rins;
	fuid = _fuid;
	state = SendFirstStream;
	return OK;
}
int FetchMasterCommand::receiveError(
	int _errid, 
	const ObjectUidTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	idbg("");
	state = SendError;
	return OK;
}
//-------------------------------------------------------------------------------
void FetchSlaveCommand::print()const{
	idbg("FetchSlaveCommand:");
	idbg("insz = "<<insz<<" sz = "<<sz<<" requid = "<<requid);
	idbg("fuid.first = "<<fuid.first<<" fuid.second = "<<fuid.second);
	idbg("cmduid.first = "<<cmduid.first<<" cmduid.second = "<<cmduid.second);
}
int FetchSlaveCommand::received(const cs::ipc::ConnectorUid &_rconid){
	cs::CmdPtr<cs::Command> pcmd(this);
	conid = _rconid;
	if(sz == -10){
		idbg("Received FetchSlaveCommand on peer");
		print();
		ObjectUidTp	ttov;
		Server::the().readCommandExecuterUid(ttov);
		Server::the().signalObject(ttov.first, ttov.second, pcmd);
	}else{
		idbg("Received FetchSlaveCommand on sender");
		print();
		Server::the().signalObject(tov.first, tov.second, pcmd);
	}
	return false;
}
// Executed when received back on the requesting alpha connection
int FetchSlaveCommand::execute(test::Connection &_rcon){
	if(sz >= 0){
		idbg("");
		_rcon.receiveNumber(insz, RequestUidTp(requid, 0), 0, cmduid, &conid);
	}else{
		idbg("");
		_rcon.receiveError(-1, RequestUidTp(requid, 0), cmduid, &conid);
	}
	return NOK;
}
// Executed on peer within the command executer
int FetchSlaveCommand::execute(cs::CommandExecuter& _rce, const CommandUidTp &, TimeSpec &){
	idbg("");
	cs::CmdPtr<cs::Command>	cp(this);
	_rce.receiveCommand(cp, cmduid);
	return cs::LEAVE;
}

void FetchSlaveCommand::destroyDeserializationStream(
	const std::pair<OStream *, int64> &_rps, int _id
){
	idbg("Destroy deserialization <"<<_id<<"> sz "<<_rps.second<<" streamptr "<<(void*)_rps.first);
	if(_rps.second < 0){
		//there was an error
		sz = -1;
	}
	delete _rps.first;
}

int FetchSlaveCommand::createDeserializationStream(
	std::pair<OStream *, int64> &_rps, int _id
){
	if(_id) return NOK;
	if(sz <= 0) return NOK;
	StreamPtr<OStream>			sp;
	cs::RequestUid	requid;
	Server::the().fileManager().stream(sp, fuid, requid, cs::FileManager::Forced);
	if(!sp) return BAD;
	idbg("Create deserialization <"<<_id<<"> sz "<<_rps.second<<" streamptr "<<(void*)sp.ptr());
	if(insz < 0){//back 1M
		sp->seek(FetchChunkSize);
	}
	cassert(sp);
	_rps.first = sp.release();
	_rps.second = sz;
	return OK;
}

void FetchSlaveCommand::destroySerializationStream(
	const std::pair<IStream *, int64> &_rps, int _id
){
	idbg("doing nothing as the stream will be destroied when the command will be destroyed");
}
int FetchSlaveCommand::createSerializationStream(
	std::pair<IStream *, int64> &_rps, int _id
){
	if(_id || !ins.ptr()) return NOK;
	idbg("Create serialization <"<<_id<<"> sz "<<_rps.second);
	_rps.first = ins.ptr();
	_rps.second = sz;
	return OK;
}
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
Fetch::Fetch(Connection &_rc):port(-1), rc(_rc), st(0), pp(NULL){
}
Fetch::~Fetch(){
	idbg("");
	sp.clear();
}
void Fetch::initReader(Reader &_rr){
	typedef CharFilter<' '>				SpaceFilterTp;
	typedef NotFilter<SpaceFilterTp> 	NotSpaceFilterTp;
	_rr.push(&Reader::fetchUint32, protocol::Parameter(&port));
	_rr.push(&Reader::dropChar);
	_rr.push(&Reader::checkIfCharThenPop<NotSpaceFilterTp>, protocol::Parameter(2));
	_rr.push(&Reader::fetchAString, protocol::Parameter(&straddr));
	_rr.push(&Reader::dropChar);
	_rr.push(&Reader::checkIfCharThenPop<NotSpaceFilterTp>, protocol::Parameter(5));
	_rr.push(&Reader::fetchAString, protocol::Parameter(&strpth));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
	
}
int Fetch::execute(Connection &_rc){
	idbg("path "<<strpth<<", address "<<straddr<<", port "<<port);
	protocol::Parameter &rp = _rc.writer().push(&Writer::putStatus);
	pp = &rp;
	rp = protocol::Parameter(StrDef(" OK Done FETCH@"));
	if(port == (uint)-1) port = 1222;//default ipc port
	if(straddr.empty()){
		st = InitLocal;
	}else{
		st = InitRemote;
	}
	_rc.writer().push(&Writer::reinit<Fetch>, protocol::Parameter(this));
	return OK;
}
int Fetch::reinitWriter(Writer &_rw, protocol::Parameter &_rp){
	switch(st){
		case InitLocal:{
			idbg("init local");
			//try to open stream to localfile
			cs::RequestUid reqid(rc.id(), Server::the().uid(rc), rc.newRequestId());
			int rv = Server::the().fileManager().stream(sp, reqid, strpth.c_str());
			switch(rv){
				case BAD: 
					*pp = protocol::Parameter(StrDef(" NO FETCH: Unable to open file@"));
					return Writer::Ok;
				case OK: 
					st = SendLocal;
					return Writer::Continue;
				case NOK:
					st = WaitStreamLocal;
					return Writer::No;
			}
		}break;
		case SendLocal:
			idbg("send local");
			//send local stream
			cassert(sp);
			litsz64 = sp->size();
			it.reinit(sp.ptr());
			_rw<<"* DATA {"<<(uint32)sp->size()<<"}\r\n";
			_rw.replace(&Writer::putCrlf);
			_rw.push(&Writer::putStream, protocol::Parameter(&it, &litsz64));
			return Writer::Continue;
		case InitRemote:{
			idbg("init remote");
			//try to open a temp stream
			cs::RequestUid reqid(rc.id(), Server::the().uid(rc), rc.newRequestId());
			int rv = Server::the().fileManager().stream(sp, fuid, reqid, NULL, 0);
			switch(rv){
				case BAD: 
					*pp = protocol::Parameter(StrDef(" NO FETCH: Unable to open temp file@"));
					return Writer::No;
				case OK: 
					st = SendMasterRemote;
					return Writer::Continue;
				case NOK:
					st = WaitStreamRemote;
					return Writer::No;
			}
		}break;
		case SendMasterRemote:{
			idbg("send master remote");
			cassert(sp);
			AddrInfo ai(straddr.c_str(), port, 0, AddrInfo::Inet4, AddrInfo::Stream);
			idbg("addr"<<straddr<<" port = "<<port);
			if(!ai.empty()){
				//send the master remote command
				FetchMasterCommand *pcmd(new FetchMasterCommand);
				//TODO: add a convenient init method to fetchmastercommand
				pcmd->fname = strpth;
				pcmd->requid = rc.newRequestId();
				pcmd->fromv.first = rc.id();
				pcmd->fromv.second = Server::the().uid(rc);
				pcmd->tmpfuid = fuid;
				st = WaitFirstRemote;
				cs::CmdPtr<cs::Command> cmdptr(pcmd);
				Server::the().ipc().sendCommand(ai.begin(), cmdptr);
				return Writer::No;
			}else{
				*pp = protocol::Parameter(StrDef(" NO FETCH: no such peer address@"));
				return Writer::Ok;
			}
		}
		case SendFirstRemote:{
			idbg("send first remote");
			//The first chunk of data was received
			it.reinit(sp.ptr(), 0);
			_rw<<"* DATA {"<<(uint32)litsz64<<"}\r\n";
			chunksz = FetchChunkSize;
			isfirst = 1;
			if(chunksz >= litsz64){
				_rw.replace(&Writer::putCrlf);
				chunksz = litsz64;
				litsz64 = 0;
			}else{
				//send another request
				st = WaitNextRemote;
				FetchSlaveCommand *pcmd(new FetchSlaveCommand);
				pcmd->requid = rc.newRequestId();
				pcmd->cmduid = mastercmduid;
				pcmd->fuid = fuid;
				cs::CmdPtr<cs::Command> cmdptr(pcmd);
				if(Server::the().ipc().sendCommand(conuid, cmdptr) == BAD){
					return Writer::Bad;
				}
				litsz64 -= chunksz;
			}
			_rw.push(&Writer::putStream, protocol::Parameter(&it, &chunksz));
			return Writer::Continue;
		}
		case SendNextRemote:{
			bool isfrst = isfirst & 1;
			isfirst = (isfirst + 1) & 1;
			idbg("send next remote "<<(isfrst ? FetchChunkSize : 0));
			it.reinit(sp.ptr(), isfrst ? FetchChunkSize : 0);
			chunksz = FetchChunkSize;
			if(chunksz >= litsz64){
				_rw.replace(&Writer::putCrlf);
				chunksz = litsz64;
				litsz64 = 0;
			}else{
				//send another request
				st = WaitNextRemote;
				FetchSlaveCommand *pcmd(new FetchSlaveCommand);
				pcmd->requid = rc.newRequestId();
				pcmd->cmduid = mastercmduid;
				pcmd->fuid = fuid;
				pcmd->insz = isfrst ? 0 : -1;
				cs::CmdPtr<cs::Command> cmdptr(pcmd);
				if(Server::the().ipc().sendCommand(conuid, cmdptr) == BAD){
					return Writer::Bad;
				}
				litsz64 -= chunksz;
			}
			//cassert(false);
			_rw.push(&Writer::putStream, protocol::Parameter(&it, &chunksz));
			return Writer::Continue;
		}
		case SendError:
			idbg("send error");
			*pp = protocol::Parameter(StrDef(" NO FETCH: Unable to open file@"));
			return Writer::Ok;
		case SendTempError:
			idbg("send temp error");
			*pp = protocol::Parameter(StrDef(" NO FETCH: Unable to open temp file@"));
			return Writer::Ok;
		case WaitStreamLocal:
		case WaitTempRemote:
		case WaitFirstRemote:
		case WaitStreamRemote:
			idbg("wait");
			cassert(false);
		case WaitNextRemote:
			idbg("wait next remote");
			return Writer::No;
		case ReturnBad:
			idbg("return bad");
			//most certainly serialization/deserialization problems
			return Writer::Bad;
	}
	cassert(false);
	return BAD;
}
int Fetch::receiveIStream(
	StreamPtr<IStream> &_sptr,
	const FileUidTp &_fuid,
	int			_which,
	const ObjectUidTp&,
	const clientserver::ipc::ConnectorUid *
){
	idbg("");
	sp =_sptr;
	fuid = _fuid;
	if(st == WaitStreamLocal)
		st = SendLocal;
	else st =  SendMasterRemote;
	return OK;
}

int Fetch::receiveNumber(
	const int64 &_no,
	int			_which,
	const ObjectUidTp& _objuid,
	const clientserver::ipc::ConnectorUid *_pconuid
){
	idbg("");
	mastercmduid = _objuid;
	cassert(_pconuid);
	conuid = *_pconuid;
	if(st == WaitNextRemote){//continued
		st = SendNextRemote;
	}else{
		litsz64 = _no;
		st = SendFirstRemote;
	}
	return OK;
}

int Fetch::receiveError(
	int _errid,
	const ObjectUidTp&_from,
	const clientserver::ipc::ConnectorUid *
){
	idbg("");
	switch(st){
		case WaitStreamLocal:
			st = SendError;
			break;
		case WaitTempRemote:
			st = SendTempError;
			break;
		default:
			st = ReturnBad;
	}
	return OK;
}

//---------------------------------------------------------------
// Store Command
//---------------------------------------------------------------
Store::Store(Connection &_rc):rc(_rc),st(0){
}
Store::~Store(){
	sp.clear();
}
void Store::initReader(Reader &_rr){
	_rr.push(&Reader::reinit<Store>, protocol::Parameter(this, Init));
	_rr.push(&Reader::checkChar, protocol::Parameter('}'));
	_rr.push(&Reader::fetchUint32, protocol::Parameter(&litsz));
	_rr.push(&Reader::checkChar, protocol::Parameter('{'));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::Parameter(&strpth));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
}
int Store::reinitReader(Reader &_rr, protocol::Parameter &_rp){
	switch(_rp.b.i){
		case Init:{
			cs::RequestUid reqid(rc.id(), Server::the().uid(rc), rc.newRequestId());
			int rv = Server::the().fileManager().stream(sp, reqid, strpth.c_str(), cs::FileManager::Create);
			switch(rv){
				case BAD: return Reader::Ok;
				case OK:
					_rp.b.i = SendWait;
					return Reader::Continue;
				case NOK:
					st = NOK;//waiting
					_rp.b.i = SendWait;
					return Reader::No;
			}
			break;
		}
		case SendWait:
			if(sp){
				idbg("sending wait and preparing fetch");
				it.reinit(sp.ptr());
				rc.writer()<<"* Expecting "<<litsz<<" CHARs\r\n";
				litsz64 = litsz;
				_rr.replace(&Reader::fetchLiteralStream, protocol::Parameter(&it, &litsz64));
				_rr.push(&Reader::flushWriter);
				_rr.push(&Reader::checkChar, protocol::Parameter('\n'));
				_rr.push(&Reader::checkChar, protocol::Parameter('\r'));
				return Reader::Continue;
			}else{
				idbg("no stream");
				if(st == NOK) return Reader::No;//still waiting
				idbg("we have a problem");
				return Reader::Ok;
			}
			break;
	}
	cassert(false);
	return Reader::Bad;
}
int Store::execute(Connection &_rc){
	protocol::Parameter &rp = _rc.writer().push(&Writer::putStatus);
	if(sp && sp->ok()){
		rp = protocol::Parameter(StrDef(" OK Done STORE@"));
	}else{
		rp = protocol::Parameter(StrDef(" NO STORE: Failed opening file@"));
	}
	return OK;
}

int Store::receiveOStream(
	StreamPtr<OStream> &_sptr,
	const FileUidTp &_fuid,
	int			_which,
	const ObjectUidTp&,
	const clientserver::ipc::ConnectorUid *
){
	idbg("received stream");
	sp = _sptr;
	st = OK;
	return OK;
}

int Store::receiveError(
	int _errid,
	const ObjectUidTp&_from,
	const clientserver::ipc::ConnectorUid *
){
	idbg("received error");
	st = BAD;
	return OK;
}

int Store::reinitWriter(Writer &_rw, protocol::Parameter &_rp){
	return Writer::Bad;
}
//---------------------------------------------------------------
// SendString command
//---------------------------------------------------------------
/*
	The command sent to peer with the text
*/
struct SendStringCommand: test::Command{
	SendStringCommand(){}
	SendStringCommand(
		const String &_str,
		ulong _toobjid,
		uint32 _toobjuid,
		ulong _fromobjid,
		uint32 _fromobjuid
	):str(_str), tov(_toobjid, _toobjuid), fromv(_fromobjid, _fromobjuid){}
	int received(const cs::ipc::ConnectorUid &_rconid);
	int execute(test::Connection &);
	template <class S>
	S& operator&(S &_s){
		_s.push(str, "string").push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		return _s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid");
	}
private:
	typedef std::pair<uint32, uint32> ObjPairTp;
	String						str;
	ObjPairTp					tov;
	ObjPairTp					fromv;
	cs::ipc::ConnectorUid		conid;
};

int SendStringCommand::received(const cs::ipc::ConnectorUid &_rconid){
	cs::CmdPtr<cs::Command> pcmd(this);
	conid = _rconid;
	Server::the().signalObject(tov.first, tov.second, pcmd);
	return false;
}

int SendStringCommand::execute(test::Connection &_rcon){
	return _rcon.receiveString(str, test::Connection::RequestUidTp(0, 0), 0, fromv, &conid);
}
//---------------------------------------------------------------
//---------------------------------------------------------------
SendString::SendString():port(0), objid(0), objuid(0){}
SendString::~SendString(){
}
void SendString::initReader(Reader &_rr){
	_rr.push(&Reader::fetchAString, protocol::Parameter(&str));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
	_rr.push(&Reader::fetchUint32, protocol::Parameter(&objuid));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
	_rr.push(&Reader::fetchUint32, protocol::Parameter(&objid));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
	_rr.push(&Reader::fetchUint32, protocol::Parameter(&port));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::Parameter(&addr));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
}
int SendString::execute(alpha::Connection &_rc){
	Server &rs(Server::the());
	ulong	fromobjid(_rc.id());//the id of the current connection
	uint32	fromobjuid(rs.uid(_rc));//the uid of the current connection
	AddrInfo ai(addr.c_str(), port, 0, AddrInfo::Inet4, AddrInfo::Stream);
	idbg("addr"<<addr<<"str = "<<str<<" port = "<<port<<" objid = "<<" objuid = "<<objuid);
	protocol::Parameter &rp = _rc.writer().push(&Writer::putStatus);
	if(!ai.empty()){
		rp = protocol::Parameter(StrDef(" OK Done SENDSTRING@"));
		cs::CmdPtr<cs::Command> cmdptr(new SendStringCommand(str, objid, objuid, fromobjid, fromobjuid));
		rs.ipc().sendCommand(ai.begin(), cmdptr);
	}else{
		rp = protocol::Parameter(StrDef(" NO SENDSTRING no such address@"));
	}
	return NOK;
}
//---------------------------------------------------------------
// SendStream command
//---------------------------------------------------------------
/*
	The command sent to peer with the stream.
*/
struct SendStreamCommand: test::Command{
	SendStreamCommand(){}
	SendStreamCommand(
		StreamPtr<IOStream> &_iosp,
		const String &_str,
		uint32 _myprocid,
		ulong _toobjid,
		uint32 _toobjuid,
		ulong _fromobjid,
		uint32 _fromobjuid
	):iosp(_iosp), dststr(_str), tov(_toobjid, _toobjuid), fromv(_fromobjid, _fromobjuid){}
	~SendStreamCommand(){
		idbg("");
	}
	std::pair<uint32, uint32> to()const{return tov;}
	std::pair<uint32, uint32> from()const{return fromv;}
	int received(const cs::ipc::ConnectorUid &_rconid);
	int execute(test::Connection &);
	int createDeserializationStream(std::pair<OStream *, int64> &_rps, int _id);
	void destroyDeserializationStream(const std::pair<OStream *, int64> &_rps, int _id);
	int createSerializationStream(std::pair<IStream *, int64> &_rps, int _id);
	void destroySerializationStream(const std::pair<IStream *, int64> &_rps, int _id);
	
	template <class S>
	S& operator&(S &_s){
		_s.template pushStreammer<SendStreamCommand>(this, "SendStreamCommand::iosp").push(dststr, "dststr");
		_s.push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		return _s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid");
	}
private:
	typedef std::pair<uint32, uint32> 	ObjPairTp;
	typedef std::pair<uint32, uint32> 	FileUidTp;
	StreamPtr<IOStream>			iosp;
	String						dststr;
	ObjPairTp					tov;
	ObjPairTp					fromv;
	cs::ipc::ConnectorUid		conid;
};
//-------------------------------------------------------------------------------
int SendStreamCommand::received(const cs::ipc::ConnectorUid &_rconid){
	cs::CmdPtr<cs::Command> pcmd(this);
	conid = _rconid;
	Server::the().signalObject(tov.first, tov.second, pcmd);
	return false;
}

void SendStreamCommand::destroyDeserializationStream(
	const std::pair<OStream *, int64> &_rps, int _id
){
	idbg("Destroy deserialization <"<<_id<<"> sz "<<_rps.second);
}
int SendStreamCommand::createDeserializationStream(
	std::pair<OStream *, int64> &_rps, int _id
){
	if(_id) return NOK;
	idbg("Create deserialization <"<<_id<<"> sz "<<_rps.second);
	if(dststr.empty()/* || _rps.second < 0*/) return NOK;
	idbg("File name: "<<this->dststr);
	//TODO:
	int rv = Server::the().fileManager().stream(this->iosp, this->dststr.c_str(), cs::FileManager::NoWait);
	if(rv){
		idbg("Oops, could not open file");
		return BAD;
	}else{
		_rps.first = static_cast<OStream*>(this->iosp.ptr());
	}
	return OK;
}
void SendStreamCommand::destroySerializationStream(
	const std::pair<IStream *, int64> &_rps, int _id
){
	idbg("doing nothing as the stream will be destroied when the command will be destroyed");
}
int SendStreamCommand::createSerializationStream(
	std::pair<IStream *, int64> &_rps, int _id
){
	if(_id) return NOK;
	idbg("Create serialization <"<<_id<<"> sz "<<_rps.second);
	//The stream is already opened
	_rps.first = static_cast<IStream*>(this->iosp.ptr());
	_rps.second = this->iosp->size();
	return OK;
}

int SendStreamCommand::execute(test::Connection &_rcon){
	{
	StreamPtr<IStream>	isp(static_cast<IStream*>(iosp.release()));
	idbg("");
	_rcon.receiveIStream(isp, test::Connection::FileUidTp(0,0), test::Connection::RequestUidTp(0, 0), 0, fromv, &conid);
	idbg("");
	}
	return _rcon.receiveString(dststr, test::Connection::RequestUidTp(0, 0), 0, fromv, &conid);
}
//---------------------------------------------------------------
//---------------------------------------------------------------
SendStream::SendStream():port(0), objid(0), objuid(0){}
SendStream::~SendStream(){
}
void SendStream::initReader(Reader &_rr){
	_rr.push(&Reader::fetchAString, protocol::Parameter(&dststr));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::Parameter(&srcstr));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
	_rr.push(&Reader::fetchUint32, protocol::Parameter(&objuid));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
	_rr.push(&Reader::fetchUint32, protocol::Parameter(&objid));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
	_rr.push(&Reader::fetchUint32, protocol::Parameter(&port));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::Parameter(&addr));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
}
int SendStream::execute(Connection &_rc){
	idbg("send file ["<<srcstr<<"] to: "<<dststr<<" ("<<addr<<' '<<port<<' '<<objid<<' '<<objuid<<')');
	Server &rs(Server::the());
	uint32	myprocid(0);
	uint32	fromobjid(_rc.id());
	uint32	fromobjuid(rs.uid(_rc));
	cs::RequestUid reqid(_rc.id(), rs.uid(_rc), _rc.requestId()); 
	StreamPtr<IOStream>	sp;
	int rv = Server::the().fileManager().stream(sp, reqid, srcstr.c_str());
	protocol::Parameter &rp = _rc.writer().push(&Writer::putStatus);
	switch(rv){
		case BAD:
			rp = protocol::Parameter(StrDef(" NO SENDSTRING: unable to open file@"));
			break;
		case NOK:
			rp = protocol::Parameter(StrDef(" NO SENDSTRING: stream wait not implemented yet@"));
			break;
		case OK:{
			AddrInfo ai(addr.c_str(), port, 0, AddrInfo::Inet4, AddrInfo::Stream);
			idbg("addr"<<addr<<"str = "<<srcstr<<" port = "<<port<<" objid = "<<" objuid = "<<objuid);
			if(!ai.empty()){
				rp = protocol::Parameter(StrDef(" OK Done SENDSTRING@"));
				cs::CmdPtr<cs::Command> cmdptr(new SendStreamCommand(sp, dststr, myprocid, objid, objuid, fromobjid, fromobjuid));
				rs.ipc().sendCommand(ai.begin(), cmdptr);
			}else{
				rp = protocol::Parameter(StrDef(" NO SENDSTRING no such address@"));
			}
		}break;
	}
	return NOK;
}
//---------------------------------------------------------------
// Idle command
//---------------------------------------------------------------
Idle::~Idle(){
}
void Idle::initReader(Reader &_rr){
	_rr.push(&Reader::checkChar, protocol::Parameter('e'));
	_rr.push(&Reader::checkChar, protocol::Parameter('n'));
	_rr.push(&Reader::checkChar, protocol::Parameter('o'));
	_rr.push(&Reader::checkChar, protocol::Parameter('d'));
	_rr.push(&Reader::checkChar, protocol::Parameter('\n'));
	_rr.push(&Reader::checkChar, protocol::Parameter('\r'));
}
int Idle::execute(Connection &_rc){
	_rc.writer().push(&Writer::putStatus, protocol::Parameter(StrDef(" OK Done IDLE@")));
	return OK;
}
int Idle::reinitWriter(Writer &_rw, protocol::Parameter &_rp){
	if(_rp.b.i == 1){//prepare
		cassert(typeq.size());
		if(typeq.front() == PeerStringType){
			_rw<<"* RECEIVED STRING ("<<(uint32)conidq.front().tkrid<<' '<<(uint32)conidq.front().procid<<' '<<(uint32)conidq.front().procuid;
			_rw<<") ("<<(uint32)fromq.front().first<<' '<<(uint32)fromq.front().second<<") ";
			_rp.b.i = 0;
			_rw.push(&Writer::flushAll);
			_rw.push(&Writer::putChar, protocol::Parameter('\n'));
			_rw.push(&Writer::putChar, protocol::Parameter('\r'));
			_rw.push(&Writer::putAString, protocol::Parameter((void*)stringq.front().data(), stringq.front().size()));
		}else if(typeq.front() == PeerStreamType){
			_rw<<"* RECEIVED STREAM ("<<(uint32)conidq.front().tkrid<<' '<<(uint32)conidq.front().procid<<' '<<(uint32)conidq.front().procuid;
			_rw<<") ("<<(uint32)fromq.front().first<<' '<<(uint32)fromq.front().second<<") PATH ";
			_rp.b.i = 0;
			_rw.push(&Writer::flushAll);
			_rw.push(&Writer::putChar, protocol::Parameter('\n'));
			_rw.push(&Writer::putChar, protocol::Parameter('\r'));
			_rw.push(&Writer::reinit<Idle>, protocol::Parameter(this, 2));
			_rw.push(&Writer::flushAll);
			_rw.push(&Writer::putAString, protocol::Parameter((void*)stringq.front().data(), stringq.front().size()));
		}else{
			cassert(false);
		}
		return Writer::Continue;
	}else if(_rp.b.i == 2){
		streamq.front()->seek(0);//go to begining
		litsz64 = streamq.front()->size();
		it.reinit(streamq.front().ptr());
		_rw<<" DATA {"<<(uint32)streamq.front()->size()<<"}\r\n";
		_rw.replace(&Writer::putStream, protocol::Parameter(&it, &litsz64));
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
			streamq.pop();
		}else{
			cassert(false);
		}
		if(typeq.size()){
			_rp.b.i = 1;
			return Writer::Continue;
		}
		return Writer::Ok;
	}
}
int Idle::receiveIStream(
	StreamPtr<IStream> &_sp,
	const FileUidTp &,
	int			_which,
	const ObjectUidTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	if(_conid){
		typeq.push(PeerStreamType);
		conidq.push(*_conid);
	}else{
		typeq.push(LocalStreamType);
	}
	idbg("");
	streamq.push(_sp);
	idbg("");
	_sp.release();
	fromq.push(_from);
	rc.writer().push(&Writer::reinit<Idle>, protocol::Parameter(this, 1));
	return OK;
}
int Idle::receiveString(
	const String &_str,
	int			_which,
	const ObjectUidTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	if(typeq.size() && typeq.back() == PeerStreamType){
		stringq.push(_str);
		return OK;
	}
	if(_conid){
		typeq.push(PeerStringType);
		conidq.push(*_conid);
	}else{
		typeq.push(LocalStringType);
	}
	stringq.push(_str);
	fromq.push(_from);
	rc.writer().push(&Writer::reinit<Idle>, protocol::Parameter(this, 1));
	return NOK;
}
//---------------------------------------------------------------
// Command Base
//---------------------------------------------------------------
typedef serialization::TypeMapper					TypeMapper;
typedef serialization::bin::Serializer				BinSerializer;
typedef serialization::bin::Deserializer			BinDeserializer;

Command::Command(){}
void Command::initStatic(Server &_rs){
	TypeMapper::map<SendStringCommand, BinSerializer, BinDeserializer>();
	TypeMapper::map<SendStreamCommand, BinSerializer, BinDeserializer>();
	TypeMapper::map<FetchMasterCommand, BinSerializer, BinDeserializer>();
	TypeMapper::map<FetchSlaveCommand, BinSerializer, BinDeserializer>();
	TypeMapper::map<RemoteListCommand, BinSerializer, BinDeserializer>();
}
/*virtual*/ Command::~Command(){}

int Command::receiveIStream(
	StreamPtr<IStream> &_ps,
	const FileUidTp &,
	int			_which,
	const ObjectUidTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	return BAD;
}
int Command::receiveOStream(
	StreamPtr<OStream> &,
	const FileUidTp &,
	int			_which,
	const ObjectUidTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	return BAD;
}
int Command::receiveIOStream(
	StreamPtr<IOStream> &, 
	const FileUidTp &,
	int			_which,
	const ObjectUidTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	return BAD;
}
int Command::receiveString(
	const String &_str,
	int			_which, 
	const ObjectUidTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	return BAD;
}
int receiveData(
	void *_pdata,
	int _datasz,
	int			_which, 
	const ObjectUidTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	return BAD;
}
int Command::receiveNumber(
	const int64 &_no,
	int			_which,
	const ObjectUidTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	return BAD;
}
int Command::receiveData(
	void *_v,
	int	_vsz,
	int			_which,
	const ObjectUidTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	return BAD;
}
int Command::receiveError(
	int _errid,
	const ObjectUidTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	return BAD;
}

}//namespace alpha
}//namespace test

