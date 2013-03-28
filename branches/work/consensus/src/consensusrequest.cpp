/* Implementation file consensusrequest.cpp
	
	Copyright 2011, 2012 Valentin Palade 
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

#include "consensus/consensusrequest.hpp"

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
WriteRequestMessage::WriteRequestMessage():waitresponse(false), st(OnSender), sentcount(0){
	idbg("WriteRequestSignal "<<(void*)this);
}
WriteRequestMessage::WriteRequestMessage(const RequestId &_rreqid):waitresponse(false), st(OnSender), sentcount(0),id(_rreqid){
	idbg("WriteRequestMessage "<<(void*)this);
}

WriteRequestMessage::~WriteRequestMessage(){
	idbg("~WriteRequestMessage "<<(void*)this);
	if(waitresponse && !sentcount){
		idbg("failed receiving response "/*<<sentcnt*/);
		//frame::Manager::specific().notify(frame::S_KILL | frame::S_RAISE, id.senderuid);
		notifySenderObjectWithFail();
	}
}

void WriteRequestMessage::ipcReceive(
	frame::ipc::MessageUid &_rmsguid
){
	//_rsiguid = this->ipcsiguid;
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
	
	waitresponse = false;
	
	if(st == OnSender){
		st = OnPeer;
		idbg((void*)this<<" on peer: baseport = "<<frame::ipc::ConnectionContext::the().baseport<<" host = "<<host<<":"<<port);
		id.sockaddr.port(frame::ipc::ConnectionContext::the().baseport);
		//fdt::m().signal(sig, serverUid());
		this->notifyConsensusObjectWithThis();
	}else if(st == OnPeer){
		st = BackOnSender;
		idbg((void*)this<<" back on sender: baseport = "<<frame::ipc::ConnectionContext::the().baseport<<" host = "<<host<<":"<<port);
		
		//DynamicPointer<frame::Message> msgptr(this);
		
		//frame::Manager::specific().notify(msgptr, id.senderuid);
		_rmsguid = this->ipcmsguid;
		notifySenderObjectWithThis();
	}else{
		cassert(false);
	}
}
// void WriteRequestSignal::sendThisToConsensusObject(){
// }
uint32 WriteRequestMessage::ipcPrepare(){
	uint32	rv(0);
	idbg((void*)this);
	if(st == OnSender){
		if(waitresponse){
			rv |= frame::ipc::WaitResponseFlag;
		}
		rv |= frame::ipc::SynchronousSendFlag;
		rv |= frame::ipc::SameConnectorFlag;
	}
	return rv;
}

void WriteRequestMessage::ipcComplete(int _err){
	idbg((void*)this<<" sentcount = "<<(int)sentcount<<" err = "<<_err);
	if(!_err){
		Locker<Mutex> lock(mutex());
		++sentcount;
	}
}
void WriteRequestMessage::use(){
	DynamicShared<frame::Message>::use();
	idbg((void*)this<<" usecount = "<<usecount);
}
int WriteRequestMessage::release(){
	int rv = DynamicShared<frame::Message>::release();
	idbg((void*)this<<" usecount = "<<usecount);
	return rv;
}
//--------------------------------------------------------------
//--------------------------------------------------------------
ReadRequestMessage::ReadRequestMessage():waitresponse(false), st(OnSender), sentcount(0){
	idbg("ReadRequestMessage "<<(void*)this);
}
ReadRequestMessage::ReadRequestMessage(const RequestId &_rreqid):waitresponse(false), st(OnSender), sentcount(0),id(_rreqid){
	idbg("ReadRequestMessage "<<(void*)this);
}

ReadRequestMessage::~ReadRequestMessage(){
	idbg("~ReadRequestMessage "<<(void*)this);
	if(waitresponse && !sentcount){
		idbg("failed receiving response "/*<<sentcnt*/);
		//frame::Manager::specific().notify(frame::S_KILL | frame::S_RAISE, id.senderuid);
		notifySenderObjectWithFail();
	}
}

void ReadRequestMessage::ipcReceive(
	frame::ipc::MessageUid &_rmsguid
){
	//_rsiguid = this->ipcsiguid;
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
	
	waitresponse = false;
	
	if(st == OnSender){
		st = OnPeer;
		idbg((void*)this<<" on peer: baseport = "<<frame::ipc::ConnectionContext::the().baseport<<" host = "<<host<<":"<<port);
		id.sockaddr.port(frame::ipc::ConnectionContext::the().baseport);
		this->notifyConsensusObjectWithThis();
	}else if(st == OnPeer){
		st = BackOnSender;
		idbg((void*)this<<" back on sender: baseport = "<<frame::ipc::ConnectionContext::the().baseport<<" host = "<<host<<":"<<port);
		
		_rmsguid = this->ipcmsguid;
		this->notifySenderObjectWithThis();
	}else{
		cassert(false);
	}
}
// void ReadRequestSignal::sendThisToConsensusObject(){
// }
uint32 ReadRequestMessage::ipcPrepare(){
	uint32	rv(0);
	idbg((void*)this);
	if(st == OnSender){
		if(waitresponse){
			rv |= frame::ipc::WaitResponseFlag;
		}
		rv |= frame::ipc::SynchronousSendFlag;
		rv |= frame::ipc::SameConnectorFlag;
	}
	return rv;
}

void ReadRequestMessage::ipcComplete(int _err){
	idbg((void*)this<<" sentcount = "<<(int)sentcount<<" err = "<<_err);
	if(!_err){
		Locker<Mutex> lock(mutex());
		++sentcount;
	}
}

void ReadRequestMessage::use(){
	DynamicShared<frame::Message>::use();
	idbg((void*)this<<" usecount = "<<usecount);
}
int ReadRequestMessage::release(){
	int rv = DynamicShared<frame::Message>::release();
	idbg((void*)this<<" usecount = "<<usecount);
	return rv;
}
}//namespace consensus
}//namespace solid
