// consensus/src/consensusrequest.cpp
//
// Copyright (c) 2011, 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "consensus/consensusrequest.hpp"
#include "utility/sharedmutex.hpp"

#include "frame/ipc/ipcservice.hpp"

namespace solid{
namespace consensus{

bool RequestId::operator<(const RequestId &_rcsi)const{
	if(this->sockaddr < _rcsi.sockaddr){
		return true;
	}else if(_rcsi.sockaddr < this->sockaddr){
		return false;
	}else if(this->senderuid < _rcsi.senderuid){
		return true;
	}else if(_rcsi.senderuid > this->senderuid){
		return false;
	}else return overflow_safe_less(this->requid, _rcsi.requid);
}

bool RequestId::operator==(const RequestId &_rcsi)const{
	return this->sockaddr == _rcsi.sockaddr && 
		this->senderuid == _rcsi.senderuid &&
		this->requid == _rcsi.requid;
}

size_t RequestId::hash()const{
	return sockaddr.hash() ^ this->senderuid.first ^ this->requid;
}

bool RequestId::senderEqual(const RequestId &_rcsi)const{
	return this->sockaddr == _rcsi.sockaddr && 
		this->senderuid == _rcsi.senderuid;
}

bool RequestId::senderLess(const RequestId &_rcsi)const{
	if(this->sockaddr < _rcsi.sockaddr){
		return true;
	}else if(_rcsi.sockaddr < this->sockaddr){
		return false;
	}else return this->senderuid < _rcsi.senderuid;
}

size_t RequestId::senderHash()const{
	return sockaddr.hash() ^ this->senderuid.first;
}

std::ostream &operator<<(std::ostream& _ros, const RequestId &_rreqid){
	_ros<<_rreqid.requid<<','<<' '<<_rreqid.senderuid.first<<','<<' '<<_rreqid.senderuid.second<<','<<' ';
	const SocketAddressInet4	&ra(_rreqid.sockaddr);
	char						host[SocketInfo::HostStringCapacity];
	char						port[SocketInfo::ServiceStringCapacity];
	ra.toString(
		host,
		SocketInfo::HostStringCapacity,
		port,
		SocketInfo::ServiceStringCapacity,
		SocketInfo::NumericService | SocketInfo::NumericHost
	);
	_ros<<host<<':'<<port;
	return _ros;
}

//--------------------------------------------------------------
//--------------------------------------------------------------

WriteRequestMessage::WriteRequestMessage(const uint8 _expectcnt): expectcnt(_expectcnt){
	completecnt.store(0);
	idbg("WriteRequestSignal "<<(void*)this);
	shared_mutex_safe(this);
}

WriteRequestMessage::WriteRequestMessage(const RequestId &_rreqid, const uint8 _expectcnt): expectcnt(_expectcnt), id(_rreqid){
	completecnt.store(0);
	idbg("WriteRequestMessage "<<(void*)this);
	shared_mutex_safe(this);
}

WriteRequestMessage::~WriteRequestMessage(){
	idbg("~WriteRequestMessage "<<(void*)this);
}

/*virtual*/ void WriteRequestMessage::ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, MessagePointerT &_rmsgptr){
	ipcconid = frame::ipc::ConnectionContext::the().connectionuid;
	
	char				host[SocketInfo::HostStringCapacity];
	char				port[SocketInfo::ServiceStringCapacity];
	
	id.sockaddr = frame::ipc::ConnectionContext::the().pairaddr;
	
	id.sockaddr.toString(
		host,
		SocketInfo::HostStringCapacity,
		port,
		SocketInfo::ServiceStringCapacity,
		SocketInfo::NumericService | SocketInfo::NumericHost
	);
	
	if(ipcIsBackOnSender()){
		idbg((void*)this<<" back on sender: baseport = "<<frame::ipc::ConnectionContext::the().baseport<<" host = "<<host<<":"<<port);
		if(static_cast<WriteRequestMessage*>(_rctx.requestMessage(*this).get())->consensusOnSuccess()){
			idbg("before consensusNotifyClientWithThis");
			consensusNotifyClientWithThis();
		}
	}else if(ipcIsOnReceiver()){
		idbg((void*)this<<" on peer: baseport = "<<frame::ipc::ConnectionContext::the().baseport<<" host = "<<host<<":"<<port);
		id.sockaddr.port(_rctx.baseport);
		this->consensusNotifyServerWithThis();
	}else{
		cassert(false);
	}
}

uint32 WriteRequestMessage::ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx){
	uint32	rv(0);
	idbg((void*)this);
	if(ipcIsOnSender()){
		rv |= frame::ipc::WaitResponseFlag;
	}
	rv |= frame::ipc::SynchronousSendFlag;
	rv |= frame::ipc::SameConnectorFlag;
	return rv;
}

void WriteRequestMessage::ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err){
	const uint8 cc = completecnt.fetch_add(1);
	idbg((void*)this<<" prev completecount = "<<(int)cc<<" err = "<<_err);
	if(expectcnt && cc == (expectcnt - 1) && !(cc & (1 << 7))){//there was no success sending message
		idbg("before consensusNotifyClientWithFail");
		consensusNotifyClientWithFail();
	}
}

size_t WriteRequestMessage::use(){
	size_t rv = DynamicShared<frame::ipc::Message>::use();
	idbg((void*)this<<" usecount = "<<usecount);
	return rv;
}

size_t WriteRequestMessage::release(){
	size_t rv = DynamicShared<frame::ipc::Message>::release();
	idbg((void*)this<<" usecount = "<<usecount);
	return rv;
}


//--------------------------------------------------------------
//--------------------------------------------------------------

ReadRequestMessage::ReadRequestMessage(){
	idbg("ReadRequestMessage "<<(void*)this);
	shared_mutex_safe(this);
}

ReadRequestMessage::ReadRequestMessage(const RequestId &_rreqid):id(_rreqid){
	idbg("ReadRequestMessage "<<(void*)this);
	shared_mutex_safe(this);
}

ReadRequestMessage::~ReadRequestMessage(){
	idbg("~ReadRequestMessage "<<(void*)this);
}

void ReadRequestMessage::ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, solid::frame::ipc::Message::MessagePointerT &_rmsgptr){
	ipcconid = _rctx.connectionuid;
	
	char				host[SocketInfo::HostStringCapacity];
	char				port[SocketInfo::ServiceStringCapacity];
	
	id.sockaddr = frame::ipc::ConnectionContext::the().pairaddr;
	
	id.sockaddr.toString(
		host,
		SocketInfo::HostStringCapacity,
		port,
		SocketInfo::ServiceStringCapacity,
		SocketInfo::NumericService | SocketInfo::NumericHost
	);
	
	if(ipcIsBackOnSender()){
		idbg((void*)this<<" back on sender: baseport = "<<frame::ipc::ConnectionContext::the().baseport<<" host = "<<host<<":"<<port);
		this->consensusNotifyClientWithThis();
	}else if(ipcIsOnReceiver()){
		idbg((void*)this<<" on peer: baseport = "<<frame::ipc::ConnectionContext::the().baseport<<" host = "<<host<<":"<<port);
		id.sockaddr.port(frame::ipc::ConnectionContext::the().baseport);
		this->consensusNotifyServerWithThis();
	}else{
		cassert(false);
	}
}
uint32 ReadRequestMessage::ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx){
	uint32	rv(0);
	idbg((void*)this);
	if(ipcIsOnSender()){
		rv |= frame::ipc::WaitResponseFlag;
	}
	rv |= frame::ipc::SynchronousSendFlag;
	rv |= frame::ipc::SameConnectorFlag;
	return rv;
}

void ReadRequestMessage::ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err){
	idbg((void*)this<<" err = "<<_err);
	if(!_err){
		Locker<Mutex> lock(shared_mutex(this));
	}
}

size_t ReadRequestMessage::use(){
	const size_t rv = DynamicShared<frame::ipc::Message>::use();
	idbg((void*)this<<" usecount = "<<usecount);
	return rv;
}

size_t ReadRequestMessage::release(){
	const size_t rv = DynamicShared<frame::ipc::Message>::release();
	idbg((void*)this<<" usecount = "<<usecount);
	return rv;
}


}//namespace consensus
}//namespace solid
