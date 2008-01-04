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
#include "clientserver/core/storagemanager.h"

#include "core/common.h"
#include "core/tstring.h"
#include "core/server.h"
#include "core/command.h"


#include "alphaconnection.h"
#include "alphacommands.h"
#include "alphawriter.h"
#include "alphareader.h"
#include "alphaconnection.h"



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
Fetch::Fetch(Connection &_rc):rc(_rc),st(0),pp(NULL){
}
Fetch::~Fetch(){
	idbg("");
	sp.clear();
}
void Fetch::initReader(Reader &_rr){
	_rr.push(&Reader::fetchAString, protocol::Parameter(&strpth));
	_rr.push(&Reader::checkChar, protocol::Parameter(' '));
}
int Fetch::execute(Connection &_rc){
	idbg("path: "<<strpth);
	protocol::Parameter &rp = _rc.writer().push(&Writer::putStatus);
	pp = &rp;
	rp = protocol::Parameter(StrDef(" OK Done FETCH@"));
	_rc.writer().push(&Writer::reinit<Fetch>, protocol::Parameter(this, Init));
	return OK;
}
int Fetch::reinitWriter(Writer &_rw, protocol::Parameter &_rp){
	switch(_rp.b.i){
		case Init:{
			int rv = Server::the().storage().istream(sp, strpth.c_str(), rc.id(), Server::the().uid(rc));
			switch(rv){
				case BAD: 
					*pp = protocol::Parameter(StrDef(" NO FETCH: Unable to open file@"));
					break;
				case OK: 
					_rp.b.i = Step;
					return Writer::Continue;
				case NOK:
					st = NOK;//waiting
					_rp.b.i = Step;
					return Writer::No;
			}
			return Writer::No;
		}
		case Step:
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
	}
	assert(false);
	return BAD;
}
void Fetch::receiveIStream(StreamPtr<IStream> &_sptr){
	sp = _sptr;
	st = OK;
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
			int rv = Server::the().storage().ostream(sp, strpth.c_str(), rc.id(), Server::the().uid(rc));
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
	if(sp && sp->isOk()){
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
	SendStringCommand():myport(0){}
	SendStringCommand(
		const String &_str,
		uint32 _myport,
		uint32 _toobjid,
		uint32 _toobjuid,
		uint32 _fromobjid,
		uint32 _fromobjuid
	):str(_str), myport(_myport), tov(_toobjid, _toobjuid), fromv(_fromobjid, _fromobjuid){}
	int received(const cs::ipc::ConnectorUid &_rconid);
	std::pair<uint32, uint32> to()const{return tov;}
	std::pair<uint32, uint32> from()const{return fromv;}
	int execute(test::Connection &);
	template <class S>
	S& operator&(S &_s){
		_s.push(str, "string").push(myport, "myport").push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		return _s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid");
	}
private:
	typedef std::pair<uint32, uint32> FromPairTp;
	String						str;
	uint32						myport;
	FromPairTp					tov;
	FromPairTp					fromv;
	cs::ipc::ConnectorUid		conid;
};

int SendStringCommand::received(const cs::ipc::ConnectorUid &_rconid){
	cs::CmdPtr<cs::Command> pcmd(this);
	conid = _rconid;
	Server::the().signalObject(tov.first, tov.second, pcmd);
	return false;
}

int SendStringCommand::execute(test::Connection &_rcon){
	return _rcon.receiveString(str, fromv, &conid);
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
		cs::CmdPtr<cs::Command> cmdptr(new SendStringCommand(str, port, objid, objuid, fromobjid, fromobjuid));
		rs.ipc().sendCommand(ai.begin(), cmdptr);
	}else{
		rp = protocol::Parameter(StrDef(" NO SENDSTRING no such address@"));
	}
	return OK;
}
//---------------------------------------------------------------
struct SendStreamCommand: test::Command{
	SendStreamCommand():myprocid(0){}
	SendStreamCommand(
		StreamPtr<IOStream> &_iosp,
		const String &_str,
		uint32 _myprocid,
		uint32 _toobjid,
		uint32 _toobjuid,
		uint32 _fromobjid,
		uint32 _fromobjuid
	):iosp(_iosp), dststr(_str), myprocid(_myprocid), tov(_toobjid, _toobjuid), fromv(_fromobjid, _fromobjuid){}
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
		_s.push(myprocid, "myprocid").push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		return _s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid");
	}
private:
	typedef std::pair<uint32, uint32> 	FromPairTp;
	StreamPtr<IOStream>			iosp;
	String						dststr;
	uint32						myprocid;
	FromPairTp					tov;
	FromPairTp					fromv;
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
	int rv = Server::the().storage().iostream(this->iosp, this->dststr.c_str());
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
	_rcon.receiveIOStream(iosp, fromv, &conid);
	idbg("");
	}
	return _rcon.receiveString(dststr, fromv, &conid);
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
	StreamPtr<IOStream>	sp;
	int rv = Server::the().storage().iostream(sp, srcstr.c_str()/*, _rc.id(), Server::the().uid(_rc)*/);
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
	const FromPairTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	return BAD;
}

int Command::receiveOStream(
	StreamPtr<OStream> &,
	const FromPairTp&_from,
	const cs::ipc::ConnectorUid *_conid
){
	return BAD;
}

int Command::receiveIOStream(
	StreamPtr<IOStream> &, 
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
/*virtual*/ int Command::receiveError(int){
	return NOK;
}
}//namespace alpha
}//namespace test

