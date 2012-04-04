/* Implementation file betaserverconnection.cpp
	
	Copyright 2012 Valentin Palade 
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
#include "system/socketdevice.hpp"
#include "system/specific.hpp"
#include "system/exception.hpp"

#include "utility/ostream.hpp"
#include "utility/istream.hpp"
#include "utility/iostream.hpp"
#include "utility/binaryseeker.hpp"

#include "foundation/ipc/ipcservice.hpp"
#include "foundation/requestuid.hpp"

#include "betabuffer.hpp"

#include "core/manager.hpp"
#include "core/signals.hpp"

#include "beta/betaservice.hpp"


#include "algorithm/serialization/typemapperbase.hpp"

#include "betaserverconnection.hpp"
#include "betaservercommands.hpp"

namespace fdt=foundation;

namespace concept{
namespace beta{
namespace server{

/*static*/ void Connection::initStatic(Manager &_rm){
	//Command::initStatic(_rm);
}

namespace{
static const DynamicRegisterer<Connection>	dre;
struct ReqCmp{
	int operator()(const std::pair<uint32, uint32> &_v1, const uint32 _v2)const{
		if(overflow_safe_less(_v1.first, _v2)){
			return -1;
		}else if(overflow_safe_less(_v2, _v1.first)){
			return 1;
		}else return 0;
	}
};
static const BinarySeeker<ReqCmp>	reqbs;
}//namespace

/*static*/ void Connection::dynamicRegister(){
	//DynamicExecuterT::registerDynamic<SendStreamSignal, Connection>();
}

Connection::Connection(
	const SocketDevice &_rsd
):	BaseT(_rsd), reqid(1){
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
	idbg("destroy connection id "<<this->id());
	for(CommandVectorT::const_iterator it(cmdvec.begin()); it != cmdvec.end(); ++it){
		delete it->pcmd;
	}
}


/*virtual*/ bool Connection::signal(DynamicPointer<foundation::Signal> &_sig){
	if(this->state() < 0){
		_sig.clear();
		return false;//no reason to raise the pool thread!!
	}
	de.push(this, DynamicPointer<>(_sig));
	return Object::signal(fdt::S_SIG | fdt::S_RAISE);
}

uint32 Connection::newRequestId(uint32 _pos){
	uint32 rq = reqid;
	++reqid;
	if(reqid == 0) reqid = 1;
	if(reqvec.empty() || rq > reqvec.back().first){
		//push back
		reqvec.push_back(UInt32PairT(rq, _pos));
		
	}else{
		int rv = reqbs(reqvec.begin(), reqvec.end(), rq);
		cassert(rv < 0);
		rv = -rv - 1;
		reqvec.insert(reqvec.begin() + rv, UInt32PairT(rq, _pos));
	}
	return rq;
}

bool   Connection::isRequestIdExpected(uint32 _v, uint32 &_rpos){
	idbg(_v<<" "<<_rpos);
	int rv = reqbs(reqvec.begin(), reqvec.end(), _v);
	if(rv >= 0){
		_rpos = reqvec[rv].second;
		reqvec.erase(reqvec.begin() + rv);
		return true;
	}else return false;
}

void   Connection::deleteRequestId(uint32 _v){
	idbg(""<<_v);
	if(_v == 0) return;
	int rv = reqbs(reqvec.begin(), reqvec.end(), _v);
	cassert(rv >= 0);
	//rv = -rv - 1;
	reqvec.erase(reqvec.begin() + rv);
}

/*
	The main loop with the implementation of the alpha protocol's
	state machine. We dont need a loop, we use the ConnectionSelector's
	loop - returning OK.
	The state machine is a simple switch
*/

int Connection::execute(ulong _sig, TimeSpec &_tout){
	fdt::requestuidptr->set(this->uid());
	
	if(signaled()){//we've received a signal
		ulong sm(0);
		{
			Locker<Mutex>	lock(this->mutex());
			sm = grabSignalMask(0);//grab all bits of the signal mask
			if(sm & fdt::S_KILL) return BAD;
			if(sm & fdt::S_SIG){//we have signals
				de.prepareExecute(this);
			}
		}
		if(sm & fdt::S_SIG){//we've grabed signals, execute them
			while(de.hasCurrent(this)){
				de.executeCurrent(this);
				de.next(this);
			}
		}
		//now we determine if we return with NOK or we continue
		if(!_sig) return NOK;
	}
	//const uint32 sevs = socketEventsGrab();
	switch(state()){
		case Init:
			if(this->socketIsSecure()){
				doPrepareRun();
				return this->socketSecureAccept();
			}else{
				doPrepareRun();
			}
		case Run:{
			idbg("");
			if(_sig & (fdt::TIMEOUT | fdt::ERRDONE)){
				return BAD;
			}
			bool reenter = false;
			if(!socketHasPendingRecv()){
				if(_sig & fdt::INDONE){
					const int rv = doParseBuffer(socketRecvCount());
					if(rv == BAD && exception != 0){
						state(SendException);
						return OK;
					}
				}
				switch(socketRecv(recvbufwr, recvbufend - recvbufwr)){
					case BAD: return BAD;
					case OK:
						doParseBuffer(socketRecvCount());
						reenter = true;
						break;
					case NOK:
						break;
				}
			}
			if(!socketHasPendingSend()){
				const int sz = doFillSendBuffer(useCompression(), useEncryption());
				if(sz > 0){
					switch(socketSend(sendbufbeg, sz)){
						case BAD: return BAD;
						case OK:
							reenter = true;
							break;
						case NOK:
							break;
					}
				}else if(sz < 0){
					if(exception != 0){
						state(SendException);
						return OK;
					}
					return BAD;
				}else if(_sig & fdt::OUTDONE){
					doReleaseSendBuffer();
				}
			}
			
			if(reenter) return OK;
		}break;
		case SendException:
			if(!socketHasPendingSend()){
				const int sz = doFillSendException();
				switch(socketSend(sendbufbeg, sz)){
					case BAD:
					case OK:
						return BAD;
					case NOK:
						state(SendExceptionDone);
						break;
				}
			}break;
		case SendExceptionDone:
			if(!socketHasPendingSend()){
				return BAD;
			}
			break;
	}
	return NOK;
}


void Connection::doPrepareRun(){
	state(Run);
	BaseT::doPrepareRun();
}

int Connection::doFillSendBufferData(char* _sendbufpos){
	int		writesize = 0;
	char	*sendbufpos = _sendbufpos;
	while(true){
		if(!ser.empty()){
			int rv = ser.run(
				sendbufpos,
				sendbufend - sendbufpos
			);
			if(rv < 0){
				idbg("Serialization error: "<<ser.errorString());
				exception = ExceptionFill;
				return BAD;
			}
			sendbufpos += rv;
			writesize += rv;
			if((sendbufend - sendbufpos) < minsendsize){
				break;
			}
			cassert(ser.empty());
			if(!doCommandExecuteSend(crtcmdsendidx)){
				return BAD;
			}
		}
		
		cassert(ser.empty());
		
		if(cmdque.empty()){
			break;
		}
		
		const uint32	cmdidx = cmdque.front();
		CommandStub		&rcmdstub(cmdvec[cmdidx]);
		
		cmdque.pop();
		
		rcmdstub.pcmd->prepareSerialization(ser);
		
		cassert(!ser.empty());
		
		crtcmdsendidx = cmdidx;
		ser.push(crtcmdsendidx, "tag");
	}
	return writesize;
}

bool Connection::doParseBufferData(const char *_pbuf, ulong _len){
	do{
		if(des.empty()){
			des.pushReinit<Connection, 0>(this, 0);
			des.push(crtcmdrecvidx, "command_tag");
		}
		int rv = des.run(_pbuf, _len);
		if(rv < 0){
			return false;
		}
		
		if(des.empty()){
			doCommandExecuteRecv(crtcmdrecvidx);
		}
		
		_len  -= rv;
		_pbuf += rv;
	}while(_len != 0);
	return true;
}

void Connection::doCommandExecuteRecv(uint32 _cmdidx){
	CommandStub	&rcmdstub(cmdvec[_cmdidx]);
	const int	rv = rcmdstub.pcmd->executeRecv(_cmdidx);
	switch(rv){
		case NOK:
			break;
		case CONTINUE:
			cmdque.push(_cmdidx);
			break;
		default:
			THROW_EXCEPTION_EX("Unknown command executeStart return value ", rv);
			break;
	}
}


bool Connection::doCommandExecuteSend(uint32 _cmdidx){
	CommandStub	&rcmdstub(cmdvec[_cmdidx]);
	const int	rv = rcmdstub.pcmd->executeSend(_cmdidx);
	switch(rv){
		case BAD:
			delete rcmdstub.pcmd;
			rcmdstub.pcmd = NULL;
			return false;
		case OK:
			delete rcmdstub.pcmd;
			rcmdstub.pcmd = NULL;
			break;
		case NOK:
			break;
		case CONTINUE:
			cmdque.push(_cmdidx);
			break;
		default:
			THROW_EXCEPTION_EX("Unknown command execute return value ", rv);
			break;
	}
	return true;
}

bool Connection::useEncryption()const{
	return false;
}
bool Connection::useCompression()const{
	return false;
}

template <>
int Connection::serializationReinit<Connection::DeserializerT, 0>(
	Connection::DeserializerT &_rdes, const uint64 &_rv
){
	if(crtcmdrecvidx < cmdvec.size()){
		if(!cmdvec[crtcmdrecvidx].pcmd){
		}else{
			return BAD;
		}
	}else if(crtcmdrecvidx == cmdvec.size()){
		cmdvec.push_back(CommandStub());
	}else return BAD;
	
	CommandStub	&rcmdstub(cmdvec[crtcmdrecvidx]);
	
	_rdes.pop();
	if(!rcmdstub.pcmd){
		des.pushReinit<Connection, 1>(this, 0);
		des.push(crtcmdrecvtype, "command_type");
	}else{
		rcmdstub.pcmd->prepareDeserialization(_rdes);
	}
	return CONTINUE;
}

template <>
int Connection::serializationReinit<Connection::DeserializerT, 1>(
	Connection::DeserializerT &_rdes, const uint64 &_rv
){
	CommandStub	&rcmdstub(cmdvec[crtcmdrecvidx]);
	
	_rdes.pop();
	switch(crtcmdrecvtype){
		case request::Login::Identifier:{
			rcmdstub.pcmd = new command::Login;
		}break;
		case request::Noop::Identifier:{
			rcmdstub.pcmd = new command::Noop;
		}break;
		case request::Cancel::Identifier:{
			rcmdstub.pcmd = new command::Cancel;
		}break;
		case request::Test::Identifier:{
			rcmdstub.pcmd = new command::Test;
		}break;
		default:
			return BAD;
	}
	rcmdstub.pcmd->prepareDeserialization(_rdes);
	return CONTINUE;
}

int Connection::execute(){
	return BAD;
}

int Connection::accept(fdt::Visitor &_rov){
	//static_cast<TestInspector&>(_roi).inspectConnection(*this);
	return OK;
}
}//namespace server
}//namespace beta
}//namespace concept

