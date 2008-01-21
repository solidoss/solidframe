/* Implementation file alphacommands.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "system/debug.h"
#include "system/socketaddress.h"

#include "utility/iostream.h"

#include "algorithm/protocol/namematcher.h"
#include "algorithm/serialization/binary.h"

#include "clientserver/ipc/ipcservice.h"
#include "clientserver/ipc/ipcservice.h"
#include "clientserver/core/filemanager.h"

#include "core/common.h"
#include "core/tstring.h"
#include "core/server.h"
#include "core/command.h"


#include "alphaconnection.h"
#include "alphacommands.h"
#include "alphawriter.h"
#include "alphareader.h"
#include "alphaconnection.h"
#include "alphaprotocolfilters.h"


#define StrDef(x) (void*)x, sizeof(x) - 1

namespace cs=clientserver;

namespace test{
namespace alpha{

struct Cmd{
	enum CmdId{
		LoginCmd,
		LogoutCmd,
		CapabilityCmd,
		ListCmd,
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
Command* Connection::create(const String& _name){
	assert(!pcmd);
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
	if(!is_directory(pth)){
		rp = protocol::Parameter(StrDef(" NO LIST: Not a directory@"));
		return OK;
	}
	it = fs::directory_iterator(pth);
	_rc.writer().push(&Writer::reinit<List>, protocol::Parameter(this, Step));
	if(it != end){
		_rc.writer().push(&Writer::putCrlf);
		_rc.writer().push(&Writer::putAString, protocol::Parameter((void*)it->string().data(), it->string().size()));
		if(is_directory(*it)){
			_rc.writer()<<"* DIR ";
		}else{
			_rc.writer()<<"* FILE "<<(uint32)file_size(*it)<<' ';
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
			_rw<<"* FILE "<<(uint32)file_size(*it)<<' ';
		}
		return Writer::Continue;
	}
	return Writer::Ok;
}
//---------------------------------------------------------------

//---------------------------------------------------------------
struct FetchStreamRequest: test::Command{
	FetchStreamRequest(){}
	int received(const cs::ipc::ConnectorUid &_rconid);
	int execute(cs::Object &);
	template <class S>
	S& operator&(S &_s){
		_s.push(fname, "filename");
		_s.push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		return _s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid");
	}
	typedef std::pair<uint32, uint32> 	ObjUidTp;
	typedef std::pair<uint32, uint32> 	FileUidTp;
	String					fname;
	ObjUidTp				tov;
	ObjUidTp				fromv;
	FileUidTp				fuid;
	cs::ipc::ConnectorUid	conid;
};

struct FetchStreamResponse: test::Command{
	FetchStreamResponse(){}
	~FetchStreamResponse(){
		idbg("");
	}
	int received(const cs::ipc::ConnectorUid &_rconid);
	int execute(test::Connection &);
	int createDeserializationStream(std::pair<OStream *, int64> &_rps, int _id);
	void destroyDeserializationStream(const std::pair<OStream *, int64> &_rps, int _id);
	int createSerializationStream(std::pair<IStream *, int64> &_rps, int _id);
	void destroySerializationStream(const std::pair<IStream *, int64> &_rps, int _id);
	
	template <class S>
	S& operator&(S &_s){
		_s.template pushStreammer<FetchStreamResponse>(this, "FetchStreamResponse::isp");
		_s.push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		return _s;
	}
//data:	
	typedef std::pair<uint32, uint32> 	ObjUidTp;
	typedef std::pair<uint32, uint32> 	FileUidTp;
	ObjUidTp					tov;
	cs::ipc::ConnectorUid		conid;
};
//-------------------------------------------------------------------------------

int FetchStreamRequest::received(const cs::ipc::ConnectorUid &_rconid){
	cs::CmdPtr<cs::Command> pcmd(this);
	conid = _rconid;
	Server::the().signalObject(tov.first, tov.second, pcmd);
	return false;
}

int FetchStreamRequest::execute(cs::Object &){
	return -1;
}

//-------------------------------------------------------------------------------

int FetchStreamResponse::received(const cs::ipc::ConnectorUid &_rconid){
	cs::CmdPtr<cs::Command> pcmd(this);
	conid = _rconid;
	Server::the().signalObject(tov.first, tov.second, pcmd);
	return false;
}

void FetchStreamResponse::destroyDeserializationStream(
	const std::pair<OStream *, int64> &_rps, int _id
){
	idbg("Destroy deserialization <"<<_id<<"> sz "<<_rps.second);
}
int FetchStreamResponse::createDeserializationStream(
	std::pair<OStream *, int64> &_rps, int _id
){
	if(_id) return NOK;
	idbg("Create deserialization <"<<_id<<"> sz "<<_rps.second);
	return OK;
}
void FetchStreamResponse::destroySerializationStream(
	const std::pair<IStream *, int64> &_rps, int _id
){
	idbg("doing nothing as the stream will be destroied when the command will be destroyed");
}
int FetchStreamResponse::createSerializationStream(
	std::pair<IStream *, int64> &_rps, int _id
){
	if(_id) return NOK;
	idbg("Create serialization <"<<_id<<"> sz "<<_rps.second);
	return OK;
}

int FetchStreamResponse::execute(test::Connection &_rcon){
	return OK;
}
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
	if(port == -1) port = 1222;//default ipc port
	if(straddr.empty()){
		_rc.writer().push(&Writer::reinit<Fetch>, protocol::Parameter(this, InitLocal));
	}else{
		_rc.writer().push(&Writer::reinit<Fetch>, protocol::Parameter(this, InitRemote));
	}
	return OK;
}
int Fetch::reinitWriter(Writer &_rw, protocol::Parameter &_rp){
	switch(_rp.b.i){
		case InitLocal:{
			cs::FileManager::RequestUid reqid(rc.id(), Server::the().uid(rc), rc.newRequestId());
			int rv = Server::the().fileManager().stream(sp, reqid, strpth.c_str());
			switch(rv){
				case BAD: 
					*pp = protocol::Parameter(StrDef(" NO FETCH: Unable to open file@"));
					break;
				case OK: 
					_rp.b.i = StepLocal;
					return Writer::Continue;
				case NOK:
					st = NOK;//waiting
					_rp.b.i = StepLocal;
					return Writer::No;
			}
			return Writer::No;
		}
		case StepLocal:
			if(sp){
				litsz64 = sp->size();
				it.reinit(sp.ptr());
				_rw<<"* DATA {"<<(uint32)sp->size()<<"}\r\n";
				_rw.replace(&Writer::putCrlf);
				_rw.push(&Writer::putStream, protocol::Parameter(&it, &litsz64));
				return Writer::Continue;
			}else{
				if(st == NOK) return NOK;//still waiting
				//no more waiting
				*pp = protocol::Parameter(StrDef(" NO FETCH: Unable to open file@"));
			}
			return OK;
		case InitRemote:{
			cs::FileManager::RequestUid reqid(rc.id(), Server::the().uid(rc), rc.newRequestId());
			int rv = Server::the().fileManager().stream(sp, fuid, reqid);
			switch(rv){
				case BAD: 
					*pp = protocol::Parameter(StrDef(" NO FETCH: Unable to open temp file@"));
					break;
				case OK: 
					_rp.b.i = StepOneRemote;
					return Writer::Continue;
				case NOK:
					st = NOK;//waiting
					_rp.b.i = StepOneRemote;
					return Writer::No;
			}
			return Writer::No;
		}
		case StepOneRemote:
			if(sp){
				//we have a temp stream and its id - 
				return Writer::Continue;
			}else{
				if(st == NOK) return NOK;//still waiting
				//no more waiting
				*pp = protocol::Parameter(StrDef(" NO FETCH: Unable to open temp file@"));
			}
			return OK;
	}
	assert(false);
	return BAD;
}
int Fetch::receiveIStream(
	StreamPtr<IStream> &_sptr,
	const FileUidTp &_fuid,
	const FromPairTp&,
	const clientserver::ipc::ConnectorUid *
){
	sp =_sptr;
	fuid = _fuid;
	st = OK;
	return OK;
}

int Fetch::error(int _err){
	//TODO: am I waiting for a command?!
	st = BAD;
	return OK;//for now .. Yes I am!!
}
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
			cs::FileManager::RequestUid reqid(rc.id(), Server::the().uid(rc), rc.requestId());
			int rv = Server::the().fileManager().stream(sp, reqid, strpth.c_str());
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
				it.reinit(sp.ptr());
				rc.writer()<<"* Expecting "<<litsz<<" CHARs\r\n";
				litsz64 = litsz;
				_rr.replace(&Reader::fetchLiteralStream, protocol::Parameter(&it, &litsz64));
				_rr.push(&Reader::flushWriter);
				_rr.push(&Reader::checkChar, protocol::Parameter('\n'));
				_rr.push(&Reader::checkChar, protocol::Parameter('\r'));
				return Reader::Continue;
			}else{
				if(st == NOK) return Reader::No;//still waiting
				return Reader::Ok;
			}
			break;
	}
	assert(false);
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
void Store::receiveOStream(StreamPtr<OStream> &_sp){
	sp = _sp;
}
int Store::error(int _err){
	st = BAD;
	return OK;
}
int Store::reinitWriter(Writer &_rw, protocol::Parameter &_rp){
	return Writer::Bad;
}
//---------------------------------------------------------------
struct SendStringCommand: test::Command{
	SendStringCommand(){}
	SendStringCommand(
		const String &_str,
		uint32 _toobjid,
		uint32 _toobjuid,
		uint32 _fromobjid,
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
	return _rcon.receiveString(str, 0, fromv, &conid);
}

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
	uint32	fromobjid(_rc.id());//the id of the current connection
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
	return OK;
}
//---------------------------------------------------------------
struct SendStreamCommand: test::Command{
	SendStreamCommand(){}
	SendStreamCommand(
		StreamPtr<IOStream> &_iosp,
		const String &_str,
		uint32 _myprocid,
		uint32 _toobjid,
		uint32 _toobjuid,
		uint32 _fromobjid,
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
	int rv = Server::the().fileManager().stream(this->iosp, this->dststr.c_str());
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
	//StreamPtr<IOStream>	iosp(static_cast<IOStream*>(iosp.release()));
	idbg("");
	_rcon.receiveIOStream(iosp, std::pair<uint32, uint32>(0,0), 0, fromv, &conid);
	idbg("");
	}
	return _rcon.receiveString(dststr, 0, fromv, &conid);
}
//-------------------------------------------------------------------------------
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
	cs::FileManager::RequestUid reqid(_rc.id(), rs.uid(_rc), _rc.requestId()); 
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
	return OK;
}
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
		assert(typeq.size());
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
			assert(false);
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
			assert(false);
		}
		if(typeq.size()){
			_rp.b.i = 1;
			return Writer::Continue;
		}
		return Writer::Ok;
	}
}
int Idle::receiveIOStream(
	StreamPtr<IOStream> &_sp,
	const FileUidTp &,
	const FromPairTp&_from,
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
	const FromPairTp&_from,
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
	return OK;
}
//---------------------------------------------------------------

//---------------------------------------------------------------
//---------------------------------------------------------------
Command::Command(){}
void Command::initStatic(Server &_rs){
	_rs.binMapper().map<SendStringCommand>();
	_rs.binMapper().map<SendStreamCommand>();
}
/*virtual*/ Command::~Command(){}

int Command::receiveIStream(
	StreamPtr<IStream> &_ps,
	const FileUidTp &,
	const FromPairTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	return BAD;
}

int Command::receiveOStream(
	StreamPtr<OStream> &,
	const FileUidTp &,
	const FromPairTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	return BAD;
}

int Command::receiveIOStream(
	StreamPtr<IOStream> &, 
	const FileUidTp &,
	const FromPairTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	return BAD;
}

int Command::receiveString(
	const String &_str, 
	const FromPairTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	return BAD;
}

int Command::receiveError(
	int _errid,
	uint32 _reqid,
	const FromPairTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	return BAD;
}

/*virtual*/ int Command::receiveError(int){
	return NOK;
}
}//namespace alpha
}//namespace test

