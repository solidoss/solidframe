/* Implementation file alphaconnection.cpp
	
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
#include "system/timespec.hpp"
#include "system/mutex.hpp"
#include "system/socketaddress.hpp"

#include "utility/ostream.hpp"
#include "utility/istream.hpp"
#include "utility/iostream.hpp"

#include "foundation/ipc/ipcservice.hpp"
#include "foundation/requestuid.hpp"


#include "core/manager.hpp"
#include "core/signals.hpp"

#include "alpha/alphaservice.hpp"

#include "alphaconnection.hpp"
#include "alphacommand.hpp"
#include "alphasignals.hpp"
#include "alphaprotocolfilters.hpp"
#include "audit/log.hpp"

namespace fdt=foundation;
static const char	*hellostr = "Welcome to alpha service!!!\r\n"; 
static const char *sigstr = "Signaled!!!\r\n";

namespace concept{
namespace alpha{

void Logger::doInFlush(const char *_pb, unsigned _bl){
	if(Log::instance().isSet(Log::any, Log::Input)){
		Log::instance().record(Log::Input, Log::any, 0, __FILE__, __FUNCTION__, __LINE__).write(_pb, _bl);
		Log::instance().done();
	}
}

void Logger::doOutFlush(const char *_pb, unsigned _bl){
	if(Log::instance().isSet(Log::any, Log::Output)){
		Log::instance().record(Log::Output, Log::any, 0, __FILE__, __FUNCTION__, __LINE__).write(_pb, _bl);
		Log::instance().done();
	}
}

/*static*/ void Connection::initStatic(Manager &_rm){
	Command::initStatic(_rm);
}
/*static*/ void Connection::dynamicRegister(DynamicMap &_rdm){
	BaseTp::dynamicRegister<IStreamSignal>(_rdm);
	BaseTp::dynamicRegister<OStreamSignal>(_rdm);
	BaseTp::dynamicRegister<IOStreamSignal>(_rdm);
	BaseTp::dynamicRegister<StreamErrorSignal>(_rdm);
	BaseTp::dynamicRegister<RemoteListSignal>(_rdm);
	BaseTp::dynamicRegister<FetchSlaveSignal>(_rdm);
	BaseTp::dynamicRegister<SendStringSignal>(_rdm);
	BaseTp::dynamicRegister<SendStreamSignal>(_rdm);
}
Connection::Connection(SocketAddress *_paddr):
						 	//BaseTp(_pch),
						 	wtr(*this, &logger),
						 	rdr(*this, wtr, &logger), pcmd(NULL),
						 	paddr(_paddr),
						 	reqid(1){
	
	cassert(paddr);
	state(Connect);
	
	
}

Connection::Connection(const SocketDevice &_rsd):
						 	BaseTp(_rsd),
						 	wtr(*this, &logger),
						 	rdr(*this, wtr, &logger), pcmd(NULL),
						 	paddr(NULL),
						 	reqid(1){
	
	state(Init);
}


/*
NOTE: 
* Releasing the connection here and not in a base class destructor because
here we know the exact type of the object - so the service can do other things 
based on the type.
* Also it ensures a proper/safe visiting. Suppose the unregister would have taken 
place in a base destructor. If the visited object is a leaf, one may visit
destroyed data.
NOTE: 
* Visitable data must be destroyed after releasing the connection!!!
*/

Connection::~Connection(){
	idbg("destroy connection id "<<this->id()<<" pcmd "<<pcmd);
	delete pcmd; pcmd = NULL;
	concept::Manager &rm = concept::Manager::the();
	rm.service(*this).removeConnection(*this);
}

/*
	The main loop with the implementation of the alpha protocol's
	state machine. We dont need a loop, we use the ConnectionSelector's
	loop - returning OK.
	The state machine is a simple switch
*/

int Connection::execute(ulong _sig, TimeSpec &_tout){
	concept::Manager &rm = concept::Manager::the();
	fdt::requestuidptr->set(this->id(), rm.uid(*this));
	//_tout.add(2400);
	if(_sig & (fdt::TIMEOUT | fdt::ERRDONE)){
		if(state() == ConnectTout){
			state(Connect);
			return fdt::UNREGISTER;
		}else
			idbg("timeout occured - destroy connection "<<state());
			return BAD;
	}
	
	if(signaled()){//we've received a signal
		ulong sm(0);
		{
			Mutex::Locker	lock(rm.mutex(*this));
			sm = grabSignalMask(0);//grab all bits of the signal mask
			if(sm & fdt::S_KILL) return BAD;
			if(sm & fdt::S_SIG){//we have signals
				grabSignals();//grab them
			}
		}
		if(sm & fdt::S_SIG){//we've grabed signals, execute them
			switch(this->execSignals(*this)){
				case BAD: 
					return BAD;
				case OK: //expected signal received
					_sig |= fdt::OKDONE;
				case NOK://unexpected signal received
					break;
			}
		}
		//now we determine if we return with NOK or we continue
		if(!_sig) return NOK;
	}
	if(socketEvents() & fdt::ERRDONE){
		return BAD;
	}
// 	if(socketEvents() & fdt::OUTDONE){
// 		switch(state()){
// 			case IdleExecute:
// 			case ExecuteTout:
// 				state(Execute);
// 				break;
// 			case Execute:
// 				break;
// 			case Connect:
// 				state(Init);
// 				break;
// 			default:
// 				state(Parse);
// 		}
// 	}
	int rc;
	switch(state()){
		case Init:
			if(this->socketIsSecure()){
				int rv = this->socketSecureAccept();
				state(Banner);
				return rv;
			}else{
				state(Banner);
			}
		case Banner:{
			concept::Manager	&rm = concept::Manager::the();
			uint32			myport(rm.ipc().basePort());
			ulong			objid(this->id());
			uint32			objuid(rm.uid(*this));
			char			host[SocketAddress::MaxSockHostSz];
			char			port[SocketAddress::MaxSockServSz];
			SocketAddress	addr;
			writer()<<"* Hello from alpha server ("<<myport<<" "<<(uint32)objid<<" "<<objuid<<") [";
			socketLocalAddress(addr);
			addr.name(
				host,
				SocketAddress::MaxSockHostSz,
				port,
				SocketAddress::MaxSockServSz,
				SocketAddress::NumericService | SocketAddress::NumericHost
			);
			writer()<<host<<':'<<port<<" -> ";
			socketRemoteAddress(addr);
			addr.name(
				host,
				SocketAddress::MaxSockHostSz,
				port,
				SocketAddress::MaxSockServSz,
				SocketAddress::NumericService | SocketAddress::NumericHost
			);
			writer()<<host<<':'<<port<<"]"<<'\r'<<'\n';
			writer().push(&Writer::flushAll);
			state(Execute);
			}break;
		case ParsePrepare:
			idbg("PrepareReader");
			prepareReader();
		case Parse:
			//idbg("Parse");
			state(Parse);
			switch((rc = reader().run())){
				case OK: break;
				case NOK:
					if(hasPendingRequests()){
						socketTimeout(_tout, 3000);
					}else{
						_tout.add(2000);
					}
					state(ParseTout);
					return NOK;
				case BAD:
					return BAD;
				case YIELD:
					return OK;
			}
			if(reader().isError()){
				delete pcmd; pcmd = NULL;
				logger.inFlush();
				state(Execute);
				writer().push(Writer::putStatus);
				break;
			}
		case ExecutePrepare:
			logger.inFlush();
			idbg("PrepareExecute");
			pcmd->execute(*this);
			state(Execute);
		case Execute:
			//idbg("Execute");
			switch((rc = writer().run())){
				case NOK:
					if(hasPendingRequests()){
						socketTimeout(_tout, 3000);
						state(ExecuteIOTout);
					}else{
						_tout.add(2000);
						idbg("no pendin io - wait twenty seconds");
						state(ExecuteTout);
					}
					return NOK;
				case OK:
					if(state() != IdleExecute){
						delete pcmd; pcmd = NULL;
						state(ParsePrepare); rc = OK;
					}else{
						state(Parse); rc = OK;
					}
				case YIELD:
					return OK;
				default:
					idbg("rc = "<<rc);
					return rc;
			}
			break;
		case IdleExecute:
			//idbg("IdleExecute");
			if(socketEvents() & fdt::OUTDONE){
				state(Execute);
				return OK;
			}return NOK;
		case Connect:
			/*
			switch(channel().connect(*paddr)){
				case BAD: return BAD;
				case OK:  state(Init);break;
				case NOK: state(ConnectTout); return REGISTER;
			};*/
			break;
		case ConnectTout:
			state(Init);
			//delete(paddr); paddr = NULL;
			break;
		case ParseTout:
			if(socketEvents() & fdt::INDONE){
				state(Parse);
				return OK;
			}
			return NOK;
		case ExecuteIOTout:
			idbg("State: ExecuteTout");
			if(socketEvents() & fdt::OUTDONE){
				state(Execute);
				return OK;
			}
		case ExecuteTout:
			return NOK;
	}
	return OK;
}

int Connection::execute(){
	return BAD;
}
//prepare the reader and the writer for a new command
void Connection::prepareReader(){
	writer().clear();
	reader().clear();
	//logger.outFlush();
	reader().push(&Reader::checkChar, protocol::Parameter('\n'));
	reader().push(&Reader::checkChar, protocol::Parameter('\r'));
	reader().push(&Reader::fetchKey<Reader, Connection, AtomFilter, Command>, protocol::Parameter(this, 64));
	reader().push(&Reader::checkChar, protocol::Parameter(' '));
	reader().push(&Reader::fetchFilteredString<TagFilter>, protocol::Parameter(&writer().tag(),64));
}

void Connection::dynamicReceive(DynamicPointer<RemoteListSignal> &_psig){
	idbg("");
	if(_psig->requid && _psig->requid != reqid) return;
	idbg("");
	newRequestId();//prevent multiple responses with the same id
	if(pcmd){
		int rv;
		if(!_psig->err){
			rv = pcmd->receiveData((void*)_psig->ppthlst, -1, 0, ObjectUidTp(), &_psig->conid);
			_psig->ppthlst = NULL;
		}else{
			rv = pcmd->receiveError(_psig->err, ObjectUidTp(), &_psig->conid);
		}
		switch(rv){
			case BAD:
				idbg("");
				break;
			case OK:
				idbg("");
				if(state() == ParseTout){
					state(Parse);
				}
				if(state() == ExecuteTout){
					state(Execute);
				}
				break;
			case NOK:
				idbg("");
				state(IdleExecute);
				break;
		}
	}
}
void Connection::dynamicReceive(DynamicPointer<FetchSlaveSignal> &_psig){
	idbg("");
	if(_psig->requid && _psig->requid != reqid) return;
	idbg("");
	newRequestId();//prevent multiple responses with the same id
	if(pcmd){
		int rv;
		if(_psig->sz >= 0){
			idbg("");
			rv = pcmd->receiveNumber(_psig->insz, 0, _psig->siguid, &_psig->conid);
		}else{
			idbg("");
			rv = pcmd->receiveError(-1, _psig->siguid, &_psig->conid);
		}
		switch(rv){
			case BAD:
				idbg("");
				break;
			case OK:
				idbg("");
				if(state() == ParseTout){
					state(Parse);
				}
				if(state() == ExecuteTout){
					state(Execute);
				}
				break;
			case NOK:
				idbg("");
				state(IdleExecute);
				break;
		}
	}
}
void Connection::dynamicReceive(DynamicPointer<SendStringSignal> &_psig){
}
void Connection::dynamicReceive(DynamicPointer<SendStreamSignal> &_psig){
}
void Connection::dynamicReceive(DynamicPointer<IStreamSignal> &_psig){
	idbg("");
	if(_psig->requid.first && _psig->requid.first != reqid) return;
	idbg("");
	newRequestId();//prevent multiple responses with the same id
	if(pcmd){
		switch(pcmd->receiveIStream(_psig->sptr, _psig->fileuid, 0, ObjectUidTp(), NULL)){
			case BAD:
				idbg("");
				break;
			case OK:
				idbg("");
				if(state() == ParseTout){
					state(Parse);
				}
				if(state() == ExecuteTout){
					state(Execute);
				}
				break;
			case NOK:
				idbg("");
				state(IdleExecute);
				break;
		}
	}
}
void Connection::dynamicReceive(DynamicPointer<OStreamSignal> &_psig){
	idbg("");
	if(_psig->requid.first && _psig->requid.first != reqid) return;
	idbg("");
	newRequestId();//prevent multiple responses with the same id
	if(pcmd){
		switch(pcmd->receiveOStream(_psig->sptr, _psig->fileuid, 0, ObjectUidTp(), NULL)){
			case BAD:
				idbg("");
				break;
			case OK:
				idbg("");
				if(state() == ParseTout){
					state(Parse);
				}
				if(state() == ExecuteTout){
					state(Execute);
				}
				break;
			case NOK:
				idbg("");
				state(IdleExecute);
				break;
		}
	}
}
void Connection::dynamicReceive(DynamicPointer<IOStreamSignal> &_psig){
	idbg("");
	if(_psig->requid.first && _psig->requid.first != reqid) return;
	idbg("");
	newRequestId();//prevent multiple responses with the same id
	if(pcmd){
		switch(pcmd->receiveIOStream(_psig->sptr, _psig->fileuid, 0, ObjectUidTp(), NULL)){
			case BAD:
				idbg("");
				break;
			case OK:
				idbg("");
				if(state() == ParseTout){
					state(Parse);
				}
				if(state() == ExecuteTout){
					state(Execute);
				}
				break;
			case NOK:
				idbg("");
				state(IdleExecute);
				break;
		}
	}
}
void Connection::dynamicReceive(DynamicPointer<StreamErrorSignal> &_psig){
	idbg("");
	if(_psig->requid.first && _psig->requid.first != reqid) return;
	idbg("");
	newRequestId();//prevent multiple responses with the same id
	if(pcmd){
		switch(pcmd->receiveError(_psig->errid, ObjectUidTp(), NULL)){
			case BAD:
				idbg("");
				break;
			case OK:
				idbg("");
				if(state() == ParseTout){
					state(Parse);
				}
				if(state() == ExecuteTout){
					state(Execute);
				}
				break;
			case NOK:
				idbg("");
				state(IdleExecute);
				break;
		}
	}
}


int Connection::accept(fdt::Visitor &_rov){
	//static_cast<TestInspector&>(_roi).inspectConnection(*this);
	return OK;
}

}//namespace alpha
}//namespace concept
