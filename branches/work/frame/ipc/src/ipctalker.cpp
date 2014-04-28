// frame/ipc/src/ipctalker.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <queue>
#include <cerrno>
#include <cstring>
#include <ostream>

#include "system/timespec.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/specific.hpp"

#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "frame/manager.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/ipc/ipcconnectionuid.hpp"

#include "ipctalker.hpp"
#include "ipcsession.hpp"
#include "ipcpacket.hpp"

#ifdef HAS_CPP11

#include <unordered_map>

#else

#include <map>

#endif
namespace solid{
namespace frame{
namespace ipc{


#ifdef USTATISTICS
namespace{
struct StatisticData{
	StatisticData(){
		memset(this, 0, sizeof(StatisticData));
	}
	~StatisticData();
	
	void receivedManyPackets();
	void receivedKeepAlive();
	void receivedData();
	void receivedDataUnknown();
	void receivedConnecting();
	void failedAcceptSession();
	void receivedConnectingError();
	void receivedAccepting();
	void receivedAcceptingError();
	void receivedUnknown();
	void maxTimerQueueSize(ulong _sz);
	void maxSessionExecQueueSize(ulong _sz);
	void maxSendQueueSize(ulong _sz);
	void sendPending();
	void signaled();
	void pushTimer();
	void receivedPackets0();
	void receivedPackets1();
	void receivedPackets2();
	void receivedPackets3();
	void receivedPackets4();
	
	ulong	rcvdmannypackets;
	ulong	rcvdkeepalive;
	ulong	rcvddata;
	ulong	rcvddataunknown;
	ulong	rcvdconnecting;
	ulong	failedacceptsession;
	ulong	rcvdconnectingerror;
	ulong	rcvdaccepting;
	ulong	rcvdacceptingerror;
	ulong	rcvdunknown;
	ulong	maxtimerqueuesize;
	ulong	maxsessionexecqueuesize;
	ulong	maxsendqueuesize;
	ulong	sendpending;
	ulong	signaledcount;
	ulong	pushtimercount;
	ulong	receivedpackets0;
	ulong	receivedpackets1;
	ulong	receivedpackets2;
	ulong	receivedpackets3;
	ulong	receivedpackets4;
};

std::ostream& operator<<(std::ostream &_ros, const StatisticData &_rsd);
}//namespace
#endif


struct Talker::Data{
	struct MessageData{
		MessageData(
			DynamicPointer<Message> &_rmsgptr,
			const SerializationTypeIdT &_rtid,
			uint16 _sesidx,
			uint16 _sesuid,
			uint32 _flags
		):	msgptr(_rmsgptr), stid(_rtid), sessionidx(_sesidx),
			sessionuid(_sesuid), flags(_flags){}
		
		DynamicPointer<Message>	msgptr;
		SerializationTypeIdT 	stid;
		uint16					sessionidx;
		uint16					sessionuid;
		uint32					flags;
	};
	struct EventData{
		EventData(
			uint16 _sessionidx = 0xffff,
			uint16 _sessionuid = 0xffff,
			int32 _event = 0,
			uint32 _flags = 0
		):	sessionidx(_sessionidx), sessionuid(_sessionuid),
			event(_event), flags(_flags){}
		
		uint16					sessionidx;
		uint16					sessionuid;
		int32					event;
		uint32					flags;
	};
	struct RecvPacket{
		RecvPacket(
			char *_data = NULL,
			uint16 _size = 0,
			uint16 _sessionidx = 0
		):data(_data), size(_size), sessionidx(_sessionidx){}
		char	*data;
		uint16	size;
		uint16	sessionidx;
	};
	struct SendPacket{
		SendPacket(
			const char *_data = NULL,
			uint16 _size = 0,
			uint16 _sessionidx = 0,
			uint32 _id = 0
		):data(_data), size(_size), sessionidx(_sessionidx), id(_id){}
		const char *data;
		uint16		size;
		uint16		sessionidx;
		uint32		id;
	};
	
	struct TimerData{
		TimerData(
			const TimeSpec &_rtimepos,
			uint32 _id,
			uint16 _sessionidx
		): timepos(_rtimepos), id(_id), sessionidx(_sessionidx){}
		TimeSpec	timepos;
		uint32		id;
		uint16		sessionidx;
	};
	
	struct TimerDataCmp{
		bool operator()(
			const TimerData &_rtd1,
			const TimerData &_rtd2
		)const{
			return _rtd1.timepos > _rtd2.timepos;
		}
	};
	struct SessionStub{
		SessionStub(
			Session *_psession = NULL,
			uint16 _uid = 0
		):psession(_psession), uid(_uid), inexeq(false){}
		Session		*psession;
		uint16		uid;
		bool		inexeq;
	};
	
	
	typedef std::vector<RecvPacket>				RecvPacketVectorT;
	typedef Queue<MessageData>					MessageQueueT;
	typedef Queue<EventData>					EventQueueT;
	typedef std::pair<uint16, uint16>			UInt16PairT;
	typedef Stack<UInt16PairT>					UInt16PairStackT;
	typedef std::pair<Session*, uint16>			SessionPairT;
	typedef std::vector<SessionPairT>			SessionPairVectorT;
	typedef std::vector<uint16>					UInt16VectorT;
	typedef std::vector<SessionStub>			SessionStubVectorT;
	typedef Queue<uint16>						UInt16QueueT;
	typedef std::pair<
		const SocketAddressStub4*,
		int
	>											BaseAddr4;
	typedef std::pair<
		const SocketAddressStub6*,
		int
	>											BaseAddr6;
#ifdef HAS_CPP11
	typedef std::unordered_map<
		const SocketAddressInet4*,
		uint32,
		SocketAddressHash,
		SocketAddressEqual
	>											PeerAddr4MapT;
	typedef std::unordered_map<
		const BaseAddress4T,
		uint32,
		SocketAddressHash,
		SocketAddressEqual
	>											BaseAddr4MapT;
	
	typedef std::unordered_map<
		const SocketAddressInet6, 
		uint32,
		SocketAddressHash,
		SocketAddressEqual
	>											PeerAddr6MapT;
	
	typedef std::unordered_map<
		const BaseAddress6T,
		uint32,
		SocketAddressHash,
		SocketAddressEqual
	>											BaseAddr6MapT;

#else
	typedef std::map<
		const SocketAddressInet4*,
		uint32,
		SocketAddressCompare
	>											PeerAddr4MapT;
	typedef std::map<
		const BaseAddress4T,
		uint32,
		SocketAddressCompare
	>											BaseAddr4MapT;
	
	typedef std::map<
		const SocketAddressInet6*, 
		uint32,
		SocketAddressCompare
	>											PeerAddr6MapT;
	
	typedef std::map<
		const BaseAddress6T,
		uint32,
		SocketAddressCompare
	>											BaseAddr6MapT;
#endif
	typedef std::priority_queue<
		TimerData,
		std::vector<TimerData>,
		TimerDataCmp
	>											TimerQueueT;
	typedef Queue<SendPacket>					SendQueueT;
	
public:
	Data(
		Service &_rservice,
		uint16 _tkridx
	):	rservice(_rservice), tkridx(_tkridx), pendingreadpacket(NULL), nextsessionidx(1){
	}
	~Data();
public:
	Service					&rservice;
	const uint16			tkridx;
	RecvPacketVectorT		receivedpktvec;
	char					*pendingreadpacket;
	MessageQueueT			msgq;
	EventQueueT				eventq;
	UInt16PairStackT		freesessionstack;
	uint16					nextsessionidx;
	SessionPairVectorT		newsessionvec;
	UInt16VectorT			closingsessionvec;
	SessionStubVectorT		sessionvec;
	UInt16QueueT			sessionexecq;
	PeerAddr4MapT			peeraddr4map;
	BaseAddr4MapT			baseaddr4map;
	TimerQueueT				timerq;
	SendQueueT				sendq;
#ifdef USTATISTICS
	StatisticData			statistics;
#endif
};


Talker::Data::~Data(){
	if(pendingreadpacket){
		Packet::deallocate(pendingreadpacket);
	}
	for(RecvPacketVectorT::const_iterator it(receivedpktvec.begin()); it != receivedpktvec.end(); ++it){
		Packet::deallocate(it->data);
	}
	for(SessionPairVectorT::const_iterator it(newsessionvec.begin()); it != newsessionvec.end(); ++it){
		delete it->first;
	}
	for(SessionStubVectorT::const_iterator it(sessionvec.begin()); it != sessionvec.end(); ++it){
		delete it->psession;
	}
}
//======================================================================

Talker::Talker(
	const SocketDevice &_rsd,
	Service &_rservice,
	uint16 _tkridx
):BaseT(_rsd), d(*new Data(_rservice, _tkridx)){
	d.sessionvec.push_back(
		Data::SessionStub(new Session(_rservice))
	);
}

//----------------------------------------------------------------------
Talker::~Talker(){
	//at this moment the talker is unregistered from Manager, so it has no id
	Context		ctx(d.rservice, -1, ObjectUidT(-1, -1));
	delete &d;
}
//----------------------------------------------------------------------
void Talker::execute(ExecuteContext &_rexectx){
	Manager		&rm = d.rservice.manager();
	Context		ctx(d.rservice, d.tkridx, rm.id(*this));
	TalkerStub	ts(*this, d.rservice, _rexectx.currentTime());
	ulong		sig = _rexectx.eventMask();
	idbgx(Debug::ipc, "this = "<<(void*)this<<" &d = "<<(void*)&d);
	{
		const ulong		sm = grabSignalMask();
		if(sm/* || d.closingsessionvec.size()*/){
			if(sm & frame::S_KILL){
				idbgx(Debug::ipc, "talker - dying");
				_rexectx.close();
				return;
			}
			
			idbgx(Debug::ipc, "talker - signaled");
			if(sm == frame::S_RAISE){
				sig |= frame::EventSignal;
				Locker<Mutex>	lock(rm.mutex(*this));
			
				if(d.newsessionvec.size()){
					doInsertNewSessions(ts);
				}
				if(d.msgq.size()){
					doDispatchMessages();
				}
				if(d.eventq.size()){
					doDispatchEvents();
				}
			}else{
				idbgx(Debug::ipc, "unknown signal");
			}
		}
	}
	sig |= socketEventsGrab();
	
	if(sig & frame::EventDoneError){
		_rexectx.close();
		return;
	}
	
	if(d.closingsessionvec.size()){
		//this is to ensure the locking order: first service then talker
		d.rservice.disconnectTalkerSessions(*this, ts);
	}
	
	bool	must_reenter(false);
	AsyncE	rv;
	
	rv = doReceivePackets(ts, 4, sig);
	
	if(rv == AsyncSuccess){
		must_reenter = true;
	}else if(rv == AsyncError){
		_rexectx.close();
		return;
	}
	
	must_reenter = doProcessReceivedPackets(ts) || must_reenter;
	
	rv = doSendPackets(ts, sig);
	if(rv == AsyncSuccess){
		must_reenter = true;
	}else if(rv == AsyncError){
		_rexectx.close();
		return;
	}
	
	must_reenter = doExecuteSessions(ts) || must_reenter;
	
	if(must_reenter){
		_rexectx.reschedule();
	}else if(d.timerq.size()){
		_rexectx.waitUntil(d.timerq.top().timepos);
	}
}

//----------------------------------------------------------------------
AsyncE Talker::doReceivePackets(TalkerStub &_rstub, uint _atmost, const ulong _sig){
	if(this->socketHasPendingRecv()){
		return AsyncWait;
	}
	if(_sig & frame::EventDoneRecv){
		doDispatchReceivedPacket(_rstub, d.pendingreadpacket, socketRecvSize(), socketRecvAddr());
		d.pendingreadpacket = NULL;
	}
	while(_atmost--){
		char 			*pbuf(Packet::allocate());
		const uint32	bufsz(Packet::Capacity);
		switch(socketRecvFrom(pbuf, bufsz)){
			case aio::AsyncSuccess:
				doDispatchReceivedPacket(_rstub, pbuf, socketRecvSize(), socketRecvAddr());
				break;
			case aio::AsyncWait:
				d.pendingreadpacket = pbuf;
				return AsyncWait;
			case aio::AsyncError:
				Packet::deallocate(pbuf);
				return AsyncError;
		}
	}
	
	COLLECT_DATA_0(d.statistics.receivedManyPackets);
	return AsyncSuccess;//can still read from socket
}
//----------------------------------------------------------------------
bool Talker::doPreprocessReceivedPackets(TalkerStub &_rstub){
	for(Data::RecvPacketVectorT::const_iterator it(d.receivedpktvec.begin()); it != d.receivedpktvec.end(); ++it){
		
		const Data::RecvPacket	&rcvpkt(*it);
		Data::SessionStub		&rss(d.sessionvec[rcvpkt.sessionidx]);
		Packet					pkt(rcvpkt.data, Packet::Capacity);
		
		pkt.bufferSize(rcvpkt.size);
		
		if(rss.psession->preprocessReceivedPacket(pkt, _rstub)){
			if(!rss.inexeq){
				d.sessionexecq.push(rcvpkt.sessionidx);
				rss.inexeq = true;
			}
		}
		pkt.release();
	}
	return false;
}
//----------------------------------------------------------------------
bool Talker::doProcessReceivedPackets(TalkerStub &_rstub){
	//ConnectionUid	conuid(d.tkridx);
	TalkerStub		&ts = _rstub;
#ifdef USTATISTICS	
	if(d.receivedpktvec.size() == 0){
		COLLECT_DATA_0(d.statistics.receivedPackets0);
	}else if(d.receivedpktvec.size() == 1){
		COLLECT_DATA_0(d.statistics.receivedPackets1);
	}else if(d.receivedpktvec.size() == 2){
		COLLECT_DATA_0(d.statistics.receivedPackets2);
	}else if(d.receivedpktvec.size() == 3){
		COLLECT_DATA_0(d.statistics.receivedPackets3);
	}else if(d.receivedpktvec.size() == 4){
		COLLECT_DATA_0(d.statistics.receivedPackets4);
	}
#endif
	
	for(Data::RecvPacketVectorT::const_iterator it(d.receivedpktvec.begin()); it != d.receivedpktvec.end(); ++it){
		
		const Data::RecvPacket	&rcvpkt(*it);
		Data::SessionStub		&rss(d.sessionvec[rcvpkt.sessionidx]);
		Packet					pkt(rcvpkt.data, Packet::Capacity);
		
		pkt.bufferSize(rcvpkt.size);
		
		Context::the().msgctx.connectionuid.sesidx = rcvpkt.sessionidx;
		Context::the().msgctx.connectionuid.sesuid = rss.uid;
		ts.sessionidx = rcvpkt.sessionidx;
		rss.psession->prepareContext(Context::the());
		
		if(rss.psession->pushReceivedPacket(pkt, ts/*, conuid*/)){
			if(!rss.inexeq){
				d.sessionexecq.push(rcvpkt.sessionidx);
				rss.inexeq = true;
			}
		}
	}
	d.receivedpktvec.clear();
	return false;
}
//----------------------------------------------------------------------
void Talker::doDispatchReceivedPacket(
	TalkerStub &_rstub,
	char *_pbuf,
	const uint32 _bufsz,
	const SocketAddress &_rsa
){
	Packet pkt(_pbuf, Packet::Capacity);
	pkt.bufferSize(_bufsz);
	vdbgx(Debug::ipc, " RECEIVED "<<pkt);
	switch(pkt.type()){
		case Packet::KeepAliveType:
			COLLECT_DATA_0(d.statistics.receivedKeepAlive);
		case Packet::DataType:{
			COLLECT_DATA_0(d.statistics.receivedData);
			idbgx(Debug::ipc, "data packet");
			if(!pkt.isRelay()){
				SocketAddressInet4				inaddr(_rsa);
				Data::PeerAddr4MapT::iterator	pit(d.peeraddr4map.find(&inaddr));
				if(pit != d.peeraddr4map.end()){
					idbgx(Debug::ipc, "found session for packet "<<pit->second);
					d.receivedpktvec.push_back(Data::RecvPacket(_pbuf, _bufsz, pit->second));
					pkt.release();
				}else{
					COLLECT_DATA_0(d.statistics.receivedDataUnknown);
					if(pkt.check()){
						d.rservice.connectSession(inaddr);
					}
					//proc
					Packet::deallocate(pkt.release());
				}
			}else{
				uint16	sessidx;
				uint16	sessuid;
				unpack(sessidx, sessuid, pkt.relay());
				if(sessidx < d.sessionvec.size() && d.sessionvec[sessidx].uid == sessuid && d.sessionvec[sessidx].psession){
					idbgx(Debug::ipc, "found session for packet "<<sessidx<<','<<sessuid);
					d.receivedpktvec.push_back(Data::RecvPacket(_pbuf, _bufsz, sessidx));
					pkt.release();
				}else{
					Packet::deallocate(pkt.release());
				}
			}
		}break;
		
		case Packet::ConnectType:{
			COLLECT_DATA_0(d.statistics.receivedConnecting);
			ConnectData			conndata;
			
			bool				rv = Session::parseConnectPacket(pkt, conndata, _rsa);
			
			Packet::deallocate(pkt.release());
			
			if(rv){
				rv = d.rservice.acceptSession(_rsa, conndata);
				if(!rv){
					const int error = specific_error_back().value();
					if(error == 0){
						COLLECT_DATA_0(d.statistics.failedAcceptSession);
						wdbgx(Debug::ipc, "connecting packet: silent drop");
					}else{
						wdbgx(Debug::ipc, "connecting packet: send error");
						d.sessionvec.front().psession->dummySendError(_rstub, _rsa, error);
						if(!d.sessionvec.front().inexeq){
							d.sessionexecq.push(0);
							d.sessionvec.front().inexeq = true;
						}
					}
				}
			}else{
				edbgx(Debug::ipc, "connecting packet: parse");
				COLLECT_DATA_0(d.statistics.receivedConnectingError);
			}
			
		}break;
		
		case Packet::AcceptType:{
			COLLECT_DATA_0(d.statistics.receivedAccepting);
			AcceptData			accdata;
			
			const bool			rv = Session::parseAcceptPacket(pkt, accdata, _rsa);
			const bool			isrelay = pkt.isRelay();
			const uint32		relayid = isrelay ? pkt.relay() : 0;
			
			Packet::deallocate(pkt.release());
			
			if(!rv){
				edbgx(Debug::ipc, "accepted packet: error parse");
				COLLECT_DATA_0(d.statistics.receivedAcceptingError);
			}else if(d.rservice.checkAcceptData(_rsa, accdata)){
				if(!isrelay){
					SocketAddressInet4				sa(_rsa);
					BaseAddress4T					ba(sa, accdata.baseport);
					Data::BaseAddr4MapT::iterator	bit(d.baseaddr4map.find(ba));
					if(bit != d.baseaddr4map.end() && d.sessionvec[bit->second].psession){
						idbgx(Debug::ipc, "accept for session "<<bit->second<<" AcceptData: "<<accdata);
						Data::SessionStub	&rss(d.sessionvec[bit->second]);
						_rstub.sessionidx = bit->second;
						rss.psession->completeConnect(_rstub, _rsa.port());
						//register in peer map
						d.peeraddr4map[&rss.psession->peerAddress4()] = bit->second;
						//the connector has at least some updates to send
						if(!rss.inexeq){
							d.sessionexecq.push(bit->second);
							rss.inexeq = true;
						}
					}else{
						idbgx(Debug::ipc, "");
					}
				}else{
					uint16	sessidx;
					uint16	sessuid;
					unpack(sessidx, sessuid, relayid);
					if(sessidx < d.sessionvec.size() && d.sessionvec[sessidx].uid == sessuid && d.sessionvec[sessidx].psession){
						idbgx(Debug::ipc, "relay accept for session "<<sessidx<<','<<sessuid<<" AcceptData: "<<accdata);
						Data::SessionStub	&rss(d.sessionvec[sessidx]);
						_rstub.sessionidx = sessidx;
						rss.psession->completeConnect(_rstub, _rsa.port(), accdata.relayid);
						//register in peer map
						
						//d.peeraddr4map[&rss.psession->peerAddress4()] = sessidx;
						//TODO!!!
						
						//the connector has at least some updates to send
						if(!rss.inexeq){
							d.sessionexecq.push(sessidx);
							rss.inexeq = true;
						}
					}else{
						idbgx(Debug::ipc, "");
					}
					
				}
			}else{
				//TODO:...
				idbgx(Debug::ipc, "");
			}
		}break;
		case Packet::ErrorType:{
			if(!pkt.isRelay()){
				SocketAddressInet4				sa(_rsa);
				BaseAddress4T					ba(sa, _rsa.port());
				Data::BaseAddr4MapT::iterator	bit(d.baseaddr4map.find(ba));
				if(bit != d.baseaddr4map.end()){
					Data::SessionStub	&rss(d.sessionvec[bit->second]);
					if(rss.psession){
						_rstub.sessionidx = bit->second;
						
						if(rss.psession->pushReceivedErrorPacket(pkt, _rstub) && !rss.inexeq){
							d.sessionexecq.push(bit->second);
							rss.inexeq = true;
						}
					}else{
						wdbgx(Debug::ipc, "no session for error packet");
					}
				}else{
					wdbgx(Debug::ipc, "no session for error packet");
				}
			}else{
				uint16	sessidx;
				uint16	sessuid;
				unpack(sessidx, sessuid, pkt.relay());
				if(sessidx < d.sessionvec.size() && d.sessionvec[sessidx].uid == sessuid && d.sessionvec[sessidx].psession){
					idbgx(Debug::ipc, "found session for packet "<<sessidx<<','<<sessuid);
					d.receivedpktvec.push_back(Data::RecvPacket(_pbuf, _bufsz, sessidx));
					pkt.release();
				}else{
					Packet::deallocate(pkt.release());
				}
			}
			Packet::deallocate(pkt.release());
		}break;
		default:
			COLLECT_DATA_0(d.statistics.receivedUnknown);
			Packet::deallocate(pkt.release());
			cassert(false);
	}
}
//----------------------------------------------------------------------
bool Talker::doExecuteSessions(TalkerStub &_rstub){
	TalkerStub &ts = _rstub;
	COLLECT_DATA_1(d.statistics.maxTimerQueueSize, d.timerq.size());
	//first we consider all timers
	while(d.timerq.size() && d.timerq.top().timepos <= ts.currentTime()){
		const Data::TimerData		&rtd(d.timerq.top());
		Data::SessionStub			&rss(d.sessionvec[rtd.sessionidx]);
		ts.sessionidx = rtd.sessionidx;
		if(rss.psession && rss.psession->executeTimeout(ts, rtd.id)){
			if(!rss.inexeq){
				d.sessionexecq.push(ts.sessionidx);
				rss.inexeq = true;
			}
		}
		d.timerq.pop();
	}
	
	ulong sz(d.sessionexecq.size());
	COLLECT_DATA_1(d.statistics.maxSessionExecQueueSize, sz);
	while(sz--){
		ts.sessionidx = d.sessionexecq.front();
		d.sessionexecq.pop();
		Data::SessionStub	&rss(d.sessionvec[ts.sessionidx]);
		Context::the().msgctx.connectionuid.sesidx = ts.sessionidx;
		Context::the().msgctx.connectionuid.sesuid = rss.uid;
		
		rss.psession->prepareContext(Context::the());
		
		rss.inexeq = false;
		switch(rss.psession->execute(ts)){
			case AsyncSuccess:
				d.sessionexecq.push(ts.sessionidx);
			case AsyncWait:
				break;
			case AsyncError:
				d.closingsessionvec.push_back(ts.sessionidx);
				rss.inexeq = true;
				break;
		}
	}
	
	return d.sessionexecq.size() != 0 || d.closingsessionvec.size() != 0;
}
//----------------------------------------------------------------------
AsyncE Talker::doSendPackets(TalkerStub &_rstub, const ulong _sig){
	if(socketHasPendingSend()){
		return AsyncWait;
	}
	
	TalkerStub &ts = _rstub;
	
	if(_sig & frame::EventDoneSend){
		cassert(d.sendq.size());
		COLLECT_DATA_1(d.statistics.maxSendQueueSize, d.sendq.size());
		Data::SendPacket	&rsp(d.sendq.front());
		Data::SessionStub	&rss(d.sessionvec[rsp.sessionidx]);
		
		ts.sessionidx = rsp.sessionidx;
		
		Context::the().msgctx.connectionuid.sesidx = rsp.sessionidx;
		Context::the().msgctx.connectionuid.sesuid = rss.uid;
		rss.psession->prepareContext(Context::the());
		
		if(rss.psession->pushSentPacket(ts, rsp.id, rsp.data, rsp.size)){
			if(!rss.inexeq){
				d.sessionexecq.push(rsp.sessionidx);
				rss.inexeq = true;
			}
		}
		d.sendq.pop();
	}
	
	while(d.sendq.size()){
		Data::SendPacket	&rsp(d.sendq.front());
		Data::SessionStub	&rss(d.sessionvec[rsp.sessionidx]);
		
		switch(socketSendTo(rsp.data, rsp.size, rss.psession->peerAddress())){
			case aio::AsyncSuccess:
				Context::the().msgctx.connectionuid.sesidx = rsp.sessionidx;
				Context::the().msgctx.connectionuid.sesuid = rss.uid;
				rss.psession->prepareContext(Context::the());
				
				ts.sessionidx = rsp.sessionidx;
				
				if(rss.psession->pushSentPacket(ts, rsp.id, rsp.data, rsp.size)){
					if(!rss.inexeq){
						d.sessionexecq.push(rsp.sessionidx);
						rss.inexeq = true;
					}
				}
			case aio::AsyncWait:
				COLLECT_DATA_0(d.statistics.sendPending);
				return AsyncWait;
			case aio::AsyncError: return AsyncError;
		}
		d.sendq.pop();
	}
	return AsyncWait;
}
//----------------------------------------------------------------------
//The talker's mutex should be locked
//return ok if the talker should be notified
bool Talker::pushMessage(
	DynamicPointer<Message> &_rmsgptr,
	const SerializationTypeIdT &_rtid,
	const ConnectionUid &_rconid,
	uint32 _flags
){
	COLLECT_DATA_0(d.statistics.signaled);
	d.msgq.push(Data::MessageData(_rmsgptr, _rtid, _rconid.sesidx, _rconid.sesuid, _flags));
	return (d.msgq.size() == 1 && d.eventq.empty());
}
//----------------------------------------------------------------------
bool Talker::pushEvent(
	const ConnectionUid &_rconid,
	int32 _event,
	uint32 _flags
){
	d.eventq.push(Data::EventData(_rconid.sesidx, _rconid.sesuid, _event, _flags));
	return (d.eventq.size() == 1 && d.msgq.empty());
}
//----------------------------------------------------------------------
//The talker's mutex should be locked
//Return's the new process connector's id
void Talker::pushSession(Session *_pses, ConnectionUid &_rconid, bool _exists){
	vdbgx(Debug::ipc, "exists "<<_exists);
	if(_exists){
		++_rconid.sesuid;
	}else{
		if(d.freesessionstack.size()){
			vdbgx(Debug::ipc, "");
			_rconid.sesidx = d.freesessionstack.top().first;
			_rconid.sesuid = d.freesessionstack.top().second;
			d.freesessionstack.pop();
		}else{
			vdbgx(Debug::ipc, "");
			cassert(d.nextsessionidx < (uint16)0xffff);
			_rconid.sesidx = d.nextsessionidx;
			_rconid.sesuid = 0;
			++d.nextsessionidx;//TODO: it must be reseted somewhere!!!
		}
	}
	d.newsessionvec.push_back(Data::SessionPairT(_pses, _rconid.sesidx));
}
//----------------------------------------------------------------------
void Talker::doInsertNewSessions(TalkerStub &_rstub){
	for(Data::SessionPairVectorT::const_iterator it(d.newsessionvec.begin()); it != d.newsessionvec.end(); ++it){
		vdbgx(Debug::ipc, "newsession idx = "<<it->second<<" session vector size = "<<d.sessionvec.size());
		
		if(it->second >= d.sessionvec.size()){
			d.sessionvec.resize(it->second + 1);
		}
		
		Data::SessionStub &rss(d.sessionvec[it->second]);
		
		Context::the().msgctx.connectionuid.sesidx = it->second;
		Context::the().msgctx.connectionuid.sesuid = rss.uid;
		_rstub.sessionidx = it->second;
		
		if(rss.psession == NULL){
			rss.psession = it->first;
			rss.psession->prepare(_rstub);
			if(!rss.psession->isRelayType()){
				d.baseaddr4map[rss.psession->peerBaseAddress4()] = it->second;
				if(rss.psession->isConnected()){
					d.peeraddr4map[&rss.psession->peerAddress4()] = it->second;
				}
			}
			if(!rss.inexeq){
				d.sessionexecq.push(it->second);
				rss.inexeq = true;
			}
		}else{//a reconnect
			rss.psession->prepareContext(Context::the());
			if(!rss.psession->isRelayType()){
				d.peeraddr4map.erase(&rss.psession->peerAddress4());
			}
			rss.psession->reconnect(it->first);
			++rss.uid;
			if(!rss.psession->isRelayType()){
				d.peeraddr4map[&rss.psession->peerAddress4()] = it->second;
			}
			if(!rss.inexeq){
				d.sessionexecq.push(it->second);
				rss.inexeq = true;
			}
			Context::the().msgctx.connectionuid.sesidx = 0xffff;
			Context::the().msgctx.connectionuid.sesuid = 0xffff;
			it->first->prepareContext(Context::the());
			delete it->first;
		}
	}
	d.newsessionvec.clear();
	d.nextsessionidx = d.sessionvec.size();
}
//----------------------------------------------------------------------
void Talker::doDispatchMessages(){
	//dispatch signals before disconnecting sessions
	while(d.msgq.size()){
		cassert(d.msgq.front().sessionidx < d.sessionvec.size());
		Data::MessageData	&rsd(d.msgq.front());
		Data::SessionStub	&rss(d.sessionvec[rsd.sessionidx]);
		const uint32		flags(rsd.flags);
		
		Context::the().msgctx.connectionuid.sesidx = rsd.sessionidx;
		Context::the().msgctx.connectionuid.sesuid = rss.uid;

		if(
			rss.psession && 
			(
				!(flags & SameConnectorFlag) ||
				rss.uid == rsd.sessionuid
			)
		){
			rss.psession->prepareContext(Context::the());
			rss.psession->pushMessage(rsd.msgptr, rsd.stid, flags);
			if(!rss.inexeq){
				d.sessionexecq.push(rsd.sessionidx);
				rss.inexeq = true;
			}
		}
		
		d.msgq.pop();
	}
}
//----------------------------------------------------------------------
void Talker::doDispatchEvents(){
	while(d.eventq.size()){
		cassert(d.eventq.front().sessionidx < d.sessionvec.size());
		Data::EventData		&red(d.eventq.front());
		Data::SessionStub	&rss(d.sessionvec[red.sessionidx]);
		
		Context::the().msgctx.connectionuid.sesidx = red.sessionidx;
		Context::the().msgctx.connectionuid.sesuid = rss.uid;

		if(
			rss.psession && 
			rss.uid == red.sessionuid
		){
			rss.psession->prepareContext(Context::the());
			rss.psession->pushEvent(red.event, red.flags);
			if(!rss.inexeq){
				d.sessionexecq.push(red.sessionidx);
				rss.inexeq = true;
			}
		}
		
		d.msgq.pop();
	}
}
//----------------------------------------------------------------------
//this should be called under ipc service's mutex lock
void Talker::disconnectSessions(TalkerStub &_rstub){
	Manager 		&rm(d.rservice.manager());
	Locker<Mutex>	lock(rm.mutex(*this));
	
	if(d.newsessionvec.size()){
		doInsertNewSessions(_rstub);
	}
	
	for(Data::UInt16VectorT::const_iterator it(d.closingsessionvec.begin()); it != d.closingsessionvec.end(); ++it){
		Data::SessionStub &rss(d.sessionvec[*it]);
		vdbgx(Debug::ipc, "disconnecting sessions "<<(void*)rss.psession);
		cassert(rss.psession);
		
		Context::the().msgctx.connectionuid.sesidx = *it;
		Context::the().msgctx.connectionuid.sesuid = rss.uid;
		rss.psession->prepareContext(Context::the());
		
		if(rss.psession->isDisconnected()){
			idbgx(Debug::ipc, "deleting session "<<(void*)rss.psession<<" on pos "<<*it);
			d.rservice.disconnectSession(rss.psession);
			//unregister from base and peer:
			if(!rss.psession->isRelayType()){
				if(!rss.psession->peerAddress4().isInvalid()){
					d.peeraddr4map.erase(&rss.psession->peerAddress4());
				}
				if(
					!rss.psession->peerBaseAddress4().first.isInvalid()
				){
					d.baseaddr4map.erase(rss.psession->peerBaseAddress4());
				}
			}
			delete rss.psession;
			rss.psession = NULL;
			rss.inexeq = false;
			++rss.uid;
			d.freesessionstack.push(Data::UInt16PairT(*it, rss.uid));
		}
	}
	d.closingsessionvec.clear();
}
//----------------------------------------------------------------------
bool TalkerStub::pushSendPacket(uint32 _id, const char *_pb, uint32 _bl){
	if(rt.d.sendq.size()){
		rt.d.sendq.push(Talker::Data::SendPacket(_pb, _bl, this->sessionidx, _id));
		return false;
	}
	Talker::Data::SessionStub &rss(rt.d.sessionvec[this->sessionidx]);
	//try to send the packet right now:
	switch(rt.socketSendTo(_pb, _bl, rss.psession->peerAddress())){
		case aio::AsyncSuccess:
			break;
		case aio::AsyncError:
		case aio::AsyncWait:
			rt.d.sendq.push(Talker::Data::SendPacket(_pb, _bl, this->sessionidx, _id));
			COLLECT_DATA_0(rt.d.statistics.sendPending);
			return false;
	}
	return true;//the packet was written on socket
}
uint32 TalkerStub::relayId()const{
	return pack(sessionidx, rt.d.sessionvec[sessionidx].uid);
}
//----------------------------------------------------------------------
void TalkerStub::pushTimer(uint32 _id, const TimeSpec &_rtimepos){
	COLLECT_DATA_0(rt.d.statistics.pushTimer);
	rt.d.timerq.push(Talker::Data::TimerData(_rtimepos, _id, this->sessionidx));
}
//----------------------------------------------------------------------
int TalkerStub::basePort()const{
	return rt.d.rservice.basePort();
}
//======================================================================
std::ostream& ConnectData::print(std::ostream& _ros)const{
	_ros<<s<<f<<i<<p<<c<<' '<<(int)type<<' '<<version_major<<'.'<<version_minor<<' ';
	_ros<<flags<<' '<<baseport<<' '<<timestamp_s<<':'<<timestamp_n<<' ';
	_ros<<relayid<<' '<<receivernetworkid<<':';
	
	std::string	hoststr;
	std::string	portstr;
	
	receiveraddress.toString(
		hoststr,
		portstr,
		ReverseResolveInfo::NumericService | ReverseResolveInfo::NumericHost
	);
	_ros<<hoststr<<':'<<portstr<<"<-"<<sendernetworkid<<':';
	senderaddress.toString(
		hoststr,
		portstr,
		ReverseResolveInfo::NumericService | ReverseResolveInfo::NumericHost
	);
	_ros<<hoststr<<':'<<portstr;
	return _ros;
}

std::ostream& AcceptData::print(std::ostream& _ros)const{
	_ros<<flags<<' '<<baseport<<' '<<timestamp_s<<':'<<timestamp_n<<' '<<relayid;
	return _ros;
}

std::ostream& operator<<(std::ostream& _ros, const ConnectData &_rd){
	return _rd.print(_ros);
}
std::ostream& operator<<(std::ostream& _ros, const AcceptData &_rd){
	return _rd.print(_ros);
}
//======================================================================
#ifdef USTATISTICS

namespace{

StatisticData::~StatisticData(){
	rdbgx(Debug::ipc, "Statistics:\r\n"<<*this);
}
	
void StatisticData::receivedManyPackets(){
	++rcvdmannypackets;
}
void StatisticData::receivedKeepAlive(){
	++rcvdkeepalive;
}
void StatisticData::receivedData(){
	++rcvddata;
}
void StatisticData::receivedDataUnknown(){
	++rcvddataunknown;
}
void StatisticData::receivedConnecting(){
	++rcvdconnecting;
}
void StatisticData::failedAcceptSession(){
	++failedacceptsession;
}
void StatisticData::receivedConnectingError(){
	++rcvdconnectingerror;
}
void StatisticData::receivedAccepting(){
	++rcvdaccepting;
}
void StatisticData::receivedAcceptingError(){
	++rcvdacceptingerror;
}
void StatisticData::receivedUnknown(){
	++rcvdunknown;
}
void StatisticData::maxTimerQueueSize(ulong _sz){
	if(maxtimerqueuesize < _sz) maxtimerqueuesize = _sz;
}
void StatisticData::maxSessionExecQueueSize(ulong _sz){
	if(maxsessionexecqueuesize < _sz) maxsessionexecqueuesize = _sz;
}
void StatisticData::maxSendQueueSize(ulong _sz){
	if(maxsendqueuesize < _sz) maxsendqueuesize = _sz;
}
void StatisticData::sendPending(){
	++sendpending;
}
void StatisticData::signaled(){
	++signaledcount;
}
void StatisticData::pushTimer(){
	++pushtimercount;
}
void StatisticData::receivedPackets0(){
	++receivedpackets0;
}
void StatisticData::receivedPackets1(){
	++receivedpackets1;
}
void StatisticData::receivedPackets2(){
	++receivedpackets2;
}
void StatisticData::receivedPackets3(){
	++receivedpackets3;
}
void StatisticData::receivedPackets4(){
	++receivedpackets4;
}


std::ostream& operator<<(std::ostream &_ros, const StatisticData &_rsd){
	_ros<<"rcvdmannypackets          "<<_rsd.rcvdmannypackets<<std::endl;
	_ros<<"rcvdkeepalive             "<<_rsd.rcvdkeepalive<<std::endl;
	_ros<<"rcvddata                  "<<_rsd.rcvddata<<std::endl;
	_ros<<"rcvddataunknown           "<<_rsd.rcvddataunknown<<std::endl;
	_ros<<"rcvdconnecting            "<<_rsd.rcvdconnecting<<std::endl;
	_ros<<"failedacceptsession       "<<_rsd.failedacceptsession<<std::endl;
	_ros<<"rcvdconnectingerror       "<<_rsd.rcvdconnectingerror<<std::endl;
	_ros<<"rcvdaccepting             "<<_rsd.rcvdaccepting<<std::endl;
	_ros<<"rcvdacceptingerror        "<<_rsd.rcvdacceptingerror<<std::endl;
	_ros<<"rcvdunknown               "<<_rsd.rcvdunknown<<std::endl;
	_ros<<"maxtimerqueuesize         "<<_rsd.maxtimerqueuesize<<std::endl;
	_ros<<"maxsessionexecqueuesize   "<<_rsd.maxsessionexecqueuesize<<std::endl;
	_ros<<"maxsendqueuesize          "<<_rsd.maxsendqueuesize<<std::endl;
	_ros<<"sendpending               "<<_rsd.sendpending<<std::endl;
	_ros<<"signaledcount             "<<_rsd.signaledcount<<std::endl;
	_ros<<"pushtimercount            "<<_rsd.pushtimercount<<std::endl;
	_ros<<"receivedpackets0          "<<_rsd.receivedpackets0<<std::endl;
	_ros<<"receivedpackets1          "<<_rsd.receivedpackets1<<std::endl;
	_ros<<"receivedpackets2          "<<_rsd.receivedpackets2<<std::endl;
	_ros<<"receivedpackets3          "<<_rsd.receivedpackets3<<std::endl;
	_ros<<"receivedpackets4          "<<_rsd.receivedpackets4<<std::endl;
	return _ros;
}

}//namespace
#endif
}//namespace ipc
}//namesapce frame
}//namesapce solid


