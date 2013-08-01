// betaserverconnection.cpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
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

#include "frame/ipc/ipcservice.hpp"
#include "frame/requestuid.hpp"

#include "betabuffer.hpp"

#include "core/manager.hpp"
#include "core/messages.hpp"

#include "beta/betaservice.hpp"


#include "serialization/typemapperbase.hpp"

#include "betaserverconnection.hpp"
#include "betaservercommands.hpp"

using namespace solid;

namespace concept{
namespace beta{
namespace server{

/*static*/ void Connection::initStatic(Manager &_rm){
	//Command::initStatic(_rm);
	dynamicRegister();
}

namespace{
struct ReqCmp{
	int operator()(const std::pair<uint32, uint32> &_v1, const uint32 _v2)const{
		if(overflow_safe_less(_v1.first, _v2)){
			return -1;
		}else if(overflow_safe_less(_v2, _v1.first)){
			return 1;
		}else return 0;
	}
};

const BinarySeeker<ReqCmp>	reqbs = BinarySeeker<ReqCmp>();

}//namespace

/*static*/ Connection::DynamicMapperT		Connection::dm;

/*static*/ void Connection::dynamicRegister(){
	//DynamicHandlerT::registerDynamic<SendStreamSignal, Connection>();
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


/*virtual*/ bool Connection::notify(DynamicPointer<frame::Message> &_rmsgptr){
	if(this->state() < 0){
		_rmsgptr.clear();
		return false;//no reason to raise the pool thread!!
	}
	DynamicPointer<>	dp(_rmsgptr);
	dv.push_back(dp);
	return frame::Object::notify(frame::S_SIG | frame::S_RAISE);
}

uint32 Connection::newRequestId(uint32 _pos){
	uint32 rq = reqid;
	++reqid;
	if(reqid == 0) reqid = 1;
	if(reqvec.empty() || rq > reqvec.back().first){
		//push back
		reqvec.push_back(UInt32PairT(rq, _pos));
		
	}else{
		BinarySeekerResultT rv = reqbs(reqvec.begin(), reqvec.end(), rq);
		cassert(!rv.first);
		reqvec.insert(reqvec.begin() + rv.second, UInt32PairT(rq, _pos));
	}
	return rq;
}

bool   Connection::isRequestIdExpected(uint32 _v, uint32 &_rpos){
	idbg(_v<<" "<<_rpos);
	BinarySeekerResultT rv = reqbs(reqvec.begin(), reqvec.end(), _v);
	if(rv.first){
		_rpos = reqvec[rv.second].second;
		reqvec.erase(reqvec.begin() + rv.second);
		return true;
	}else return false;
}

void   Connection::deleteRequestId(uint32 _v){
	idbg(""<<_v);
	if(_v == 0) return;
	BinarySeekerResultT rv = reqbs(reqvec.begin(), reqvec.end(), _v);
	cassert(rv.first);
	reqvec.erase(reqvec.begin() + rv.second);
}

/*
	The main loop with the implementation of the alpha protocol's
	state machine. We dont need a loop, we use the ConnectionSelector's
	loop - returning OK.
	The state machine is a simple switch
*/

int Connection::execute(ulong _sig, TimeSpec &_tout){
	frame::requestuidptr->set(frame::Manager::specific().id(*this));
	
	if(notified()){//we've received a signal
		DynamicHandler<DynamicMapperT>	dh(dm);
		ulong							sm(0);
		{
			Locker<Mutex>	lock(frame::Manager::specific().mutex(*this));
			sm = grabSignalMask(0);//grab all bits of the signal mask
			if(sm & frame::S_KILL) return BAD;
			if(sm & frame::S_SIG){//we have signals
				dh.init(dv.begin(), dv.end());
				dv.clear();
			}
		}
		if(sm & frame::S_SIG){//we've grabed signals, execute them
			for(size_t i = 0; i < dh.size(); ++i){
				dh.handle(*this, i);
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
			if(_sig & (frame::TIMEOUT | frame::ERRDONE)){
				return BAD;
			}
			bool reenter = false;
			if(!socketHasPendingRecv()){
				if(_sig & frame::INDONE){
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
				}else if(_sig & frame::OUTDONE){
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

void Connection::dynamicHandle(solid::DynamicPointer<> &_dp){
	
}

}//namespace server
}//namespace beta
}//namespace concept

