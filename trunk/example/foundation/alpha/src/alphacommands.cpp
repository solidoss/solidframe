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
#include "system/thread.hpp"
#include "system/timespec.hpp"

#include "utility/iostream.hpp"

#include "algorithm/protocol/namematcher.hpp"
#include "algorithm/serialization/binary.hpp"

#include "foundation/ipc/ipcservice.hpp"
#include "foundation/ipc/ipcservice.hpp"
#include "foundation/filemanager.hpp"
#include "foundation/signalexecuter.hpp"
#include "foundation/requestuid.hpp"

#include "core/common.hpp"
#include "core/tstring.hpp"
#include "core/manager.hpp"
#include "core/signals.hpp"


#include "alphaconnection.hpp"
#include "alphacommands.hpp"
#include "alphasignals.hpp"
#include "alphawriter.hpp"
#include "alphareader.hpp"
#include "alphaconnection.hpp"
#include "alphaprotocolfilters.hpp"


#define StrDef(x) (void*)x, sizeof(x) - 1

namespace fdt=foundation;

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
static const protocol::NameMatcher cmdm(cmds);
//---------------------------------------------------------------
/*
	The creator method called by fdt::Reader::fetchKey when the 
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
	_rc.writer().push(&Writer::putStatus, protocol::Parameter(StrDef(" OK Done NOOP@")));
	return OK;
}
int Basic::execLogout(Connection &_rc){
	_rc.writer().push(&Writer::returnValue<true>, protocol::Parameter(Writer::Bad));
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
		RemoteListSignal *psig(new RemoteListSignal(0/*1000 + (int) (10000.0 * (rand() / (RAND_MAX + 1.0)))*/));
		idbg("remotelist with "<<psig->tout<<" miliseconds delay");
		psig->strpth = strpth;
		psig->requid = _rc.newRequestId();
		psig->fromv.first = _rc.id();
		psig->fromv.second = Manager::the().uid(_rc);
		state = Wait;
		DynamicPointer<fdt::Signal> sigptr(psig);
		Manager::the().ipc().sendSignal(ai.begin(), sigptr, fdt::ipc::Service::SameConnectorFlag);
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
	const foundation::ipc::ConnectorUid *_conid
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
	const fdt::ipc::ConnectorUid *_conid
){
	idbg("");
	state = SendError;
	return OK;
}
//---------------------------------------------------------------
// Fetch command
//---------------------------------------------------------------
Fetch::Fetch(Connection &_rc):port(-1), rc(_rc), st(0), pp(NULL){
}
Fetch::~Fetch(){
	idbg(""<<(void*)this<<' '<<(void*)sp.ptr());
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
			fdt::RequestUid reqid(rc.id(), Manager::the().uid(rc), rc.newRequestId());
			int rv = Manager::the().fileManager().stream(sp, reqid, strpth.c_str());
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
			fdt::RequestUid reqid(rc.id(), Manager::the().uid(rc), rc.newRequestId());
			int rv = Manager::the().fileManager().stream(sp, fuid, reqid, NULL, 0);
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
			idbg("send master remote "<<(void*)this);
			cassert(sp);
			AddrInfo ai(straddr.c_str(), port, 0, AddrInfo::Inet4, AddrInfo::Stream);
			idbg("addr"<<straddr<<" port = "<<port);
			if(!ai.empty()){
				//send the master remote command
				FetchMasterSignal *psig(new FetchMasterSignal);
				//TODO: add a convenient init method to fetchmastercommand
				psig->fname = strpth;
				psig->requid = rc.newRequestId();
				psig->fromv.first = rc.id();
				psig->fromv.second = Manager::the().uid(rc);
				psig->tmpfuid = fuid;
				st = WaitFirstRemote;
				DynamicPointer<fdt::Signal> sigptr(psig);
				Manager::the().ipc().sendSignal(ai.begin(), sigptr);
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
				FetchSlaveSignal *psig(new FetchSlaveSignal);
				psig->requid = rc.newRequestId();
				psig->siguid = mastersiguid;
				psig->fuid = fuid;
				DynamicPointer<fdt::Signal> sigptr(psig);
				if(Manager::the().ipc().sendSignal(conuid, sigptr) == BAD){
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
				FetchSlaveSignal *psig(new FetchSlaveSignal);
				psig->fromv.first = rc.id();
				psig->fromv.second = Manager::the().uid(rc);
				psig->requid = rc.newRequestId();
				psig->siguid = mastersiguid;
				psig->fuid = fuid;
				psig->insz = isfrst ? 0 : -1;
				DynamicPointer<fdt::Signal> sigptr(psig);
				if(Manager::the().ipc().sendSignal(conuid, sigptr) == BAD){
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
	StreamPointer<IStream> &_sptr,
	const FileUidTp &_fuid,
	int			_which,
	const ObjectUidTp&,
	const foundation::ipc::ConnectorUid *
){
	idbg(""<<(void*)this);
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
	const foundation::ipc::ConnectorUid *_pconuid
){
	idbg("");
	mastersiguid = _objuid;
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
	const foundation::ipc::ConnectorUid *
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
			st = SendError;
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
			fdt::RequestUid reqid(rc.id(), Manager::the().uid(rc), rc.newRequestId());
			int rv = Manager::the().fileManager().stream(sp, reqid, strpth.c_str(), fdt::FileManager::Create);
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
	StreamPointer<OStream> &_sptr,
	const FileUidTp &_fuid,
	int			_which,
	const ObjectUidTp&,
	const foundation::ipc::ConnectorUid *
){
	idbg("received stream");
	sp = _sptr;
	st = OK;
	return OK;
}

int Store::receiveError(
	int _errid,
	const ObjectUidTp&_from,
	const foundation::ipc::ConnectorUid *
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
	Manager &rm(Manager::the());
	ulong	fromobjid(_rc.id());//the id of the current connection
	uint32	fromobjuid(rm.uid(_rc));//the uid of the current connection
	AddrInfo ai(addr.c_str(), port, 0, AddrInfo::Inet4, AddrInfo::Stream);
	idbg("addr"<<addr<<"str = "<<str<<" port = "<<port<<" objid = "<<" objuid = "<<objuid);
	protocol::Parameter &rp = _rc.writer().push(&Writer::putStatus);
	if(!ai.empty()){
		rp = protocol::Parameter(StrDef(" OK Done SENDSTRING@"));
		DynamicPointer<fdt::Signal> sigptr(new SendStringSignal(str, objid, objuid, fromobjid, fromobjuid));
		rm.ipc().sendSignal(ai.begin(), sigptr);
	}else{
		rp = protocol::Parameter(StrDef(" NO SENDSTRING no such address@"));
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
	Manager &rm(Manager::the());
	uint32	myprocid(0);
	uint32	fromobjid(_rc.id());
	uint32	fromobjuid(rm.uid(_rc));
	fdt::RequestUid reqid(_rc.id(), rm.uid(_rc), _rc.requestId()); 
	StreamPointer<IOStream>	sp;
	int rv = Manager::the().fileManager().stream(sp, reqid, srcstr.c_str());
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
				DynamicPointer<fdt::Signal> sigptr(new SendStreamSignal(sp, dststr, myprocid, objid, objuid, fromobjid, fromobjuid));
				rm.ipc().sendSignal(ai.begin(), sigptr);
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
	StreamPointer<IStream> &_sp,
	const FileUidTp &,
	int			_which,
	const ObjectUidTp&_from,
	const fdt::ipc::ConnectorUid *_conid
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
	const fdt::ipc::ConnectorUid *_conid
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
void Command::initStatic(Manager &_rm){
	TypeMapper::map<SendStringSignal, BinSerializer, BinDeserializer>();
	TypeMapper::map<SendStreamSignal, BinSerializer, BinDeserializer>();
	TypeMapper::map<FetchMasterSignal, BinSerializer, BinDeserializer>();
	TypeMapper::map<FetchSlaveSignal, BinSerializer, BinDeserializer>();
	TypeMapper::map<RemoteListSignal, BinSerializer, BinDeserializer>();
}
/*virtual*/ Command::~Command(){}

int Command::receiveIStream(
	StreamPointer<IStream> &_ps,
	const FileUidTp &,
	int			_which,
	const ObjectUidTp&_from,
	const fdt::ipc::ConnectorUid *_conid
){
	return BAD;
}
int Command::receiveOStream(
	StreamPointer<OStream> &,
	const FileUidTp &,
	int			_which,
	const ObjectUidTp&_from,
	const fdt::ipc::ConnectorUid *_conid
){
	return BAD;
}
int Command::receiveIOStream(
	StreamPointer<IOStream> &, 
	const FileUidTp &,
	int			_which,
	const ObjectUidTp&_from,
	const fdt::ipc::ConnectorUid *_conid
){
	return BAD;
}
int Command::receiveString(
	const String &_str,
	int			_which, 
	const ObjectUidTp&_from,
	const fdt::ipc::ConnectorUid *_conid
){
	return BAD;
}
int receiveData(
	void *_pdata,
	int _datasz,
	int			_which, 
	const ObjectUidTp&_from,
	const foundation::ipc::ConnectorUid *_conid
){
	return BAD;
}
int Command::receiveNumber(
	const int64 &_no,
	int			_which,
	const ObjectUidTp&_from,
	const fdt::ipc::ConnectorUid *_conid
){
	return BAD;
}
int Command::receiveData(
	void *_v,
	int	_vsz,
	int			_which,
	const ObjectUidTp&_from,
	const fdt::ipc::ConnectorUid *_conid
){
	return BAD;
}
int Command::receiveError(
	int _errid,
	const ObjectUidTp&_from,
	const foundation::ipc::ConnectorUid *_conid
){
	return BAD;
}

}//namespace alpha
}//namespace concept

