// frame/ipc/src/ipcsession.cpp
//
// Copyright (c) 2010,2013 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
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
#include "serialization/binary.hpp"
#include "frame/message.hpp"
#include "frame/manager.hpp"
#include "frame/ipc/ipcservice.hpp"
#include "ipcsession.hpp"
#include "ipcpacket.hpp"

//#define ENABLE_MORE_DEBUG
namespace solid{
namespace frame{
namespace ipc{


struct PktCmp{
	bool operator()(const uint32 id1, const uint32 id2)const{
		return overflow_safe_less(id1, id2);
	}
	bool operator()(const Packet &_rpkt1, const Packet &_rpkt2)const{
		return operator()(_rpkt1.id(), _rpkt2.id());
	}
	
};

namespace{

struct StaticData{
	enum{
		RefreshIndexMask = (1 << 7) - 1
	};
	StaticData();
	static StaticData const& the();
	uint32 retransmitTimeout(const size_t _pos)const;
	size_t connectRetransmitPosition()const{
		return connectretransmitpos;
	}
	size_t connectRetransmitPositionRelay()const{
		return connectretransmitposrelay;
	}
private:
	typedef std::vector<uint32>	UInt32VectorT;
	UInt32VectorT	toutvec;
	size_t			connectretransmitpos;
	size_t			connectretransmitposrelay;
};


struct ConnectDataMessage: Message{
    ConnectDataMessage(){}
    ConnectDataMessage(const ConnectData& _rcd):data(_rcd){}
    
	ConnectData data;
};
}//namespace

#ifdef USTATISTICS
namespace {
struct StatisticData{
	StatisticData(){
		memset(this, 0, sizeof(StatisticData));
	}
	~StatisticData();
	void reconnect();
	void pushMessage();
	void pushReceivedPacket();
	void retransmitId(const uint16 _cnt);
	void sendKeepAlive();
	void sendPending();
	void pushExpectedReceivedPacket();
	void alreadyReceived();
	void tooManyPacketsOutOfOrder();
	void sendUpdatesSize(const uint16 _sz);
	void sendOnlyUpdatesSize(const uint16 _sz);
	void sendOnlyUpdatesSize1();
	void sendOnlyUpdatesSize2();
	void sendOnlyUpdatesSize3();
	void sendMessageIdxQueueSize(const ulong _sz);
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
	void sendStackSize0();
	void sendStackSize1();
	void sendStackSize2();
	void sendStackSize3();
	void sendStackSize4();
	
	ulong	reconnectcnt;
	ulong	pushmsgcnt;
	ulong	pushreceivedpacketcnt;
	ulong	maxretransmitid;
	ulong	sendkeepalivecnt;
	ulong	sendpendingcnt;
	ulong	pushexpectedreceivedpacketcnt;
	ulong	alreadyreceivedcnt;
	ulong	toomanypacketsoutofordercnt;
	ulong	maxsendupdatessize;
	ulong	sendonlyupdatescnt;
	ulong	sendonlyupdates1;
	ulong	sendonlyupdates2;
	ulong	sendonlyupdates3;
	ulong	maxsendonlyupdatessize;
	ulong	maxsendmsgidxqueuesize;
	ulong	tryschedulekeepalivecnt;
	ulong	schedulekeepalivecnt;
	ulong	failedtimeoutcnt;
	ulong	timeoutcnt;
	ulong	sendasynchronouswhilesynchronous;
	ulong	sendsynchronouswhilesynchronous;
	ulong	maxsendsynchronouswhilesynchronous;
	ulong	sendasynchronous;
	ulong	faileddecompressions;
	ulong	sendstacksize0;
	ulong	sendstacksize1;
	ulong	sendstacksize2;
	ulong	sendstacksize3;
	ulong	sendstacksize4;
	uint64	senduncompressed;
	uint64	sendcompressed;
};

std::ostream& operator<<(std::ostream &_ros, const StatisticData &_rsd);
}//namespace
#endif

//== Session::Data ====================================================
typedef DynamicPointer<Message>			DynamicMessagePointerT;

struct Session::Data{
	enum{
		UpdatePacketId = 0xffffffff,//the id of a packet containing only updates
		MaxOutOfOrder = 4,//please also change moveToNextOutOfOrderPacket
		MinPacketDataSize = 8,
	};
	
	enum Types{
		Dummy,
		Direct4,
		Direct6,
		Relayed44,
		//Relayed66
		//Relayed46
		//Relayed64
		LastType//add types above
	};
	enum States{
		RelayInit = 0,
		Connecting,//Connecting must be first see isConnecting
		RelayConnecting,
		Accepting,
		RelayAccepting,
		WaitAccept,
		Authenticating,
		Connected,
		WaitDisconnecting,
		Disconnecting,
		Reconnecting,
		Disconnected,
		DummyExecute,
	};
	
	struct BinSerializer: Service::SerializerT{
		BinSerializer(
			const serialization::TypeMapperBase &_rtmb
		):Service::SerializerT(_rtmb){}
		BinSerializer(
		){}
		static unsigned specificCount(){
			//return MaxSendSignalQueueSize;
			return ConnectionContext::the().service().configuration().session.maxsendmessagequeuesize;
		}
		void specificRelease(){}
	};

	struct BinDeserializer: Service::DeserializerT{
		BinDeserializer(
			const serialization::TypeMapperBase &_rtmb
		):Service::DeserializerT(_rtmb){}
		BinDeserializer(
		){}
		static unsigned specificCount(){
			//return MaxSendSignalQueueSize;
			return ConnectionContext::the().service().configuration().session.maxsendmessagequeuesize;
		}
		void specificRelease(){}
	};
	
	typedef BinSerializer						BinSerializerT;
	typedef BinDeserializer						BinDeserializerT;

	
	struct RecvMessageData{
		RecvMessageData(
			Message *_pmsg = NULL,
			BinDeserializerT *_pdeserializer = NULL
		):msgptr(_pmsg), pdeserializer(_pdeserializer){}
		DynamicPointer<Message>		msgptr;
		BinDeserializerT			*pdeserializer;
	};
	
	struct SendMessageData{
		SendMessageData(
			const DynamicPointer<Message>& _rmsgptr,
			const SerializationTypeIdT	&_rtid,
			uint32 _flags,
			uint32 _idx
		):msgptr(_rmsgptr), tid(_rtid), pserializer(NULL), flags(_flags), idx(_idx), uid(0){}
		~SendMessageData(){
			//cassert(!pserializer);
		}
		bool operator<(const SendMessageData &_owc)const{
			if(msgptr.get()){
				if(_owc.msgptr.get()){
				}else return true;
			}else return false;
			
			return overflow_safe_less(idx, _owc.idx);
		}
		DynamicMessagePointerT	msgptr;
		SerializationTypeIdT	tid;
		BinSerializerT			*pserializer;
		uint32					flags;
		uint32					idx;
		uint32					uid;
	};
	
	typedef	std::vector<uint16>					UInt16VectorT;
	
	struct SendPacketData{
		SendPacketData():uid(0), sending(0), mustdelete(0){}
		Packet				packet;
		UInt16VectorT		msgidxvec;
		uint16				uid;//we need uid for timer
		uint8				sending;
		uint8				mustdelete;
	};
	
	typedef Queue<uint32>						UInt32QueueT;
	typedef Stack<uint32>						UInt32StackT;
	typedef Stack<uint16>						UInt16StackT;
	typedef Stack<uint8>						UInt8StackT;
	typedef std::vector<Packet>					PacketVectorT;
	typedef Queue<RecvMessageData>				RecvMessageQueueT;
	typedef std::vector<SendMessageData>		SendMessageVectorT;
	typedef std::vector<SendPacketData>			SendPacketVectorT;
	
	struct MessageStub{
		MessageStub():tid(SERIALIZATION_INVALIDID),flags(0){}
		MessageStub(
			const DynamicPointer<Message>	&_rmsgptr,
			const SerializationTypeIdT &_rtid,
			uint32 _flags
		):msgptr(_rmsgptr), tid(_rtid), flags(_flags){}
		DynamicPointer<Message>	msgptr;
		SerializationTypeIdT	tid;
		uint32					flags;
	};
	typedef Queue<MessageStub>					MessageQueueT;

public:
	Data(
		Service &_rsvc,
		uint8 _type,
		uint8 _state,
		uint16 _baseport = 0
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
	
	Session::DataDummy& dummy();
	Session::DataDummy const& dummy()const;
	
	Session::DataDirect4& direct4();
	Session::DataDirect4 const& direct4()const;
	
	Session::DataDirect6& direct6();
	Session::DataDirect6 const& direct6()const;
	
	Session::DataRelayed44& relayed44();
	Session::DataRelayed44 const& relayed44()const;
	
	bool isRelayType()const{
		return type == Relayed44;
	}
	
	bool moveToNextOutOfOrderPacket(Packet &_rpkt);
	bool keepOutOfOrderPacket(Packet &_rpkt);
	bool mustSendUpdates();
	void incrementExpectedId();
	void incrementSendId();
	MessageUid pushSendWaitMessage(
		DynamicPointer<Message> &_rmsgptr,
		const SerializationTypeIdT &_rtid,
		uint32 _flags,
		uint32 _id
	);	
	void popSentWaitMessages(SendPacketData &_rspd);
	void popSentWaitMessage(const MessageUid &_rmsguid);
	void popSentWaitMessage(const uint32 _idx);
	void freeSentPacket(const uint32 _idx);
	void clearSentPacket(const uint32 _idx);
	
	uint32 computeRetransmitTimeout(const uint32 _retrid, const uint32 _pktid);
	uint32 currentKeepAlive(const TalkerStub &_rstub)const;
	uint32 registerPacket(Packet &_rpkt);
	
	bool isExpectingImmediateDataFromPeer()const{
		return rcvdmsgq.size() > 1 || ((rcvdmsgq.size() == 1) && rcvdmsgq.front().msgptr.get());
	}
	uint16 keepAlivePacketIndex()const{
		return 0;
	}
	void moveMessagesToSendQueue();
	void moveAuthMessagesToSendQueue();
	
	void pushMessageToSendQueue(
		DynamicPointer<Message> &_rmsgptr,
		const uint32 _flags,
		const SerializationTypeIdT _tid
	);
	void resetKeepAlive();
	//because connect can take longer than a normal opperation
	//we need a slow start especially for relay sessions
	void resetRetransmitPosition(){
		if(isRelayType()){
			retransmittimepos = StaticData::the().connectRetransmitPositionRelay();
		}else{
			retransmittimepos = StaticData::the().connectRetransmitPosition();
		}
	}
	//returns false if there is no other message but the current one
	bool moveToNextSendMessage();
public:
	uint8					type;
	uint8					state;
	uint8					sendpendingcount;
	uint8					outoforderbufcount;
	
	uint32					rcvexpectedid;
	uint32					sentmsgwaitresponse;
	uint32					retransmittimepos;
	uint32					sendmsgid;
	uint32					sendid;
	//uint32					keepalivetimeout;
	uint32					currentsyncid;
	uint32					currentsendsyncid;
	uint16					currentpacketmsgcount;
	uint16					baseport;
	SocketAddressStub		pairaddr;
	
	UInt32QueueT			rcvdidq;
	PacketVectorT			outoforderbufvec;
	RecvMessageQueueT		rcvdmsgq;
	SendMessageVectorT		sendmsgvec;
	UInt32StackT			sendmsgfreeposstk;
	SendPacketVectorT		sendpacketvec;//will have about 6 items
	UInt8StackT				sendpacketfreeposstk;
	MessageQueueT			msgq;
	UInt32QueueT			sendmsgidxq;
	Packet					updatespacket;
	TimeSpec				rcvtimepos;
#ifdef USTATISTICS
	StatisticData			statistics;
#endif
};
//---------------------------------------------------------------------
struct Session::DataDummy: Session::Data{
	enum{
		UnknownSendType,
		ErrorSendType
	};
	
	struct SendStub{
		SendStub(uint8 _sendtype = UnknownSendType): sendtype(_sendtype){}
		uint8			sendtype;
		SocketAddress	sa;
	};
	
	struct ErrorStub{
		ErrorStub(int _error = 0):error(_error){}
		int		error;
	};
	
	typedef Queue<SendStub>		SendQueueT;
	typedef Queue<ErrorStub>	ErrorQueueT;
	
	Packet		crtpkt;
	SendQueueT	sendq;
	ErrorQueueT	errorq;
	
	DataDummy(Service &_rsvc) :Session::Data(_rsvc, Dummy, DummyExecute){}
};
//---------------------------------------------------------------------
struct Session::DataDirect4: Session::Data{
	DataDirect4(
		Service &_rsvc,
		const SocketAddressInet4 &_raddr
	):Data(_rsvc, Direct4, Connecting, _raddr.port()), addr(_raddr){
		Data::pairaddr = addr;
	}
	DataDirect4(
		Service &_rsvc,
		const SocketAddressInet4 &_raddr,
		uint16 _baseport
	):Data(_rsvc, Direct4, Accepting, _baseport), addr(_raddr){
		Data::pairaddr = addr;
	}
	SocketAddressInet4	addr;
};
//---------------------------------------------------------------------
struct Session::DataDirect6: Session::Data{
	DataDirect6(
		Service &_rsvc,
		const SocketAddressInet6 &_raddr
	):Data(_rsvc, Direct6, Connecting, _raddr.port()), addr(_raddr){
		Data::pairaddr = addr;
	}
	DataDirect6(
		Service &_rsvc,
		const SocketAddressInet6 &_raddr,
		uint16 _baseport
	):Data(_rsvc, Direct6, Accepting, _baseport), addr(_raddr){
		Data::pairaddr = addr;
	}
	SocketAddressInet6	addr;
};
//---------------------------------------------------------------------
struct Session::DataRelayed44: Session::Data{
	DataRelayed44(
		Service &_rsvc,
		const uint32 _netid,
		const SocketAddressInet4 &_raddr,
		const uint32 _gwidx
	):	Data(_rsvc, Relayed44, RelayInit, _raddr.port()), relayaddr(_raddr), netid(_netid),
		peerrelayid(0), lstgwidx((_gwidx + _rsvc.configuration().gatewayaddrvec.size()) % _rsvc.configuration().gatewayaddrvec.size()), crtgwidx(_gwidx){
	}
	
	DataRelayed44(
		Service &_rsvc,
		const uint32 _netid,
		const SocketAddressInet4 &_raddr,
		const ConnectData &_rconndata,
		const uint32 _gwidx
	):	Data(_rsvc, Relayed44, RelayAccepting), addr(_raddr), relayaddr(_rconndata.senderaddress), netid(_netid),
		peerrelayid(_rconndata.relayid), lstgwidx((_gwidx + _rsvc.configuration().gatewayaddrvec.size()) % _rsvc.configuration().gatewayaddrvec.size()), crtgwidx(_gwidx){
		
		Data::pairaddr = addr;
	}
	
	SocketAddressInet4	addr;
	SocketAddressInet4	relayaddr;
	const uint32		netid;
	uint32				peerrelayid;
	const uint8			lstgwidx;
	uint8				crtgwidx;
};

//---------------------------------------------------------------------
inline Session::DataDirect4& Session::Data::direct4(){
	cassert(this->type == Direct4);
	return static_cast<Session::DataDirect4&>(*this);
}
inline Session::DataDirect4 const& Session::Data::direct4()const{
	cassert(this->type == Direct4);
	return static_cast<Session::DataDirect4 const&>(*this);
}
//---------------------------------------------------------------------
inline Session::DataDummy& Session::Data::dummy(){
	cassert(this->type == Dummy);
	return static_cast<Session::DataDummy&>(*this);
}
inline Session::DataDummy const& Session::Data::dummy()const{
	cassert(this->type == Dummy);
	return static_cast<Session::DataDummy const&>(*this);
}
//---------------------------------------------------------------------
inline Session::DataDirect6& Session::Data::direct6(){
	cassert(this->type == Direct6);
	return static_cast<Session::DataDirect6&>(*this);
}
inline Session::DataDirect6 const& Session::Data::direct6()const{
	cassert(this->type == Direct6);
	return static_cast<Session::DataDirect6 const&>(*this);
}
inline Session::DataRelayed44& Session::Data::relayed44(){
	cassert(this->type == Relayed44);
	return static_cast<Session::DataRelayed44&>(*this);
}
inline Session::DataRelayed44 const& Session::Data::relayed44()const{
	cassert(this->type == Relayed44);
	return static_cast<Session::DataRelayed44 const&>(*this);
}
//---------------------------------------------------------------------
Session::Data::Data(
	Service &_rsvc,
	uint8 _type,
	uint8 _state,
	uint16 _baseport
):	type(_type), state(_state), sendpendingcount(0),
	outoforderbufcount(0), rcvexpectedid(2), sentmsgwaitresponse(0),
	retransmittimepos(0), sendmsgid(0), 
	sendid(1)/*, keepalivetimeout(_keepalivetout)*/,
	currentsendsyncid(-1), currentpacketmsgcount(_rsvc.configuration().session.maxmessagepacketcount), baseport(_baseport)
{
	outoforderbufvec.resize(MaxOutOfOrder);
	resetRetransmitPosition();
	//first packet is for keepalive
	sendpacketvec.resize(7);//see: Session::Data::freeSentPacket
	for(uint32 i(_rsvc.configuration().session.maxsendpacketcount); i >= 1; --i){
		sendpacketfreeposstk.push(i);
	}
	sendmsgvec.reserve(_rsvc.configuration().session.maxsendmessagequeuesize);
	idbgx(Debug::ipc, "Sizeof(Session::Data) = "<<sizeof(*this));
#ifdef USTATISTICS
	idbgx(Debug::ipc, "Sizeof(StatisticData) = "<<sizeof(this->statistics));
#endif
}
//---------------------------------------------------------------------
Session::Data::~Data(){
	Context 			&rctx = Context::the();
	ConnectionContext	&rmsgctx = rctx.msgctx;
	while(msgq.size()){
		//msgq.front().msgptr->ipcOnComplete(rctx.msgctx, -1);
		rmsgctx.service().controller().onMessageComplete(*msgq.front().msgptr, rmsgctx, -1);
		msgq.pop();
	}
	while(this->rcvdmsgq.size()){
		if(rcvdmsgq.front().pdeserializer){
			rcvdmsgq.front().pdeserializer->clear();
			pushDeserializer(rcvdmsgq.front().pdeserializer);
			rcvdmsgq.front().pdeserializer = NULL;
		}
		rcvdmsgq.front().msgptr.clear();
		rcvdmsgq.pop();
	}
	for(Data::SendMessageVectorT::iterator it(sendmsgvec.begin()); it != sendmsgvec.end(); ++it){
		SendMessageData &rsmd(*it);
		if(rsmd.pserializer){
			rsmd.pserializer->clear();
			pushSerializer(rsmd.pserializer);
			rsmd.pserializer = NULL;
		}
		if(rsmd.msgptr.get()){
			if((rsmd.flags & WaitResponseFlag) && (rsmd.flags & SentFlag)){
				//the was successfully sent but the response did not arrive
				//rsmd.msgptr->ipcOnComplete(rctx.msgctx, -2);
				rmsgctx.service().controller().onMessageComplete(*rsmd.msgptr, rmsgctx, -2);
			}else{
				//the message was not successfully sent
				rmsgctx.service().controller().onMessageComplete(*rsmd.msgptr, rmsgctx, -1);
			}
			rsmd.msgptr.clear();
		}
		rsmd.tid = SERIALIZATION_INVALIDID;
	}	
}
//---------------------------------------------------------------------
bool Session::Data::moveToNextOutOfOrderPacket(Packet &_rpkt){
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
	_rpkt = outoforderbufvec[idx];
	outoforderbufvec[idx] = outoforderbufvec[outoforderbufcount - 1];
	incrementExpectedId();
	--outoforderbufcount;
	return true;
}
//---------------------------------------------------------------------
bool Session::Data::keepOutOfOrderPacket(Packet &_rpkt){
	if(outoforderbufcount == MaxOutOfOrder) return false;
	outoforderbufvec[outoforderbufcount] = _rpkt;
	++outoforderbufcount;
	return true;
}
//---------------------------------------------------------------------
inline bool Session::Data::mustSendUpdates(){
	return rcvdidq.size() && 
		//we are not expecting anything from the peer - send updates right away
		(!isExpectingImmediateDataFromPeer() || 
		//or we've already waited too long to send the updates
		rcvdidq.size() >= ConnectionContext::the().service().configuration().session.maxrecvnoupdatecount);
}
//---------------------------------------------------------------------
inline void Session::Data::incrementExpectedId(){
	++rcvexpectedid;
	if(rcvexpectedid > Packet::LastPacketId) rcvexpectedid = 0;
}
//---------------------------------------------------------------------
inline void Session::Data::incrementSendId(){
	++sendid;
	if(sendid > Packet::LastPacketId) sendid = 0;
}
//---------------------------------------------------------------------
MessageUid Session::Data::pushSendWaitMessage(
	DynamicPointer<Message> &_rmsgptr,
	const SerializationTypeIdT &_rtid,
	uint32 _flags,
	uint32 _idx
){
	_flags &= ~SentFlag;
//	_flags &= ~WaitResponseFlag;
	cassert(_rmsgptr.get());
	
	if(sendmsgfreeposstk.size()){
		const uint32	idx(sendmsgfreeposstk.top());
		SendMessageData	&rsmd(sendmsgvec[idx]);
		
		sendmsgfreeposstk.pop();
		
		cassert(!rsmd.msgptr.get());
		rsmd.msgptr = _rmsgptr;
		rsmd.tid = _rtid;
		
		cassert(!_rmsgptr.get());
		rsmd.flags = _flags;
		rsmd.idx = _idx;
		idbgx(Debug::ipc, "idx = "<<idx<<" flags = "<<_flags);
		return MessageUid(idx, rsmd.uid);
	}else{
		idbgx(Debug::ipc, "idx = "<<sendmsgvec.size());
		sendmsgvec.push_back(SendMessageData(_rmsgptr, _rtid, _flags, _idx));
		cassert(!_rmsgptr.get());
		return MessageUid(sendmsgvec.size() - 1, 0);
	}
}
//---------------------------------------------------------------------
void Session::Data::popSentWaitMessages(Session::Data::SendPacketData &_rspd){
	switch(_rspd.msgidxvec.size()){
		case 0:break;
		case 1:
			popSentWaitMessage(_rspd.msgidxvec.front());
			break;
		case 2:
			popSentWaitMessage(_rspd.msgidxvec[0]);
			popSentWaitMessage(_rspd.msgidxvec[1]);
			break;
		case 3:
			popSentWaitMessage(_rspd.msgidxvec[0]);
			popSentWaitMessage(_rspd.msgidxvec[1]);
			popSentWaitMessage(_rspd.msgidxvec[2]);
			break;
		case 4:
			popSentWaitMessage(_rspd.msgidxvec[0]);
			popSentWaitMessage(_rspd.msgidxvec[1]);
			popSentWaitMessage(_rspd.msgidxvec[2]);
			popSentWaitMessage(_rspd.msgidxvec[3]);
			break;
		default:
			for(
				UInt16VectorT::const_iterator it(_rspd.msgidxvec.begin());
				it != _rspd.msgidxvec.end();
				++it
			){
				popSentWaitMessage(*it);
			}
			break;
	}
	_rspd.msgidxvec.clear();
}
//---------------------------------------------------------------------
void Session::Data::popSentWaitMessage(const MessageUid &_rmsguid){
	if(_rmsguid.idx < sendmsgvec.size()){
		idbgx(Debug::ipc, ""<<_rmsguid.idx);
		SendMessageData &rsmd(sendmsgvec[_rmsguid.idx]);
		
		if(rsmd.uid != _rmsguid.uid) return;
		++rsmd.uid;
		if(rsmd.msgptr.get()){
			cassert(!rsmd.pserializer);
			//rsmd.msgptr->ipcOnComplete(Context::the().msgctx, 0);
			ConnectionContext	&rmsgctx = Context::the().msgctx;
			rmsgctx.service().controller().onMessageComplete(*rsmd.msgptr, rmsgctx, 0);
			rsmd.msgptr.clear();
		}
		sendmsgfreeposstk.push(_rmsguid.idx);
		--sentmsgwaitresponse;
	}
}
//---------------------------------------------------------------------
void Session::Data::popSentWaitMessage(const uint32 _idx){
	idbgx(Debug::ipc, ""<<_idx);
	SendMessageData &rsmd(sendmsgvec[_idx]);
	cassert(rsmd.msgptr.get());
	if(rsmd.flags & DisconnectAfterSendFlag){
		idbgx(Debug::ipc, "DisconnectAfterSendFlag - disconnecting");
		++rsmd.uid;
		rsmd.msgptr.clear();
		sendmsgfreeposstk.push(_idx);
		this->state = Disconnecting;
	}
	if(rsmd.flags & WaitResponseFlag){
		//let it leave a little longer
		idbgx(Debug::ipc, "message waits for response "<<_idx<<' '<<rsmd.uid);
		rsmd.flags |= SentFlag;
	}else{
		//rsmd.msgptr->ipcOnComplete(Context::the().msgctx, 0);
		ConnectionContext	&rmsgctx = Context::the().msgctx;
		rmsgctx.service().controller().onMessageComplete(*rsmd.msgptr, rmsgctx, 0);
		++rsmd.uid;
		rsmd.msgptr.clear();
		sendmsgfreeposstk.push(_idx);
	}	
}
//---------------------------------------------------------------------
void Session::Data::freeSentPacket(const uint32 _pktid){
	uint	idx;
	if(/*	sendpacketvec[0].packet.packet() && */
		sendpacketvec[0].packet.id() == _pktid
	){
		idx = 0;
		goto KeepAlive;
	}
	if(	sendpacketvec[1].packet.buffer() && 
		sendpacketvec[1].packet.id() == _pktid
	){
		idx = 1;
		goto Success;
	}
	if(	sendpacketvec[2].packet.buffer() && 
		sendpacketvec[2].packet.id() == _pktid
	){
		idx = 2;
		goto Success;
	}
	if(	sendpacketvec[3].packet.buffer() && 
		sendpacketvec[3].packet.id() == _pktid
	){
		idx = 3;
		goto Success;
	}
	if(	sendpacketvec[4].packet.buffer() && 
		sendpacketvec[4].packet.id() == _pktid
	){
		idx = 4;
		goto Success;
	}
	if(	sendpacketvec[5].packet.buffer() && 
		sendpacketvec[5].packet.id() == _pktid
	){
		idx = 5;
		goto Success;
	}
	if(	sendpacketvec[6].packet.buffer() && 
		sendpacketvec[6].packet.id() == _pktid
	){
		idx = 6;
		goto Success;
	}
	return;
	KeepAlive:{
		SendPacketData &rspd(sendpacketvec[0]);
		if(!rspd.sending){
			//we can now initiate the sending of keepalive packet
			rspd.packet.resend(0);
		}else{
			rspd.mustdelete = 1;
		}
	}
	return;
	Success:{
		SendPacketData &rspd(sendpacketvec[idx]);
		if(!rspd.sending){
			clearSentPacket(idx);
		}else{
			rspd.mustdelete = 1;
		}
	}
	
}
//----------------------------------------------------------------------
void Session::Data::clearSentPacket(const uint32 _idx){
	vdbgx(Debug::ipc, ""<<sendpacketfreeposstk.size());
	SendPacketData &rspd(sendpacketvec[_idx]);
	popSentWaitMessages(rspd);
	rspd.packet.clear();
	++rspd.uid;
	sendpacketfreeposstk.push(_idx);
}
//----------------------------------------------------------------------
inline uint32 Session::Data::computeRetransmitTimeout(const uint32 _retrid, const uint32 _pktid){
	if(!(_pktid & StaticData::RefreshIndexMask)){
		//recalibrate the retransmittimepos
		retransmittimepos = 0;
	}
	if(_retrid > retransmittimepos){
		retransmittimepos = _retrid;
	}
	return StaticData::the().retransmitTimeout(retransmittimepos + _retrid);
}
//----------------------------------------------------------------------
inline uint32 Session::Data::currentKeepAlive(const TalkerStub &_rstub)const{
	uint32	seskeepalive;
	uint32	reskeepalive;
	
	if(!isRelayType()){
		seskeepalive = _rstub.service().configuration().session.keepalive;
		reskeepalive = _rstub.service().configuration().session.responsekeepalive;
		vdbgx(Debug::ipc, "");
	}else{
		seskeepalive = _rstub.service().configuration().session.relaykeepalive;
		reskeepalive = _rstub.service().configuration().session.relayresponsekeepalive;
		vdbgx(Debug::ipc, "");
	}
	
	uint32			keepalive(0);
	
	if(sentmsgwaitresponse && reskeepalive){
		keepalive = reskeepalive;
		vdbgx(Debug::ipc, "");
	}else if(seskeepalive){
		keepalive = seskeepalive;
		vdbgx(Debug::ipc, "");
	}
	
	switch(state){
		case WaitDisconnecting:
			keepalive = 0;
			vdbgx(Debug::ipc, "");
			break;
		case Authenticating:
			keepalive = 1000;
			vdbgx(Debug::ipc, "");
			break;
		default:;
	}
	
	if(
		keepalive && 
		(rcvdmsgq.empty() || (rcvdmsgq.size() == 1 && !rcvdmsgq.front().msgptr.get())) && 
		msgq.empty() && sendmsgidxq.empty() && _rstub.currentTime() >= rcvtimepos
	){
		vdbgx(Debug::ipc, ""<<keepalive);
		return keepalive;
	}else{
		return 0;
	}
		
}
//----------------------------------------------------------------------
uint32 Session::Data::registerPacket(Packet &_rpkt){
	if(sendpacketfreeposstk.empty()) return -1;
	
	const uint32	idx(sendpacketfreeposstk.top());
	SendPacketData	&rspd(sendpacketvec[idx]);
	
	sendpacketfreeposstk.pop();
	
	cassert(rspd.packet.empty());
	
	rspd.packet = _rpkt;
	return idx;
}
//----------------------------------------------------------------------
void Session::Data::moveMessagesToSendQueue(){
	while(msgq.size() && sendmsgidxq.size() < ConnectionContext::the().service().configuration().session.maxsendmessagequeuesize){
		pushMessageToSendQueue(
			msgq.front().msgptr, 
			msgq.front().flags,
			msgq.front().tid
		);
		msgq.pop();
	}
}
void Session::Data::moveAuthMessagesToSendQueue(){
	size_t qsz = msgq.size();
	while(qsz){
		if(
			((msgq.front().flags & AuthenticationFlag) != 0) &&
			sendmsgidxq.size() <  ConnectionContext::the().service().configuration().session.maxsendmessagequeuesize
		){
			pushMessageToSendQueue(
				msgq.front().msgptr, 
				msgq.front().flags,
				msgq.front().tid
			);
			msgq.pop();
		}else{
			msgq.push(msgq.front());
			msgq.pop();
		}
		--qsz;
	}
}
void Session::Data::pushMessageToSendQueue(
	DynamicPointer<Message> &_rmsgptr,
	const uint32 _flags,
	const SerializationTypeIdT _tid
){
	cassert(_rmsgptr.get());
	const MessageUid	uid(pushSendWaitMessage(_rmsgptr, _tid, _flags, sendmsgid++));
	SendMessageData 	&rsmd(sendmsgvec[uid.idx]);
	
	sendmsgidxq.push(uid.idx);
	if(rsmd.msgptr->ipcIsOnSender()){
		rsmd.msgptr->ipcRequestMessageUid() = uid;
	}
	if(rsmd.flags & WaitResponseFlag){
		rsmd.msgptr->ipcResetState();
	}
	ConnectionContext	&rmsgctx = Context::the().msgctx;
	uint32				tmp_flgs = rmsgctx.service().controller().onMessagePrepare(*rsmd.msgptr, rmsgctx);//rsmd.msgptr->ipcOnPrepare(Context::the().msgctx);
	
	rsmd.flags |= tmp_flgs;
	
	vdbgx(Debug::ipc, "flags = "<<(rsmd.flags & SentFlag)<<" tmpflgs = "<<(tmp_flgs & SentFlag)<<" msguid = "<<uid.idx<<','<<uid.uid<<" msg = "<<(void*)rsmd.msgptr.get());
	
	if(tmp_flgs & WaitResponseFlag){
		++sentmsgwaitresponse;
	}/*else{
		rsmd.flags &= ~WaitResponseFlag;
	}*/
	vdbgx(Debug::ipc, "msgidx = "<<uid.idx<<" flags = "<<rsmd.flags);
}
//----------------------------------------------------------------------
void Session::Data::resetKeepAlive(){
	SendPacketData	&rspd(sendpacketvec[0]);//the keep alive packet
	++rspd.uid;
}
//----------------------------------------------------------------------
bool Session::Data::moveToNextSendMessage(){
	
	if(sendmsgidxq.size() == 1) return false;
	
	uint32 	crtidx(sendmsgidxq.front());
	
	sendmsgidxq.pop();
	sendmsgidxq.push(crtidx);

	if(currentsendsyncid != static_cast<uint32>(-1) && crtidx != currentsendsyncid){
		cassert(!(sendmsgvec[crtidx].flags & SynchronousSendFlag));
		crtidx = currentsendsyncid;
	}
	
	Data::SendMessageData	&rsmd(sendmsgvec[crtidx]);
	
	vdbgx(Debug::ipc, "flags = "<<(rsmd.flags & SentFlag));
	
	if(rsmd.flags & SynchronousSendFlag){
		currentsendsyncid = crtidx;
		uint32 tmpidx(sendmsgidxq.front());
		while(tmpidx != crtidx){
			Data::SendMessageData	&tmprsmd(sendmsgvec[tmpidx]);
			if(!(tmprsmd.flags & SynchronousSendFlag)){
				COLLECT_DATA_0(statistics.sendAsynchronousWhileSynchronous);
				return true;
			}
			sendmsgidxq.pop();
			sendmsgidxq.push(tmpidx);
			tmpidx = sendmsgidxq.front();
		}
		COLLECT_DATA_1(statistics.sendSynchronousWhileSynchronous, sendmsgidxq.size());
		return false;
	}else{
		COLLECT_DATA_0(statistics.sendAsynchronous);
		return true;
	}
}

//=====================================================================
//	Session
//=====================================================================

/*static*/ bool Session::parseAcceptPacket(
	const Packet &_rpkt,
	AcceptData &_raccdata,
	const SocketAddress &_rfromsa
){
	if(_rpkt.dataSize() < AcceptData::BaseSize){
		return false;
	}
	
	const char *pos = _rpkt.data();

	pos = serialization::binary::load(pos, _raccdata.flags);
	pos = serialization::binary::load(pos, _raccdata.baseport);
	pos = serialization::binary::load(pos, _raccdata.timestamp_s);
	pos = serialization::binary::load(pos, _raccdata.timestamp_n);
	pos = serialization::binary::load(pos, _raccdata.relayid);
	
	vdbgx(Debug::ipc, "AcceptData: "<<_raccdata);
	return true;
}
//---------------------------------------------------------------------
/*static*/ bool Session::parseConnectPacket(
	const Packet &_rpkt, ConnectData &_rcd,
	const SocketAddress &_rfromsa
){
	if(_rpkt.dataSize() < ConnectData::BaseSize){
		return false;
	}
	
	const char *pos = _rpkt.data();

	pos = serialization::binary::load(pos, _rcd.s);
	pos = serialization::binary::load(pos, _rcd.f);
	pos = serialization::binary::load(pos, _rcd.i);
	pos = serialization::binary::load(pos, _rcd.p);
	pos = serialization::binary::load(pos, _rcd.c);
	pos = serialization::binary::load(pos, _rcd.type);
	pos = serialization::binary::load(pos, _rcd.version_major);
	pos = serialization::binary::load(pos, _rcd.version_minor);
	pos = serialization::binary::load(pos, _rcd.flags);
	pos = serialization::binary::load(pos, _rcd.baseport);
	pos = serialization::binary::load(pos, _rcd.timestamp_s);
	pos = serialization::binary::load(pos, _rcd.timestamp_n);
	if(_rcd.s != 's' || _rcd.f != 'f' || _rcd.i != 'i' || _rcd.p != 'p' || _rcd.c != 'c'){
		return false;
	}
	
	if(_rcd.type == ConnectData::BasicType){
		
	}else if(_rcd.type == ConnectData::Relay4Type){
		Binary<4>	bin;
		uint16		port;

		pos = serialization::binary::load(pos, _rcd.relayid);
		
		pos = serialization::binary::load(pos, _rcd.receivernetworkid);
		pos = serialization::binary::load(pos, bin);
		pos = serialization::binary::load(pos, port);
		
		_rcd.receiveraddress.fromBinary(bin, port);
		
		pos = serialization::binary::load(pos, _rcd.sendernetworkid);
		pos = serialization::binary::load(pos, bin);
		pos = serialization::binary::load(pos, port);
		_rcd.senderaddress.fromBinary(bin, port);
		if(_rcd.senderaddress.isInvalid()){
			_rcd.senderaddress = _rfromsa;
			_rcd.senderaddress.port(_rcd.baseport);
		}
	}else if(_rcd.type == ConnectData::Relay6Type){
		
	}else{
		return true;
	}
	
	vdbgx(Debug::ipc, "ConnectData: "<<_rcd);
	return true;
}
//---------------------------------------------------------------------
/*static*/ bool Session::parseErrorPacket(
	const Packet &_rpkt,
	ErrorData &_rdata
){
	if(_rpkt.dataSize() < sizeof(uint32)){
		return false;
	}
	uint32 v;
	serialization::binary::load(_rpkt.data(), v);
	_rdata.error = static_cast<int32>(v);
	
	return true;
}
//---------------------------------------------------------------------
/*static*/ bool Session::fillAcceptPacket(
	Packet &_rpkt,
	const AcceptData &_rad
){
	char 				*ppos = _rpkt.dataEnd();
	
	ppos = serialization::binary::store(ppos, _rad.flags);
	ppos = serialization::binary::store(ppos, _rad.baseport);
	ppos = serialization::binary::store(ppos, _rad.timestamp_s);
	ppos = serialization::binary::store(ppos, _rad.timestamp_n);
	ppos = serialization::binary::store(ppos, _rad.relayid);

	_rpkt.dataSize(_rpkt.dataSize() + (ppos - _rpkt.dataEnd()));
	return true;
}
//---------------------------------------------------------------------
/*static*/ bool Session::fillConnectPacket(
	Packet &_rpkt,
	const ConnectData &_rcd
){
	char	*ppos = _rpkt.dataEnd();
	
	ppos = serialization::binary::store(ppos, _rcd.s);
	ppos = serialization::binary::store(ppos, _rcd.f);
	ppos = serialization::binary::store(ppos, _rcd.i);
	ppos = serialization::binary::store(ppos, _rcd.p);
	ppos = serialization::binary::store(ppos, _rcd.c);
	ppos = serialization::binary::store(ppos, _rcd.type);
	ppos = serialization::binary::store(ppos, _rcd.version_major);
	ppos = serialization::binary::store(ppos, _rcd.version_minor);
	ppos = serialization::binary::store(ppos, _rcd.flags);
	ppos = serialization::binary::store(ppos, _rcd.baseport);
	ppos = serialization::binary::store(ppos, _rcd.timestamp_s);
	ppos = serialization::binary::store(ppos, _rcd.timestamp_n);
	
	if(_rcd.type == ConnectData::Relay4Type){
		Binary<4>	bin;
		uint16		port;
		
		_rcd.receiveraddress.toBinary(bin, port);
		
		ppos = serialization::binary::store(ppos, _rcd.relayid);
		ppos = serialization::binary::store(ppos, _rcd.receivernetworkid);
		ppos = serialization::binary::store(ppos, bin);
		ppos = serialization::binary::store(ppos, port);
		
		_rcd.senderaddress.toBinary(bin, port);
		
		ppos = serialization::binary::store(ppos, _rcd.sendernetworkid);
		ppos = serialization::binary::store(ppos, bin);
		ppos = serialization::binary::store(ppos, port);

	}
	
	_rpkt.dataSize(_rpkt.dataSize() + (ppos - _rpkt.dataEnd()));
	return true;
}
//---------------------------------------------------------------------
/*static*/ bool Session::fillErrorPacket(
	Packet &_rpkt,
	const ErrorData &_rad
){
	char 				*ppos = _rpkt.dataEnd();
	uint32 v = _rad.error;
	ppos = serialization::binary::store(ppos, v);
	
	_rpkt.dataSize(_rpkt.dataSize() + (ppos - _rpkt.dataEnd()));
	return true;
}
//---------------------------------------------------------------------
/*static*/ uint32 Session::computeResendTime(const size_t _cnt){
	uint32 rv = 0;
	for(size_t i = 0; i < _cnt; ++i){
		rv += StaticData::the().retransmitTimeout(i + StaticData::the().connectRetransmitPositionRelay());
	}
	return rv;
}
//---------------------------------------------------------------------
bool Session::isRelayType()const{
	return d.isRelayType();
}
//---------------------------------------------------------------------
Session::Session(
	Service &_rsvc,
	const SocketAddressInet4 &_raddr
):d(*(new DataDirect4(_rsvc, _raddr))){
	vdbgx(Debug::ipc, "Created connect session "<<(void*)this);
}
//---------------------------------------------------------------------
Session::Session(
	Service &_rsvc,
	const SocketAddressInet4 &_raddr,
	const ConnectData &_rconndata
):d(*(new DataDirect4(_rsvc, _raddr, _rconndata.baseport))){
	
	DynamicPointer<Message>	msgptr(new ConnectDataMessage(_rconndata));
	pushMessage(msgptr, 0, 0);
	
	vdbgx(Debug::ipc, "Created accept session "<<(void*)this);
}
//---------------------------------------------------------------------
Session::Session(
	Service &_rsvc,
	const SocketAddressInet6 &_raddr
):d(*(new DataDirect6(_rsvc, _raddr))){
	vdbgx(Debug::ipc, "Created connect session "<<(void*)this);
}
//---------------------------------------------------------------------
Session::Session(
	Service &_rsvc,
	const SocketAddressInet6 &_raddr,
	const ConnectData &_rconndata
):d(*(new DataDirect6(_rsvc, _raddr, _rconndata.baseport))){
	
	DynamicPointer<Message>	msgptr(new ConnectDataMessage(_rconndata));
	pushMessage(msgptr, 0, 0);
	
	vdbgx(Debug::ipc, "Created accept session "<<(void*)this);
}
//---------------------------------------------------------------------
Session::Session(
	Service &_rsvc,
	const uint32 _netid,
	const SocketAddressInet4 &_raddr,
	const uint32 _gwidx
):d(*(new DataRelayed44(_rsvc, _netid, _raddr, _gwidx))){}
//---------------------------------------------------------------------
Session::Session(
	Service &_rsvc,
	const uint32 _netid,
	const SocketAddressInet4 &_raddr,
	const ConnectData &_rconndata,
	const uint32 _gwidx
):d(*(new DataRelayed44(_rsvc, _netid, _raddr, _rconndata, _gwidx))){
	
	DynamicPointer<Message>	msgptr(new ConnectDataMessage(_rconndata));
	pushMessage(msgptr, 0, 0);
	
}
//---------------------------------------------------------------------
Session::Session(
	Service &_rsvc
):d(*(new DataDummy(_rsvc))){
	vdbgx(Debug::ipc, "Created dummy session "<<(void*)this);
}
//---------------------------------------------------------------------
Session::~Session(){
	vdbgx(Debug::ipc, "delete session "<<(void*)this);
	Context 	&rctx = Context::the();
	rctx.msgctx.psession = this;
	switch(d.type){
		case Data::Dummy:
			delete &d.dummy();
			break;
		case Data::Direct4:
			delete &d.direct4();
			break;
		case Data::Direct6:
			delete &d.direct6();
			break;
		case Data::Relayed44:
			delete &d.relayed44();
			break;
		default:
			THROW_EXCEPTION_EX("Unknown data type ", (int)d.type);
	}
	
}
//---------------------------------------------------------------------
const BaseAddress4T Session::peerBaseAddress4()const{
	return BaseAddress4T(d.direct4().addr, d.direct4().baseport);
}
//---------------------------------------------------------------------
const BaseAddress6T Session::peerBaseAddress6()const{
	return BaseAddress6T(d.direct6().addr, d.direct6().baseport);
}
//---------------------------------------------------------------------
const RelayAddress4T Session::peerRelayAddress4()const{
	return RelayAddress4T(BaseAddress4T(d.relayed44().relayaddr, d.relayed44().baseport), d.relayed44().netid);
}
//---------------------------------------------------------------------
// const RelayAddress6T Session::peerRelayAddress6()const{
// 	return BaseAddress6T(d.direct6().addr, d.direct6().baseport);
// }
//---------------------------------------------------------------------
const SocketAddressStub& Session::peerAddress()const{
	return d.pairaddr;
}
//---------------------------------------------------------------------
const SocketAddressInet4& Session::peerAddress4()const{
	return d.direct4().addr;
}
//---------------------------------------------------------------------
const SocketAddressInet6& Session::peerAddress6()const{
	return d.direct6().addr;
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
ConnectionContext::MessagePointerT* Session::requestMessageSafe(const MessageUid &_rmsguid)const{
	if(_rmsguid.idx < d.sendmsgvec.size()){
		Data::SendMessageData &rsmd(d.sendmsgvec[_rmsguid.idx]);
		if(rsmd.uid == _rmsguid.uid){
			return &rsmd.msgptr;
		}
	}
	return NULL;
}
//---------------------------------------------------------------------
void Session::prepare(TalkerStub &_rstub){
	Packet p(
		Specific::popBuffer(Specific::sizeToIndex(Packet::KeepAliveSize)),
		Specific::indexToCapacity(Specific::sizeToIndex(Packet::KeepAliveSize))
	);
	p.reset();
	p.type(Packet::KeepAliveType);
	p.id(Packet::UpdatePacketId);
	if(isRelayType()){
		p.relay(d.relayed44().peerrelayid);
	}
	d.sendpacketvec[0].packet = p;
}
//---------------------------------------------------------------------
void Session::reconnect(Session *_pses){
	COLLECT_DATA_0(d.statistics.reconnect);
	vdbgx(Debug::ipc, "reconnecting session "<<(void*)_pses);
	
	int						adjustcount = 0;
	
	//first we reset the peer addresses
	if(_pses){
		if(!isRelayType()){
			d.direct4().addr.port(_pses->d.direct4().addr.port());
			d.pairaddr = d.direct4().addr;
		}else{
			cassert(_pses->isRelayType());
			d.relayed44().addr = _pses->d.relayed44().addr;
			d.relayed44().relayaddr.port(_pses->d.relayed44().relayaddr.port());
			d.relayed44().peerrelayid = _pses->d.relayed44().peerrelayid;
		}
		cassert(_pses->d.msgq.size());
		if(d.state == Data::Accepting){
			d.msgq.front().msgptr = _pses->d.msgq.front().msgptr;
		}else if(d.state == Data::RelayAccepting){
			cassert(d.msgq.size());
			d.msgq.front().msgptr = _pses->d.msgq.front().msgptr;
		}else{
			d.msgq.push(_pses->d.msgq.front());
		}
		_pses->d.msgq.pop();
		
		adjustcount = 1;
		
		if(!d.isRelayType()){
			d.state = Data::Accepting;
		}else{
			d.state = Data::RelayAccepting;
		}
		
	}else{
		if(!d.isRelayType()){
			d.state = Data::Connecting;
		}else{
			d.state = Data::RelayConnecting;
		}
	}
	
	d.resetRetransmitPosition();
	
	//clear the receive queue
	while(d.rcvdmsgq.size()){
		Data::RecvMessageData	&rrmd(d.rcvdmsgq.front());
		rrmd.msgptr.clear();
		if(rrmd.pdeserializer){
			rrmd.pdeserializer->clear();
			d.pushDeserializer(rrmd.pdeserializer);
		}
		d.rcvdmsgq.pop();
	}
	
	//keep sending messages that do not require the same connector
	for(int sz(d.msgq.size() - adjustcount); sz; --sz){
		if(!(d.msgq.front().flags & SameConnectorFlag)) {
			vdbgx(Debug::ipc, "message scheduled for resend");
			d.msgq.push(d.msgq.front());
		}else{
			vdbgx(Debug::ipc, "message not scheduled for resend");
		}
		d.msgq.pop();
	}
	//clear the sendmsgidxq
	while(d.sendmsgidxq.size()){
		d.sendmsgidxq.pop();
	}
	bool				mustdisconnect = false;
	ConnectionContext	&rmsgctx = Context::the().msgctx;
	
	//see which sent/sending messages must be cleard
	for(Data::SendMessageVectorT::iterator it(d.sendmsgvec.begin()); it != d.sendmsgvec.end(); ++it){
		Data::SendMessageData &rsmd(*it);
		vdbgx(Debug::ipc, "pos = "<<(it - d.sendmsgvec.begin())<<" flags = "<<(rsmd.flags & SentFlag));
		
		if(rsmd.pserializer){
			rsmd.pserializer->clear();
			d.pushSerializer(rsmd.pserializer);
			rsmd.pserializer = NULL;
		}
		if(!rsmd.msgptr) continue;
		
		if((rsmd.flags & DisconnectAfterSendFlag) != 0){
			idbgx(Debug::ipc, "DisconnectAfterSendFlag");
			mustdisconnect = true;
		}
		
		if(!(rsmd.flags & SameConnectorFlag)){
			if(rsmd.flags & WaitResponseFlag && rsmd.flags & SentFlag){
				//if the message was sent and were waiting for response - were not sending twice
				//rsmd.msgptr->ipcOnComplete(Context::the().msgctx, -2);
				
				rmsgctx.service().controller().onMessageComplete(*rsmd.msgptr, rmsgctx, -2);
				rsmd.msgptr.clear();
				++rsmd.uid;
			}
			//we can try resending the message
		}else{
			vdbgx(Debug::ipc, "message not scheduled for resend");
			if(rsmd.flags & WaitResponseFlag && rsmd.flags & SentFlag){
				//rsmd.msgptr->ipcOnComplete(Context::the().msgctx, -2);
				rmsgctx.service().controller().onMessageComplete(*rsmd.msgptr, rmsgctx, -2);
			}else{
				//rsmd.msgptr->ipcOnComplete(Context::the().msgctx, -1);
				rmsgctx.service().controller().onMessageComplete(*rsmd.msgptr, rmsgctx, -1);
			}
			rsmd.msgptr.clear();
			++rsmd.uid;
		}
	}
	//sort the sendmsgvec, so that we can insert into msgq the messages that can be resent
	//in the exact order they were inserted
	std::sort(d.sendmsgvec.begin(), d.sendmsgvec.end());
	
	//then we push into msgq the messages from sendmsgvec
	for(Data::SendMessageVectorT::const_iterator it(d.sendmsgvec.begin()); it != d.sendmsgvec.end(); ++it){
		const Data::SendMessageData &rsmd(*it);
		if(rsmd.msgptr.get()){
			//the sendmsgvec may contain messages sent successfully, waiting for a response
			//those messages are not queued in the scq
			d.msgq.push(Data::MessageStub(rsmd.msgptr, rsmd.tid, rsmd.flags));
		}else break;
	}
	
	d.sendmsgvec.clear();
	
	//delete the sending packets
	for(Data::SendPacketVectorT::iterator it(d.sendpacketvec.begin()); it != d.sendpacketvec.end(); ++it){
		Data::SendPacketData	&rspd(*it);
		++rspd.uid;
		if(it != d.sendpacketvec.begin()){
			rspd.packet.clear();
			rspd.msgidxvec.clear();
			cassert(rspd.sending == 0);
		}
	}
	
	//clear the stacks
	
	d.updatespacket.clear();
	
	while(d.sendpacketfreeposstk.size()){
		d.sendpacketfreeposstk.pop();
	}
	for(uint32 i(ConnectionContext::the().service().configuration().session.maxsendpacketcount); i >= 1; --i){
		d.sendpacketfreeposstk.push(i);
	}
	while(d.sendmsgfreeposstk.size()){
		d.sendmsgfreeposstk.pop();
	}
	d.rcvexpectedid = 2;
	d.sendid = 1;
	d.sentmsgwaitresponse = 0;
	d.currentpacketmsgcount = ConnectionContext::the().service().configuration().session.maxmessagepacketcount;
	d.sendpacketvec[0].packet.id(Packet::UpdatePacketId);
	d.sendpacketvec[0].packet.resend(0);
	//d.state = Data::Accepting;
}
//---------------------------------------------------------------------
bool Session::pushMessage(
	DynamicPointer<Message> &_rmsgptr,
	const SerializationTypeIdT &_rtid,
	uint32 _flags
){
	COLLECT_DATA_0(d.statistics.pushMessage);
	d.msgq.push(Data::MessageStub(_rmsgptr, _rtid, _flags));
	return d.msgq.size() == 1;
}
//---------------------------------------------------------------------
bool Session::pushEvent(
	int32 _event,
	uint32 _flags
){
	//TODO:
	return true;
}

//---------------------------------------------------------------------
bool Session::preprocessReceivedPacket(
	Packet &_rpkt,
	TalkerStub &_rstub
){
	d.rcvdidq.push(_rpkt.id());
	return d.rcvdidq.size() >= ConnectionContext::the().service().configuration().session.maxrecvnoupdatecount;
}
//---------------------------------------------------------------------
bool Session::pushReceivedPacket(
	Packet &_rpkt,
	TalkerStub &_rstub/*,
	const ConnectionUid &_rconuid*/
){
	COLLECT_DATA_0(d.statistics.pushReceivedPacket);
	d.rcvtimepos = _rstub.currentTime();
	d.resetKeepAlive();
	if(_rpkt.id() == d.rcvexpectedid){
		if(!_rpkt.decompress(_rstub.service().controller())){
			_rpkt.clear();//silently drop invalid packet
			COLLECT_DATA_0(d.statistics.failedDecompression);
			return false;
		}
		return doPushExpectedReceivedPacket(_rstub, _rpkt/*, _rconuid*/);
	}else{
		return doPushUnxpectedReceivedPacket(_rstub, _rpkt/*, _rconuid*/);
	}
}
//---------------------------------------------------------------------
void Session::completeConnect(
	TalkerStub &_rstub,
	uint16 _pairport
){
	if(d.state == Data::Connected) return;
	if(d.state == Data::Authenticating) return;
	
	d.direct4().addr.port(_pairport);
	Context::the().msgctx.psession = this;
	doCompleteConnect(_rstub);
}
//---------------------------------------------------------------------
void Session::completeConnect(TalkerStub &_rstub, uint16 _pairport, uint32 _relayid){
	if(d.state == Data::Connected) return;
	if(d.state == Data::Authenticating) return;
	
	d.relayed44().addr.port(_pairport);
	d.relayed44().peerrelayid = _relayid;
	
	d.sendpacketvec[0].packet.relay(_relayid);
	Context::the().msgctx.psession = this;
	doCompleteConnect(_rstub);
}
//---------------------------------------------------------------------
void Session::doCompleteConnect(TalkerStub &_rstub){
	Controller &rctrl = _rstub.service().controller();
	if(!rctrl.hasAuthentication()){
		d.state = Data::Connected;
	}else{
		DynamicPointer<Message>	msgptr;
		MessageUid 				msguid(0xffffffff, 0xffffffff);
		uint32					flags = 0;
		SerializationTypeIdT 	tid = SERIALIZATION_INVALIDID;
		const AsyncE			authrv = rctrl.authenticate(msgptr, msguid, flags, tid);
		switch(authrv){
			case AsyncSuccess:
				d.state = Data::Connected;
				//d.keepalivetimeout = _rstub.service().keepAliveTimeout();
				break;
			case AsyncWait:
				d.state = Data::Authenticating;
				//d.keepalivetimeout = 1000;
				d.pushMessageToSendQueue(
					msgptr,
					flags,
					tid
				);
				flags |= WaitResponseFlag;
				break;
			case AsyncError:
				if(msgptr.get()){
					d.state = Data::WaitDisconnecting;
					//d.keepalivetimeout = 0;
					flags |= DisconnectAfterSendFlag;
					d.pushMessageToSendQueue(
						msgptr,
						flags,
						tid
					);
				}else{
					d.state = Data::Disconnecting;
				}
				break;
			default:
				THROW_EXCEPTION_EX("Invalid return value for authenticate", authrv);
		}
	}
	
	d.freeSentPacket(1);
	//put the received accepting packet from peer as update - it MUST have id 0
	d.rcvdidq.push(1);
}
//---------------------------------------------------------------------
bool Session::executeTimeout(
	TalkerStub &_rstub,
	uint32 _id
){
	uint16					idx;
	uint16					uid;
	
	unpack(idx, uid, _id);
	
	Data::SendPacketData	&rspd(d.sendpacketvec[idx]);
	
	COLLECT_DATA_0(d.statistics.timeout);
	
	if(rspd.uid != uid){
		vdbgx(Debug::ipc, "timeout for ("<<idx<<','<<uid<<')'<<' '<<rspd.uid);
		COLLECT_DATA_0(d.statistics.failedTimeout);
		return false;
	}
	
	vdbgx(Debug::ipc, "timeout for ("<<idx<<','<<uid<<')'<<' '<<rspd.packet.id());
	cassert(!rspd.packet.empty());
	
	rspd.packet.resend(rspd.packet.resend() + 1);
	
	COLLECT_DATA_1(d.statistics.retransmitId, rspd.packet.resend());
	
	if(rspd.packet.resend() > _rstub.service().configuration().session.dataretransmitcount){
		if(rspd.packet.type() == Packet::ConnectType){
			if(rspd.packet.resend() > _rstub.service().configuration().session.connectretransmitcount){//too many resends for connect type
				vdbgx(Debug::ipc, "preparing to disconnect process");
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
	
	if(!rspd.sending){
		uint32 keepalive = 0;
		if(idx == d.keepAlivePacketIndex()){
			keepalive = d.currentKeepAlive(_rstub);
			if(keepalive){
				if(rspd.packet.resend() == 1){
					rspd.packet.id(d.sendid);
					d.incrementSendId();
				}
				COLLECT_DATA_0(d.statistics.sendKeepAlive);
			}else{
				return false;
			}
		}
		//resend the packet
		vdbgx(Debug::ipc, "resending "<<rspd.packet);
		if(_rstub.pushSendPacket(idx, rspd.packet.buffer(), rspd.packet.bufferSize())){
			TimeSpec tpos(_rstub.currentTime());
			tpos += d.computeRetransmitTimeout(rspd.packet.resend(), rspd.packet.id());

			_rstub.pushTimer(_id, tpos);
			vdbgx(Debug::ipc, "send packet"<<rspd.packet.id()<<" done");
		}else{
			COLLECT_DATA_0(d.statistics.sendPending);
			rspd.sending = 1;
			++d.sendpendingcount;
			vdbgx(Debug::ipc, "send packet"<<rspd.packet.id()<<" pending");
		}
	}
	
	return false;
}
//---------------------------------------------------------------------
AsyncE Session::execute(TalkerStub &_rstub){
	Context::the().msgctx.psession = this;
	switch(d.state){
		case Data::RelayInit:
			return doExecuteRelayInit(_rstub);
		case Data::Connecting:
			return doExecuteConnecting(_rstub);
		case Data::RelayConnecting:
			return doExecuteRelayConnecting(_rstub);
		case Data::Accepting:
			return doExecuteAccepting(_rstub);
		case Data::RelayAccepting:
			return doExecuteRelayAccepting(_rstub);
		case Data::WaitAccept:
			return AsyncWait;
		case Data::Authenticating:
			return doExecuteConnectedLimited(_rstub);
		case Data::Connected:
			return doExecuteConnected(_rstub);
		case Data::WaitDisconnecting:
			return doExecuteConnectedLimited(_rstub);
		case Data::Disconnecting:
			return doExecuteDisconnecting(_rstub);
		case Data::Reconnecting:
			if(d.sendpendingcount) return AsyncWait;
			reconnect(NULL);
			return AsyncSuccess;
		case Data::DummyExecute:
			return doExecuteDummy(_rstub);
	}
	return AsyncError;
}
//---------------------------------------------------------------------
bool Session::pushSentPacket(
	TalkerStub &_rstub,
	uint32 _id,
	const char *_data,
	const uint16 _size
){
	if(d.type == Data::Dummy){
		return doDummyPushSentPacket(_rstub, _id, _data, _size);
	}
	if(_id == -1){//an update packet
		//free it right away
		d.updatespacket.clear();
		doTryScheduleKeepAlive(_rstub);
		return d.rcvdidq.size() != 0;
	}
	
	//all we do is set a timer for this packet
	Data::SendPacketData &rspd(d.sendpacketvec[_id]);
	
	if(_id == d.keepAlivePacketIndex()){
		vdbgx(Debug::ipc, "sent keep alive packet done");
		if(rspd.mustdelete){
			rspd.mustdelete = 0;
			rspd.packet.resend(0);
		}
		TimeSpec tpos(_rstub.currentTime());
		tpos += d.computeRetransmitTimeout(rspd.packet.resend(), rspd.packet.id());
	
		_rstub.pushTimer(pack(_id, rspd.uid), tpos);
		return false;
	}
	
	cassert(rspd.sending);
	cassert(rspd.packet.buffer() == _data);
	rspd.sending = 0;
	--d.sendpendingcount;
	
	bool b(false);
	
	if(!d.sendpendingcount && d.state == Data::Reconnecting){
		b = true;
	}
	Context::the().msgctx.psession = this;
	if(rspd.mustdelete){
		d.clearSentPacket(_id);
		if(d.state == Data::Disconnecting){
			b = true;
		}
		return b;
	}
	
	//schedule a timer for this packet
	TimeSpec tpos(_rstub.currentTime());
	tpos += d.computeRetransmitTimeout(rspd.packet.resend(), rspd.packet.id());
	
	_rstub.pushTimer(pack(_id, rspd.uid), tpos);
	return b;
}
//---------------------------------------------------------------------
bool Session::doPushExpectedReceivedPacket(
	TalkerStub &_rstub,
	Packet &_rpkt/*,
	const ConnectionUid &_rconid*/
){
	COLLECT_DATA_0(d.statistics.pushExpectedReceivedPacket);
	vdbgx(Debug::ipc, "expected "<<_rpkt);
	//the expected packet
	d.rcvdidq.push(_rpkt.id());
	
	bool mustexecute(false);
	
	if(_rpkt.updateCount()){
		mustexecute = doFreeSentPackets(_rpkt/*, _rconid*/);
	}
	
	if(d.state == Data::Authenticating || d.state == Data::Connected){
	}else{
		return true;
	}
	
	if(_rpkt.type() == Packet::DataType){
		doParsePacket(_rstub, _rpkt/*, _rconid*/);
	}
	
	d.incrementExpectedId();//move to the next packet
	//while(d.inbufq.top().id() == d.expectedid){
	Packet	p;
	while(d.moveToNextOutOfOrderPacket(p)){
		//this is already done on receive
		if(_rpkt.type() == Packet::DataType){
			doParsePacket(_rstub, p/*, _rconid*/);
		}
	}
	return mustexecute || d.mustSendUpdates();
}
//---------------------------------------------------------------------
bool Session::doPushUnxpectedReceivedPacket(
	TalkerStub &_rstub,
	Packet &_rpkt
	/*,
	const ConnectionUid &_rconid*/
){
	vdbgx(Debug::ipc, "unexpected "<<_rpkt<<" expectedid = "<<d.rcvexpectedid);
	PktCmp	pc;
	bool	mustexecute = false;
	
	if(d.state == Data::Connecting){
		return false;
	}
	
	if(_rpkt.id() <= Packet::LastPacketId){
		if(pc(_rpkt.id(), d.rcvexpectedid)){
			COLLECT_DATA_0(d.statistics.alreadyReceived);
			//the peer doesnt know that we've already received the packet
			//add it as update
			d.rcvdidq.push(_rpkt.id());
		}else{
			if(!_rpkt.decompress(_rstub.service().controller())){
				_rpkt.clear();//silently drop invalid packet
				COLLECT_DATA_0(d.statistics.failedDecompression);
				return false;
			}
			if(_rpkt.updateCount()){//we have updates
				mustexecute = doFreeSentPackets(_rpkt/*, _rconid*/);
			}
			//try to keep the packet for future parsing
			uint32 pktid(_rpkt.id());
			if(d.keepOutOfOrderPacket(_rpkt)){
				vdbgx(Debug::ipc, "out of order packet");
				d.rcvdidq.push(pktid);//for peer updates
			}else{
				vdbgx(Debug::ipc, "too many packets out-of-order "<<d.outoforderbufcount);
				COLLECT_DATA_0(d.statistics.tooManyPacketsOutOfOrder);
			}
		}
	}else if(_rpkt.id() == Packet::UpdatePacketId){//a packet containing only updates
		if(!_rpkt.decompress(_rstub.service().controller())){
			_rpkt.clear();//silently drop invalid packet
			COLLECT_DATA_0(d.statistics.failedDecompression);
			return false;
		}
		mustexecute = doFreeSentPackets(_rpkt/*, _rconid*/);
		if(d.state == Data::Disconnecting){
			return true;
		}
		doTryScheduleKeepAlive(_rstub);
	}
	return mustexecute || d.mustSendUpdates();
}
//---------------------------------------------------------------------
bool Session::doFreeSentPackets(const Packet &_rpkt/*, const ConnectionUid &_rconid*/){
	uint32 sz(d.sendpacketfreeposstk.size());
	for(uint32 i(0); i < _rpkt.updateCount(); ++i){
		d.freeSentPacket(_rpkt.update(i));
	}
	return (sz == 0) && (d.sendpacketfreeposstk.size());
}
//---------------------------------------------------------------------
void Session::doParsePacketDataType(
	TalkerStub &_rstub, const Packet &_rpkt,
	const char *&_bpos, int &_blen, int _firstblen
){
	uint8				datatype(*_bpos);
	CRCValue<uint8>		crcval(CRCValue<uint8>::check_and_create(datatype));
	
	if(!crcval.ok()){
#ifdef ENABLE_MORE_DEBUG
		while(d.rcvdmsgq.size()){
			Data::RecvMessageData &rrmd(d.rcvdmsgq.front());
			edbgx(Debug::ipc, "rrmd. = "<<(void*)rrmd.pmsg<<" rrmd.pdeserializer = "<<(void*)rrmd.pdeserializer);
			d.rcvdmsgq.pop();
		}
#endif
		THROW_EXCEPTION("Deserialization error");
		cassert(false);
	}
	
	++_bpos;
	--_blen;
#ifdef ENABLE_MORE_DEBUG
	if(_rpkt.flags() & Packet::DebugFlag){
		edbgx(Debug::ipc, "value = "<<crcval.value()<<" msgqsz = "<<d.rcvdmsgq.size());
	}
#endif
	switch(crcval.value()){
		case Packet::ContinuedMessage:
			vdbgx(Debug::ipc, "continuedmassage");
			cassert(_blen == _firstblen);
			if(!d.rcvdmsgq.front().msgptr.get()){
				d.rcvdmsgq.pop();
			}
			//we cannot have a continued signal on the same packet
			break;
		case Packet::NewMessage:
			vdbgx(Debug::ipc, "newmessage");
			if(d.rcvdmsgq.size()){
				idbgx(Debug::ipc, "switch to new rcq.size = "<<d.rcvdmsgq.size());
				Data::RecvMessageData &rrmd(d.rcvdmsgq.front());
				if(rrmd.msgptr.get()){//the previous signal didnt end, we reschedule
					d.rcvdmsgq.push(rrmd);
					cassert(rrmd.msgptr.empty());
				}
				rrmd.pdeserializer = d.popDeserializer(_rstub.service().typeMapperBase());
				rrmd.pdeserializer->push(rrmd.msgptr, "message");
			}else{
				idbgx(Debug::ipc, "switch to new rcq.size = 0");
				d.rcvdmsgq.push(Data::RecvMessageData(NULL, d.popDeserializer(_rstub.service().typeMapperBase())));
				d.rcvdmsgq.front().pdeserializer->push(d.rcvdmsgq.front().msgptr, "message");
			}
			break;
		case Packet::OldMessage:
			vdbgx(Debug::ipc, "oldmessage");
			cassert(d.rcvdmsgq.size() > 1);
			if(d.rcvdmsgq.front().msgptr.get()){
				d.rcvdmsgq.push(d.rcvdmsgq.front());
				cassert(d.rcvdmsgq.front().msgptr.empty());
			}
			d.rcvdmsgq.pop();
			break;
		default:{
#ifdef ENABLE_MORE_DEBUG
			while(d.rcvdmsgq.size()){
				Data::RecvMessageData &rrmd(d.rcvdmsgq.front());
				edbgx(Debug::ipc, "rrmd.pmsg = "<<(void*)rrmd.pmsg<<" rrmd.pdeserializer = "<<(void*)rrmd.pdeserializer);
				d.rcvdmsgq.pop();
			}
#endif
			THROW_EXCEPTION("Deserialization error");
			cassert(false);
		}
	}
}
//---------------------------------------------------------------------
void Session::doParsePacket(TalkerStub &_rstub, const Packet &_rpkt){
	const char *bpos(_rpkt.data());
	int			blen(_rpkt.dataSize());
	int			rv(0);
	int 		firstblen(blen - 1);
	Context		&rctx = Context::the();
	
	idbgx(Debug::ipc, "packetid = "<<_rpkt.id());
	rctx.msgctx.psession = this;
	
#ifdef ENABLE_MORE_DEBUG
	if(_rpkt.flags() & Packet::DebugFlag){
		//dump the wait queue
		uint32 sz = d.rcvdmsgq.size();
		edbgx(Debug::ipc, "pktidx = "<<_rpkt.id()<<" sz = "<<sz<<" blen = "<<blen);
		while(sz){
			Data::RecvMessageData &rrmd(d.rcvdmsgq.front());
			edbgx(Debug::ipc, "pmsg = "<<(void*)rrmd.pmsg<<" pdes = "<<(void*)rrsd.pdeserializer);
			d.rcvdmsgq.push(rrsd);
			d.rcvdmsgq.pop();
			--sz;
		}
	}
#endif

	while(blen >= 2){
		
		idbgx(Debug::ipc, "blen = "<<blen<<" bpos "<<(bpos - _rpkt.data()));
		
		doParsePacketDataType(_rstub, _rpkt, bpos, blen, firstblen);
		
		Data::RecvMessageData &rrsd(d.rcvdmsgq.front());
		
		rv = rrsd.pdeserializer->run(bpos, blen, rctx.msgctx);
		
		if(rv < 0){
			THROW_EXCEPTION_EX("Deserialization error", rrsd.pdeserializer->error());
			cassert(false);
			return;
		}
		
		blen -= rv;
		bpos += rv;
		
#ifdef ENABLE_MORE_DEBUG		
		if(rv == 1){
			edbgx(Debug::ipc, "pktid = "<<_rpkt.id()<<" blen = "<<blen<<" bpos "<<(bpos - _rpkt.data()));
			edbgx(Debug::ipc, "pmsg = "<<(void*)rrsd.pmsg<<" pdes = "<<(void*)rrsd.pdeserializer);
		}
#endif

		if(rrsd.pdeserializer->empty()){//done one message
			Controller					&rctrl = _rstub.service().controller();
			DynamicPointer<Message>		msgptr = rrsd.msgptr;
			MessageUid 					msguid = msgptr->ipcIsBackOnSender() ? msgptr->ipcRequestMessageUid() : MessageUid();
			
			cassert(rrsd.msgptr.empty());
			
			vdbgx(Debug::ipc, "received message with message waiting = "<<msguid.idx<<','<<msguid.uid);
			
			if(d.state == Data::Connected){
				if(!rctrl.onMessageReceive(msgptr, rctx.msgctx)){
					d.state = Data::Disconnecting;
				}
				
			}else if(d.state == Data::Authenticating){
				idbgx(Debug::ipc, " - Authenticating");
				uint32					flags = 0;
				SerializationTypeIdT 	tid = SERIALIZATION_INVALIDID;
				const AsyncE 			authrv = rctrl.authenticate(msgptr, msguid, flags, tid);
				switch(authrv){
					case AsyncSuccess:
						d.state = Data::Connected;
						//d.keepalivetimeout = _rstub.service().keepAliveTimeout();
						if(msgptr.get()){
							flags |= WaitResponseFlag;
							d.pushMessageToSendQueue(msgptr, flags, tid);
						}
						break;
					case AsyncWait:
						if(msgptr.get()){
							flags |= WaitResponseFlag;
							d.pushMessageToSendQueue(msgptr, flags, tid);
						}
						break;
					case AsyncError:
						if(msgptr.get()){
							d.state = Data::WaitDisconnecting;
							//d.keepalivetimeout = 0;
							flags |= DisconnectAfterSendFlag;
							d.pushMessageToSendQueue(
								msgptr,
								flags,
								tid
							);
						}else{
							d.state = Data::Disconnecting;
						}
						break;
					default:{
						THROW_EXCEPTION_EX("Invalid return value for authenticate ", authrv);
					}
				}
			}
			
			idbgx(Debug::ipc, "done message "<<msguid.idx<<','<<msguid.uid);
			
			if(msguid.idx != 0xffffffff){//a valid message waiting for response
				d.popSentWaitMessage(msguid);
			}
			d.pushDeserializer(rrsd.pdeserializer);
			//we do not pop it because in case of a new message,
			//we must know if the previous message terminate
			//so we dont mistakingly reschedule another message!!
			rrsd.pdeserializer = NULL;
		}
	}

}
//---------------------------------------------------------------------
//we need to aquire the address of the relay
AsyncE Session::doExecuteRelayInit(TalkerStub &_rstub){
	DataRelayed44		&rd = d.relayed44();
	const Configuration	&rcfg = _rstub.service().configuration();
	const  size_t		gwcnt = rcfg.gatewayaddrvec.size();
	
	idbgx(Debug::ipc, "gatewayCount = "<<gwcnt);
	
	if(gwcnt == 0){
		//no relay for that destination - quit
		d.state = Data::Disconnecting;
		return AsyncSuccess;
	}
	
	bool found = true;
	
	while(rcfg.gatewayaddrvec[rd.crtgwidx].isInvalid()){
		rd.crtgwidx = (rd.crtgwidx + 1) % rcfg.gatewayaddrvec.size();
		if(rd.crtgwidx == rd.lstgwidx){
			found = false;
			break;
		}
	}
	
	if(found){
		rd.addr = rcfg.gatewayaddrvec[rd.crtgwidx];
		d.pairaddr = rd.addr;
	}
	
	d.state = Data::RelayConnecting;
	
	return AsyncSuccess;
}
//---------------------------------------------------------------------
AsyncE Session::doExecuteConnecting(TalkerStub &_rstub){
	const uint32			pktid(Specific::sizeToIndex(128));
	Packet					pkt(Specific::popBuffer(pktid), Specific::indexToCapacity(pktid));
	
	pkt.reset();
	pkt.type(Packet::ConnectType);
	pkt.id(d.sendid);
	d.incrementSendId();
	
	{
		ConnectData		cd;
		
		cd.type = ConnectData::BasicType;
		cd.baseport = _rstub.basePort();
		cd.timestamp_s = _rstub.service().timeStamp().seconds();
		cd.timestamp_n = _rstub.service().timeStamp().nanoSeconds();
		
		fillConnectPacket(pkt, cd);
	}
	
	const uint32			pktidx(d.registerPacket(pkt));
	Data::SendPacketData	&rspd(d.sendpacketvec[pktidx]);
	
	cassert(pktidx == 1);
	vdbgx(Debug::ipc, "send "<<rspd.packet);
	
	if(_rstub.pushSendPacket(pktidx, rspd.packet.buffer(), rspd.packet.bufferSize())){
		//packet sent - setting a timer for it
		//schedule a timer for this packet
		TimeSpec tpos(_rstub.currentTime());
		tpos += d.computeRetransmitTimeout(rspd.packet.resend(), rspd.packet.id());
		
		_rstub.pushTimer(pack(pktidx, rspd.uid) , tpos);
		vdbgx(Debug::ipc, "sent connecting "<<rspd.packet<<" done");
	}else{
		COLLECT_DATA_0(d.statistics.sendPending);
		rspd.sending = 1;
		++d.sendpendingcount;
		vdbgx(Debug::ipc, "sent connecting "<<rspd.packet<<" pending");
	}
	
	d.state = Data::WaitAccept;
	return AsyncWait;
}
//---------------------------------------------------------------------
AsyncE Session::doExecuteRelayConnecting(TalkerStub &_rstub){
	const uint32			pktid(Specific::sizeToIndex(128));
	Packet					pkt(Specific::popBuffer(pktid), Specific::indexToCapacity(pktid));
	
	pkt.reset();
	pkt.type(Packet::ConnectType);
	pkt.id(d.sendid);
	pkt.relay(_rstub.relayId());
	d.incrementSendId();
	
	
	
	{
		ConnectData			cd;
		
		cd.type = ConnectData::Relay4Type;
		cd.baseport = _rstub.basePort();
		cd.timestamp_s = _rstub.service().timeStamp().seconds();
		cd.timestamp_n = _rstub.service().timeStamp().nanoSeconds();
		cd.receivernetworkid = d.relayed44().netid;
		cd.sendernetworkid = _rstub.service().configuration().localnetid;
		
		cd.relayid = _rstub.relayId();
		cd.receiveraddress = this->d.relayed44().relayaddr;
		cd.senderaddress.address("0.0.0.0");
		
		fillConnectPacket(pkt, cd);
		
	}
		
	const uint32			pktidx(d.registerPacket(pkt));
	Data::SendPacketData	&rspd(d.sendpacketvec[pktidx]);
	
	cassert(pktidx == 1);
	vdbgx(Debug::ipc, "send "<<rspd.packet<<" to "<<this->d.relayed44().addr);
	
	rspd.packet.relayPacketSizeStore();
	
	if(_rstub.pushSendPacket(pktidx, rspd.packet.buffer(), rspd.packet.bufferSize())){
		//packet sent - setting a timer for it
		//schedule a timer for this packet
		TimeSpec tpos(_rstub.currentTime());
		tpos += d.computeRetransmitTimeout(rspd.packet.resend(), rspd.packet.id());
		
		_rstub.pushTimer(pack(pktidx, rspd.uid) , tpos);
		vdbgx(Debug::ipc, "sent connecting "<<rspd.packet<<" done");
	}else{
		COLLECT_DATA_0(d.statistics.sendPending);
		rspd.sending = 1;
		++d.sendpendingcount;
		vdbgx(Debug::ipc, "sent connecting "<<rspd.packet<<" pending");
	}
	
	d.state = Data::WaitAccept;
	return AsyncWait;
}
//---------------------------------------------------------------------
AsyncE Session::doExecuteAccepting(TalkerStub &_rstub){
	const uint32	pktid(Specific::sizeToIndex(64));
	Packet			pkt(Specific::popBuffer(pktid), Specific::indexToCapacity(pktid));
	
	pkt.reset();
	pkt.type(Packet::AcceptType);
	pkt.id(d.sendid);
	d.incrementSendId();
	
	
	{
		const ConnectData	&rcd = static_cast<ConnectDataMessage*>(d.msgq.front().msgptr.get())->data;
		AcceptData			ad;
		
		ad.flags = 0;
		ad.baseport = _rstub.basePort();
		ad.relayid = 0;
		ad.timestamp_s = rcd.timestamp_s;
		ad.timestamp_n = rcd.timestamp_n;
		
		fillAcceptPacket(pkt, ad);

		d.msgq.pop();//the connectdatamessage
	}
	
	const uint32			pktidx(d.registerPacket(pkt));
	Data::SendPacketData	&rspd(d.sendpacketvec[pktidx]);
	cassert(pktidx == 1);
	
	vdbgx(Debug::ipc, "send "<<rspd.packet);
	if(_rstub.pushSendPacket(pktidx, rspd.packet.buffer(), rspd.packet.bufferSize())){
		//packet sent - setting a timer for it
		//schedule a timer for this packet
		TimeSpec tpos(_rstub.currentTime());
		tpos += d.computeRetransmitTimeout(rspd.packet.resend(), rspd.packet.id());
		
		_rstub.pushTimer(pack(pktidx, rspd.uid), tpos);
		vdbgx(Debug::ipc, "sent accepting "<<rspd.packet<<" done");
	}else{
		COLLECT_DATA_0(d.statistics.sendPending);
		rspd.sending = 1;
		++d.sendpendingcount;
		vdbgx(Debug::ipc, "sent accepting "<<rspd.packet<<" pending");
	}
	
	Controller &rctrl = _rstub.service().controller();
	
	if(!rctrl.hasAuthentication()){
		d.state = Data::Connected;
	}else{
		d.state = Data::Authenticating;
	}
	return AsyncSuccess;
}
//---------------------------------------------------------------------
AsyncE Session::doExecuteRelayAccepting(TalkerStub &_rstub){
	const uint32	pktid(Specific::sizeToIndex(64));
	Packet			pkt(Specific::popBuffer(pktid), Specific::indexToCapacity(pktid));
	
	pkt.reset();
	pkt.type(Packet::AcceptType);
	pkt.id(d.sendid);
	d.incrementSendId();
	
	
	{
		
		const ConnectData	&rcd = static_cast<ConnectDataMessage*>(d.msgq.front().msgptr.get())->data;
		AcceptData			ad;
		
		ad.flags = 0;
		ad.baseport = _rstub.basePort();
		ad.relayid = _rstub.relayId();;
		ad.timestamp_s = rcd.timestamp_s;
		ad.timestamp_n = rcd.timestamp_n;
		
		pkt.relay(rcd.relayid);
		d.sendpacketvec[0].packet.relay(rcd.relayid);
		
		fillAcceptPacket(pkt, ad);
		
		
		
		vdbgx(Debug::ipc, "relayid "<<rcd.relayid<<" bufrelay "<<pkt.relay());
		
		d.msgq.pop();//the connectdatamessage
	}
	
	
	
	const uint32			pktidx(d.registerPacket(pkt));
	Data::SendPacketData	&rspd(d.sendpacketvec[pktidx]);
	cassert(pktidx == 1);
	
	vdbgx(Debug::ipc, "send "<<rspd.packet);
	
	rspd.packet.relayPacketSizeStore();
	
	if(_rstub.pushSendPacket(pktidx, rspd.packet.buffer(), rspd.packet.bufferSize())){
		//packet sent - setting a timer for it
		//schedule a timer for this packet
		TimeSpec tpos(_rstub.currentTime());
		tpos += d.computeRetransmitTimeout(rspd.packet.resend(), rspd.packet.id());
		
		_rstub.pushTimer(pack(pktidx, rspd.uid), tpos);
		vdbgx(Debug::ipc, "sent accepting "<<rspd.packet<<" done");
	}else{
		COLLECT_DATA_0(d.statistics.sendPending);
		rspd.sending = 1;
		++d.sendpendingcount;
		vdbgx(Debug::ipc, "sent accepting "<<rspd.packet<<" pending");
	}
	
	Controller &rctrl = _rstub.service().controller();
	
	if(!rctrl.hasAuthentication()){
		d.state = Data::Connected;
	}else{
		d.state = Data::Authenticating;
	}
	return AsyncSuccess;
}
//---------------------------------------------------------------------
AsyncE Session::doTrySendUpdates(TalkerStub &_rstub){
	if(d.rcvdidq.size() && d.updatespacket.empty() && d.mustSendUpdates()){
		//send an updates packet
		const uint32	pktid(Specific::sizeToIndex(256));
		Packet			pkt(Specific::popBuffer(pktid), Specific::indexToCapacity(pktid));
		
		pkt.reset();
		pkt.type(Packet::DataType);
		pkt.id(Data::UpdatePacketId);
		
		if(isRelayType()){
			pkt.relay(d.relayed44().peerrelayid);
		}
		
		d.resetKeepAlive();
		
		if(d.rcvdidq.size()){
			pkt.updateInit();
		}
		
		while(d.rcvdidq.size() && pkt.updateCount() < 16){
			pkt.updatePush(d.rcvdidq.front());
			d.rcvdidq.pop();
		}
		
		pkt.optimize(256);
		
		COLLECT_DATA_1(d.statistics.sendOnlyUpdatesSize, pkt.updateCount());
#ifdef USTATISTICS
		if(pkt.updateCount() == 1){
			COLLECT_DATA_0(d.statistics.sendOnlyUpdatesSize1);
		}
		if(pkt.updateCount() == 2){
			COLLECT_DATA_0(d.statistics.sendOnlyUpdatesSize2);
		}
		if(pkt.updateCount() == 3){
			COLLECT_DATA_0(d.statistics.sendOnlyUpdatesSize3);
		}
#endif

		if(isRelayType()){
			pkt.relayPacketSizeStore();
		}
		vdbgx(Debug::ipc, "send "<<pkt);
		
		if(_rstub.pushSendPacket(-1, pkt.buffer(), pkt.bufferSize())){
			vdbgx(Debug::ipc, "sent updates "<<pkt<<" done");
			doTryScheduleKeepAlive(_rstub);
		}else{
			d.updatespacket = pkt;
			vdbgx(Debug::ipc, "sent updates "<<pkt<<" pending");
			return AsyncWait;
		}
		return AsyncSuccess;
	}
	return AsyncError;
}
//---------------------------------------------------------------------
AsyncE Session::doExecuteConnectedLimited(TalkerStub &_rstub){
	vdbgx(Debug::ipc, ""<<d.sendpacketfreeposstk.size());
	Controller 	&rctrl = _rstub.service().controller();
	
	while(d.sendpacketfreeposstk.size()){
		if(d.state == Data::Authenticating){
			d.moveAuthMessagesToSendQueue();
		}
		if(
			d.sendmsgidxq.empty()
		){
			break;
		}
		
		//we can still send packets
		Packet 					pkt(Packet::allocate(), Packet::Capacity);
		
		const uint32			pktidx(d.registerPacket(pkt));
		Data::SendPacketData	&rspd(d.sendpacketvec[pktidx]);
		
		rspd.packet.reset();
		rspd.packet.type(Packet::DataType);
		rspd.packet.id(d.sendid);
		
		if(isRelayType()){
			rspd.packet.relay(d.relayed44().peerrelayid);
		}

		d.incrementSendId();
		if(d.rcvdidq.size()){
			rspd.packet.updateInit();
		}
		
		while(d.rcvdidq.size() && rspd.packet.updateCount() < 8){
			rspd.packet.updatePush(d.rcvdidq.front());
			d.rcvdidq.pop();
		}
		
		COLLECT_DATA_1(d.statistics.sendUpdatesSize, rspd.packet.updateCount());
		
		doFillSendPacket(_rstub, pktidx);
		
		COLLECT_DATA_1(d.statistics.sendUncompressed, rspd.packet.bufferSize());
		
		rspd.packet.compress(rctrl);
		
		COLLECT_DATA_1(d.statistics.sendCompressed, rspd.packet.bufferSize());
		
		d.resetKeepAlive();
		
		if(isRelayType()){
			rspd.packet.relayPacketSizeStore();
		}
		
		vdbgx(Debug::ipc, "send "<<rspd.packet);
		
		if(_rstub.pushSendPacket(pktidx, rspd.packet.buffer(), rspd.packet.bufferSize())){
			//packet sent - setting a timer for it
			//schedule a timer for this packet
			TimeSpec tpos(_rstub.currentTime());
			tpos += d.computeRetransmitTimeout(rspd.packet.resend(), rspd.packet.id());
			
			_rstub.pushTimer(pack(pktidx, rspd.uid), tpos);
			vdbgx(Debug::ipc, "sent data "<<rspd.packet<<" pending");
		}else{
			COLLECT_DATA_0(d.statistics.sendPending);
			rspd.sending = 1;
			++d.sendpendingcount;
			vdbgx(Debug::ipc, "sent data "<<rspd.packet<<" pending");
		}
	}
	doTrySendUpdates(_rstub);
	return AsyncWait;
}
//---------------------------------------------------------------------
AsyncE Session::doExecuteConnected(TalkerStub &_rstub){
	vdbgx(Debug::ipc, ""<<d.sendpacketfreeposstk.size());
	Controller 	&rctrl = _rstub.service().controller();

#ifdef USTATISTICS
	if(d.sendpacketfreeposstk.size() == 0){
		COLLECT_DATA_0(d.statistics.sendStackSize0);
	}else if(d.sendpacketfreeposstk.size() == 1){
		COLLECT_DATA_0(d.statistics.sendStackSize1);
	}else if(d.sendpacketfreeposstk.size() == 2){
		COLLECT_DATA_0(d.statistics.sendStackSize2);
	}else if(d.sendpacketfreeposstk.size() == 3){
		COLLECT_DATA_0(d.statistics.sendStackSize3);
	}else if(d.sendpacketfreeposstk.size() == 4){
		COLLECT_DATA_0(d.statistics.sendStackSize4);
	}
#endif

	while(d.sendpacketfreeposstk.size()){
		if(
			d.msgq.empty() &&
			d.sendmsgidxq.empty()
		){
			break;
		}
	//if(d.sendpacketfreeposstk.size() && (d.msgq.size() || d.sendmsgidxq.size())){
		//we can still send packets
		Packet 					pkt(Packet::allocate(), Packet::Capacity);
		
		const uint32			pktidx(d.registerPacket(pkt));
		Data::SendPacketData	&rspd(d.sendpacketvec[pktidx]);
		
		rspd.packet.reset();
		rspd.packet.type(Packet::DataType);
		rspd.packet.id(d.sendid);
		
		if(isRelayType()){
			rspd.packet.relay(d.relayed44().peerrelayid);
		}
		
		d.incrementSendId();
		
		if(d.rcvdidq.size()){
			rspd.packet.updateInit();
		}
		
		while(d.rcvdidq.size() && rspd.packet.updateCount() < 8){
			rspd.packet.updatePush(d.rcvdidq.front());
			d.rcvdidq.pop();
		}
		
		COLLECT_DATA_1(d.statistics.sendUpdatesSize, rspd.packet.updateCount());

		d.moveMessagesToSendQueue();
		
		doFillSendPacket(_rstub, pktidx);
		
		COLLECT_DATA_1(d.statistics.sendUncompressed, rspd.packet.bufferSize());
		
		rspd.packet.compress(rctrl);
		
		COLLECT_DATA_1(d.statistics.sendCompressed, rspd.packet.bufferSize());
		
		d.resetKeepAlive();
	
		if(isRelayType()){
			rspd.packet.relayPacketSizeStore();
		}
		
		vdbgx(Debug::ipc, "send "<<rspd.packet);
		
		if(_rstub.pushSendPacket(pktidx, rspd.packet.buffer(), rspd.packet.bufferSize())){
			//packet sent - setting a timer for it
			//schedule a timer for this packet
			TimeSpec tpos(_rstub.currentTime());
			tpos += d.computeRetransmitTimeout(rspd.packet.resend(), rspd.packet.id());
			
			_rstub.pushTimer(pack(pktidx, rspd.uid), tpos);
			vdbgx(Debug::ipc, "sent data "<<rspd.packet<<" pending");
		}else{
			COLLECT_DATA_0(d.statistics.sendPending);
			rspd.sending = 1;
			++d.sendpendingcount;
			vdbgx(Debug::ipc, "sent data "<<rspd.packet<<" pending");
		}
	}
	doTrySendUpdates(_rstub);
	//return (d.sendpacketfreeposstk.size() && (d.msgq.size() || d.sendmsgidxq.size())) ? OK : NOK;
	return AsyncWait;
}
//---------------------------------------------------------------------
void Session::doFillSendPacket(TalkerStub &_rstub, const uint32 _pktidx){
	Data::SendPacketData	&rspd(d.sendpacketvec[_pktidx]);
	Data::BinSerializerT	*pser(NULL);
	Controller				&rctrl = _rstub.service().controller();
	Context					&rctx = Context::the();
	
	rctx.msgctx.psession = this;
	COLLECT_DATA_1(d.statistics.sendMessageIdxQueueSize, d.sendmsgidxq.size());
	
	while(d.sendmsgidxq.size()){
		if(d.currentpacketmsgcount){
			Data::SendMessageData	&rsmd(d.sendmsgvec[d.sendmsgidxq.front()]);
			
			if(rsmd.pserializer){//a continued message
				if(d.currentpacketmsgcount == _rstub.service().configuration().session.maxmessagepacketcount){
					vdbgx(Debug::ipc, "oldmessage data size "<<rspd.packet.dataSize());
					rspd.packet.dataType(Packet::OldMessage);
				}else{
					vdbgx(Debug::ipc, "continuedmessage data size "<<rspd.packet.dataSize()<<" headsize = "<<rspd.packet.headerSize());
					rspd.packet.dataType(Packet::ContinuedMessage);
				}
			}else{//a new message
				vdbgx(Debug::ipc, "newmessage data size "<<rspd.packet.dataSize()<<" msg = "<<(void*)rsmd.msgptr.get()<<" flags = "<<rsmd.flags<<" idx = "<<d.sendmsgidxq.front());
				rspd.packet.dataType(Packet::NewMessage);
				if(pser){
					rsmd.pserializer = pser;
					pser = NULL;
				}else{
					rsmd.pserializer = d.popSerializer(_rstub.service().typeMapperBase());
				}
				Message *pmsg(rsmd.msgptr.get());
				if(rsmd.tid == SERIALIZATION_INVALIDID){
					rsmd.pserializer->push(pmsg, "message");
				}else{
					rsmd.pserializer->push(pmsg, _rstub.service().typeMapper(), rsmd.tid, "message");
				}	
			}
			--d.currentpacketmsgcount;
			
			const uint32 tofill = rspd.packet.dataFreeSize() - rctrl.reservedDataSize();
			
			int rv = rsmd.pserializer->run(rspd.packet.dataEnd(), tofill, rctx.msgctx);
			
			vdbgx(Debug::ipc, "d.crtmsgbufcnt = "<<d.currentpacketmsgcount<<" serialized len = "<<rv);
			
			if(rv < 0){
				THROW_EXCEPTION("Serialization error");
				cassert(false);
			}
			if(rv > tofill){
				THROW_EXCEPTION_EX("Serialization error: invalid return value", tofill);
				cassert(false);
			}
			
			
			
			rspd.packet.dataSize(rspd.packet.dataSize() + rv);
			
			if(rsmd.pserializer->empty()){//finished with this message
#ifdef ENABLE_MORE_DEBUG
				if(rv == 1){
					edbgx(Debug::ipc, "pktid = "<<rspd.packet.id()<<" size = "<<tofill<<" datatype = "/*<<lastdatatype*/);
					rspd.packet.flags(rspd.packet.flags() | Packet::DebugFlag);
				}
#endif
				vdbgx(Debug::ipc, "donemessage");
				if(pser) d.pushSerializer(pser);
				pser = rsmd.pserializer;
				pser->clear();
				rsmd.pserializer = NULL;
				vdbgx(Debug::ipc, "cached wait message");
				rspd.msgidxvec.push_back(d.sendmsgidxq.front());
				d.sendmsgidxq.pop();
				if(rsmd.flags & SynchronousSendFlag){
					d.currentsendsyncid = -1;
				}
				vdbgx(Debug::ipc, "sendmsgidxq poped "<<d.sendmsgidxq.size());
				d.currentpacketmsgcount = _rstub.service().configuration().session.maxmessagepacketcount;
				if(rspd.packet.dataFreeSize() < (rctrl.reservedDataSize() + Data::MinPacketDataSize)){
					break;
				}
			}else{
				break;
			}
			
		}else if(d.moveToNextSendMessage()){
			vdbgx(Debug::ipc, "scqpop "<<d.sendmsgidxq.size());
			d.currentpacketmsgcount = _rstub.service().configuration().session.maxmessagepacketcount;
		}else{//we continue sending the current message
			d.currentpacketmsgcount = _rstub.service().configuration().session.maxmessagepacketcount - 1;
		}
	}//while
	
	if(pser) d.pushSerializer(pser);
}
//---------------------------------------------------------------------
AsyncE Session::doExecuteDisconnecting(TalkerStub &_rstub){
	//d.state = Data::Disconnected;
	AsyncE	rv;
	while((rv = doTrySendUpdates(_rstub)) == AsyncSuccess){
	}
	if(rv == AsyncError){
		if(d.state == Data::WaitDisconnecting){
			return AsyncWait;
		}
		d.state = Data::Disconnected;
	}
	return rv;
}
//---------------------------------------------------------------------
void Session::doTryScheduleKeepAlive(TalkerStub &_rstub){
	d.resetKeepAlive();
	COLLECT_DATA_0(d.statistics.tryScheduleKeepAlive);
	vdbgx(Debug::ipc, "try send keepalive");
	const uint32 keepalive = d.currentKeepAlive(_rstub);
	if(keepalive){
		
		COLLECT_DATA_0(d.statistics.scheduleKeepAlive);
		
		const uint32 			idx(d.keepAlivePacketIndex());
		Data::SendPacketData	&rspd(d.sendpacketvec[idx]);
		
		++rspd.uid;
		
		//schedule a timer for this packet
		TimeSpec tpos(_rstub.currentTime());
		tpos += keepalive;
		
		vdbgx(Debug::ipc, "can send keepalive "<<idx<<' '<<rspd.uid);
		
		_rstub.pushTimer(pack(idx, rspd.uid), tpos);
	}
}
//---------------------------------------------------------------------
void Session::prepareContext(Context &_rctx){
	_rctx.msgctx.pairaddr = d.pairaddr;
	_rctx.msgctx.baseport = d.baseport;
	if(!d.isRelayType()){
		_rctx.msgctx.netid = LocalNetworkId;
	}else{
		_rctx.msgctx.netid = d.relayed44().netid;
	}
}
//---------------------------------------------------------------------
void Session::dummySendError(
	TalkerStub &_rstub,
	const SocketAddress &_rsa,
	int _error
){
	cassert(d.type == Data::Dummy);
	d.dummy().errorq.push(DataDummy::ErrorStub(_error));
	d.dummy().sendq.push(DataDummy::SendStub(DataDummy::ErrorSendType));
	d.dummy().sendq.back().sa = _rsa;
}
//---------------------------------------------------------------------
bool Session::doDummyPushSentPacket(
	TalkerStub &_rstub,
	uint32 _id,
	const char *_data,
	const uint16 _size
){
	DataDummy &rdd = d.dummy();
	rdd.sendq.pop();
	d.pairaddr.clear();
	return rdd.sendq.size() != 0;
}
//---------------------------------------------------------------------
AsyncE Session::doExecuteDummy(TalkerStub &_rstub){
	cassert(d.type == Data::Dummy);
	DataDummy &rdd = d.dummy();
	if(rdd.sendq.empty()) return AsyncWait;
	
	if(rdd.crtpkt.empty()){
		const uint32	pktid(Specific::sizeToIndex(128));
		Packet			pkt(Specific::popBuffer(pktid), Specific::indexToCapacity(pktid));
		
		rdd.crtpkt = pkt;
	}
	
	while(rdd.sendq.size()){
		DataDummy::SendStub &rss = rdd.sendq.front();
		
		d.pairaddr = rss.sa;
		
		if(rss.sendtype == DataDummy::ErrorSendType){
			rdd.crtpkt.reset();
			rdd.crtpkt.type(Packet::ErrorType);
			rdd.crtpkt.id(0);
		
			ErrorData ed;
			ed.error = rdd.errorq.front().error;
			rdd.errorq.pop();
			
			fillErrorPacket(rdd.crtpkt, ed);
			
			vdbgx(Debug::ipc, "send "<<rdd.crtpkt);
			
			if(_rstub.pushSendPacket(0, rdd.crtpkt.buffer(), rdd.crtpkt.bufferSize())){
				vdbgx(Debug::ipc, "sent error packet done");
				rdd.sendq.pop();
			}else{
				return AsyncWait;
			}
		}else{
			edbgx(Debug::ipc, "unsupported sendtype = "<<rss.sendtype);
			rdd.sendq.pop();
		}
		
	}
	
	d.pairaddr.clear();
	
	return AsyncWait;
}
//---------------------------------------------------------------------
bool Session::pushReceivedErrorPacket(
	Packet &_rpkt,
	TalkerStub &_rstub
){
	ErrorData	ed;
	
	if(parseErrorPacket(_rpkt, ed)){
		idbgx(Debug::ipc, "Received error ("<<ed.error<<"): "<<Service::errorText(ed.error));
	}else{
		cassert(false);
	}
	
	d.state = Data::Disconnecting;
	
	return true;
}
//======================================================================
namespace{
#ifdef HAS_SAFE_STATIC
/*static*/ StaticData const& StaticData::the(){
	static const StaticData sd;
	return sd;
}
#else
StaticData& static_data_instance(){
	static const StaticData sd;
	return sd;
}
void once_static_data(){
	static_data_instance();
}
/*static*/ StaticData const& StaticData::the(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_static_data, once);
	return static_data_instance();
}
#endif
//----------------------------------------------------------------------
StaticData::StaticData(){
	toutvec.push_back(  50);
	toutvec.push_back( 100);
	toutvec.push_back( 200);
	toutvec.push_back( 400);
	toutvec.push_back( 600);
	toutvec.push_back( 800);
	toutvec.push_back( 1000);
	toutvec.push_back( 1500);
	toutvec.push_back( 2000);
	toutvec.push_back( 3000);
	toutvec.push_back( 4000);
	connectretransmitposrelay = 6;
	connectretransmitpos = 3;
}

//----------------------------------------------------------------------
uint32 StaticData::retransmitTimeout(const size_t _pos)const{
	if(_pos < toutvec.size()){
		return toutvec[_pos];
	}else{
		return toutvec.back();
	}
}
}//namespace

//----------------------------------------------------------------------
//----------------------------------------------------------------------
namespace{
#ifdef HAS_SAFE_STATIC
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
Context::Context(Service &_rsrv, const uint16 _tkridx, ObjectUidT const &_robjuid):msgctx(_rsrv, _tkridx, _robjuid){
	Thread::specific(specificId(), this);
}
//----------------------------------------------------------------------
Context::~Context(){
	//Thread::specific(specificId(), NULL);
}
//----------------------------------------------------------------------
//----------------------------------------------------------------------
/*static*/ const ConnectionContext& ConnectionContext::the(){
	return Context::the().msgctx;
}
//======================================================================
#ifdef USTATISTICS
namespace {
StatisticData::~StatisticData(){
	rdbgx(Debug::ipc, "Statistics:\r\n"<<*this);
}
void StatisticData::reconnect(){
	++reconnectcnt;
}
void StatisticData::pushMessage(){
	++pushmsgcnt;
}
void StatisticData::pushReceivedPacket(){
	++pushreceivedpacketcnt;
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
void StatisticData::pushExpectedReceivedPacket(){
	++pushexpectedreceivedpacketcnt;
}
void StatisticData::alreadyReceived(){
	++alreadyreceivedcnt;
}
void StatisticData::tooManyPacketsOutOfOrder(){
	++toomanypacketsoutofordercnt;
}
void StatisticData::sendUpdatesSize(const uint16 _sz){
	if(maxsendupdatessize < _sz) maxsendupdatessize = _sz;
}
void StatisticData::sendOnlyUpdatesSize(const uint16 _sz){
	++sendonlyupdatescnt;
	if(maxsendonlyupdatessize < _sz) maxsendonlyupdatessize = _sz;
}
void StatisticData::sendOnlyUpdatesSize1(){
	++sendonlyupdates1;
}
void StatisticData::sendOnlyUpdatesSize2(){
	++sendonlyupdates2;
}
void StatisticData::sendOnlyUpdatesSize3(){
	++sendonlyupdates3;
}

void StatisticData::sendMessageIdxQueueSize(const ulong _sz){
	if(maxsendmsgidxqueuesize < _sz) maxsendmsgidxqueuesize = _sz;
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

void StatisticData::sendStackSize0(){
	++sendstacksize0;
}

void StatisticData::sendStackSize1(){
	++sendstacksize1;
}

void StatisticData::sendStackSize2(){
	++sendstacksize2;
}

void StatisticData::sendStackSize3(){
	++sendstacksize3;
}

void StatisticData::sendStackSize4(){
	++sendstacksize4;
}

std::ostream& operator<<(std::ostream &_ros, const StatisticData &_rsd){
	_ros<<"reconnectcnt                         = "<<_rsd.reconnectcnt<<std::endl;
	_ros<<"pushmsgcnt                           = "<<_rsd.pushmsgcnt<<std::endl;
	_ros<<"pushreceivedpacketcnt                = "<<_rsd.pushreceivedpacketcnt<<std::endl;
	_ros<<"maxretransmitid                      = "<<_rsd.maxretransmitid<<std::endl;
	_ros<<"sendkeepalivecnt                     = "<<_rsd.sendkeepalivecnt<<std::endl;
	_ros<<"sendpendingcnt                       = "<<_rsd.sendpendingcnt<<std::endl;
	_ros<<"pushexpectedreceivedpacketcnt        = "<<_rsd.pushexpectedreceivedpacketcnt<<std::endl;
	_ros<<"alreadyreceivedcnt                   = "<<_rsd.alreadyreceivedcnt<<std::endl;
	_ros<<"toomanypacketsoutofordercnt          = "<<_rsd.toomanypacketsoutofordercnt<<std::endl;
	_ros<<"maxsendupdatessize                   = "<<_rsd.maxsendupdatessize<<std::endl;
	_ros<<"sendonlyupdatescnt                   = "<<_rsd.sendonlyupdatescnt<<std::endl;
	_ros<<"sendonlyupdates1                     = "<<_rsd.sendonlyupdates1<<std::endl;
	_ros<<"sendonlyupdates2                     = "<<_rsd.sendonlyupdates2<<std::endl;
	_ros<<"sendonlyupdates3                     = "<<_rsd.sendonlyupdates3<<std::endl;
	_ros<<"maxsendonlyupdatessize               = "<<_rsd.maxsendonlyupdatessize<<std::endl;
	_ros<<"maxsendmsgidxqueuesize            = "<<_rsd.maxsendmsgidxqueuesize<<std::endl;
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
	_ros<<"sendstacksize0                       = "<<_rsd.sendstacksize0<<std::endl;
	_ros<<"sendstacksize1                       = "<<_rsd.sendstacksize1<<std::endl;
	_ros<<"sendstacksize2                       = "<<_rsd.sendstacksize2<<std::endl;
	_ros<<"sendstacksize3                       = "<<_rsd.sendstacksize3<<std::endl;
	_ros<<"sendstacksize4                       = "<<_rsd.sendstacksize4<<std::endl;
	return _ros;
}
}//namespace
#endif


}//namespace ipc
}//namespace frame
}//namespace solid
