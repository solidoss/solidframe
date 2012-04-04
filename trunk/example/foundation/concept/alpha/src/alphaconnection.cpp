/* Implementation file alphaconnection.cpp
	
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
//static const char	*hellostr = "Welcome to alpha service!!!\r\n"; 
//static const char *sigstr = "Signaled!!!\r\n";

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
namespace{
static const DynamicRegisterer<Connection>	dre;
}
/*static*/ void Connection::dynamicRegister(){
	DynamicExecuterT::registerDynamic<InputStreamSignal, Connection>();
	DynamicExecuterT::registerDynamic<OutputStreamSignal, Connection>();
	DynamicExecuterT::registerDynamic<InputOutputStreamSignal, Connection>();
	DynamicExecuterT::registerDynamic<StreamErrorSignal, Connection>();
	DynamicExecuterT::registerDynamic<RemoteListSignal, Connection>();
	DynamicExecuterT::registerDynamic<FetchSlaveSignal, Connection>();
	DynamicExecuterT::registerDynamic<SendStringSignal, Connection>();
	DynamicExecuterT::registerDynamic<SendStreamSignal, Connection>();
}

Connection::Connection(
	SocketAddressInfo &_rai
):	wtr(&logger), rdr(wtr, &logger),
	pcmd(NULL), ai(_rai), reqid(1){
	aiit = ai.begin();
	state(Connect);
}

Connection::Connection(
	const SocketDevice &_rsd
):	BaseT(_rsd), wtr(&logger),rdr(wtr, &logger),
	pcmd(NULL), reqid(1){
	
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
}


/*virtual*/ bool Connection::signal(DynamicPointer<foundation::Signal> &_sig){
	if(this->state() < 0){
		_sig.clear();
		return false;//no reason to raise the pool thread!!
	}
	dr.push(this, DynamicPointer<>(_sig));
	return Object::signal(fdt::S_SIG | fdt::S_RAISE);
}


/*
	The main loop with the implementation of the alpha protocol's
	state machine. We dont need a loop, we use the ConnectionSelector's
	loop - returning OK.
	The state machine is a simple switch
*/

int Connection::execute(ulong _sig, TimeSpec &_tout){
	//concept::Manager &rm = concept::Manager::the();
	fdt::requestuidptr->set(this->uid());
	//_tout.add(2400);
	if(_sig & (fdt::TIMEOUT | fdt::ERRDONE)){
		if(state() == ConnectWait){
			state(Connect);
		}else
			if(_sig & fdt::TIMEOUT){
				edbg("timeout occured - destroy connection "<<state());
			}else{
				edbg("error occured - destroy connection "<<state());
			}
			return BAD;
	}
	
	if(signaled()){//we've received a signal
		ulong sm(0);
		{
			Locker<Mutex>	lock(this->mutex());
			sm = grabSignalMask(0);//grab all bits of the signal mask
			if(sm & fdt::S_KILL) return BAD;
			if(sm & fdt::S_SIG){//we have signals
				dr.prepareExecute(this);
			}
		}
		if(sm & fdt::S_SIG){//we've grabed signals, execute them
			while(dr.hasCurrent(this)){
				dr.executeCurrent(this);
				dr.next(this);
			}
		}
		//now we determine if we return with NOK or we continue
		if(!_sig) return NOK;
	}
	const uint32 sevs = socketEventsGrab();
	if(sevs & fdt::ERRDONE){
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
			wtr.buffer(protocol::SpecificBuffer(2*1024));
			rdr.buffer(protocol::SpecificBuffer(2*1024));
			if(this->socketIsSecure()){
				int rv = this->socketSecureAccept();
				state(Banner);
				return rv;
			}else{
				state(Banner);
			}
		case Banner:{
			concept::Manager	&rm = concept::Manager::the();
			uint32				myport(rm.ipc().basePort());
			IndexT				objid(this->id());
			IndexT				svcid(fdt::Manager::the().computeServiceId(objid));
			IndexT				objidx(fdt::Manager::the().computeIndex(objid));
			uint32				objuid(this->uid().second);
			char				host[SocketAddress::HostNameCapacity];
			char				port[SocketAddress::ServiceNameCapacity];
			SocketAddress		addr;
			
			
			
			writer()<<"* Hello from alpha server ("<<myport<<' '<<svcid<<' '<<objidx<<' '<< objid<<' '<<objuid<<") [";
			socketLocalAddress(addr);
			addr.name(
				host,
				SocketAddress::HostNameCapacity,
				port,
				SocketAddress::ServiceNameCapacity,
				SocketAddress::NumericService | SocketAddress::NumericHost
			);
			writer()<<host<<':'<<port<<" -> ";
			socketRemoteAddress(addr);
			addr.name(
				host,
				SocketAddress::HostNameCapacity,
				port,
				SocketAddress::ServiceNameCapacity,
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
					if(socketHasPendingSend()){
						socketTimeoutSend(3000);
					}else if(socketHasPendingRecv()){
						socketTimeoutRecv(3000);
					}else{
						_tout.add(2000);
					}
					state(ParseTout);
					return NOK;
				case BAD:
					edbg("");
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
					if(socketHasPendingSend()){
						socketTimeoutSend(3000);
						state(ExecuteIOTout);
					}/*else if(socketHasPendingRecv()){
						socketTimeoutRecv(3000);
						state(ExecuteIOTout);
					}*/else{
						_tout.add(2000);
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
					edbg("rc = "<<rc);
					return rc;
			}
			break;
		case IdleExecute:
			//idbg("IdleExecute");
			if(sevs & fdt::OUTDONE){
				state(Execute);
				return OK;
			}return NOK;
		case Connect:
			switch(socketConnect(aiit)){
				case BAD:
					if(++aiit){
						state(Connect);
						return OK;
					}
					return BAD;
				case OK:
					idbg("");
					state(Init);
					//idbg("");
					return NOK;//for register
				case NOK:
					state(ConnectWait);
					idbg("");
					return NOK;
			}
			break;
		case ConnectWait:
			state(Init);
			//delete(paddr); paddr = NULL;
			break;
		case ParseTout:
			if(sevs & fdt::INDONE){
				state(Parse);
				return OK;
			}
			return NOK;
		case ExecuteIOTout:
			idbg("State: ExecuteTout ");
			if(sevs & fdt::OUTDONE){
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

void Connection::dynamicExecute(DynamicPointer<> &_dp){
	wdbg("Received unknown signal on ipcservice");
}

void Connection::dynamicExecute(DynamicPointer<RemoteListSignal> &_psig){
	idbg("");
	if(_psig->requid && _psig->requid != reqid){
		idbg("RemoteListSignal signal rejected");
		return;
	}
	idbg("");
	newRequestId();//prevent multiple responses with the same id
	if(pcmd){
		int rv;
		if(!_psig->err){
			rv = pcmd->receiveData((void*)_psig->ppthlst, -1, 0, ObjectUidT(), &_psig->conid);
			_psig->ppthlst = NULL;
		}else{
			rv = pcmd->receiveError(_psig->err, ObjectUidT(), &_psig->conid);
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
void Connection::dynamicExecute(DynamicPointer<FetchSlaveSignal> &_psig){
	idbg("");
	if(_psig->requid && _psig->requid != reqid) return;
	idbg("");
	newRequestId();//prevent multiple responses with the same id
	if(pcmd){
		int rv;
		if(_psig->filesz >= 0){
			idbg("");
			rv = pcmd->receiveNumber(_psig->filesz, 0, _psig->siguid, &_psig->conid);
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
void Connection::dynamicExecute(DynamicPointer<SendStringSignal> &_psig){
}
void Connection::dynamicExecute(DynamicPointer<SendStreamSignal> &_psig){
}
void Connection::dynamicExecute(DynamicPointer<InputStreamSignal> &_psig){
	idbg("");
	if(_psig->requid.first && _psig->requid.first != reqid) return;
	idbg("");
	newRequestId();//prevent multiple responses with the same id
	if(pcmd){
		switch(pcmd->receiveInputStream(_psig->sptr, _psig->fileuid, 0, ObjectUidT(), NULL)){
			case BAD:
				idbg("");
				break;
			case OK:
				idbg("");
				if(state() == ParseTout){
					state(Parse);
				}
				if(state() == ExecuteTout){
					idbg("");
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
void Connection::dynamicExecute(DynamicPointer<OutputStreamSignal> &_psig){
	idbg("");
	if(_psig->requid.first && _psig->requid.first != reqid) return;
	idbg("");
	newRequestId();//prevent multiple responses with the same id
	if(pcmd){
		switch(pcmd->receiveOutputStream(_psig->sptr, _psig->fileuid, 0, ObjectUidT(), NULL)){
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
void Connection::dynamicExecute(DynamicPointer<InputOutputStreamSignal> &_psig){
	idbg("");
	if(_psig->requid.first && _psig->requid.first != reqid) return;
	idbg("");
	newRequestId();//prevent multiple responses with the same id
	if(pcmd){
		switch(pcmd->receiveInputOutputStream(_psig->sptr, _psig->fileuid, 0, ObjectUidT(), NULL)){
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
void Connection::dynamicExecute(DynamicPointer<StreamErrorSignal> &_psig){
	idbg("");
	if(_psig->requid.first && _psig->requid.first != reqid) return;
	idbg("");
	newRequestId();//prevent multiple responses with the same id
	if(pcmd){
		switch(pcmd->receiveError(_psig->errid, ObjectUidT(), NULL)){
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
