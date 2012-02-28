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

#include "system/exception.hpp"
#include "system/debug.hpp"
#include "system/socketaddress.hpp"
#include "system/specific.hpp"
#include "system/thread.hpp"
#include "utility/queue.hpp"
#include "algorithm/serialization/binary.hpp"
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
		return overflow_safe_less(id2, id1);
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
	void sendAsynchronousWhileSynchronous();
	void sendSynchronousWhileSynchronous(ulong _sz);
	void sendAsynchronous();
	void failedDecompression();
	void sendUncompressed(ulong _sz);
	void sendCompressed(ulong _sz);
	
	ulong	reconnectcnt;
	ulong	pushsignalcnt;
	ulong	pushreceivedbuffercnt;
	ulong	 maxretransmitid;
	ulong	sendkeepalivecnt;
	ulong	sendpendingcnt;
	ulong	pushexpectedreceivedbuffercnt;
	ulong	alreadyreceivedcnt;
	ulong	toomanybuffersoutofordercnt;
	ulong	maxsendupdatessize;
	ulong	sendonlyupdatescnt;
	ulong	maxsendonlyupdatessize;
	ulong	maxsendsignalidxqueuesize;
	ulong	tryschedulekeepalivecnt;
	ulong	schedulekeepalivecnt;
	ulong	failedtimeoutcnt;
	ulong	timeoutcnt;
	ulong	sendasynchronouswhilesynchronous;
	ulong	sendsynchronouswhilesynchronous;
	ulong	maxsendsynchronouswhilesynchronous;
	ulong	sendasynchronous;
	ulong	faileddecompressions;
	uint64	senduncompressed;
	uint64	sendcompressed;
};

std::ostream& operator<<(std::ostream &_ros, const StatisticData &_rsd);
}//namespace
#endif

//== Session::Data ====================================================
typedef DynamicPointer<foundation::Signal>			DynamicSignalPointerT;

struct Session::Data{
	enum States{
		Connecting = 0,//Connecting must be first see isConnecting
		Accepting,
		WaitAccept,
		Authenticating,
		Connected,
		WaitDisconnecting,
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
	struct BinSerializer:serialization::binary::Serializer{
		BinSerializer(
			const serialization::TypeMapperBase &_rtmb
		):serialization::binary::Serializer(_rtmb){}
		BinSerializer(
		){}
		static unsigned specificCount(){return MaxSendSignalQueueSize;}
		void specificRelease(){}
	};

	struct BinDeserializer:serialization::binary::Deserializer{
		BinDeserializer(
			const serialization::TypeMapperBase &_rtmb
		):serialization::binary::Deserializer(_rtmb){}
		BinDeserializer(
		){}
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
			const SerializationTypeIdT	&_rtid,
			uint16 _bufid,
			uint16 _flags,
			uint32 _id
		):signal(_rsig), tid(_rtid), pserializer(NULL), bufid(_bufid), flags(_flags), id(_id), uid(0){}
		~SendSignalData(){
			//cassert(!pserializer);
		}
		bool operator<(const SendSignalData &_owc)const{
			if(signal.ptr()){
				if(_owc.signal.ptr()){
				}else return true;
			}else return false;
			
			return overflow_safe_less(id, _owc.id);
		}
		DynamicSignalPointerT	signal;
		SerializationTypeIdT	tid;
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
		const SocketAddressPair*,
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
	struct SignalStub{
		SignalStub():tid(SERIALIZATION_INVALIDID),flags(0){}
		SignalStub(
			const DynamicPointer<Signal>	&_rsig,
			const SerializationTypeIdT &_rtid,
			uint32 _flags
		):signal(_rsig), tid(_rtid), flags(_flags){}
		DynamicPointer<Signal>	signal;
		SerializationTypeIdT	tid;
		uint32					flags;
	};
	typedef Queue<SignalStub>					SignalQueueT;
public:
	Data(
		const SocketAddressPair4 &_raddr,
		uint32 _keepalivetout
	);
	
	Data(
		const SocketAddressPair4 &_raddr,
		int _baseport,
		uint32 _keepalivetout
	);
	
	~Data();
	
	BinSerializerT* popSerializer(const serialization::TypeMapperBase &_rtmb){
		BinSerializerT* p = Specific::uncache<BinSerializerT>();
		p->typeMapper(_rtmb);
		return p;
	}

	void pushSerializer(BinSerializerT* _p){
		cassert(_p->empty());
		Specific::cache(_p);
	}

	BinDeserializerT* popDeserializer(const serialization::TypeMapperBase &_rtmb){
		BinDeserializerT* p = Specific::uncache<BinDeserializerT>();
		p->typeMapper(_rtmb);
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
		const SerializationTypeIdT &_rtid,
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
	void moveAuthSignalsToSendQueue();
	
	void pushSignalToSendQueue(
		DynamicPointer<Signal> &_sigptr,
		const uint32 _flags,
		const SerializationTypeIdT _tid
	);
	void resetKeepAlive();
	//returns false if there is no other signal but the current one
	bool moveToNextSendSignal();
public:
	SocketAddress4			addr;
	SocketAddressPair		pairaddr;
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
	uint32					currentsyncid;
	uint32					currentsendsyncid;
	
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
	const SocketAddressPair4 &_raddr,
	uint32 _keepalivetout
):	addr(_raddr), pairaddr(addr), 
	baseaddr(&pairaddr, addr.port()),
	rcvexpectedid(2), state(Connecting), sendpendingcount(0),
	outoforderbufcnt(0), sentsignalwaitresponse(0),
	retansmittimepos(0), sendsignalid(0), 
	currentbuffersignalcount(MaxSignalBufferCount), sendid(1),
	keepalivetimeout(_keepalivetout),
	currentsendsyncid(-1)
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
	const SocketAddressPair4 &_raddr,
	int _baseport,
	uint32 _keepalivetout
):	addr(_raddr), pairaddr(addr), baseaddr(&pairaddr, _baseport),
	rcvexpectedid(2), state(Accepting), sendpendingcount(0),
	outoforderbufcnt(0), sentsignalwaitresponse(0),
	retansmittimepos(0), sendsignalid(0), 
	currentbuffersignalcount(MaxSignalBufferCount), sendid(1),
	keepalivetimeout(_keepalivetout),
	currentsendsyncid(-1)
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
	Context::the().sigctx.signaluid.idx = 0xffffffff;
	Context::the().sigctx.signaluid.uid = 0xffffffff;
	while(signalq.size()){
		signalq.front().signal->ipcFail(0);
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
		Context::the().sigctx.signaluid.idx = it - sendsignalvec.begin();
		Context::the().sigctx.signaluid.uid = rssd.uid;
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
		rssd.tid = SERIALIZATION_INVALIDID;
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
	const SerializationTypeIdT &_rtid,
	uint16 _bufid,
	uint16 _flags,
	uint32 _id
){
	_flags &= ~Service::SentFlag;
//	_flags &= ~Service::WaitResponseFlag;
	
	if(sendsignalfreeposstk.size()){
		const uint32	idx(sendsignalfreeposstk.top());
		SendSignalData	&rssd(sendsignalvec[idx]);
		
		sendsignalfreeposstk.pop();
		
		rssd.bufid = _bufid;
		cassert(!rssd.signal.ptr());
		rssd.signal = _sig;
		rssd.tid = _rtid;
		
		//rssd.signal.context().waitid = SignalUid(idx, rssd.uid);
		cassert(!_sig.ptr());
		rssd.flags = _flags;
		rssd.id = _id;
		
		//return rssd.signal.context().waitid;
		return SignalUid(idx, rssd.uid);
	}else{
		
		sendsignalvec.push_back(SendSignalData(_sig, _rtid, _bufid, _flags, _id));
		cassert(!_sig.ptr());
		//sendsignalvec.back().signal.context().waitid = SignalUid(sendsignalvec.size() - 1, 0);
		//return sendsignalvec.back().signal.context().waitid;
		return SignalUid(sendsignalvec.size() - 1, 0);
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
			popSentWaitSignal(_rsbd.signalidxvec[3]);
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
		Context::the().sigctx.signaluid.idx = _rsiguid.idx;
		Context::the().sigctx.signaluid.uid = rssd.uid;
		++rssd.uid;
		rssd.signal->ipcSuccess();
		rssd.signal.clear();
		sendsignalfreeposstk.push(_rsiguid.idx);
		--sentsignalwaitresponse;
	}
}
//---------------------------------------------------------------------
void Session::Data::popSentWaitSignal(const uint32 _idx){
	idbgx(Dbg::ipc, ""<<_idx);
	SendSignalData &rssd(sendsignalvec[_idx]);
	cassert(rssd.signal.ptr());
	if(rssd.flags & Service::DisconnectAfterSendFlag){
		idbgx(Dbg::ipc, "DisconnectAfterSendFlag - disconnecting");
		++rssd.uid;
		rssd.signal.clear();
		sendsignalfreeposstk.push(_idx);
		this->state = Disconnecting;
	}
	if(rssd.flags & Service::WaitResponseFlag){
		//let it leave a little longer
		idbgx(Dbg::ipc, "signal waits for response "<<_idx<<' '<<rssd.uid);
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
		pushSignalToSendQueue(
			signalq.front().signal, 
			signalq.front().flags,
			signalq.front().tid
		);
		signalq.pop();
	}
}
void Session::Data::moveAuthSignalsToSendQueue(){
	size_t qsz = signalq.size();
	while(qsz){
		if(
			((signalq.front().flags & Service::AuthenticationFlag) != 0) &&
			sendsignalidxq.size() < Data::MaxSendSignalQueueSize
		){
			pushSignalToSendQueue(
				signalq.front().signal, 
				signalq.front().flags,
				signalq.front().tid
			);
			signalq.pop();
		}else{
			signalq.push(signalq.front());
			signalq.pop();
		}
		--qsz;
	}
}
void Session::Data::pushSignalToSendQueue(
	DynamicPointer<Signal> &_sigptr,
	const uint32 _flags,
	const SerializationTypeIdT _tid
){
		cassert(_sigptr.ptr());
		const SignalUid	uid(pushSendWaitSignal(_sigptr, _tid, 0, _flags, sendsignalid++));
		SendSignalData 	&rssd(sendsignalvec[uid.idx]);
		
		Context::the().sigctx.signaluid = uid;
		
		sendsignalidxq.push(uid.idx);
		
		const uint32 tmp_flgs(rssd.signal->ipcPrepare());
		rssd.flags |= tmp_flgs;
		vdbgx(Dbg::ipc, "flags = "<<(rssd.flags&Service::SentFlag)<<" tmpflgs = "<<(tmp_flgs & Service::SentFlag));
		
		if(tmp_flgs & Service::WaitResponseFlag){
			++sentsignalwaitresponse;
		}else{
			rssd.flags &= ~Service::WaitResponseFlag;
		}

}
//----------------------------------------------------------------------
void Session::Data::resetKeepAlive(){
	SendBufferData	&rsbd(sendbuffervec[0]);//the keep alive buffer
	++rsbd.uid;
}
//----------------------------------------------------------------------
bool Session::Data::moveToNextSendSignal(){
	
	if(sendsignalidxq.size() == 1) return false;
	
	uint32 	crtidx(sendsignalidxq.front());
	
	sendsignalidxq.pop();
	sendsignalidxq.push(crtidx);

	if(currentsendsyncid != static_cast<uint32>(-1) && crtidx != currentsendsyncid){
		cassert(!(sendsignalvec[crtidx].flags & Service::SynchronousSendFlag));
		crtidx = currentsendsyncid;
	}
	
	Data::SendSignalData	&rssd(sendsignalvec[crtidx]);
	
	vdbgx(Dbg::ipc, "flags = "<<(rssd.flags&Service::SentFlag));
	
	if(rssd.flags & Service::SynchronousSendFlag){
		currentsendsyncid = crtidx;
		uint32 tmpidx(sendsignalidxq.front());
		while(tmpidx != crtidx){
			Data::SendSignalData	&tmprssd(sendsignalvec[tmpidx]);
			if(!(tmprssd.flags & Service::SynchronousSendFlag)){
				COLLECT_DATA_0(statistics.sendAsynchronousWhileSynchronous);
				return true;
			}
			sendsignalidxq.pop();
			sendsignalidxq.push(tmpidx);
			tmpidx = sendsignalidxq.front();
		}
		COLLECT_DATA_1(statistics.sendSynchronousWhileSynchronous, sendsignalidxq.size());
		return false;
	}else{
		COLLECT_DATA_0(statistics.sendAsynchronous);
		return true;
	}
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
	const SocketAddressPair4 &_raddr,
	uint32 _keepalivetout
):d(*(new Data(_raddr, _keepalivetout))){
	vdbgx(Dbg::ipc, "Created connect session "<<(void*)this);
}
//---------------------------------------------------------------------
Session::Session(
	const SocketAddressPair4 &_raddr,
	int _basport,
	uint32 _keepalivetout
):d(*(new Data(_raddr, _basport, _keepalivetout))){
	vdbgx(Dbg::ipc, "Created accept session "<<(void*)this);
}
//---------------------------------------------------------------------
Session::~Session(){
	vdbgx(Dbg::ipc, "delete session "<<(void*)this);
	delete &d;
}
//---------------------------------------------------------------------
const SocketAddressPair4* Session::peerAddr4()const{
	const SocketAddressPair *p = &(d.pairaddr);
	return reinterpret_cast<const SocketAddressPair4*>(p);
}
//---------------------------------------------------------------------
const Session::Addr4PairT* Session::baseAddr4()const{
	const Data::BaseAddrT *p = &(d.baseaddr);
	return reinterpret_cast<const std::pair<const SocketAddressPair4*, int>*>(p);
}
//---------------------------------------------------------------------
const SocketAddressPair* Session::peerSockAddr()const{
	const SocketAddressPair *p = &(d.pairaddr);
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
bool Session::isDisconnected()const{
	return d.state == Data::Disconnected;
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
		Specific::popBuffer(Specific::sizeToIndex(Buffer::minSize())),
		Specific::indexToCapacity(Specific::sizeToIndex(Buffer::minSize()))
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
		if(!(d.signalq.front().flags & Service::SameConnectorFlag)) {
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
	bool mustdisconnect = false;
	//see which sent/sending signals must be cleard
	for(Data::SendSignalVectorT::iterator it(d.sendsignalvec.begin()); it != d.sendsignalvec.end(); ++it){
		Data::SendSignalData &rssd(*it);
		vdbgx(Dbg::ipc, "pos = "<<(it - d.sendsignalvec.begin())<<" flags = "<<(rssd.flags&Service::SentFlag));
		Context::the().sigctx.signaluid.idx = it - d.sendsignalvec.begin();
		Context::the().sigctx.signaluid.uid = rssd.uid;
		
		if(rssd.pserializer){
			rssd.pserializer->clear();
			d.pushSerializer(rssd.pserializer);
			rssd.pserializer = NULL;
		}
		if(!rssd.signal) continue;
		
		if((rssd.flags & Service::DisconnectAfterSendFlag) != 0){
			idbgx(Dbg::ipc, "DisconnectAfterSendFlag");
			mustdisconnect = true;
		}
		
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
			d.signalq.push(Data::SignalStub(rssd.signal, rssd.tid, rssd.flags));
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
	d.state = Data::Disconnecting;
}
//---------------------------------------------------------------------
int Session::pushSignal(
	DynamicPointer<Signal> &_rsig,
	const SerializationTypeIdT &_rtid,
	uint32 _flags
){
	COLLECT_DATA_0(d.statistics.pushSignal);
	d.signalq.push(Data::SignalStub(_rsig, _rtid, _flags));
	if(d.signalq.size() == 1) return NOK;
	return OK;
}
//---------------------------------------------------------------------
bool Session::pushReceivedBuffer(
	Buffer &_rbuf,
	Talker::TalkerStub &_rstub/*,
	const ConnectionUid &_rconuid*/
){
	COLLECT_DATA_0(d.statistics.pushReceivedBuffer);
	d.rcvtimepos = _rstub.currentTime();
	d.resetKeepAlive();
	if(!_rbuf.decompress(_rstub.service().controller())){
		_rbuf.clear();//silently drop invalid buffer
		COLLECT_DATA_0(d.statistics.failedDecompression);
		return false;
	}
	if(_rbuf.id() == d.rcvexpectedid){
		return doPushExpectedReceivedBuffer(_rstub, _rbuf/*, _rconuid*/);
	}else{
		return doPushUnxpectedReceivedBuffer(_rstub, _rbuf/*, _rconuid*/);
	}
}
//---------------------------------------------------------------------
void Session::completeConnect(
	Talker::TalkerStub &_rstub,
	int _pairport
){
	if(d.state == Data::Connected) return;
	if(d.state == Data::Authenticating) return;
	
	d.addr.port(_pairport);
	
	Controller &rctrl = _rstub.service().controller();
	
	if(!rctrl.hasAuthentication()){
		d.state = Data::Connected;
	}else{
		DynamicPointer<Signal>	sigptr;
		SignalUid 				siguid(0xffffffff, 0xffffffff);
		uint32					flags = 0;
		SerializationTypeIdT 	tid = SERIALIZATION_INVALIDID;
		const int				authrv = rctrl.authenticate(sigptr, siguid, flags, tid);
		switch(authrv){
			case BAD:
				if(sigptr.ptr()){
					d.state = Data::WaitDisconnecting;
					d.keepalivetimeout = 0;
					flags |= Service::DisconnectAfterSendFlag;
					d.pushSignalToSendQueue(
						sigptr,
						flags,
						tid
					);
				}else{
					d.state = Data::Disconnecting;
				}
				break;
			case OK:
				d.state = Data::Connected;
				d.keepalivetimeout = _rstub.service().keepAliveTimeout();
				break;
			case NOK:
				d.state = Data::Authenticating;
				d.keepalivetimeout = 1000;
				d.pushSignalToSendQueue(
					sigptr,
					flags,
					tid
				);
				flags |= Service::WaitResponseFlag;
				break;
			default:
				THROW_EXCEPTION_EX("Invalid return value for authenticate", authrv);
		}
	}
	
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
		}else if(d.state == Data::Authenticating || d.state == Data::Connected){
			d.state = Data::Reconnecting;
			return true;
		}else{
			d.state = Data::Disconnecting;
			return true;//disconnecting
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
		case Data::Authenticating:
			return doExecuteConnectedLimited(_rstub);
		case Data::Connected:
			return doExecuteConnected(_rstub);
		case Data::WaitDisconnecting:
			return doExecuteConnectedLimited(_rstub);
		case Data::Disconnecting:
			return doExecuteDisconnecting(_rstub);
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
		d.clearSentBuffer(_id);
		if(d.state == Data::Disconnecting){
			b = true;
		}
		return b;
	}
	
	//schedule a timer for this buffer
	TimeSpec tpos(_rstub.currentTime());
	tpos += d.computeRetransmitTimeout(rsbd.buffer.retransmitId(), rsbd.buffer.id());
	
	_rstub.pushTimer(pack32(_id, rsbd.uid), tpos);
	return b;
}
//---------------------------------------------------------------------
bool Session::doPushExpectedReceivedBuffer(
	Talker::TalkerStub &_rstub,
	Buffer &_rbuf/*,
	const ConnectionUid &_rconid*/
){
	COLLECT_DATA_0(d.statistics.pushExpectedReceivedBuffer);
	vdbgx(Dbg::ipc, "expected "<<_rbuf);
	//the expected buffer
	d.rcvdidq.push(_rbuf.id());
	
	bool mustexecute(false);
	
	if(_rbuf.updatesCount()){
		mustexecute = doFreeSentBuffers(_rbuf/*, _rconid*/);
	}
	
	if(d.state == Data::Authenticating || d.state == Data::Connected){
	}else{
		return true;
	}
	
	if(_rbuf.type() == Buffer::DataType){
		doParseBuffer(_rstub, _rbuf/*, _rconid*/);
	}
	
	d.incrementExpectedId();//move to the next buffer
	//while(d.inbufq.top().id() == d.expectedid){
	Buffer	b;
	while(d.moveToNextOutOfOrderBuffer(b)){
		//this is already done on receive
		if(_rbuf.type() == Buffer::DataType){
			doParseBuffer(_rstub, b/*, _rconid*/);
		}
	}
	return mustexecute || d.mustSendUpdates();
}
//---------------------------------------------------------------------
bool Session::doPushUnxpectedReceivedBuffer(
	Talker::TalkerStub &_rstub,
	Buffer &_rbuf
	/*,
	const ConnectionUid &_rconid*/
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
			mustexecute = doFreeSentBuffers(_rbuf/*, _rconid*/);
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
		mustexecute = doFreeSentBuffers(_rbuf/*, _rconid*/);
		if(d.state == Data::Disconnecting){
			return true;
		}
		doTryScheduleKeepAlive(_rstub);
	}
	return mustexecute || d.mustSendUpdates();
}
//---------------------------------------------------------------------
bool Session::doFreeSentBuffers(const Buffer &_rbuf/*, const ConnectionUid &_rconid*/){
	uint32 sz(d.sendbufferfreeposstk.size());
	for(uint32 i(0); i < _rbuf.updatesCount(); ++i){
		d.freeSentBuffer(_rbuf.update(i));
	}
	return (sz == 0) && (d.sendbufferfreeposstk.size());
}
//---------------------------------------------------------------------
void Session::doParseBufferDataType(Talker::TalkerStub &_rstub, const char *&_bpos, int &_blen, int _firstblen){
	uint8				datatype(*_bpos);
	CRCValue<uint8>		crcval(CRCValue<uint8>::check_and_create(datatype));
	
	if(!crcval.ok()){
		THROW_EXCEPTION("Deserialization error");
		cassert(false);
	}
	
	++_bpos;
	--_blen;
	
	switch(crcval.value()){
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
				rrsd.pdeserializer = d.popDeserializer(_rstub.service().typeMapperBase());
				rrsd.pdeserializer->push(rrsd.psignal);
			}else{
				idbgx(Dbg::ipc, "switch to new rcq.size = 0");
				d.rcvdsignalq.push(Data::RecvSignalData(NULL, d.popDeserializer(_rstub.service().typeMapperBase())));
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
			THROW_EXCEPTION("Deserialization error");
			cassert(false);
		}
	}
}
//---------------------------------------------------------------------
void Session::doParseBuffer(Talker::TalkerStub &_rstub, const Buffer &_rbuf/*, const ConnectionUid &_rconid*/){
	const char *bpos(_rbuf.data());
	int			blen(_rbuf.dataSize());
	int			rv;
	int 		firstblen(blen - 1);
	
	idbgx(Dbg::ipc, "bufferid = "<<_rbuf.id());
	
	while(blen > 2){
		
		idbgx(Dbg::ipc, "blen = "<<blen<<" bpos "<<(bpos - _rbuf.data()));
		
		doParseBufferDataType(_rstub, bpos, blen, firstblen);
		
		Data::RecvSignalData &rrsd(d.rcvdsignalq.front());
		
		rv = rrsd.pdeserializer->run(bpos, blen);
		
		if(rv < 0){
			THROW_EXCEPTION_EX("Deserialization error", rrsd.pdeserializer->error());
			cassert(false);
		}
		
		blen -= rv;
		bpos += rv;
		
		if(rrsd.pdeserializer->empty()){//done one signal.
			SignalUid 				siguid(0xffffffff, 0xffffffff);
			Controller				&rctrl = _rstub.service().controller();
			Signal					*psignal = rrsd.psignal;
			rrsd.psignal = NULL;
			
			if(d.state == Data::Connected){
				//rrsd.psignal->ipcReceived(siguid);
				if(!rctrl.receive(psignal, siguid)){
					d.state = Data::Disconnecting;
				}
			}else if(d.state == Data::Authenticating){
				DynamicPointer<Signal>	sigptr(psignal);
				uint32					flags = 0;
				SerializationTypeIdT 	tid = SERIALIZATION_INVALIDID;
				const int 				authrv = rctrl.authenticate(sigptr, siguid, flags, tid);
				switch(authrv){
					case BAD:
						if(sigptr.ptr()){
							d.state = Data::WaitDisconnecting;
							d.keepalivetimeout = 0;
							flags |= Service::DisconnectAfterSendFlag;
							d.pushSignalToSendQueue(
								sigptr,
								flags,
								tid
							);
						}else{
							d.state = Data::Disconnecting;
						}
						break;
					case OK:
						d.state = Data::Connected;
						d.keepalivetimeout = _rstub.service().keepAliveTimeout();
						if(sigptr.ptr()){
							flags |= Service::WaitResponseFlag;
							d.pushSignalToSendQueue(sigptr, flags, tid);
						}
						break;
					case NOK:
						if(sigptr.ptr()){
							flags |= Service::WaitResponseFlag;
							d.pushSignalToSendQueue(sigptr, flags, tid);
						}
						break;
					default:
						THROW_EXCEPTION_EX("Invalid return value for authenticate ", authrv);
				}
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
	const uint32	bufid(Specific::sizeToIndex(64));
	Buffer			buf(Specific::popBuffer(bufid), Specific::indexToCapacity(bufid));
	
	buf.resetHeader();
	buf.type(Buffer::ConnectingType);
	buf.id(d.sendid);
	d.incrementSendId();
	
	int32					*pi = (int32*)buf.dataEnd();
	
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
	const uint32	bufid(Specific::sizeToIndex(64));
	Buffer			buf(Specific::popBuffer(bufid), Specific::indexToCapacity(bufid));
	
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
	
	Controller &rctrl = _rstub.service().controller();
	if(!rctrl.hasAuthentication()){
		d.state = Data::Connected;
	}else{
		d.state = Data::Authenticating;
	}
	return OK;
}
//---------------------------------------------------------------------
int Session::doTrySendUpdates(Talker::TalkerStub &_rstub){
	if(d.rcvdidq.size() && d.updatesbuffer.empty() && d.mustSendUpdates()){
		//send an updates buffer
		const uint32	bufid(Specific::sizeToIndex(256));
		Buffer			buf(Specific::popBuffer(bufid), Specific::indexToCapacity(bufid));
		
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
			return NOK;
		}
		return OK;
	}
	return BAD;
}
//---------------------------------------------------------------------
int Session::doExecuteConnectedLimited(Talker::TalkerStub &_rstub){
	vdbgx(Dbg::ipc, ""<<d.sendbufferfreeposstk.size());
	Controller 	&rctrl = _rstub.service().controller();
	
	while(d.sendbufferfreeposstk.size()){
		if(d.state == Data::Authenticating){
			d.moveAuthSignalsToSendQueue();
		}
		if(
			d.sendsignalidxq.empty()
		){
			break;
		}
		
		//we can still send buffers
		Buffer 					buf(Buffer::allocateDataForReading(), Buffer::ReadCapacity);
		
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
		
		doFillSendBuffer(_rstub, bufidx);
		
		COLLECT_DATA_1(d.statistics.sendUncompressed, rsbd.buffer.bufferSize());
		
		rsbd.buffer.compress(rctrl);
		
		COLLECT_DATA_1(d.statistics.sendCompressed, rsbd.buffer.bufferSize());
		
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
	doTrySendUpdates(_rstub);
	return NOK;
}
//---------------------------------------------------------------------
int Session::doExecuteConnected(Talker::TalkerStub &_rstub){
	vdbgx(Dbg::ipc, ""<<d.sendbufferfreeposstk.size());
	Controller 	&rctrl = _rstub.service().controller();
	
	while(d.sendbufferfreeposstk.size()){
		if(
			d.signalq.empty() &&
			d.sendsignalidxq.empty()
		){
			break;
		}
		
		//we can still send buffers
		Buffer 					buf(Buffer::allocateDataForReading(), Buffer::ReadCapacity);
		
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
		
		doFillSendBuffer(_rstub, bufidx);
		
		COLLECT_DATA_1(d.statistics.sendUncompressed, rsbd.buffer.bufferSize());
		
		rsbd.buffer.compress(rctrl);
		
		COLLECT_DATA_1(d.statistics.sendCompressed, rsbd.buffer.bufferSize());
		
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
	doTrySendUpdates(_rstub);
	return NOK;
}
//---------------------------------------------------------------------
void Session::doFillSendBuffer(Talker::TalkerStub &_rstub, const uint32 _bufidx){
	Data::SendBufferData	&rsbd(d.sendbuffervec[_bufidx]);
	Data::BinSerializerT	*pser(NULL);
	Controller 	&rctrl = _rstub.service().controller();
	COLLECT_DATA_1(d.statistics.sendSignalIdxQueueSize, d.sendsignalidxq.size());
	
	while(d.sendsignalidxq.size()){
		if(d.currentbuffersignalcount){
			Data::SendSignalData	&rssd(d.sendsignalvec[d.sendsignalidxq.front()]);
			
			Context::the().sigctx.signaluid.idx = d.sendsignalidxq.front();
			Context::the().sigctx.signaluid.uid = rssd.uid;
			
			if(rssd.pserializer){//a continued signal
				if(d.currentbuffersignalcount == Data::MaxSignalBufferCount){
					vdbgx(Dbg::ipc, "oldsignal data size "<<rsbd.buffer.dataSize());
					rsbd.buffer.dataType(Buffer::OldSignal);
				}else{
					vdbgx(Dbg::ipc, "continuedsignal data size "<<rsbd.buffer.dataSize());
					rsbd.buffer.dataType(Buffer::ContinuedSignal);
				}
			}else{//a new commnad
				vdbgx(Dbg::ipc, "newsignal data size "<<rsbd.buffer.dataSize());
				rsbd.buffer.dataType(Buffer::NewSignal);
				if(pser){
					rssd.pserializer = pser;
					pser = NULL;
				}else{
					rssd.pserializer = d.popSerializer(_rstub.service().typeMapperBase());
				}
				Signal *psig(rssd.signal.ptr());
				if(rssd.tid == SERIALIZATION_INVALIDID){
					rssd.pserializer->push(psig,"signal");
				}else{
					rssd.pserializer->push(psig,Service::the().typeMapper(), rssd.tid, "signal");
				}	
			}
			--d.currentbuffersignalcount;
			
			const uint32 tofill = rsbd.buffer.dataFreeSize() - rctrl.reservedDataSize();
			
			int rv = rssd.pserializer->run(rsbd.buffer.dataEnd(), tofill);
			
			vdbgx(Dbg::ipc, "d.crtsigbufcnt = "<<d.currentbuffersignalcount<<" serialized len = "<<rv);
			
			if(rv < 0){
				THROW_EXCEPTION("Serialization error");
				cassert(false);
			}
			if(rv > tofill){
				THROW_EXCEPTION_EX("Serialization error: invalid return value", tofill);
				cassert(false);
			}
			
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
				if(rssd.flags & Service::SynchronousSendFlag){
					d.currentsendsyncid = -1;
				}
				vdbgx(Dbg::ipc, "sendsignalidxq poped "<<d.sendsignalidxq.size());
				d.currentbuffersignalcount = Data::MaxSignalBufferCount;
				if(rsbd.buffer.dataFreeSize() < (rctrl.reservedDataSize() + 16)){
					break;
				}
			}else{
				break;
			}
			
		}else if(d.moveToNextSendSignal()){
			vdbgx(Dbg::ipc, "scqpop "<<d.sendsignalidxq.size());
			d.currentbuffersignalcount = Data::MaxSignalBufferCount;
		}else{//we continue sending the current signal
			d.currentbuffersignalcount = Data::MaxSignalBufferCount - 1;
		}
	}//while
	
	if(pser) d.pushSerializer(pser);
}
//---------------------------------------------------------------------
int Session::doExecuteDisconnecting(Talker::TalkerStub &_rstub){
	//d.state = Data::Disconnected;
	int rv;
	while((rv = doTrySendUpdates(_rstub)) == OK){
	}
	if(rv == BAD){
		if(d.state == Data::WaitDisconnecting){
			return NOK;
		}
		d.state = Data::Disconnected;
	}
	return rv;
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
void Session::prepareContext(Context &_rctx){
	_rctx.sigctx.pairaddr = d.addr;
	_rctx.sigctx.baseport = baseAddr4()->second;
}
//======================================================================
namespace{
#ifdef HAVE_SAFE_STATIC
/*static*/ StaticData& StaticData::instance(){
	static StaticData sd;
	return sd;
}
#else
StaticData& static_data_instance(){
	static StaticData sd;
	return sd;
}
void once_static_data(){
	static_data_instance();
}
/*static*/ StaticData& StaticData::instance(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_static_data, once);
	return static_data_instance();
}
#endif
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

//----------------------------------------------------------------------
//----------------------------------------------------------------------
namespace{
#ifdef HAVE_SAFE_STATIC
const uint32 specificId(){
	static const uint32 id(Thread::specificId());
	return id;
}
#else
uint32 specificIdStub(){
	static const uint32 id(Thread::specificId());
	return id;
}
void once_stub(){
	specificIdStub();
}

const uint32 specificId(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_stub, once);
	return specificIdStub();
}

#endif
}
/*static*/ Context& Context::the(){
	return *reinterpret_cast<Context*>(Thread::specific(specificId()));
}
//----------------------------------------------------------------------
Context::Context(uint32 _tkrid):sigctx(_tkrid){
	Thread::specific(specificId(), this);
}
//----------------------------------------------------------------------
Context::~Context(){
	//Thread::specific(specificId(), NULL);
}
//----------------------------------------------------------------------
//----------------------------------------------------------------------
/*static*/ const SignalContext& SignalContext::the(){
	return Context::the().sigctx;
}


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
void StatisticData::sendAsynchronousWhileSynchronous(){
	++sendasynchronouswhilesynchronous;
}
void StatisticData::sendSynchronousWhileSynchronous(ulong _sz){
	++sendsynchronouswhilesynchronous;
	if(maxsendsynchronouswhilesynchronous < _sz){
		maxsendsynchronouswhilesynchronous = _sz;
	}
}
void StatisticData::sendAsynchronous(){
	++sendasynchronous;
}

void StatisticData::failedDecompression(){
	++faileddecompressions;
}

void StatisticData::sendUncompressed(ulong _sz){
	senduncompressed += _sz;
}

void StatisticData::sendCompressed(ulong _sz){
	sendcompressed += _sz;
}

std::ostream& operator<<(std::ostream &_ros, const StatisticData &_rsd){
	_ros<<"reconnectcnt                         = "<<_rsd.reconnectcnt<<std::endl;
	_ros<<"pushsignalcnt                        = "<<_rsd.pushsignalcnt<<std::endl;
	_ros<<"pushreceivedbuffercnt                = "<<_rsd.pushreceivedbuffercnt<<std::endl;
	_ros<<"maxretransmitid                      = "<<_rsd.maxretransmitid<<std::endl;
	_ros<<"sendkeepalivecnt                     = "<<_rsd.sendkeepalivecnt<<std::endl;
	_ros<<"sendpendingcnt                       = "<<_rsd.sendpendingcnt<<std::endl;
	_ros<<"pushexpectedreceivedbuffercnt        = "<<_rsd.pushexpectedreceivedbuffercnt<<std::endl;
	_ros<<"alreadyreceivedcnt                   = "<<_rsd.alreadyreceivedcnt<<std::endl;
	_ros<<"toomanybuffersoutofordercnt          = "<<_rsd.toomanybuffersoutofordercnt<<std::endl;
	_ros<<"maxsendupdatessize                   = "<<_rsd.maxsendupdatessize<<std::endl;
	_ros<<"sendonlyupdatescnt                   = "<<_rsd.sendonlyupdatescnt<<std::endl;
	_ros<<"maxsendonlyupdatessize               = "<<_rsd.maxsendonlyupdatessize<<std::endl;
	_ros<<"maxsendsignalidxqueuesize            = "<<_rsd.maxsendsignalidxqueuesize<<std::endl;
	_ros<<"tryschedulekeepalivecnt              = "<<_rsd.tryschedulekeepalivecnt<<std::endl;
	_ros<<"schedulekeepalivecnt                 = "<<_rsd.schedulekeepalivecnt<<std::endl;
	_ros<<"failedtimeoutcnt                     = "<<_rsd.failedtimeoutcnt<<std::endl;
	_ros<<"timeoutcnt                           = "<<_rsd.timeoutcnt<<std::endl;
	_ros<<"sendasynchronouswhilesynchronous     = "<<_rsd.sendasynchronouswhilesynchronous<<std::endl;
	_ros<<"sendsynchronouswhilesynchronous      = "<<_rsd.sendsynchronouswhilesynchronous<<std::endl;
	_ros<<"maxsendsynchronouswhilesynchronous   = "<<_rsd.maxsendsynchronouswhilesynchronous<<std::endl;
	_ros<<"sendasynchronous                     = "<<_rsd.sendasynchronous<<std::endl;
	_ros<<"faileddecompressions                 = "<<_rsd.faileddecompressions<<std::endl;
	_ros<<"senduncompressed                     = "<<_rsd.senduncompressed<<std::endl;
	_ros<<"sendcompressed                       = "<<_rsd.sendcompressed<<std::endl;
	return _ros;
}
}//namespace
#endif
}//namespace ipc
}//namespace foundation
