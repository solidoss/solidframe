/* Implementation file ipctalker.cpp
	
	Copyright 2007, 2008 Valentin Palade 
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
#include "ipcbuffer.hpp"

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
	
	void receivedManyBuffers();
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
	void receivedBuffers0();
	void receivedBuffers1();
	void receivedBuffers2();
	void receivedBuffers3();
	void receivedBuffers4();
	
	ulong	rcvdmannybuffers;
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
	ulong	receivedbuffers0;
	ulong	receivedbuffers1;
	ulong	receivedbuffers2;
	ulong	receivedbuffers3;
	ulong	receivedbuffers4;
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
	struct RecvBuffer{
		RecvBuffer(
			char *_data = NULL,
			uint16 _size = 0,
			uint16 _sessionidx = 0
		):data(_data), size(_size), sessionidx(_sessionidx){}
		char	*data;
		uint16	size;
		uint16	sessionidx;
	};
	struct SendBuffer{
		SendBuffer(
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
	
	
	typedef std::vector<RecvBuffer>				RecvBufferVectorT;
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
	typedef Queue<SendBuffer>					SendQueueT;
	
public:
	Data(
		Service &_rservice,
		uint16 _id
	):	rservice(_rservice), tkrid(_id), pendingreadbuffer(NULL), nextsessionidx(1){
	}
	~Data();
public:
	Service					&rservice;
	const uint16			tkrid;
	RecvBufferVectorT		receivedbufvec;
	char					*pendingreadbuffer;
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
	if(pendingreadbuffer){
		Buffer::deallocate(pendingreadbuffer);
	}
	for(RecvBufferVectorT::const_iterator it(receivedbufvec.begin()); it != receivedbufvec.end(); ++it){
		Buffer::deallocate(it->data);
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
	uint16 _id
):BaseT(_rsd), d(*new Data(_rservice, _id)){
	d.sessionvec.push_back(
		Data::SessionStub(new Session(_rservice))
	);
}

//----------------------------------------------------------------------
Talker::~Talker(){
	Context		ctx(d.rservice, d.tkrid, d.rservice.manager().id(*this).second);
	delete &d;
}
//----------------------------------------------------------------------
int Talker::execute(ulong _sig, TimeSpec &_tout){
	Manager		&rm = d.rservice.manager();
	Context		ctx(d.rservice, d.tkrid, rm.id(*this).second);
	TalkerStub	ts(*this, d.rservice, _tout);
	
	idbgx(Debug::ipc, "this = "<<(void*)this<<" &d = "<<(void*)&d);
	{
		const ulong		sm = grabSignalMask();
		if(sm/* || d.closingsessionvec.size()*/){
			if(sm & frame::S_KILL){
				idbgx(Debug::ipc, "talker - dying");
				return BAD;
			}
			
			idbgx(Debug::ipc, "talker - signaled");
			if(sm == frame::S_RAISE){
				_sig |= frame::SIGNALED;
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
	_sig |= socketEventsGrab();
	
	if(_sig & frame::ERRDONE){
		return BAD;
	}
	
	bool	must_reenter(false);
	int		rv;
	
	if(d.closingsessionvec.size()){
		//this is to ensure the locking order: first service then talker
		d.rservice.disconnectTalkerSessions(*this);
	}
	
	rv = doReceiveBuffers(ts, 4, _sig);
	
	if(rv == OK){
		must_reenter = true;
	}else if(rv == BAD){
		return BAD;
	}
	
	must_reenter = doProcessReceivedBuffers(ts) || must_reenter;
	
	rv = doSendBuffers(ts, _sig);
	if(rv == OK){
		must_reenter = true;
	}else if(rv == BAD){
		return BAD;
	}
	
	must_reenter = doExecuteSessions(ts) || must_reenter;
	
	if(d.timerq.size()){
		_tout = d.timerq.top().timepos;
	}
	
	return must_reenter ? OK : NOK;
}

//----------------------------------------------------------------------
int Talker::doReceiveBuffers(TalkerStub &_rstub, uint _atmost, const ulong _sig){
	if(this->socketHasPendingRecv()){
		return NOK;
	}
	if(_sig & frame::INDONE){
		doDispatchReceivedBuffer(_rstub, d.pendingreadbuffer, socketRecvSize(), socketRecvAddr());
		d.pendingreadbuffer = NULL;
	}
	while(_atmost--){
		char 			*pbuf(Buffer::allocate());
		const uint32	bufsz(Buffer::Capacity);
		switch(socketRecvFrom(pbuf, bufsz)){
			case BAD:
				Buffer::deallocate(pbuf);
				return BAD;
			case OK:
				doDispatchReceivedBuffer(_rstub, pbuf, socketRecvSize(), socketRecvAddr());
				break;
			case NOK:
				d.pendingreadbuffer = pbuf;
				return NOK;
		}
	}
	
	COLLECT_DATA_0(d.statistics.receivedManyBuffers);
	return OK;//can still read from socket
}
//----------------------------------------------------------------------
bool Talker::doPreprocessReceivedBuffers(TalkerStub &_rstub){
	for(Data::RecvBufferVectorT::const_iterator it(d.receivedbufvec.begin()); it != d.receivedbufvec.end(); ++it){
		
		const Data::RecvBuffer	&rcvbuf(*it);
		Data::SessionStub		&rss(d.sessionvec[rcvbuf.sessionidx]);
		Buffer					buf(rcvbuf.data, Buffer::Capacity);
		
		buf.bufferSize(rcvbuf.size);
		
		//conuid.sessionidx = rcvbuf.sessionidx;
		//conuid.sessionuid = rss.uid;
// 		Context::the().msgctx.connectionuid.idx = rcvbuf.sessionidx;
// 		Context::the().msgctx.connectionuid.uid = rss.uid;
// 		ts.sessionidx = rcvbuf.sessionidx;
// 		rss.psession->prepareContext(Context::the());
		
		if(rss.psession->preprocessReceivedBuffer(buf, _rstub)){
			if(!rss.inexeq){
				d.sessionexecq.push(rcvbuf.sessionidx);
				rss.inexeq = true;
			}
		}
		buf.release();
	}
	return false;
}
//----------------------------------------------------------------------
bool Talker::doProcessReceivedBuffers(TalkerStub &_rstub){
	//ConnectionUid	conuid(d.tkrid);
	TalkerStub		&ts = _rstub;
#ifdef USTATISTICS	
	if(d.receivedbufvec.size() == 0){
		COLLECT_DATA_0(d.statistics.receivedBuffers0);
	}else if(d.receivedbufvec.size() == 1){
		COLLECT_DATA_0(d.statistics.receivedBuffers1);
	}else if(d.receivedbufvec.size() == 2){
		COLLECT_DATA_0(d.statistics.receivedBuffers2);
	}else if(d.receivedbufvec.size() == 3){
		COLLECT_DATA_0(d.statistics.receivedBuffers3);
	}else if(d.receivedbufvec.size() == 4){
		COLLECT_DATA_0(d.statistics.receivedBuffers4);
	}
#endif
	
	for(Data::RecvBufferVectorT::const_iterator it(d.receivedbufvec.begin()); it != d.receivedbufvec.end(); ++it){
		
		const Data::RecvBuffer	&rcvbuf(*it);
		Data::SessionStub		&rss(d.sessionvec[rcvbuf.sessionidx]);
		Buffer					buf(rcvbuf.data, Buffer::Capacity);
		
		buf.bufferSize(rcvbuf.size);
		
		//conuid.sessionidx = rcvbuf.sessionidx;
		//conuid.sessionuid = rss.uid;
		Context::the().msgctx.connectionuid.idx = rcvbuf.sessionidx;
		Context::the().msgctx.connectionuid.uid = rss.uid;
		ts.sessionidx = rcvbuf.sessionidx;
		rss.psession->prepareContext(Context::the());
		
		if(rss.psession->pushReceivedBuffer(buf, ts/*, conuid*/)){
			if(!rss.inexeq){
				d.sessionexecq.push(rcvbuf.sessionidx);
				rss.inexeq = true;
			}
		}
	}
	d.receivedbufvec.clear();
	return false;
}
//----------------------------------------------------------------------
void Talker::doDispatchReceivedBuffer(
	TalkerStub &_rstub,
	char *_pbuf,
	const uint32 _bufsz,
	const SocketAddress &_rsa
){
	Buffer buf(_pbuf, Buffer::Capacity);
	buf.bufferSize(_bufsz);
	vdbgx(Debug::ipc, " RECEIVED "<<buf);
	switch(buf.type()){
		case Buffer::KeepAliveType:
			COLLECT_DATA_0(d.statistics.receivedKeepAlive);
		case Buffer::DataType:{
			COLLECT_DATA_0(d.statistics.receivedData);
			idbgx(Debug::ipc, "data buffer");
			if(!buf.isRelay()){
				SocketAddressInet4				inaddr(_rsa);
				Data::PeerAddr4MapT::iterator	pit(d.peeraddr4map.find(&inaddr));
				if(pit != d.peeraddr4map.end()){
					idbgx(Debug::ipc, "found session for buffer "<<pit->second);
					d.receivedbufvec.push_back(Data::RecvBuffer(_pbuf, _bufsz, pit->second));
					buf.release();
				}else{
					COLLECT_DATA_0(d.statistics.receivedDataUnknown);
					if(buf.check()){
						d.rservice.connectSession(inaddr);
					}
					//proc
					Buffer::deallocate(buf.release());
				}
			}else{
				uint16	sessidx;
				uint16	sessuid;
				unpack(sessidx, sessuid, buf.relay());
				if(sessidx < d.sessionvec.size() && d.sessionvec[sessidx].uid == sessuid && d.sessionvec[sessidx].psession){
					idbgx(Debug::ipc, "found session for buffer "<<sessidx<<','<<sessuid);
					d.receivedbufvec.push_back(Data::RecvBuffer(_pbuf, _bufsz, sessidx));
					buf.release();
				}else{
					Buffer::deallocate(buf.release());
				}
			}
		}break;
		
		case Buffer::ConnectType:{
			COLLECT_DATA_0(d.statistics.receivedConnecting);
			ConnectData			conndata;
			
			int					error = Session::parseConnectBuffer(buf, conndata, _rsa);
			
			Buffer::deallocate(buf.release());
			
			if(error){
				edbgx(Debug::ipc, "connecting buffer: parse "<<error);
				COLLECT_DATA_0(d.statistics.receivedConnectingError);
			}else{
				error = d.rservice.acceptSession(_rsa, conndata);
				if(error < 0){	
					COLLECT_DATA_0(d.statistics.failedAcceptSession);
					wdbgx(Debug::ipc, "connecting buffer: silent drop "<<error);
				}else if(error > 0){
					wdbgx(Debug::ipc, "connecting buffer: send error "<<error);
					d.sessionvec.front().psession->dummySendError(_rstub, _rsa, error);
					if(!d.sessionvec.front().inexeq){
						d.sessionexecq.push(0);
						d.sessionvec.front().inexeq = true;
					}
				}
			}
			
		}break;
		
		case Buffer::AcceptType:{
			COLLECT_DATA_0(d.statistics.receivedAccepting);
			AcceptData			accdata;
			
			int					error = Session::parseAcceptBuffer(buf, accdata, _rsa);
			const bool			isrelay = buf.isRelay();
			const uint32		relayid = isrelay ? buf.relay() : 0;
			
			Buffer::deallocate(buf.release());
			
			if(error){
				edbgx(Debug::ipc, "accepted buffer: error parse "<<error);
				COLLECT_DATA_0(d.statistics.receivedAcceptingError);
			}else if(d.rservice.checkAcceptData(_rsa, accdata)){
				if(!isrelay){
					SocketAddressInet4				sa(_rsa);
					BaseAddress4T					ba(sa, accdata.baseport);
					Data::BaseAddr4MapT::iterator	bit(d.baseaddr4map.find(ba));
					if(bit != d.baseaddr4map.end() && d.sessionvec[bit->second].psession){
						idbgx(Debug::ipc, "accept for session "<<bit->second);
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
						idbgx(Debug::ipc, "relay accept for session "<<sessidx<<','<<sessuid);
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
		case Buffer::ErrorType:{
			SocketAddressInet4				sa(_rsa);
			BaseAddress4T					ba(sa, _rsa.port());
			Data::BaseAddr4MapT::iterator	bit(d.baseaddr4map.find(ba));
			if(bit != d.baseaddr4map.end()){
				Data::SessionStub	&rss(d.sessionvec[bit->second]);
				if(rss.psession){
					_rstub.sessionidx = bit->second;
					
					if(rss.psession->pushReceivedErrorBuffer(buf, _rstub) && !rss.inexeq){
						d.sessionexecq.push(bit->second);
						rss.inexeq = true;
					}
				}else{
					wdbgx(Debug::ipc, "no session for error buffer");
				}
			}else{
				wdbgx(Debug::ipc, "no session for error buffer");
			}
			Buffer::deallocate(buf.release());
		}break;
		default:
			COLLECT_DATA_0(d.statistics.receivedUnknown);
			Buffer::deallocate(buf.release());
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
		Context::the().msgctx.connectionuid.idx = ts.sessionidx;
		Context::the().msgctx.connectionuid.uid = rss.uid;
		
		rss.psession->prepareContext(Context::the());
		
		rss.inexeq = false;
		switch(rss.psession->execute(ts)){
			case BAD:
				d.closingsessionvec.push_back(ts.sessionidx);
				rss.inexeq = true;
				break;
			case OK:
				d.sessionexecq.push(ts.sessionidx);
			case NOK:
				break;
		}
	}
	
	return d.sessionexecq.size() != 0 || d.closingsessionvec.size() != 0;
}
//----------------------------------------------------------------------
int Talker::doSendBuffers(TalkerStub &_rstub, const ulong _sig){
	if(socketHasPendingSend()){
		return NOK;
	}
	
	TalkerStub &ts = _rstub;
	
	if(_sig & frame::OUTDONE){
		cassert(d.sendq.size());
		COLLECT_DATA_1(d.statistics.maxSendQueueSize, d.sendq.size());
		Data::SendBuffer	&rsb(d.sendq.front());
		Data::SessionStub	&rss(d.sessionvec[rsb.sessionidx]);
		
		ts.sessionidx = rsb.sessionidx;
		
		Context::the().msgctx.connectionuid.idx = rsb.sessionidx;
		Context::the().msgctx.connectionuid.uid = rss.uid;
		rss.psession->prepareContext(Context::the());
		
		if(rss.psession->pushSentBuffer(ts, rsb.id, rsb.data, rsb.size)){
			if(!rss.inexeq){
				d.sessionexecq.push(rsb.sessionidx);
				rss.inexeq = true;
			}
		}
		d.sendq.pop();
	}
	
	while(d.sendq.size()){
		Data::SendBuffer	&rsb(d.sendq.front());
		Data::SessionStub	&rss(d.sessionvec[rsb.sessionidx]);
		
		switch(socketSendTo(rsb.data, rsb.size, rss.psession->peerAddress())){
			case BAD: return BAD;
			case OK:
				Context::the().msgctx.connectionuid.idx = rsb.sessionidx;
				Context::the().msgctx.connectionuid.uid = rss.uid;
				rss.psession->prepareContext(Context::the());
				
				ts.sessionidx = rsb.sessionidx;
				
				if(rss.psession->pushSentBuffer(ts, rsb.id, rsb.data, rsb.size)){
					if(!rss.inexeq){
						d.sessionexecq.push(rsb.sessionidx);
						rss.inexeq = true;
					}
				}
			case NOK:
				COLLECT_DATA_0(d.statistics.sendPending);
				return NOK;
		}
		d.sendq.pop();
	}
	return NOK;
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
	d.msgq.push(Data::MessageData(_rmsgptr, _rtid, _rconid.idx, _rconid.uid, _flags));
	return (d.msgq.size() == 1 && d.eventq.empty());
}
//----------------------------------------------------------------------
bool Talker::pushEvent(
	const ConnectionUid &_rconid,
	int32 _event,
	uint32 _flags
){
	d.eventq.push(Data::EventData(_rconid.idx, _rconid.uid, _event, _flags));
	return (d.eventq.size() == 1 && d.msgq.empty());
}
//----------------------------------------------------------------------
//The talker's mutex should be locked
//Return's the new process connector's id
void Talker::pushSession(Session *_pses, ConnectionUid &_rconid, bool _exists){
	vdbgx(Debug::ipc, "exists "<<_exists);
	if(_exists){
		++_rconid.uid;
	}else{
		if(d.freesessionstack.size()){
			vdbgx(Debug::ipc, "");
			_rconid.idx = d.freesessionstack.top().first;
			_rconid.uid = d.freesessionstack.top().second;
			d.freesessionstack.pop();
		}else{
			vdbgx(Debug::ipc, "");
			cassert(d.nextsessionidx < (uint16)0xffff);
			_rconid.idx = d.nextsessionidx;
			_rconid.uid = 0;
			++d.nextsessionidx;//TODO: it must be reseted somewhere!!!
		}
	}
	d.newsessionvec.push_back(Data::SessionPairT(_pses, _rconid.idx));
}
//----------------------------------------------------------------------
void Talker::doInsertNewSessions(TalkerStub &_rstub){
	for(Data::SessionPairVectorT::const_iterator it(d.newsessionvec.begin()); it != d.newsessionvec.end(); ++it){
		vdbgx(Debug::ipc, "newsession idx = "<<it->second<<" session vector size = "<<d.sessionvec.size());
		
		if(it->second >= d.sessionvec.size()){
			d.sessionvec.resize(it->second + 1);
		}
		
		Data::SessionStub &rss(d.sessionvec[it->second]);
		
		Context::the().msgctx.connectionuid.idx = it->second;
		Context::the().msgctx.connectionuid.uid = rss.uid;
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
			Context::the().msgctx.connectionuid.idx = 0xffff;
			Context::the().msgctx.connectionuid.uid = 0xffff;
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
		
		Context::the().msgctx.connectionuid.idx = rsd.sessionidx;
		Context::the().msgctx.connectionuid.uid = rss.uid;

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
		
		Context::the().msgctx.connectionuid.idx = red.sessionidx;
		Context::the().msgctx.connectionuid.uid = rss.uid;

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
void Talker::disconnectSessions(){
	Manager 		&rm(d.rservice.manager());
	Locker<Mutex>	lock(rm.mutex(*this));
	//delete sessions
	
	for(Data::UInt16VectorT::const_iterator it(d.closingsessionvec.begin()); it != d.closingsessionvec.end(); ++it){
		Data::SessionStub &rss(d.sessionvec[*it]);
		vdbgx(Debug::ipc, "disconnecting sessions "<<(void*)rss.psession);
		cassert(rss.psession);
		
		Context::the().msgctx.connectionuid.idx = *it;
		Context::the().msgctx.connectionuid.uid = rss.uid;
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
int Talker::execute(){
	return BAD;
}
//----------------------------------------------------------------------
bool Talker::TalkerStub::pushSendBuffer(uint32 _id, const char *_pb, uint32 _bl){
	if(rt.d.sendq.size()){
		rt.d.sendq.push(Talker::Data::SendBuffer(_pb, _bl, this->sessionidx, _id));
		return false;
	}
	Talker::Data::SessionStub &rss(rt.d.sessionvec[this->sessionidx]);
	//try to send the buffer right now:
	switch(rt.socketSendTo(_pb, _bl, rss.psession->peerAddress())){
		case BAD:
		case NOK:
			rt.d.sendq.push(Talker::Data::SendBuffer(_pb, _bl, this->sessionidx, _id));
			COLLECT_DATA_0(rt.d.statistics.sendPending);
			return false;
		case OK:
			break;
	}
	return true;//the buffer was written on socket
}
uint32 Talker::TalkerStub::relayId()const{
	return pack(sessionidx, rt.d.sessionvec[sessionidx].uid);
}
//----------------------------------------------------------------------
void Talker::TalkerStub::pushTimer(uint32 _id, const TimeSpec &_rtimepos){
	COLLECT_DATA_0(rt.d.statistics.pushTimer);
	rt.d.timerq.push(Data::TimerData(_rtimepos, _id, this->sessionidx));
}
//----------------------------------------------------------------------
int Talker::TalkerStub::basePort()const{
	return rt.d.rservice.basePort();
}
//======================================================================
#ifdef USTATISTICS

namespace{

StatisticData::~StatisticData(){
	rdbgx(Debug::ipc, "Statistics:\r\n"<<*this);
}
	
void StatisticData::receivedManyBuffers(){
	++rcvdmannybuffers;
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
void StatisticData::receivedBuffers0(){
	++receivedbuffers0;
}
void StatisticData::receivedBuffers1(){
	++receivedbuffers1;
}
void StatisticData::receivedBuffers2(){
	++receivedbuffers2;
}
void StatisticData::receivedBuffers3(){
	++receivedbuffers3;
}
void StatisticData::receivedBuffers4(){
	++receivedbuffers4;
}


std::ostream& operator<<(std::ostream &_ros, const StatisticData &_rsd){
	_ros<<"rcvdmannybuffers          "<<_rsd.rcvdmannybuffers<<std::endl;
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
	_ros<<"receivedbuffers0          "<<_rsd.receivedbuffers0<<std::endl;
	_ros<<"receivedbuffers1          "<<_rsd.receivedbuffers1<<std::endl;
	_ros<<"receivedbuffers2          "<<_rsd.receivedbuffers2<<std::endl;
	_ros<<"receivedbuffers3          "<<_rsd.receivedbuffers3<<std::endl;
	_ros<<"receivedbuffers4          "<<_rsd.receivedbuffers4<<std::endl;
	return _ros;
}
}//namespace
#endif
}//namespace ipc
}//namesapce frame
}//namesapce solid


