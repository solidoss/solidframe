/* Implementation file ipcsession.cpp
	
	Copyright 2010 Valentin Palade
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
	bool operator()(const uint32 id1, const uint32 id2)const{
		if(id1 > id2)
			return (id1 - id2) <= (uint32)(0xffffffff/2);
		else
			return (id2 - id1) > (uint32)(0xffffffff/2);
	}
	bool operator()(const Buffer &_rbuf1, const Buffer &_rbuf2)const{
		return operator()(_rbuf1.id(), _rbuf2.id());
	}
	
};

namespace{

struct StaticData{
	enum{
		DataRetransmitCount = 8,
		ConnectRetransmitCount = 16,
		//StartRetransmitTimeout = 100
		RefreshIndex = (1 << 7) - 1
	};
	StaticData();
	static StaticData& instance();
	ulong retransmitTimeout(uint _pos);
private:
	typedef std::vector<ulong> ULongVectorT;
	ULongVectorT	toutvec;
};


union UInt32Pair{
	uint32	u32;
	uint16	u16[2];
};

uint32 pack32(const uint16 _f, const uint16 _s){
	UInt32Pair p;
	p.u16[0] = _f;
	p.u16[1] = _s;
	return p.u32;
}

uint16 unpackFirst16(const uint32 _v){
	UInt32Pair p;
	p.u32 = _v;
	return p.u16[0];
}

uint16 unpackSecond16(const uint32 _v){
	UInt32Pair p;
	p.u32 = _v;
	return p.u16[1];
}


}//namespace

#ifdef USTATISTICS
namespace {
struct StatisticData{
	StatisticData(){
		memset(this, 0, sizeof(StatisticData));
	}
	~StatisticData();
	void reconnect();
	void pushSignal();
	void pushReceivedBuffer();
	void retransmitId(const uint16 _cnt);
	void sendKeepAlive();
	void sendPending();
	void pushExpectedReceivedBuffer();
	void alreadyReceived();
	void tooManyBuffersOutOfOrder();
	void sendUpdatesSize(const uint16 _sz);
	void sendOnlyUpdatesSize(const uint16 _sz);
	void sendSignalIdxQueueSize(const ulong _sz);
	void tryScheduleKeepAlive();
	void scheduleKeepAlive();
	void failedTimeout();
	void timeout();
	
	ulong reconnectcnt;
	ulong pushsignalcnt;
	ulong pushreceivedbuffercnt;
	ulong maxretransmitid;
	ulong sendkeepalivecnt;
	ulong sendpendingcnt;
	ulong pushexpectedreceivedbuffercnt;
	ulong alreadyreceivedcnt;
	ulong toomanybuffersoutofordercnt;
	ulong maxsendupdatessize;
	ulong sendonlyupdatescnt;
	ulong maxsendonlyupdatessize;
	ulong maxsendsignalidxqueuesize;
	ulong tryschedulekeepalivecnt;
	ulong schedulekeepalivecnt;
	ulong failedtimeoutcnt;
	ulong timeoutcnt;
};

std::ostream& operator<<(std::ostream &_ros, const StatisticData &_rsd);
}//namespace
#endif

//== Session::Data ====================================================
typedef serialization::IdTypeMap					IdTypeMap;

struct Session::Data{
	enum States{
		Connecting = 0,//Connecting must be first see isConnecting
		Accepting,
		WaitAccept,
		Connected,
		Disconnecting,
		Reconnecting,
		Disconnected,
	};
	enum{
		UpdateBufferId = 0xffffffff,//the id of a buffer containing only updates
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
			//cassert(!pserializer);
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
		SendBufferData():uid(0), sending(0), mustdelete(0){}
		Buffer				buffer;
		UInt16VectorT		signalidxvec;
		uint16				uid;//we need uid for timer
		uint8				sending;
		uint8				mustdelete;
	};
	
	typedef std::pair<
		const SockAddrPair*,
		int
	>											BaseAddrT;
	typedef Queue<uint32>						UInt32QueueT;
	typedef Stack<uint32>						UInt32StackT;
	typedef Stack<uint16>						UInt16StackT;
	typedef Stack<uint8>						UInt8StackT;
	typedef std::vector<Buffer>					BufferVectorT;
	typedef Queue<RecvSignalData>				RecvSignalQueueT;
	typedef std::vector<SendSignalData>			SendSignalVectorT;
	typedef std::vector<SendBufferData>			SendBufferVectorT;
	typedef std::pair<
		DynamicPointer<Signal>,
		uint32
		>										SignalPairT;
	typedef Queue<SignalPairT>					SignalQueueT;
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
	void incrementSendId();
	SignalUid pushSendWaitSignal(
		DynamicPointer<Signal> &_sig,
		uint16 _bufid,
		uint16 _flags,
		uint32 _id
	);	
	void popSentWaitSignals(SendBufferData &_rsbd);
	void popSentWaitSignal(const SignalUid &_rsiguid);
	void popSentWaitSignal(const uint32 _idx);
	void freeSentBuffer(const uint32 _idx);
	void clearSentBuffer(const uint32 _idx);
	
	uint32 computeRetransmitTimeout(const uint32 _retrid, const uint32 _bufid);
	bool canSendKeepAlive(const TimeSpec &_tpos)const;
	uint32 registerBuffer(Buffer &_rbuf);
	
	bool isExpectingImmediateDataFromPeer()const{
		return rcvdsignalq.size() > 1 || ((rcvdsignalq.size() == 1) && rcvdsignalq.front().psignal);
	}
	uint16 keepAliveBufferIndex()const{
		return 0;
	}
	void moveSignalsToSendQueue();
	void resetKeepAlive();
public:
	SocketAddress			addr;
	SockAddrPair			pairaddr;
	BaseAddrT				baseaddr;
	uint32					rcvexpectedid;
	uint8					state;
	uint8					sendpendingcount;
	uint16					outoforderbufcnt;
	uint32					sentsignalwaitresponse;
	uint32					retansmittimepos;
	uint32					sendsignalid;
	uint16					currentbuffersignalcount;//MaxSignalBufferCount
	uint32					sendid;
	uint32					keepalivetimeout;
	
	UInt32QueueT			rcvdidq;
	BufferVectorT			outoforderbufvec;
	RecvSignalQueueT		rcvdsignalq;
	SendSignalVectorT		sendsignalvec;
	UInt32StackT			sendsignalfreeposstk;
	SendBufferVectorT		sendbuffervec;//will have about 6 items
	UInt8StackT				sendbufferfreeposstk;
	SignalQueueT			signalq;
	UInt32QueueT			sendsignalidxq;
	Buffer					updatesbuffer;
	TimeSpec				rcvtimepos;
#ifdef USTATISTICS
	StatisticData			statistics;
#endif
};

//---------------------------------------------------------------------
Session::Data::Data(
	const Inet4SockAddrPair &_raddr,
	uint32 _keepalivetout
):	addr(_raddr), pairaddr(addr), 
	baseaddr(&pairaddr, addr.port()),
	rcvexpectedid(2), state(Connecting), sendpendingcount(0),
	outoforderbufcnt(0), sentsignalwaitresponse(0),
	retansmittimepos(0), sendsignalid(0), 
	currentbuffersignalcount(MaxSignalBufferCount), sendid(1),
	keepalivetimeout(_keepalivetout)
{
	outoforderbufvec.resize(MaxOutOfOrder);
	//first buffer is for keepalive
	sendbuffervec.resize(MaxSendBufferCount + 1);
	for(uint32 i(MaxSendBufferCount); i >= 1; --i){
		sendbufferfreeposstk.push(i);
	}
	sendsignalvec.reserve(Data::MaxSendSignalQueueSize);
}
//---------------------------------------------------------------------
Session::Data::Data(
	const Inet4SockAddrPair &_raddr,
	int _baseport,
	uint32 _keepalivetout
):	addr(_raddr), pairaddr(addr), baseaddr(&pairaddr, _baseport),
	rcvexpectedid(2), state(Accepting), sendpendingcount(0),
	outoforderbufcnt(0), sentsignalwaitresponse(0),
	retansmittimepos(0), sendsignalid(0), 
	currentbuffersignalcount(MaxSignalBufferCount), sendid(1),
	keepalivetimeout(_keepalivetout)
{
	outoforderbufvec.resize(MaxOutOfOrder);
	//first buffer is for keepalive
	sendbuffervec.resize(MaxSendBufferCount + 1);
	for(uint32 i(MaxSendBufferCount); i >= 1; --i){
		sendbufferfreeposstk.push(i);
	}
	sendsignalvec.reserve(Data::MaxSendSignalQueueSize);
}
//---------------------------------------------------------------------
Session::Data::~Data(){
	while(signalq.size()){
		signalq.front().first->ipcFail(0);
		signalq.pop();
	}
	while(this->rcvdsignalq.size()){
		delete rcvdsignalq.front().psignal;
		if(rcvdsignalq.front().pdeserializer){
			rcvdsignalq.front().pdeserializer->clear();
			pushDeserializer(rcvdsignalq.front().pdeserializer);
			rcvdsignalq.front().pdeserializer = NULL;
		}
		rcvdsignalq.pop();
	}
	for(Data::SendSignalVectorT::iterator it(sendsignalvec.begin()); it != sendsignalvec.end(); ++it){
		SendSignalData &rssd(*it);
		if(rssd.signal.ptr()){
			if((rssd.flags & Service::WaitResponseFlag) && (rssd.flags & Service::SentFlag)){
				//the was successfully sent but the response did not arrive
				rssd.signal->ipcFail(1);
			}else{
				//the signal was not successfully sent
				rssd.signal->ipcFail(0);
			}
			rssd.signal.clear();
		}
		if(rssd.pserializer){
			rssd.pserializer->clear();
			pushSerializer(rssd.pserializer);
			rssd.pserializer = NULL;
		}
	}
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
inline bool Session::Data::mustSendUpdates(){
	return rcvdidq.size() && 
		//we are not expecting anything from the peer - send updates right away
		(!isExpectingImmediateDataFromPeer() || 
		//or we've already waited too long to send the updates
		rcvdidq.size() >= Data::MaxRecvNoUpdateCount);
}
//---------------------------------------------------------------------
inline void Session::Data::incrementExpectedId(){
	++rcvexpectedid;
	if(rcvexpectedid > Buffer::LastBufferId) rcvexpectedid = 0;
}
//---------------------------------------------------------------------
inline void Session::Data::incrementSendId(){
	++sendid;
	if(sendid > Buffer::LastBufferId) sendid = 0;
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
		cassert(!_sig.ptr());
		rssd.flags = _flags;
		
		return SignalUid(idx, rssd.uid);
	
	}else{
		
		sendsignalvec.push_back(SendSignalData(_sig, _bufid, _flags, _id));
		cassert(!_sig.ptr());
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
//---------------------------------------------------------------------
void Session::Data::freeSentBuffer(const uint32 _bufid){
	uint	idx;
	if(/*	sendbuffervec[0].buffer.buffer() && */
		sendbuffervec[0].buffer.id() == _bufid
	){
		idx = 0;
		goto KeepAlive;
	}
	if(	sendbuffervec[1].buffer.buffer() && 
		sendbuffervec[1].buffer.id() == _bufid
	){
		idx = 1;
		goto Success;
	}
	if(	sendbuffervec[2].buffer.buffer() && 
		sendbuffervec[2].buffer.id() == _bufid
	){
		idx = 2;
		goto Success;
	}
	if(	sendbuffervec[3].buffer.buffer() && 
		sendbuffervec[3].buffer.id() == _bufid
	){
		idx = 3;
		goto Success;
	}
	if(	sendbuffervec[4].buffer.buffer() && 
		sendbuffervec[4].buffer.id() == _bufid
	){
		idx = 4;
		goto Success;
	}
	if(	sendbuffervec[5].buffer.buffer() && 
		sendbuffervec[5].buffer.id() == _bufid
	){
		idx = 5;
		goto Success;
	}
	if(	sendbuffervec[6].buffer.buffer() && 
		sendbuffervec[6].buffer.id() == _bufid
	){
		idx = 6;
		goto Success;
	}
	return;
	KeepAlive:{
		SendBufferData &rsbd(sendbuffervec[0]);
		if(!rsbd.sending){
			//we can now initiate the sending of keepalive buffer
			rsbd.buffer.retransmitId(0);
		}else{
			rsbd.mustdelete = 1;
		}
	}
	return;
	Success:{
		SendBufferData &rsbd(sendbuffervec[idx]);
		if(!rsbd.sending){
			clearSentBuffer(idx);
		}else{
			rsbd.mustdelete = 1;
		}
	}
	
}
//----------------------------------------------------------------------
void Session::Data::clearSentBuffer(const uint32 _idx){
	vdbgx(Dbg::ipc, ""<<sendbufferfreeposstk.size());
	SendBufferData &rsbd(sendbuffervec[_idx]);
	popSentWaitSignals(rsbd);
	rsbd.buffer.clear();
	++rsbd.uid;
	sendbufferfreeposstk.push(_idx);
}
//----------------------------------------------------------------------
inline uint32 Session::Data::computeRetransmitTimeout(const uint32 _retrid, const uint32 _bufid){
	if(!(_bufid & StaticData::RefreshIndex)){
		//recalibrate the retansmittimepos
		retansmittimepos = 0;
	}
	if(_retrid > retansmittimepos){
		retansmittimepos = _retrid;
		return StaticData::instance().retransmitTimeout(retansmittimepos);
	}else{
		return StaticData::instance().retransmitTimeout(retansmittimepos + _retrid);
	}
}
//----------------------------------------------------------------------
inline bool Session::Data::canSendKeepAlive(const TimeSpec &_tpos)const{
	return keepalivetimeout && sentsignalwaitresponse &&
		(rcvdsignalq.empty() || rcvdsignalq.size() == 1 && !rcvdsignalq.front().psignal) && 
		signalq.empty() && sendsignalidxq.empty() && _tpos >= rcvtimepos;
}
//----------------------------------------------------------------------
uint32 Session::Data::registerBuffer(Buffer &_rbuf){
	if(sendbufferfreeposstk.empty()) return -1;
	const uint32	idx(sendbufferfreeposstk.top());
	SendBufferData	&rsbd(sendbuffervec[idx]);
	
	sendbufferfreeposstk.pop();
	
	cassert(rsbd.buffer.empty());
	
	rsbd.buffer = _rbuf;
	return idx;
}
//----------------------------------------------------------------------
void Session::Data::moveSignalsToSendQueue(){
	while(signalq.size() && sendsignalidxq.size() < Data::MaxSendSignalQueueSize){
		uint32 			flags(signalq.front().second);
		cassert(signalq.front().first.ptr());
		const SignalUid	uid(pushSendWaitSignal(signalq.front().first, 0, flags, sendsignalid++));
		SendSignalData 	&rssd(sendsignalvec[uid.idx]);
		
		sendsignalidxq.push(uid.idx);
		
		switch(rssd.signal->ipcPrepare(uid)){
			case OK://signal doesnt wait for response
				rssd.flags &= ~Service::WaitResponseFlag;
				break;
			case NOK://signal wait for response
				++sentsignalwaitresponse;
				rssd.flags |= Service::WaitResponseFlag;
				break;
			default:
				cassert(false);
		}
		signalq.pop();
	}
}
//----------------------------------------------------------------------
void Session::Data::resetKeepAlive(){
	SendBufferData	&rsbd(sendbuffervec[0]);//the keep alive buffer
	++rsbd.uid;
}
//=====================================================================
//	Session
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
	vdbgx(Dbg::ipc, "Created connect session"<<(void*)this);
}
//---------------------------------------------------------------------
Session::Session(
	const Inet4SockAddrPair &_raddr,
	int _basport,
	uint32 _keepalivetout
):d(*(new Data(_raddr, _basport, _keepalivetout))){
	vdbgx(Dbg::ipc, "Created accept session"<<(void*)this);
}
//---------------------------------------------------------------------
Session::~Session(){
	vdbgx(Dbg::ipc, "delete session"<<(void*)this);
	delete &d;
}
//---------------------------------------------------------------------
const Inet4SockAddrPair* Session::peerAddr4()const{
	const SockAddrPair *p = &(d.pairaddr);
	return reinterpret_cast<const Inet4SockAddrPair*>(p);
}
//---------------------------------------------------------------------
const Session::Addr4PairT* Session::baseAddr4()const{
	const Data::BaseAddrT *p = &(d.baseaddr);
	return reinterpret_cast<const std::pair<const Inet4SockAddrPair*, int>*>(p);
}
//---------------------------------------------------------------------
const SockAddrPair* Session::peerSockAddr()const{
	const SockAddrPair *p = &(d.pairaddr);
	return p;
}
//---------------------------------------------------------------------
bool Session::isConnected()const{
	return d.state > Data::Connecting;
}
//---------------------------------------------------------------------
bool Session::isDisconnecting()const{
	return d.state == Data::Disconnecting;
}
//---------------------------------------------------------------------
bool Session::isConnecting()const{
	return d.state == Data::Connecting;
}
//---------------------------------------------------------------------
bool Session::isAccepting()const{
	return d.state == Data::Accepting;
}
//---------------------------------------------------------------------
void Session::prepare(){
	Buffer b(
		Specific::popBuffer(Specific::sizeToId(Buffer::minSize())),
		Specific::idToCapacity(Specific::sizeToId(Buffer::minSize()))
	);
	b.resetHeader();
	b.type(Buffer::KeepAliveType);
	b.id(Buffer::UpdateBufferId);
	d.sendbuffervec[0].buffer = b;
}
//---------------------------------------------------------------------
void Session::reconnect(Session *_pses){
	COLLECT_DATA_0(d.statistics.reconnect);
	vdbgx(Dbg::ipc, "reconnecting session "<<(void*)_pses);
	//first we reset the peer addresses
	if(_pses){
		d.addr = _pses->d.addr;
		d.pairaddr = d.addr;
		d.state = Data::Accepting;
	}else{
		d.state = Data::Connecting;
	}
	//clear the receive queue
	while(d.rcvdsignalq.size()){
		Data::RecvSignalData	&rrsd(d.rcvdsignalq.front());
		delete rrsd.psignal;
		if(rrsd.pdeserializer){
			rrsd.pdeserializer->clear();
			d.pushDeserializer(rrsd.pdeserializer);
		}
		d.rcvdsignalq.pop();
	}
	//keep sending signals that do not require the same connector
	for(int sz(d.signalq.size()); sz; --sz){
		if(!(d.signalq.front().second & Service::SameConnectorFlag)) {
			vdbgx(Dbg::ipc, "signal scheduled for resend");
			d.signalq.push(d.signalq.front());
		}else{
			vdbgx(Dbg::ipc, "signal not scheduled for resend");
		}
		d.signalq.pop();
	}
	//clear the sendsignalidxq
	while(d.sendsignalidxq.size()){
		d.sendsignalidxq.pop();
	}
	//see which sent/sending signals must be cleard
	for(Data::SendSignalVectorT::iterator it(d.sendsignalvec.begin()); it != d.sendsignalvec.end(); ++it){
		Data::SendSignalData &rssd(*it);
		if(rssd.pserializer){
			rssd.pserializer->clear();
			d.pushSerializer(rssd.pserializer);
			rssd.pserializer = NULL;
		}
		if(!rssd.signal) continue;
		
		if(!(rssd.flags & Service::SameConnectorFlag)){
			if(rssd.flags & Service::WaitResponseFlag && rssd.flags & Service::SentFlag){
				//if the signal was sent and were waiting for response - were not sending twice
				rssd.signal->ipcFail(1);
				rssd.signal.clear();
				++rssd.uid;
			}
			//we can try resending the signal
		}else{
			vdbgx(Dbg::ipc, "signal not scheduled for resend");
			if(rssd.flags & Service::WaitResponseFlag && rssd.flags & Service::SentFlag){
				rssd.signal->ipcFail(1);
			}else{
				rssd.signal->ipcFail(0);
			}
			rssd.signal.clear();
			++rssd.uid;
		}
	}
	//sort the sendsignalvec, so that we can insert into signalq the signals that can be resent
	//in the exact order they were inserted
	std::sort(d.sendsignalvec.begin(), d.sendsignalvec.end());
	
	//then we push into signalq the signals from sendsignalvec
	for(Data::SendSignalVectorT::const_iterator it(d.sendsignalvec.begin()); it != d.sendsignalvec.end(); ++it){
		const Data::SendSignalData &rssd(*it);
		if(rssd.signal.ptr()){
			//the sendsignalvec may contain signals sent successfully, waiting for a response
			//those signals are not queued in the scq
			d.signalq.push(Data::SignalPairT(rssd.signal, rssd.flags));
		}else break;
	}
	
	d.sendsignalvec.clear();
	
	//delete the sending buffers
	for(Data::SendBufferVectorT::iterator it(d.sendbuffervec.begin()); it != d.sendbuffervec.end(); ++it){
		Data::SendBufferData	&rsbd(*it);
		++rsbd.uid;
		if(it != d.sendbuffervec.begin()){
			rsbd.buffer.clear();
			rsbd.signalidxvec.clear();
			cassert(rsbd.sending == 0);
		}
	}
	
	//clear the stacks
	
	d.updatesbuffer.clear();
	
	while(d.sendbufferfreeposstk.size()){
		d.sendbufferfreeposstk.pop();
	}
	for(uint32 i(Data::MaxSendBufferCount); i >= 1; --i){
		d.sendbufferfreeposstk.push(i);
	}
	while(d.sendsignalfreeposstk.size()){
		d.sendsignalfreeposstk.pop();
	}
	d.rcvexpectedid = 2;
	d.sendid = 1;
	d.sentsignalwaitresponse = 0;
	d.currentbuffersignalcount = Data::MaxSignalBufferCount;
	d.sendbuffervec[0].buffer.id(Buffer::UpdateBufferId);
	d.sendbuffervec[0].buffer.retransmitId(0);
}
//---------------------------------------------------------------------
int Session::pushSignal(DynamicPointer<Signal> &_rsig, uint32 _flags){
	COLLECT_DATA_0(d.statistics.pushSignal);
	d.signalq.push(Data::SignalPairT(_rsig, _flags));
	if(d.signalq.size() == 1) return NOK;
	return OK;
}
//---------------------------------------------------------------------
bool Session::pushReceivedBuffer(
	Buffer &_rbuf,
	Talker::TalkerStub &_rstub,
	const ConnectionUid &_rconuid
){
	COLLECT_DATA_0(d.statistics.pushReceivedBuffer);
	d.rcvtimepos = _rstub.currentTime();
	d.resetKeepAlive();
	if(_rbuf.id() == d.rcvexpectedid){
		return doPushExpectedReceivedBuffer(_rbuf, _rstub, _rconuid);
	}else{
		return doPushUnxpectedReceivedBuffer(_rbuf, _rstub, _rconuid);
	}
}
//---------------------------------------------------------------------
void Session::completeConnect(int _pairport){
	if(d.state == Data::Connected) return;
	
	d.state = Data::Connected;
	d.addr.port(_pairport);
	
	d.freeSentBuffer(1);
	//put the received accepting buffer from peer as update - it MUST have id 0
	d.rcvdidq.push(1);
}
//---------------------------------------------------------------------
bool Session::executeTimeout(
	Talker::TalkerStub &_rstub,
	uint32 _id
){
	uint16	idx(unpackFirst16(_id));
	uint16	uid(unpackSecond16(_id));
	
	Data::SendBufferData &rsbd(d.sendbuffervec[idx]);
	
	COLLECT_DATA_0(d.statistics.timeout);
	if(rsbd.uid != uid){
		vdbgx(Dbg::ipc, "timeout for ("<<idx<<','<<uid<<')');
		COLLECT_DATA_0(d.statistics.failedTimeout);
		return false;
	}
	vdbgx(Dbg::ipc, "timeout for ("<<idx<<','<<uid<<')'<<' '<<rsbd.buffer.id());
	cassert(!rsbd.buffer.empty());
	
	rsbd.buffer.retransmitId(rsbd.buffer.retransmitId() + 1);
	
	COLLECT_DATA_1(d.statistics.retransmitId, rsbd.buffer.retransmitId());
	
	if(rsbd.buffer.retransmitId() > StaticData::DataRetransmitCount){
		if(rsbd.buffer.type() == Buffer::ConnectingType){
			if(rsbd.buffer.retransmitId() > StaticData::ConnectRetransmitCount){//too many resends for connect type
				vdbgx(Dbg::ipc, "preparing to disconnect process");
				cassert(d.state != Data::Disconnecting);
				d.state = Data::Disconnecting;
				return true;//disconnecting
			}
		}else{
			d.state = Data::Reconnecting;
			return true;
		}
	}
	
	if(!rsbd.sending/* && (idx || d.canSendKeepAlive(_rstub.currentTime()))*/){
		if(!idx){
			if(d.canSendKeepAlive(_rstub.currentTime())){
				rsbd.buffer.id(d.sendid);
				d.incrementSendId();
				COLLECT_DATA_0(d.statistics.sendKeepAlive);
			}else{
				return false;
			}
		}
		//resend the buffer
		if(_rstub.pushSendBuffer(idx, rsbd.buffer.buffer(), rsbd.buffer.bufferSize())){
			TimeSpec tpos(_rstub.currentTime());
			if(idx){
				tpos += d.computeRetransmitTimeout(rsbd.buffer.retransmitId(), rsbd.buffer.id());
			}else{
				tpos += d.keepalivetimeout;
			}
	
			_rstub.pushTimer(_id, tpos);
			vdbgx(Dbg::ipc, "send buffer"<<rsbd.buffer.id()<<" done");
		}else{
			COLLECT_DATA_0(d.statistics.sendPending);
			rsbd.sending = 1;
			++d.sendpendingcount;
			vdbgx(Dbg::ipc, "send buffer"<<rsbd.buffer.id()<<" pending");
		}
	}
	
	return false;
}
//---------------------------------------------------------------------
int Session::execute(Talker::TalkerStub &_rstub){
	switch(d.state){
		case Data::Connecting:
			return doExecuteConnecting(_rstub);
		case Data::Accepting:
			return doExecuteAccepting(_rstub);
		case Data::WaitAccept:
			return NOK;
		case Data::Connected:
			return doExecuteConnected(_rstub);
		case Data::Disconnecting:
			return doExecuteDisconnect(_rstub);
		case Data::Reconnecting:
			if(d.sendpendingcount) return NOK;
			reconnect(NULL);
			return OK;//reschedule
	}
	return BAD;
}
//---------------------------------------------------------------------
bool Session::pushSentBuffer(
	Talker::TalkerStub &_rstub,
	uint32 _id,
	const char *_data,
	const uint16 _size
){
	if(_id == -1){//an update buffer
		//free it right away
		d.updatesbuffer.clear();
		doTryScheduleKeepAlive(_rstub);
		return d.rcvdidq.size() != 0;
	}
	
	//all we do is set a timer for this buffer
	Data::SendBufferData &rsbd(d.sendbuffervec[_id]);
	
	if(_id == 0){//the keepalive buffer
		vdbgx(Dbg::ipc, "sent keep alive buffer done");
		if(rsbd.mustdelete){
			rsbd.mustdelete = 0;
			rsbd.buffer.retransmitId(0);
		}
		TimeSpec tpos(_rstub.currentTime());
		tpos += d.keepalivetimeout;

		_rstub.pushTimer(pack32(_id, rsbd.uid), tpos);
		return false;
	}
	
	cassert(rsbd.sending);
	cassert(rsbd.buffer.buffer() == _data);
	rsbd.sending = 0;
	--d.sendpendingcount;
	
	bool b(false);
	
	if(!d.sendpendingcount && d.state == Data::Reconnecting){
		b = true;
	}
	
	if(rsbd.mustdelete){
		if(_id){
			d.clearSentBuffer(_id);
			return b;
		}else{
			rsbd.mustdelete = 0;
			rsbd.buffer.retransmitId(0);
			return b;
		}
	}
	
	//schedule a timer for this buffer
	TimeSpec tpos(_rstub.currentTime());
	tpos += d.computeRetransmitTimeout(rsbd.buffer.retransmitId(), rsbd.buffer.id());
	
	_rstub.pushTimer(pack32(_id, rsbd.uid), tpos);
	return b;
}
//---------------------------------------------------------------------
bool Session::doPushExpectedReceivedBuffer(
	Buffer &_rbuf,
	Talker::TalkerStub &_rstub,
	const ConnectionUid &_rconid
){
	COLLECT_DATA_0(d.statistics.pushExpectedReceivedBuffer);
	vdbgx(Dbg::ipc, "expected "<<_rbuf);
	//the expected buffer
	d.rcvdidq.push(_rbuf.id());
	
	bool mustexecute(false);
	
	if(_rbuf.updatesCount()){
		mustexecute = doFreeSentBuffers(_rbuf, _rconid);
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
	return mustexecute || d.mustSendUpdates();
}
//---------------------------------------------------------------------
bool Session::doPushUnxpectedReceivedBuffer(
	Buffer &_rbuf,
	Talker::TalkerStub &_rstub,
	const ConnectionUid &_rconid
){
	vdbgx(Dbg::ipc, "unexpected "<<_rbuf<<" expectedid = "<<d.rcvexpectedid);
	BufCmp	bc;
	bool	mustexecute = false;
	if(d.state == Data::Connecting){
		return false;
	}
	if(bc(_rbuf.id(), d.rcvexpectedid)){
		COLLECT_DATA_0(d.statistics.alreadyReceived);
		//the peer doesnt know that we've already received the buffer
		//add it as update
		d.rcvdidq.push(_rbuf.id());
	}else if(_rbuf.id() <= Buffer::LastBufferId){
		if(_rbuf.updatesCount()){//we have updates
			mustexecute = doFreeSentBuffers(_rbuf, _rconid);
		}
		//try to keep the buffer for future parsing
		uint32 bufid(_rbuf.id());
		if(d.keepOutOfOrderBuffer(_rbuf)){
			vdbgx(Dbg::ipc, "out of order buffer");
			d.rcvdidq.push(bufid);//for peer updates
		}else{
			vdbgx(Dbg::ipc, "too many buffers out-of-order "<<d.outoforderbufcnt);
			COLLECT_DATA_0(d.statistics.tooManyBuffersOutOfOrder);
		}
	}else if(_rbuf.id() == Buffer::UpdateBufferId){//a buffer containing only updates
		mustexecute = doFreeSentBuffers(_rbuf, _rconid);
		doTryScheduleKeepAlive(_rstub);
	}
	return mustexecute || d.mustSendUpdates();
}
//---------------------------------------------------------------------
bool Session::doFreeSentBuffers(const Buffer &_rbuf, const ConnectionUid &_rconid){
	uint32 sz(d.sendbufferfreeposstk.size());
	for(uint32 i(0); i < _rbuf.updatesCount(); ++i){
		d.freeSentBuffer(_rbuf.update(i));
	}
	return (sz == 0) && (d.sendbufferfreeposstk.size());
}
//---------------------------------------------------------------------
void Session::doParseBufferDataType(const char *&_bpos, int &_blen, int _firstblen){
	uint8	datatype(*_bpos);
	
	++_bpos;
	--_blen;
	
	switch(datatype){
		case Buffer::ContinuedSignal:
			vdbgx(Dbg::ipc, "continuedsignal");
			cassert(_blen == _firstblen);
			if(!d.rcvdsignalq.front().psignal){
				d.rcvdsignalq.pop();
			}
			//we cannot have a continued signal on the same buffer
			break;
		case Buffer::NewSignal:
			vdbgx(Dbg::ipc, "newsignal");
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
			vdbgx(Dbg::ipc, "oldsignal");
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
			SignalUid 		siguid(0xffffffff, 0xffffffff);
			SockAddrPair	pairaddr(d.addr);
			if(rrsd.psignal->ipcReceived(siguid, _rconid, pairaddr, baseAddr4()->second)){
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
int Session::doExecuteConnecting(Talker::TalkerStub &_rstub){
	const uint32	bufid(Specific::sizeToId(64));
	Buffer			buf(Specific::popBuffer(bufid), Specific::idToCapacity(bufid));
	
	buf.resetHeader();
	buf.type(Buffer::ConnectingType);
	buf.id(d.sendid);
	d.incrementSendId();
	
	int32			*pi = (int32*)buf.dataEnd();
	
	*pi = htonl(_rstub.basePort());
	buf.dataSize(buf.dataSize() + sizeof(int32));
	
	const uint32			bufidx(d.registerBuffer(buf));
	Data::SendBufferData	&rsbd(d.sendbuffervec[bufidx]);
	cassert(bufidx == 1);
	
	if(_rstub.pushSendBuffer(bufidx, rsbd.buffer.buffer(), rsbd.buffer.bufferSize())){
		//buffer sent - setting a timer for it
		//schedule a timer for this buffer
		TimeSpec tpos(_rstub.currentTime());
		tpos += d.computeRetransmitTimeout(rsbd.buffer.retransmitId(), rsbd.buffer.id());
		
		_rstub.pushTimer(pack32(bufidx, rsbd.uid) , tpos);
		vdbgx(Dbg::ipc, "sent connecting "<<rsbd.buffer<<" done");
	}else{
		COLLECT_DATA_0(d.statistics.sendPending);
		rsbd.sending = 1;
		++d.sendpendingcount;
		vdbgx(Dbg::ipc, "sent connecting "<<rsbd.buffer<<" pending");
	}
	
	d.state = Data::WaitAccept;
	return NOK;
}
//---------------------------------------------------------------------
int Session::doExecuteAccepting(Talker::TalkerStub &_rstub){
	const uint32	bufid(Specific::sizeToId(64));
	Buffer			buf(Specific::popBuffer(bufid), Specific::idToCapacity(bufid));
	
	buf.resetHeader();
	buf.type(Buffer::AcceptingType);
	buf.id(d.sendid);
	d.incrementSendId();
	int32 			*pi = (int32*)buf.dataEnd();
	
	*pi = htonl(_rstub.basePort());
	buf.dataSize(buf.dataSize() + sizeof(int32));
	
	const uint32			bufidx(d.registerBuffer(buf));
	Data::SendBufferData	&rsbd(d.sendbuffervec[bufidx]);
	cassert(bufidx == 1);
	
	if(_rstub.pushSendBuffer(bufidx, rsbd.buffer.buffer(), rsbd.buffer.bufferSize())){
		//buffer sent - setting a timer for it
		//schedule a timer for this buffer
		TimeSpec tpos(_rstub.currentTime());
		tpos += d.computeRetransmitTimeout(rsbd.buffer.retransmitId(), rsbd.buffer.id());
		
		_rstub.pushTimer(pack32(bufidx, rsbd.uid), tpos);
		vdbgx(Dbg::ipc, "sent accepting "<<rsbd.buffer<<" done");
	}else{
		COLLECT_DATA_0(d.statistics.sendPending);
		rsbd.sending = 1;
		++d.sendpendingcount;
		vdbgx(Dbg::ipc, "sent accepting "<<rsbd.buffer<<" pending");
	}
	
	d.state = Data::Connected;
	return OK;
}
//---------------------------------------------------------------------
int Session::doExecuteConnected(Talker::TalkerStub &_rstub){
	vdbgx(Dbg::ipc, ""<<d.sendbufferfreeposstk.size());
	while(d.sendbufferfreeposstk.size()){
		if(
			d.signalq.empty() &&
			d.sendsignalidxq.empty()
		){
			break;
		}
		//we can still send buffers
		Buffer 					buf(Buffer::allocateDataForReading(), Buffer::capacityForReading());
		
		const uint32			bufidx(d.registerBuffer(buf));
		Data::SendBufferData	&rsbd(d.sendbuffervec[bufidx]);
		
		rsbd.buffer.resetHeader();
		rsbd.buffer.type(Buffer::DataType);
		rsbd.buffer.id(d.sendid);
		d.incrementSendId();
		
		while(d.rcvdidq.size() && rsbd.buffer.updatesCount() < 8){
			rsbd.buffer.pushUpdate(d.rcvdidq.front());
			d.rcvdidq.pop();
		}
		COLLECT_DATA_1(d.statistics.sendUpdatesSize, rsbd.buffer.updatesCount());
		
		d.moveSignalsToSendQueue();
		
		doFillSendBuffer(bufidx);
		
		rsbd.buffer.optimize();
		
		d.resetKeepAlive();
		
		if(_rstub.pushSendBuffer(bufidx, rsbd.buffer.buffer(), rsbd.buffer.bufferSize())){
			//buffer sent - setting a timer for it
			//schedule a timer for this buffer
			TimeSpec tpos(_rstub.currentTime());
			tpos += d.computeRetransmitTimeout(rsbd.buffer.retransmitId(), rsbd.buffer.id());
			
			_rstub.pushTimer(pack32(bufidx, rsbd.uid), tpos);
			vdbgx(Dbg::ipc, "sent data "<<rsbd.buffer<<" pending");
		}else{
			COLLECT_DATA_0(d.statistics.sendPending);
			rsbd.sending = 1;
			++d.sendpendingcount;
			vdbgx(Dbg::ipc, "sent data "<<rsbd.buffer<<" pending");
		}
	}
	if(d.rcvdidq.size() && d.updatesbuffer.empty() && d.mustSendUpdates()){
		//send an updates buffer
		const uint32	bufid(Specific::sizeToId(256));
		Buffer			buf(Specific::popBuffer(bufid), Specific::idToCapacity(bufid));
		
		buf.resetHeader();
		buf.type(Buffer::DataType);
		buf.id(Data::UpdateBufferId);
		
		d.resetKeepAlive();
		
		while(d.rcvdidq.size() && buf.updatesCount() < 16){
			buf.pushUpdate(d.rcvdidq.front());
			d.rcvdidq.pop();
		}
		
		buf.optimize(256);
		
		COLLECT_DATA_1(d.statistics.sendOnlyUpdatesSize, buf.updatesCount());
		
		if(_rstub.pushSendBuffer(-1, buf.buffer(), buf.bufferSize())){
			vdbgx(Dbg::ipc, "sent updates "<<buf<<" done");
		}else{
			d.updatesbuffer = buf;
			vdbgx(Dbg::ipc, "sent updates "<<buf<<" pending");
		}
	}
	return NOK;
}
//---------------------------------------------------------------------
void Session::doFillSendBuffer(const uint32 _bufidx){
	Data::SendBufferData	&rsbd(d.sendbuffervec[_bufidx]);
	Data::BinSerializerT	*pser(NULL);
	
	COLLECT_DATA_1(d.statistics.sendSignalIdxQueueSize, d.sendsignalidxq.size());
	
	while(d.sendsignalidxq.size()){
		if(d.currentbuffersignalcount){
			Data::SendSignalData	&rssd(d.sendsignalvec[d.sendsignalidxq.front()]);
			
			if(rssd.pserializer){//a continued signal
				if(d.currentbuffersignalcount == Data::MaxSignalBufferCount){
					vdbgx(Dbg::ipc, "oldsignal");
					rsbd.buffer.dataType(Buffer::OldSignal);
				}else{
					vdbgx(Dbg::ipc, "continuedsignal");
					rsbd.buffer.dataType(Buffer::ContinuedSignal);
				}
			}else{//a new commnad
				vdbgx(Dbg::ipc, "newsignal");
				rsbd.buffer.dataType(Buffer::NewSignal);
				if(pser){
					rssd.pserializer = pser;
					pser = NULL;
				}else{
					rssd.pserializer = d.popSerializer();
				}
				Signal *psig(rssd.signal.ptr());
				rssd.pserializer->push(psig);
			}
			--d.currentbuffersignalcount;
			vdbgx(Dbg::ipc, "d.crtsigbufcnt = "<<d.currentbuffersignalcount);
			
			int rv = rssd.pserializer->run(rsbd.buffer.dataEnd(), rsbd.buffer.dataFreeSize());
			
			cassert(rv >= 0);//TODO: deal with the situation!
			
			rsbd.buffer.dataSize(rsbd.buffer.dataSize() + rv);
			
			if(rssd.pserializer->empty()){//finished with this signal
				vdbgx(Dbg::ipc, "donesignal");
				if(pser) d.pushSerializer(pser);
				pser = rssd.pserializer;
				pser->clear();
				rssd.pserializer = NULL;
				vdbgx(Dbg::ipc, "cached wait signal");
				rsbd.signalidxvec.push_back(d.sendsignalidxq.front());
				d.sendsignalidxq.pop();
				vdbgx(Dbg::ipc, "sendsignalidxq poped"<<d.sendsignalidxq.size());
				//we dont want to switch to an old signal
				d.currentbuffersignalcount = Data::MaxSignalBufferCount - 1;
				if(rsbd.buffer.dataFreeSize() < 16) break;
				break;
			}else{
				break;
			}
		}else if(d.sendsignalidxq.size() == 1){//only one signal
			d.currentbuffersignalcount = Data::MaxSignalBufferCount - 1;
		}else{
			d.sendsignalidxq.push(d.sendsignalidxq.front());
			//d.waitFrontSignal().pser = NULL;
			d.sendsignalidxq.pop();
			vdbgx(Dbg::ipc, "scqpop "<<d.sendsignalidxq.size());
			d.currentbuffersignalcount = Data::MaxSignalBufferCount;
		}
	}//while
	
	if(pser) d.pushSerializer(pser);
}
//---------------------------------------------------------------------
int Session::doExecuteDisconnect(Talker::TalkerStub &_rstub){
	d.state = Data::Disconnected;
	return BAD;
}
//---------------------------------------------------------------------
void Session::doTryScheduleKeepAlive(Talker::TalkerStub &_rstub){
	d.resetKeepAlive();
	COLLECT_DATA_0(d.statistics.tryScheduleKeepAlive);
	vdbgx(Dbg::ipc, "try send keepalive");
	if(d.canSendKeepAlive(_rstub.currentTime())){
		
		COLLECT_DATA_0(d.statistics.scheduleKeepAlive);
		vdbgx(Dbg::ipc, "can send keepalive");
		
		const uint32 			idx(d.keepAliveBufferIndex());
		Data::SendBufferData	&rsbd(d.sendbuffervec[idx]);
		
		++rsbd.uid;
		
		//schedule a timer for this buffer
		TimeSpec tpos(_rstub.currentTime());
		tpos += d.keepalivetimeout;
		
		_rstub.pushTimer(pack32(idx, rsbd.uid), tpos);
	}
}
//======================================================================
namespace{
/*static*/ StaticData& StaticData::instance(){
	//TODO: staticproblem
	static StaticData sd;
	return sd;
}

//----------------------------------------------------------------------
StaticData::StaticData(){
	const uint datsz = StaticData::DataRetransmitCount;
	const uint consz = StaticData::ConnectRetransmitCount;
	const uint sz = (datsz < consz ? consz + 1 : datsz + 1) * 2;
	
	toutvec.reserve(sz);
	toutvec.resize(sz);
	toutvec[0] = 200;
	toutvec[1] = 400;
	toutvec[2] = 800;
	toutvec[3] = 1200;
	toutvec[4] = 1600;
	toutvec[5] = 2000;
	cassert(sz >= 5);
	for(uint i = 6; i < sz; ++i){
		toutvec[i] = 2000;
	}
}

//----------------------------------------------------------------------
ulong StaticData::retransmitTimeout(uint _pos){
	cassert(_pos < toutvec.size());
	return toutvec[_pos];
}
}//namespace
//======================================================================
#ifdef USTATISTICS
namespace {
StatisticData::~StatisticData(){
	rdbgx(Dbg::ipc, "Statistics:\r\n"<<*this);
}
void StatisticData::reconnect(){
	++reconnectcnt;
}
void StatisticData::pushSignal(){
	++pushsignalcnt;
}
void StatisticData::pushReceivedBuffer(){
	++pushreceivedbuffercnt;
}
void StatisticData::retransmitId(const uint16 _cnt){
	if(maxretransmitid < _cnt) maxretransmitid = _cnt;
}
void StatisticData::sendKeepAlive(){
	++sendkeepalivecnt;
}
void StatisticData::sendPending(){
	++sendpendingcnt;
}
void StatisticData::pushExpectedReceivedBuffer(){
	++pushexpectedreceivedbuffercnt;
}
void StatisticData::alreadyReceived(){
	++alreadyreceivedcnt;
}
void StatisticData::tooManyBuffersOutOfOrder(){
	++toomanybuffersoutofordercnt;
}
void StatisticData::sendUpdatesSize(const uint16 _sz){
	if(maxsendupdatessize < _sz) maxsendupdatessize = _sz;
}
void StatisticData::sendOnlyUpdatesSize(const uint16 _sz){
	++sendonlyupdatescnt;
	if(maxsendonlyupdatessize < _sz) maxsendonlyupdatessize = _sz;
}
void StatisticData::sendSignalIdxQueueSize(const ulong _sz){
	if(maxsendsignalidxqueuesize < _sz) maxsendsignalidxqueuesize = _sz;
}
void StatisticData::tryScheduleKeepAlive(){
	++tryschedulekeepalivecnt;
}
void StatisticData::scheduleKeepAlive(){
	++schedulekeepalivecnt;
}
void StatisticData::failedTimeout(){
	++failedtimeoutcnt;
}
void StatisticData::timeout(){
	++timeoutcnt;
}
std::ostream& operator<<(std::ostream &_ros, const StatisticData &_rsd){
	_ros<<"reconnectcnt                  = "<<_rsd.reconnectcnt<<std::endl;
	_ros<<"pushsignalcnt                 = "<<_rsd.pushsignalcnt<<std::endl;
	_ros<<"pushreceivedbuffercnt         = "<<_rsd.pushreceivedbuffercnt<<std::endl;
	_ros<<"maxretransmitid               = "<<_rsd.maxretransmitid<<std::endl;
	_ros<<"sendkeepalivecnt              = "<<_rsd.sendkeepalivecnt<<std::endl;
	_ros<<"sendpendingcnt                = "<<_rsd.sendpendingcnt<<std::endl;
	_ros<<"pushexpectedreceivedbuffercnt = "<<_rsd.pushexpectedreceivedbuffercnt<<std::endl;
	_ros<<"alreadyreceivedcnt            = "<<_rsd.alreadyreceivedcnt<<std::endl;
	_ros<<"toomanybuffersoutofordercnt   = "<<_rsd.toomanybuffersoutofordercnt<<std::endl;
	_ros<<"maxsendupdatessize            = "<<_rsd.maxsendupdatessize<<std::endl;
	_ros<<"sendonlyupdatescnt            = "<<_rsd.sendonlyupdatescnt<<std::endl;
	_ros<<"maxsendonlyupdatessize        = "<<_rsd.maxsendonlyupdatessize<<std::endl;
	_ros<<"maxsendsignalidxqueuesize     = "<<_rsd.maxsendsignalidxqueuesize<<std::endl;
	_ros<<"tryschedulekeepalivecnt       = "<<_rsd.tryschedulekeepalivecnt<<std::endl;
	_ros<<"schedulekeepalivecnt          = "<<_rsd.schedulekeepalivecnt<<std::endl;
	_ros<<"failedtimeoutcnt              = "<<_rsd.failedtimeoutcnt<<std::endl;
	_ros<<"timeoutcnt                    = "<<_rsd.timeoutcnt<<std::endl;
	return _ros;
}
}//namespace
#endif
}//namespace ipc
}//namespace foundation
