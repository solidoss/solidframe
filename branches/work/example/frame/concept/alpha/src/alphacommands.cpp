/* Implementation file alphacommands.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "system/debug.hpp"
#include "system/socketaddress.hpp"
#include "system/filedevice.hpp"
#include "system/thread.hpp"
#include "system/timespec.hpp"

#include "utility/iostream.hpp"

#include "protocol/text/namematcher.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/ipc/ipcservice.hpp"
#include "frame/file/filemanager.hpp"
#include "frame/file/filekeys.hpp"
#include "frame/messagesteward.hpp"
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

using namespace solid;

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
	{"sendstream",Cmd::SendStreamCmd},
	{"sendstring",Cmd::SendStringCmd},
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
	_rc.writer().push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done NOOP@")));
	return OK;
}
int Basic::execLogout(Connection &_rc){
	_rc.writer().push(&Writer::returnValue<true>, protocol::text::Parameter(Writer::Bad));
	_rc.writer().push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done LOGOUT@")));
	_rc.writer().push(&Writer::putAtom, protocol::text::Parameter(StrDef("* Alpha connection closing\r\n")));
	return NOK;
}
int Basic::execCapability(Connection &_rc){
	_rc.writer().push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done CAPABILITY@")));
	_rc.writer().push(&Writer::putAtom, protocol::text::Parameter(StrDef("* CAPABILITIES noop logout login\r\n")));
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
	_rr.push(&Reader::manage, protocol::text::Parameter(Reader::ResetLogging));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&pass));
	_rr.push(&Reader::manage, protocol::text::Parameter(Reader::ClearLogging));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&user));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
}
int Login::execute(Connection &_rc){
	_rc.writer().push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done LOGIN@")));
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
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&strpth));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
}
int List::execute(Connection &_rc){
	idbg("path: "<<strpth);
	fs::path pth(strpth.c_str()/*, fs::native*/);
	protocol::text::Parameter &rp = _rc.writer().push(&Writer::putStatus);
	rp = protocol::text::Parameter(StrDef(" OK Done LIST@"));
	if(!exists( pth ) || !is_directory(pth)){
		rp = protocol::text::Parameter(StrDef(" NO LIST: Not a directory@"));
		return OK;
	}
	try{
	it = fs::directory_iterator(pth);
	}catch ( const std::exception & ex ){
		idbg("dir_iterator exception :"<<ex.what());
		return OK;
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
	return OK;
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
	return Writer::Ok;
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
int RemoteList::reinitReader<OK>(Reader &_rr, protocol::text::Parameter &){
	typedef CharFilter<' '>				SpaceFilterT;
	typedef NotFilter<SpaceFilterT> 	NotSpaceFilterT;
	
	hostvec.push_back(HostAddr());
	
	_rr.push(&Reader::returnValue<true>, protocol::text::Parameter(Reader::Ok));
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

int RemoteList::execute(Connection &_rc){
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
			DynamicPointer<frame::Message> msgptr(sig_sp);
			Manager::the().ipc().sendMessage(msgptr, rd.begin(), it->netid/*, frame::ipc::Service::SameConnectorFlag*/);
			_rc.writer().push(&Writer::reinit<RemoteList>, protocol::text::Parameter(this));
		}else{
			*pp = protocol::text::Parameter(StrDef(" NO REMOTELIST: no such peer address@"));
		}
	}
	
	return OK;
}
int RemoteList::reinitWriter(Writer &_rw, protocol::text::Parameter &_rp){
	switch(state){
		case Wait:
			return Writer::No;
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
			return Writer::Ok;
		case SendError:
			*pp = protocol::text::Parameter(StrDef(" NO LIST: Not a directory@"));
			return Writer::Ok;
	};
	cassert(false);
	return BAD;
}
int RemoteList::receiveData(
	void *_pdata,
	int _datasz,
	int	_which, 
	const ObjectUidT&_from,
	const solid::frame::ipc::ConnectionUid *_conid
){
	ppthlst = reinterpret_cast<PathListT*>(_pdata);
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
	const ObjectUidT&_from,
	const frame::ipc::ConnectionUid *_conid
){
	state = SendError;
	return OK;
}
//---------------------------------------------------------------
// Fetch command
//---------------------------------------------------------------
Fetch::Fetch(Connection &_rc):rc(_rc), state(0), litsz(-1){
}
Fetch::~Fetch(){
	idbg(""<<(void*)this<<' '<<(void*)sp_in.get());
	sp_in.clear();
	sp_out.clear();
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
int Fetch::execute(Connection &_rc){
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
	return OK;
}

int Fetch::doInitLocal(){
	idbg(""<<(void*)this);
	//try to open stream to localfile
	frame::RequestUid reqid(rc.id(), Manager::the().id(rc).second, rc.newRequestId());
	int rv = Manager::the().fileManager().stream(sp_out, reqid, strpth.c_str());
	switch(rv){
		case BAD: 
			*pp = protocol::text::Parameter(StrDef(" NO FETCH: Unable to open file@"));
			return Writer::Ok;
		case OK: 
			state = SendLocal;
			streamsz_out = sp_out->size();
			litsz = streamsz_out;
			return Writer::Continue;
		case NOK:
			state = WaitLocalStream;
			return Writer::No;
	}
	cassert(false);
	return BAD;
}

int Fetch::doGetTempStream(uint32 _sz){
	idbg(""<<(void*)this<<" "<<_sz);
	frame::file::MemoryKey	tk(_sz);
	frame::RequestUid		reqid(rc.id(), Manager::the().id(rc).second, rc.newRequestId());
	streamsz_in = _sz;
	FileUidT	fuid;
	int rv = Manager::the().fileManager().stream(sp_out, fuid, reqid, tk);
	switch(rv){
		case BAD: 
			*pp = protocol::text::Parameter(StrDef(" NO FETCH: Unable to open temp file@"));
			return Writer::No;
		case OK: 
			cassert(false);
			return Writer::Continue;
		case NOK:
			state = WaitTempStream;
			return Writer::No;
	}
	return Writer::Bad;
}

void Fetch::doSendMaster(const FileUidT &_fuid){
	idbg(""<<(void*)this);
	ResolveData rd = synchronous_resolve(straddr.c_str(), port.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream);
	idbg("addr"<<straddr<<" port = "<<port);
	if(!rd.empty()){
		//send the master remote command
		FetchMasterMessage *pmsg(new FetchMasterMessage);
		//TODO: add a convenient init method to fetchmastercommand
		pmsg->fname = strpth;
		pmsg->requid = rc.newRequestId();
		pmsg->fromv.first = rc.id();
		pmsg->fromv.second = Manager::the().id(rc).second;
		pmsg->tmpfuid = _fuid;
		pmsg->streamsz = streamsz_in;
		state = WaitRemoteStream;
		DynamicPointer<frame::Message> msgptr(pmsg);
		Manager::the().ipc().sendMessage(msgptr, rd.begin());
	}else{
		*pp = protocol::text::Parameter(StrDef(" NO FETCH: no such peer address@"));
		state = ReturnOk;
	}
}

void Fetch::doSendSlave(const FileUidT &_fuid){
	idbg(""<<(void*)this<<' '<<_fuid.first<<' '<<_fuid.second);
	FetchSlaveMessage *pmsg(new FetchSlaveMessage);
	pmsg->fromv.first = rc.id();
	pmsg->fromv.second = Manager::the().id(rc).second;
	pmsg->requid = rc.newRequestId();
	pmsg->msguid = mastermsguid;
	pmsg->fuid = _fuid;
	pmsg->streamsz = streamsz_in;
	DynamicPointer<frame::Message> msgptr(pmsg);
	Manager::the().ipc().sendMessage(msgptr, ipcconuid);
//	idbg("rv = "<<rv);
// 	if(rv == BAD){
// 		*pp = protocol::text::Parameter(StrDef(" NO FETCH: peer died@"));
// 		state = ReturnBad;
// 	}
}

int Fetch::doSendFirstData(Writer &_rw){
	idbg(""<<(void*)this);
	streamsz_out = streamsz_in;
	idbg(""<<streamsz_out);
	uint64 remainsz(litsz - streamsz_in);
	if(remainsz){
		uint32 tmpsz =  512 * 1024;
		if(remainsz < tmpsz) tmpsz = remainsz;
		if(doGetTempStream(tmpsz) == Writer::Bad){
			*pp = protocol::text::Parameter(StrDef(" NO FETCH: no temp stream@"));
			return Writer::Ok;
		}
	}else{
		state = ReturnCrlf;
	}
	sp_out = sp_in;
	return doSendLiteral(_rw, false);
}

int Fetch::doSendNextData(Writer &_rw){
	idbg(""<<(void*)this);
	uint64 remainsz(litsz - streamsz_in);
	streamsz_out = streamsz_in;
	if(remainsz){
		uint32 tmpsz = 2 * 512 * 1024;
		if(remainsz < tmpsz) tmpsz = remainsz;
		if(doGetTempStream(tmpsz) == Writer::Bad){
			*pp = protocol::text::Parameter(StrDef(" NO FETCH: no temp stream@"));
			return Writer::Ok;
		}
	}else{
		state = ReturnCrlf;
	}
	litsz -= streamsz_out;
	sp_out = sp_in;
	cassert(sp_out);
	it.reinit(sp_out.get());
	//_rw.push(&Writer::putCrlf);
	_rw.push(&Writer::putStream, protocol::text::Parameter(&it, &streamsz_out));
	return Writer::Continue;
}

int Fetch::doSendLiteral(Writer &_rw, bool _local){
	idbg("send literal "<<litsz<<" "<<streamsz_out);
	//send local stream
	cassert(sp_out);
	it.reinit(sp_out.get());
	_rw<<"* DATA {"<<litsz<<"}\r\n";
	litsz -= streamsz_out;
	if(_local){
		_rw.replace(&Writer::putCrlf);
	}
	_rw.push(&Writer::putStream, protocol::text::Parameter(&it, &streamsz_out));
	return Writer::Continue;
}

int Fetch::reinitWriter(Writer &_rw, protocol::text::Parameter &_rp){
	switch(state){
		case InitLocal:
			return doInitLocal();
		case SendLocal:
			return doSendLiteral(_rw, true);
		case InitRemote:
			return doGetTempStream(512 * 1024);
		case SendFirstData:
			return doSendFirstData(_rw);
		case SendNextData:
			return doSendNextData(_rw);
		case WaitLocalStream:
		case WaitTempStream:
		case WaitRemoteStream:
			return Writer::No;
		case ReturnBad:
			return Writer::Bad;
		case ReturnOk:
			return Writer::Ok;
		case ReturnCrlf:
			_rw.replace(&Writer::putCrlf);
			return Writer::Continue;
		case SendError:
			*pp = protocol::text::Parameter(StrDef(" NO FETCH: an error occured@"));
			return Writer::Ok;
		case SendTempError:
			*pp = protocol::text::Parameter(StrDef(" NO FETCH: no temp stream@"));
			return Writer::Ok;
		case SendRemoteError:
			*pp = protocol::text::Parameter(StrDef(" NO FETCH: no remote stream@"));
			return Writer::Ok;
	}
	cassert(false);
	return BAD;
}

int Fetch::receiveInputStream(
	StreamPointer<InputStream> &_sptr,
	const FileUidT &_fuid,
	int			_which,
	const ObjectUidT&,
	const solid::frame::ipc::ConnectionUid *
){
	//sp_out =_sptr;
	//fuid = _fuid;
	if(state == WaitLocalStream){
		sp_out = _sptr;
		streamsz_out = sp_out->size();
		litsz = streamsz_out;
		state = SendLocal;
	}else if(state == WaitTempStream){
		sp_in = _sptr;
		state = WaitRemoteStream;
		if(litsz == -1){
			doSendMaster(_fuid);
		}else{
			doSendSlave(_fuid);
		}
	}
	return OK;
}

int Fetch::receiveNumber(
	const int64 &_no,
	int			_which,
	const ObjectUidT& _objuid,
	const solid::frame::ipc::ConnectionUid *_pconuid
){
	mastermsguid = _objuid;
	cassert(_pconuid);
	ipcconuid = *_pconuid;
	if(litsz != -1){//continued
		streamsz_in = sp_in->size();
		idbg(""<<litsz<<" "<<streamsz_in);
		state = SendNextData;
	}else{
		litsz = _no;
		streamsz_in = sp_in->size();
		idbg(""<<litsz<<" "<<streamsz_in);
		state = SendFirstData;
	}
	return OK;
}

int Fetch::receiveError(
	int _errid,
	const ObjectUidT&_from,
	const solid::frame::ipc::ConnectionUid *
){
	switch(state){
		case WaitLocalStream:
			state = SendError;
			break;
		case WaitTempStream:
			state = SendTempError;
			break;
		case WaitRemoteStream:
			if(litsz == -1){
				//we can send an error
				state = SendRemoteError;
			}else{
				//we're already in a literal - force close connection
				state = ReturnBad;
			}
		default:
			state = ReturnBad;
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
	_rr.push(&Reader::reinit<Store>, protocol::text::Parameter(this, Init));
	_rr.push(&Reader::checkChar, protocol::text::Parameter('}'));
	_rr.push(&Reader::fetchUInt32, protocol::text::Parameter(&litsz));
	_rr.push(&Reader::checkChar, protocol::text::Parameter('{'));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&strpth));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
}
int Store::reinitReader(Reader &_rr, protocol::text::Parameter &_rp){
	switch(_rp.b.i){
		case Init:{
			frame::RequestUid reqid(rc.id(), Manager::the().id(rc).second, rc.newRequestId());
			int rv = Manager::the().fileManager().stream(sp, reqid, strpth.c_str(), frame::file::Manager::Create);
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
				it.reinit(sp.get());
				rc.writer()<<"* Expecting "<<litsz<<" CHARs\r\n";
				litsz64 = litsz;
				_rr.replace(&Reader::fetchLiteralStream, protocol::text::Parameter(&it, &litsz64));
				_rr.push(&Reader::flushWriter);
				_rr.push(&Reader::checkChar, protocol::text::Parameter('\n'));
				_rr.push(&Reader::checkChar, protocol::text::Parameter('\r'));
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
	protocol::text::Parameter &rp = _rc.writer().push(&Writer::putStatus);
	if(sp && sp->ok()){
		rp = protocol::text::Parameter(StrDef(" OK Done STORE@"));
	}else{
		rp = protocol::text::Parameter(StrDef(" NO STORE: Failed opening file@"));
	}
	return OK;
}

int Store::receiveOutputStream(
	StreamPointer<OutputStream> &_sptr,
	const FileUidT &_fuid,
	int			_which,
	const ObjectUidT&,
	const solid::frame::ipc::ConnectionUid *
){
	idbg("received stream");
	sp = _sptr;
	st = OK;
	return OK;
}

int Store::receiveError(
	int _errid,
	const ObjectUidT&_from,
	const solid::frame::ipc::ConnectionUid *
){
	idbg("received error");
	st = BAD;
	return OK;
}

int Store::reinitWriter(Writer &_rw, protocol::text::Parameter &_rp){
	return Writer::Bad;
}

//---------------------------------------------------------------
// SendString command
//---------------------------------------------------------------
SendString::SendString():port(0), objid(0), objuid(0){}
SendString::~SendString(){
}
void SendString::initReader(Reader &_rr){
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&str));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchUInt32, protocol::text::Parameter(&objuid));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchUInt32, protocol::text::Parameter(&objid));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchUInt32, protocol::text::Parameter(&port));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&addr));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
}
int SendString::execute(alpha::Connection &_rc){
	Manager &rm(Manager::the());
	ulong	fromobjid(_rc.id());//the id of the current connection
	uint32	fromobjuid(rm.id(_rc).second);//the uid of the current connection
	ResolveData rd = synchronous_resolve(addr.c_str(), port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	idbg("addr"<<addr<<"str = "<<str<<" port = "<<port<<" objid = "<<" objuid = "<<objuid);
	protocol::text::Parameter &rp = _rc.writer().push(&Writer::putStatus);
	if(!rd.empty()){
		rp = protocol::text::Parameter(StrDef(" OK Done SENDSTRING@"));
		DynamicPointer<frame::Message> msgptr(new SendStringMessage(str, objid, objuid, fromobjid, fromobjuid));
		rm.ipc().sendMessage(msgptr, rd.begin());
	}else{
		rp = protocol::text::Parameter(StrDef(" NO SENDSTRING no such address@"));
	}
	return NOK;
}
//---------------------------------------------------------------
// SendStream command
//---------------------------------------------------------------
SendStream::SendStream():port(0), objid(0), objuid(0){}
SendStream::~SendStream(){
}
void SendStream::initReader(Reader &_rr){
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&dststr));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&srcstr));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchUInt32, protocol::text::Parameter(&objuid));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchUInt32, protocol::text::Parameter(&objid));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchUInt32, protocol::text::Parameter(&port));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rr.push(&Reader::fetchAString, protocol::text::Parameter(&addr));
	_rr.push(&Reader::checkChar, protocol::text::Parameter(' '));
}
int SendStream::execute(Connection &_rc){
	idbg("send file ["<<srcstr<<"] to: "<<dststr<<" ("<<addr<<' '<<port<<' '<<objid<<' '<<objuid<<')');
	Manager &rm(Manager::the());
	uint32	myprocid(0);
	uint32	fromobjid(_rc.id());
	uint32	fromobjuid(rm.id(_rc).second);
	frame::RequestUid reqid(_rc.id(), rm.id(_rc).second, _rc.requestId()); 
	StreamPointer<InputOutputStream>	sp;
	int rv = Manager::the().fileManager().stream(sp, reqid, srcstr.c_str());
	protocol::text::Parameter &rp = _rc.writer().push(&Writer::putStatus);
	switch(rv){
		case BAD:
			rp = protocol::text::Parameter(StrDef(" NO SENDSTRING: unable to open file@"));
			break;
		case NOK:
			rp = protocol::text::Parameter(StrDef(" NO SENDSTRING: stream wait not implemented yet@"));
			break;
		case OK:{
			ResolveData rd = synchronous_resolve(addr.c_str(), port, 0, SocketInfo::Inet4, SocketInfo::Stream);
			idbg("addr"<<addr<<"str = "<<srcstr<<" port = "<<port<<" objid = "<<" objuid = "<<objuid);
			if(!rd.empty()){
				rp = protocol::text::Parameter(StrDef(" OK Done SENDSTRING@"));
				DynamicPointer<frame::Message> msgptr(new SendStreamMessage(sp, dststr, myprocid, objid, objuid, fromobjid, fromobjuid));
				rm.ipc().sendMessage(msgptr, rd.begin());
			}else{
				rp = protocol::text::Parameter(StrDef(" NO SENDSTRING no such address@"));
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
	_rr.push(&Reader::checkChar, protocol::text::Parameter('e'));
	_rr.push(&Reader::checkChar, protocol::text::Parameter('n'));
	_rr.push(&Reader::checkChar, protocol::text::Parameter('o'));
	_rr.push(&Reader::checkChar, protocol::text::Parameter('d'));
	_rr.push(&Reader::checkChar, protocol::text::Parameter('\n'));
	_rr.push(&Reader::checkChar, protocol::text::Parameter('\r'));
}
int Idle::execute(Connection &_rc){
	_rc.writer().push(&Writer::putStatus, protocol::text::Parameter(StrDef(" OK Done IDLE@")));
	return OK;
}
int Idle::reinitWriter(Writer &_rw, protocol::text::Parameter &_rp){
	if(_rp.b.i == 1){//prepare
		cassert(typeq.size());
		if(typeq.front() == PeerStringType){
			_rw<<"* RECEIVED STRING ("<<(uint32)conidq.front().tid<<' '<<(uint32)conidq.front().idx<<' '<<(uint32)conidq.front().uid;
			_rw<<") ("<<(uint32)fromq.front().first<<' '<<(uint32)fromq.front().second<<") ";
			_rp.b.i = 0;
			_rw.push(&Writer::flushAll);
			_rw.push(&Writer::putChar, protocol::text::Parameter('\n'));
			_rw.push(&Writer::putChar, protocol::text::Parameter('\r'));
			_rw.push(&Writer::putAString, protocol::text::Parameter((void*)stringq.front().data(), stringq.front().size()));
		}else if(typeq.front() == PeerStreamType){
			_rw<<"* RECEIVED STREAM ("<<(uint32)conidq.front().tid<<' '<<(uint32)conidq.front().idx<<' '<<(uint32)conidq.front().uid;
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
		streamq.front()->seek(0);//go to begining
		litsz64 = streamq.front()->size();
		it.reinit(streamq.front().get());
		_rw<<" DATA {"<<(uint32)streamq.front()->size()<<"}\r\n";
		_rw.replace(&Writer::putStream, protocol::text::Parameter(&it, &litsz64));
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
int Idle::receiveInputStream(
	StreamPointer<InputStream> &_sp,
	const FileUidT &,
	int			_which,
	const ObjectUidT&_from,
	const frame::ipc::ConnectionUid *_conid
){
	if(_conid){
		typeq.push(PeerStreamType);
		conidq.push(*_conid);
	}else{
		typeq.push(LocalStreamType);
	}
	streamq.push(_sp);
	_sp.release();
	fromq.push(_from);
	rc.writer().push(&Writer::reinit<Idle>, protocol::text::Parameter(this, 1));
	return OK;
}
int Idle::receiveString(
	const String &_str,
	int			_which,
	const ObjectUidT&_from,
	const frame::ipc::ConnectionUid *_conid
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
	rc.writer().push(&Writer::reinit<Idle>, protocol::text::Parameter(this, 1));
	return NOK;
}
//---------------------------------------------------------------
// Command Base
//---------------------------------------------------------------
Command::Command(){}
void Command::initStatic(Manager &_rm){
	MessageTypeIds ids;
	Manager::the().ipc().typeMapper().insert<SendStringMessage>();
	Manager::the().ipc().typeMapper().insert<SendStreamMessage>();
	ids.fetchmastercommand =  Manager::the().ipc().typeMapper().insert<FetchMasterMessage>();
	ids.fetchslavecommand = Manager::the().ipc().typeMapper().insert<FetchSlaveMessage>();
	ids.remotelistcommand = Manager::the().ipc().typeMapper().insert<RemoteListMessage>();
	ids.remotelistresponse = Manager::the().ipc().typeMapper().insert<RemoteListMessage, NumberType<1> >();
	idbg("ids.fetchmastercommand = "<<ids.fetchmastercommand);
	idbg("ids.fetchslavecommand = "<<ids.fetchslavecommand);
	idbg("ids.remotelistcommand = "<<ids.remotelistcommand);
	idbg("ids.remotelistresponse = "<<ids.remotelistresponse);
	MessageTypeIds::the(&ids);
}
/*virtual*/ Command::~Command(){}

int Command::receiveInputStream(
	StreamPointer<InputStream> &_ps,
	const FileUidT &,
	int			_which,
	const ObjectUidT&_from,
	const frame::ipc::ConnectionUid *_conid
){
	return BAD;
}
int Command::receiveOutputStream(
	StreamPointer<OutputStream> &,
	const FileUidT &,
	int			_which,
	const ObjectUidT&_from,
	const frame::ipc::ConnectionUid *_conid
){
	return BAD;
}
int Command::receiveInputOutputStream(
	StreamPointer<InputOutputStream> &, 
	const FileUidT &,
	int			_which,
	const ObjectUidT&_from,
	const frame::ipc::ConnectionUid *_conid
){
	return BAD;
}
int Command::receiveString(
	const String &_str,
	int			_which, 
	const ObjectUidT&_from,
	const frame::ipc::ConnectionUid *_conid
){
	return BAD;
}
int receiveData(
	void *_pdata,
	int _datasz,
	int			_which, 
	const ObjectUidT&_from,
	const solid::frame::ipc::ConnectionUid *_conid
){
	return BAD;
}
int Command::receiveNumber(
	const int64 &_no,
	int			_which,
	const ObjectUidT&_from,
	const frame::ipc::ConnectionUid *_conid
){
	return BAD;
}
int Command::receiveData(
	void *_v,
	int	_vsz,
	int			_which,
	const ObjectUidT&_from,
	const frame::ipc::ConnectionUid *_conid
){
	return BAD;
}
int Command::receiveError(
	int _errid,
	const ObjectUidT&_from,
	const solid::frame::ipc::ConnectionUid *_conid
){
	return BAD;
}

}//namespace alpha
}//namespace concept

