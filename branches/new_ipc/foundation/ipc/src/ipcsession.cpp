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
		MaxSendBufferCount = 6,
		MaxRecvNoUpdateCount = 4,// max number of buffers received, without sending update
		MaxSignalBufferCount = 8,//continuous buffers sent for a signal
		MaxSendSignalQueueSize = 32,//max count of signals sent in paralell
		MaxOutOfOrder = 4,//please also change moveToNextOutOfOrderBuffer
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

	
	struct RecvSignalData{
		RecvSignalData(
			Signal *_psignal = NULL,
			BinDeserializerT *_pdeserializer = NULL
		):psignal(_psignal), pdeserializer(_pdeserializer){}
		Signal					*psignal;
		BinDeserializerT		*pdeserializer;
	};
	
	struct SendSignalData{
		SendSignalData(
			const DynamicPointer<Signal>& _rsig,
			uint16 _bufid,
			uint16 _flags,
			uint32 _id
		):signal(_rsig), pserializer(NULL), bufid(_bufid), flags(_flags), id(_id), uid(0){}
		~SendSignalData(){
			cassert(!pserializer);
		}
		bool operator<(const SendSignalData &_owc)const{
			if(signal.ptr()){
				if(_owc.signal.ptr()){
				}else return true;
			}else return false;
			if(id < _owc.id){
				return (_owc.id - id) <= (uint32)(0xffffffff/2);
			}else{
				return (id - _owc.id) > (uint32)(0xffffffff/2);
			}
		}
		DynamicPointer<Signal>	signal;
		BinSerializerT			*pserializer;
		uint16					bufid;
		uint16					flags;
		uint32					id;
		uint32					uid;
	};
	typedef	std::vector<uint16>					UInt16VectorT;
	struct SendBufferData{
		Buffer				buffer;
		UInt16VectorT		signalidxvec;
	};
	typedef std::pair<
		const SockAddrPair*,
		int
	>											BaseProcAddrT;
	typedef Queue<uint32>						UInt32QueueT;
	typedef Stack<uint32>						UInt32StackT;
	typedef Stack<uint16>						UInt16StackT;
	typedef Stack<uint8>						UInt8StackT;
	typedef std::vector<Buffer>					BufferVectorT;
	typedef Queue<RecvSignalData>				RecvSignalQueueT;
	typedef std::vector<SendSignalData>			SendSignalVectorT;
	typedef std::vector<SendBufferData>			SendBufferVectorT;
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
	bool moveToNextOutOfOrderBuffer(Buffer &_rb);
	bool keepOutOfOrderBuffer(Buffer &_rb);
	bool mustSendUpdates();
	void incrementExpectedId();
	SignalUid pushSendWaitSignal(
		DynamicPointer<Signal> &_sig,
		uint16 _bufid,
		uint16 _flags,
		uint32 _id
	);	
	void popSentWaitSignals(SendBufferData &_rsbd);
	void popSentWaitSignal(const SignalUid &_rsiguid);
	void popSentWaitSignal(const uint32 _idx);
public:
	SocketAddress			addr;
	SockAddrPair			pairaddr;
	BaseProcAddrT			baseaddr;
	uint32					rcvexpectedid;
	uint8					state;
	uint16					outoforderbufcnt;
	uint32					sentsignalwaitresponse;
	
	UInt32QueueT			rcvdidq;
	BufferVectorT			outoforderbufvec;
	RecvSignalQueueT		rcvdsignalq;
	SendSignalVectorT		sendsignalvec;
	UInt32StackT			sendsignalfreeposstk;
	SendBufferVectorT		sendbuffervec;//will have about 6 items
	UInt8StackT				sendbufferfreeposstk;
};

//---------------------------------------------------------------------
Session::Data::Data(
	const Inet4SockAddrPair &_raddr,
	uint32 _keepalivetout
):	addr(_raddr), pairaddr(addr), 
	baseaddr(&pairaddr, addr.port()){
	outoforderbufvec.resize(MaxOutOfOrder);
	//first buffer is for keepalive
	sendbuffervec.resize(MaxSendBufferCount + 1);
	for(uint32 i(MaxSendBufferCount); i >= 1; --i){
		sendbufferfreeposstk.push(i);
	}
}
//---------------------------------------------------------------------
Session::Data::Data(
	const Inet4SockAddrPair &_raddr,
	int _baseport,
	uint32 _keepalivetout
):	addr(_raddr), pairaddr(addr), baseaddr(&pairaddr, _baseport){
	outoforderbufvec.resize(MaxOutOfOrder);
	//first buffer is for keepalive
	sendbuffervec.resize(MaxSendBufferCount + 1);
	for(uint32 i(MaxSendBufferCount); i >= 1; --i){
		sendbufferfreeposstk.push(i);
	}
}
//---------------------------------------------------------------------
Session::Data::~Data(){
}
//---------------------------------------------------------------------
bool Session::Data::moveToNextOutOfOrderBuffer(Buffer &_rb){
	cassert(MaxOutOfOrder == 4);
	unsigned idx(0);
	if(outoforderbufvec[0].empty()){
		return false;
	}
	if(outoforderbufvec[0].id() == rcvexpectedid){
		idx = 0;
		goto Success;
	}
	if(outoforderbufvec[1].empty()){
		return false;
	}
	if(outoforderbufvec[1].id() == rcvexpectedid){
		idx = 1;
		goto Success;
	}
	if(outoforderbufvec[2].empty()){
		return false;
	}
	if(outoforderbufvec[2].id() == rcvexpectedid){
		idx = 2;
		goto Success;
	}
	if(outoforderbufvec[3].empty()){
		return false;
	}
	if(outoforderbufvec[3].id() == rcvexpectedid){
		idx = 3;
		goto Success;
	}
	return false;
	Success:
	_rb = outoforderbufvec[idx];
	outoforderbufvec[idx] = outoforderbufvec[outoforderbufcnt - 1];
	incrementExpectedId();
	--outoforderbufcnt;
	return true;
}
//---------------------------------------------------------------------
bool Session::Data::keepOutOfOrderBuffer(Buffer &_rb){
	if(outoforderbufcnt == MaxOutOfOrder) return false;
	outoforderbufvec[outoforderbufcnt] = _rb;
	++outoforderbufcnt;
	return true;
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
//---------------------------------------------------------------------
SignalUid Session::Data::pushSendWaitSignal(
	DynamicPointer<Signal> &_sig,
	uint16 _bufid,
	uint16 _flags,
	uint32 _id
){
	_flags &= ~Service::SentFlag;
	_flags &= ~Service::WaitResponseFlag;
	
	if(sendsignalfreeposstk.size()){
		const uint32	idx(sendsignalfreeposstk.top());
		SendSignalData	&rssd(sendsignalvec[idx]);
		
		sendsignalfreeposstk.pop();
		
		rssd.bufid = _bufid;
		cassert(!rssd.signal.ptr());
		rssd.signal = _sig;
		rssd.flags = _flags;
		
		return SignalUid(idx, rssd.uid);
	
	}else{
		
		sendsignalvec.push_back(SendSignalData(_sig, _bufid, _flags, _id));
		return SignalUid(sendsignalvec.size() - 1, 0/*uid*/);
	}
}
//---------------------------------------------------------------------
void Session::Data::popSentWaitSignals(Session::Data::SendBufferData &_rsbd){
	switch(_rsbd.signalidxvec.size()){
		case 0:break;
		case 1:
			popSentWaitSignal(_rsbd.signalidxvec.front());
			break;
		case 2:
			popSentWaitSignal(_rsbd.signalidxvec[0]);
			popSentWaitSignal(_rsbd.signalidxvec[1]);
			break;
		case 3:
			popSentWaitSignal(_rsbd.signalidxvec[0]);
			popSentWaitSignal(_rsbd.signalidxvec[1]);
			popSentWaitSignal(_rsbd.signalidxvec[2]);
			break;
		case 4:
			popSentWaitSignal(_rsbd.signalidxvec[0]);
			popSentWaitSignal(_rsbd.signalidxvec[1]);
			popSentWaitSignal(_rsbd.signalidxvec[2]);
			popSentWaitSignal(_rsbd.signalidxvec[4]);
			break;
		default:
			for(
				UInt16VectorT::const_iterator it(_rsbd.signalidxvec.begin());
				it != _rsbd.signalidxvec.end();
				++it
			){
				popSentWaitSignal(*it);
			}
			break;
	}
	_rsbd.signalidxvec.clear();
}
//---------------------------------------------------------------------
void Session::Data::popSentWaitSignal(const SignalUid &_rsiguid){
	if(_rsiguid.idx < sendsignalvec.size()){
		SendSignalData &rssd(sendsignalvec[_rsiguid.idx]);
		
		if(rssd.uid != _rsiguid.uid) return;
		++rssd.uid;
		rssd.signal.clear();
		sendsignalfreeposstk.push(_rsiguid.idx);
		--sentsignalwaitresponse;
	}
}
//---------------------------------------------------------------------
void Session::Data::popSentWaitSignal(const uint32 _idx){
	SendSignalData &rssd(sendsignalvec[_idx]);
	cassert(rssd.signal.ptr());
	if(rssd.flags & Service::WaitResponseFlag){
		//let it leave a little longer
		idbgx(Dbg::ipc, "signal waits for response "<<_idx<<rssd.uid);
		rssd.flags |= Service::SentFlag;
	}else{
		++rssd.uid;
		rssd.signal.clear();
		sendsignalfreeposstk.push(_idx);
	}	
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
	Buffer	b;
	while(d.moveToNextOutOfOrderBuffer(b)){
		//this is already done on receive
		if(_rbuf.type() == Buffer::DataType){
			doParseBuffer(b, _rconid);
		}
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
		//try to keep the buffer for future parsin
		if(d.keepOutOfOrderBuffer(_rbuf)){
			d.rcvdidq.push(_rbuf.id());//for peer updates
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
void Session::doParseBufferDataType(const char *&_bpos, int &_blen, int _firstblen){
	uint8	datatype(*_bpos);
	
	++_bpos;
	--_blen;
	
	switch(datatype){
		case Buffer::ContinuedSignal:
			idbgx(Dbg::ipc, "continuedsignal");
			cassert(_blen == _firstblen);
			if(!d.rcvdsignalq.front().psignal){
				d.rcvdsignalq.pop();
			}
			//we cannot have a continued signal on the same buffer
			break;
		case Buffer::NewSignal:
			idbgx(Dbg::ipc, "newsignal");
			if(d.rcvdsignalq.size()){
				idbgx(Dbg::ipc, "switch to new rcq.size = "<<d.rcvdsignalq.size());
				Data::RecvSignalData &rrsd(d.rcvdsignalq.front());
				if(rrsd.psignal){//the previous signal didnt end, we reschedule
					d.rcvdsignalq.push(rrsd);
					rrsd.psignal = NULL;
				}
				rrsd.pdeserializer = d.popDeserializer();
				rrsd.pdeserializer->push(rrsd.psignal);
			}else{
				idbgx(Dbg::ipc, "switch to new rcq.size = 0");
				d.rcvdsignalq.push(Data::RecvSignalData(NULL, d.popDeserializer()));
				d.rcvdsignalq.front().pdeserializer->push(d.rcvdsignalq.front().psignal);
			}
			break;
		case Buffer::OldSignal:
			idbgx(Dbg::ipc, "oldsignal");
			cassert(d.rcvdsignalq.size() > 1);
			if(d.rcvdsignalq.front().psignal){
				d.rcvdsignalq.push(d.rcvdsignalq.front());
			}
			d.rcvdsignalq.pop();
			break;
		default:{
			cassert(false);
		}
	}
}
//---------------------------------------------------------------------
void Session::doParseBuffer(const Buffer &_rbuf, const ConnectionUid &_rconid){
	const char *bpos(_rbuf.data());
	int			blen(_rbuf.dataSize());
	int			rv;
	int 		firstblen(blen - 1);
	
	idbgx(Dbg::ipc, "bufferid = "<<_rbuf.id());
	
	while(blen > 2){
		
		idbgx(Dbg::ipc, "blen = "<<blen);
		
		doParseBufferDataType(bpos, blen, firstblen);
		
		Data::RecvSignalData &rrsd(d.rcvdsignalq.front());
		
		rv = rrsd.pdeserializer->run(bpos, blen);
		
		cassert(rv >= 0);
		blen -= rv;
		
		if(rrsd.pdeserializer->empty()){//done one signal.
			SignalUid siguid(0xffffffff, 0xffffffff);
			
			if(rrsd.psignal->ipcReceived(siguid, _rconid)){
				delete rrsd.psignal;
				rrsd.psignal = NULL;
			}
			
			idbgx(Dbg::ipc, "donesignal "<<siguid.idx<<','<<siguid.uid);
			
			if(siguid.idx != 0xffffffff && siguid.uid != 0xffffffff){//a valid signal waiting for response
				d.popSentWaitSignal(siguid);
			}
			d.pushDeserializer(rrsd.pdeserializer);
			//we do not pop it because in case of a new signal,
			//we must know if the previous signal terminate
			//so we dont mistakingly reschedule another signal!!
			rrsd.psignal = NULL;
			rrsd.pdeserializer = NULL;
		}
	}

}
//---------------------------------------------------------------------
}//namespace ipc
}//namespace foundation
