/* Implementation file ipcsession.cpp
	
	Copyright 2010 Valentin Palade
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
#include <queue>
#include <algorithm>
#include <iostream>
#include <cstring>

#include "system/debug.hpp"
#include "system/socketaddress.hpp"
#include "system/specific.hpp"
#include "utility/queue.hpp"
#include "algorithm/serialization/binary.hpp"
#include "algorithm/serialization/idtypemap.hpp"
#include "foundation/signal.hpp"
#include "foundation/manager.hpp"
#include "foundation/ipc/ipcservice.hpp"
#include "ipcsession.hpp"
#include "iodata.hpp"

namespace fdt = foundation;

namespace foundation{
namespace ipc{


struct BufCmp{
	bool operator()(const ulong id1, const ulong id2)const{
		if(id1 > id2)
			return (id1 - id2) <= (ulong)(0xffffffff/2);
		else
			return (id2 - id1) > (ulong)(0xffffffff/2);
	}
	bool operator()(const Buffer &_rbuf1, const Buffer &_rbuf2)const{
		return operator()(_rbuf1.id(), _rbuf2.id());
	}
	
};
//== Session::Data ====================================================
typedef serialization::IdTypeMap					IdTypeMap;

struct Session::Data{
	enum States{
		Connecting = 0,//Connecting must be first see isConnecting
		Accepting,
		WaitAccept,
		Connected,
		Disconnecting
	};
	enum{
		MaxSignalBufferCount = 8,//continuous buffers sent for a signal
		MaxSendSignalQueueSize = 32,//max count of signals sent in paralell
		MaxOutOfOrder = 4,
	};
	struct BinSerializer:serialization::bin::Serializer{
		BinSerializer():serialization::bin::Serializer(IdTypeMap::the()){}
		static unsigned specificCount(){return MaxSendSignalQueueSize;}
		void specificRelease(){}
	};

	struct BinDeserializer:serialization::bin::Deserializer{
		BinDeserializer():serialization::bin::Deserializer(IdTypeMap::the()){}
		static unsigned specificCount(){return MaxSendSignalQueueSize;}
		void specificRelease(){}
	};
	
	typedef BinSerializer						BinSerializerT;
	typedef BinDeserializer						BinDeserializerT;
	
	typedef std::pair<
		const SockAddrPair*,
		int
	>											BaseProcAddrT;
	typedef Queue<uint32>						UInt32QueueT;
	typedef std::priority_queue<
		Buffer,
		std::vector<Buffer>,
		BufCmp
	>											BufferPriorityQueueT;
public:
	Data(
		const Inet4SockAddrPair &_raddr,
		uint32 _keepalivetout
	);
	
	Data(
		const Inet4SockAddrPair &_raddr,
		int _baseport,
		uint32 _keepalivetout
	);
	
	~Data();
	
	BinSerializerT* popSerializer(){
		BinSerializerT* p = Specific::tryUncache<BinSerializerT>();
		if(!p){
			p = new BinSerializerT;
		}
		return p;
	}

	void pushSerializer(BinSerializerT* _p){
		cassert(_p->empty());
		Specific::cache(_p);
	}

	BinDeserializerT* popDeserializer(){
		BinDeserializerT* p = Specific::tryUncache<BinDeserializerT>();
		if(!p){
			p = new BinDeserializerT;
		}
		return p;
	}

	void pushDeserializer(BinDeserializerT* _p){
		cassert(_p->empty());
		Specific::cache(_p);
	}
	bool moveToNextOutOfOrderBuffer();
	bool mustSendUpdates();
	void incrementExpectedId();
public:
	SocketAddress			addr;
	SockAddrPair			pairaddr;
	BaseProcAddrT			baseaddr;
	uint32					rcvexpectedid;
	
	UInt32QueueT			rcvdidq;
	BufferPriorityQueueT	outoforderbufq;
};

//---------------------------------------------------------------------
Session::Data::Data(
	const Inet4SockAddrPair &_raddr,
	uint32 _keepalivetout
):	addr(_raddr), pairaddr(addr), 
	baseaddr(&pairaddr, addr.port()){
}
//---------------------------------------------------------------------
Session::Data::Data(
	const Inet4SockAddrPair &_raddr,
	int _baseport,
	uint32 _keepalivetout
):	addr(_raddr), pairaddr(addr), baseaddr(&pairaddr, _baseport){
}
//---------------------------------------------------------------------
Session::Data::~Data(){
}
//---------------------------------------------------------------------
bool Session::Data::moveToNextOutOfOrderBuffer(){
	BufCmp	bc;
	while(outoforderbufq.size() && (bc(outoforderbufq.top().id(), rcvexpectedid))){
		outoforderbufq.pop();
	}
	if(outoforderbufq.size()){
		return outoforderbufq.top().id() == rcvexpectedid;
	}
	return false;
}
//---------------------------------------------------------------------
bool Session::Data::mustSendUpdates(){
	return false;
}
//---------------------------------------------------------------------
void Session::Data::incrementExpectedId(){
	++rcvexpectedid;
	if(rcvexpectedid > Buffer::LastBufferId) rcvexpectedid = 0;
}
//=====================================================================
/*static*/ int Session::parseAcceptedBuffer(const Buffer &_rbuf){
	if(_rbuf.dataSize() == sizeof(int32)){
		int32 *pp((int32*)_rbuf.data());
		return (int)ntohl((uint32)*pp);
	}
	return BAD;
}
//---------------------------------------------------------------------
/*static*/ int Session::parseConnectingBuffer(const Buffer &_rbuf){
	if(_rbuf.dataSize() == sizeof(int32)){
		int32 *pp((int32*)_rbuf.data());
		return (int)ntohl((uint32)*pp);
	}
	return BAD;
}
//---------------------------------------------------------------------
Session::Session(
	const Inet4SockAddrPair &_raddr,
	uint32 _keepalivetout
):d(*(new Data(_raddr, _keepalivetout))){
}
//---------------------------------------------------------------------
Session::Session(
	const Inet4SockAddrPair &_raddr,
	int _basport,
	uint32 _keepalivetout
):d(*(new Data(_raddr, _basport, _keepalivetout))){
}
//---------------------------------------------------------------------
Session::~Session(){
	delete &d;
}
//---------------------------------------------------------------------
const Inet4SockAddrPair* Session::peerAddr4()const{
	const SockAddrPair *p = &(d.pairaddr);
	return reinterpret_cast<const Inet4SockAddrPair*>(p);
}
//---------------------------------------------------------------------
const Session::Addr4PairT* Session::baseAddr4()const{
	const Data::BaseProcAddrT *p = &(d.baseaddr);
	return reinterpret_cast<const std::pair<const Inet4SockAddrPair*, int>*>(p);
}
//---------------------------------------------------------------------
const SockAddrPair* Session::peerSockAddr()const{
	const SockAddrPair *p = &(d.pairaddr);
	return p;
}
//---------------------------------------------------------------------
bool Session::isConnected()const{
	return false;
}
//---------------------------------------------------------------------
bool Session::isDisconnecting()const{
	return false;
}
//---------------------------------------------------------------------
bool Session::isConnecting()const{
	return false;
}
//---------------------------------------------------------------------
bool Session::isAccepting()const{
	return false;
}
//---------------------------------------------------------------------
void Session::prepare(){
}
//---------------------------------------------------------------------
void Session::reconnect(Session *_pses){
}
//---------------------------------------------------------------------
int Session::pushSignal(DynamicPointer<Signal> &_rsig, uint32 _flag){
	return BAD;
}
//---------------------------------------------------------------------
bool Session::pushReceivedBuffer(Buffer &_rbuf, const TimeSpec &_rts, const ConnectionUid &_rconuid){
	if(_rbuf.id() == d.rcvexpectedid){
		return doPushExpectedReceivedBuffer(_rbuf, _rts, _rconuid);
	}else{
		return doPushUnxpectedReceivedBuffer(_rbuf, _rts, _rconuid);
	}
}
//---------------------------------------------------------------------
void Session::completeConnect(int _port){
}
//---------------------------------------------------------------------
bool Session::executeTimeout(
	Talker::TalkerStub &_rstub,
	uint32 _id,
	const TimeSpec &_rts
){
	return false;
}
//---------------------------------------------------------------------
int Session::execute(Talker::TalkerStub &_rstub, const TimeSpec &_rts){
	return BAD;
}
//---------------------------------------------------------------------
bool Session::pushSentBuffer(
	uint32 _id,
	const char *_data,
	const uint16 _size
){
	return false;
}
//---------------------------------------------------------------------
bool Session::doPushExpectedReceivedBuffer(
	Buffer &_rbuf,
	const TimeSpec &_rts,
	const ConnectionUid &_rconid
){
	//the expected buffer
	d.rcvdidq.push(_rbuf.id());
	
	if(_rbuf.updatesCount()){
		doFreeSentBuffers(_rbuf, _rconid);
	}
	
	if(_rbuf.type() == Buffer::DataType){
		doParseBuffer(_rbuf, _rconid);
	}
	
	d.incrementExpectedId();//move to the next buffer
	//while(d.inbufq.top().id() == d.expectedid){
	while(d.moveToNextOutOfOrderBuffer()){
		Buffer b(d.outoforderbufq.top());
		d.outoforderbufq.pop();
		//this is already done on receive
		if(_rbuf.type() == Buffer::DataType){
			doParseBuffer(b, _rconid);
		}
		d.incrementExpectedId();//move to the next buffer
	}
	return d.mustSendUpdates();
}
//---------------------------------------------------------------------
bool Session::doPushUnxpectedReceivedBuffer(
	Buffer &_rbuf,
	const TimeSpec &_rts,
	const ConnectionUid &_rconid
){
	BufCmp bc;
	//if(_rbuf.id() < d.expectedid){
	if(bc(_rbuf.id(), d.rcvexpectedid)){
		//the peer doesnt know that we've already received the buffer
		//add it as update
		d.rcvdidq.push(_rbuf.id());
	}else if(_rbuf.id() <= Buffer::LastBufferId){
		if(_rbuf.updatesCount()){//we have updates
			doFreeSentBuffers(_rbuf, _rconid);
		}
		//try to keep the buffer for future parsing
		if(d.outoforderbufq.size() < Data::MaxOutOfOrder){
			d.rcvdidq.push(_rbuf.id());//for peer updates
			d.outoforderbufq.push(_rbuf);
		}else{
			idbgx(Dbg::ipc, "to many buffers out-of-order");
		}
	}else if(_rbuf.id() == Buffer::UpdateBufferId){//a buffer containing only updates
		doFreeSentBuffers(_rbuf, _rconid);
	}
	return d.mustSendUpdates();
}
//---------------------------------------------------------------------
void Session::doFreeSentBuffers(const Buffer &_rbuf, const ConnectionUid &_rconid){
}
//---------------------------------------------------------------------
void Session::doParseBuffer(const Buffer &_rbuf, const ConnectionUid &_rconid){
}
//---------------------------------------------------------------------
}//namespace ipc
}//namespace foundation
