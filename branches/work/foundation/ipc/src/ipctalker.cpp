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

#include "foundation/manager.hpp"

#include "foundation/ipc/ipcservice.hpp"
#include "foundation/ipc/ipcconnectionuid.hpp"

#include "ipctalker.hpp"
#include "ipcsession.hpp"
#include "ipcbuffer.hpp"

#ifdef HAVE_CPP11

#include <unordered_map>

#else

#include <map>

#endif
namespace fdt = foundation;

namespace foundation{
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
};

std::ostream& operator<<(std::ostream &_ros, const StatisticData &_rsd);
}//namespace
#endif


struct Talker::Data{
	struct SignalData{
		SignalData(
			DynamicPointer<Signal> &_psig,
			const SerializationTypeIdT &_rtid,
			uint16 _sesidx,
			uint16 _sesuid,
			uint32 _flags
		):	psig(_psig), tid(_rtid), sessionidx(_sesidx),
			sessionuid(_sesuid), flags(_flags){}
		
		DynamicPointer<Signal>	psig;
		SerializationTypeIdT 	tid;
		uint16					sessionidx;
		uint16					sessionuid;
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
	typedef Queue<SignalData>					SignalQueueT;
	typedef std::pair<uint16, uint16>			UInt16PairT;
	typedef Stack<UInt16PairT>					UInt16PairStackT;
	typedef std::pair<Session*, uint16>			SessionPairT;
	typedef std::vector<SessionPairT>			SessionPairVectorT;
	typedef std::vector<uint16>					UInt16VectorT;
	typedef std::vector<SessionStub>			SessionStubVectorT;
	typedef Queue<uint16>						UInt16QueueT;
	typedef std::pair<
		const SocketAddressPair4*,
		int
	>											BaseAddr4;
	typedef std::pair<
		const SocketAddressPair6*,
		int
	>											BaseAddr6;
#ifdef HAVE_CPP11
	typedef std::unordered_map<
		const SocketAddressPair4,
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
		const SocketAddressPair6, 
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
		const SocketAddressPair4,
		uint32,
		SocketAddressCompare
	>											PeerAddr4MapT;
	typedef std::map<
		const BaseAddress4T,
		uint32,
		SocketAddressCompare
	>											BaseAddr4MapT;
	
	typedef std::map<
		const SocketAddressPair6, 
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
	):	rservice(_rservice), tkrid(_id), pendingreadbuffer(NULL), nextsessionidx(0){
	}
	~Data();
public:
	Service					&rservice;
	uint16					tkrid;
	RecvBufferVectorT		receivedbufvec;
	char					*pendingreadbuffer;
	SignalQueueT			sigq;
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
}

//----------------------------------------------------------------------
Talker::~Talker(){
	Context		ctx(d.tkrid);
	delete &d;
}
//----------------------------------------------------------------------
int Talker::execute(ulong _sig, TimeSpec &_tout){
	Manager		&rm = Manager::the();
	Context		ctx(d.tkrid);
	TalkerStub	ts(*this, d.rservice, _tout);
	idbgx(Dbg::ipc, "this = "<<(void*)this<<" &d = "<<(void*)&d);
	if(signaled() || d.closingsessionvec.size()){
		Locker<Mutex>	lock(rm.mutex(*this));
		ulong			sm = grabSignalMask(0);
		
		if(sm & fdt::S_KILL){
			idbgx(Dbg::ipc, "talker - dying");
			return BAD;
		}
		
		idbgx(Dbg::ipc, "talker - signaled");
		if(sm == fdt::S_RAISE){
			_sig |= fdt::SIGNALED;
		}else{
			idbgx(Dbg::ipc, "unknown signal");
		}
		if(d.newsessionvec.size()){
			doInsertNewSessions();
		}
		if(d.sigq.size()){
			doDispatchSignals();
		}
	}
	
	_sig |= socketEventsGrab();
	
	if(_sig & fdt::ERRDONE){
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
	if(_sig & fdt::INDONE){
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
bool Talker::doProcessReceivedBuffers(TalkerStub &_rstub){
	//ConnectionUid	conuid(d.tkrid);
	TalkerStub		&ts = _rstub;
	
	for(Data::RecvBufferVectorT::const_iterator it(d.receivedbufvec.begin()); it != d.receivedbufvec.end(); ++it){
		
		const Data::RecvBuffer	&rcvbuf(*it);
		Data::SessionStub		&rss(d.sessionvec[rcvbuf.sessionidx]);
		Buffer					buf(rcvbuf.data, Buffer::Capacity);
		
		buf.bufferSize(rcvbuf.size);
		
		//conuid.sessionidx = rcvbuf.sessionidx;
		//conuid.sessionuid = rss.uid;
		Context::the().sigctx.connectionuid.idx = rcvbuf.sessionidx;
		Context::the().sigctx.connectionuid.uid = rss.uid;
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
	const SocketAddressPair &_rsap
){
	Buffer buf(_pbuf, Buffer::Capacity);
	buf.bufferSize(_bufsz);
	vdbgx(Dbg::ipc, " RECEIVED "<<buf);
	switch(buf.type()){
		case Buffer::KeepAliveType:
			COLLECT_DATA_0(d.statistics.receivedKeepAlive);
		case Buffer::DataType:{
			COLLECT_DATA_0(d.statistics.receivedData);
			idbgx(Dbg::ipc, "data buffer");
			SocketAddressPair4				inaddr(_rsap);
			Data::PeerAddr4MapT::iterator	pit(d.peeraddr4map.find(inaddr));
			if(pit != d.peeraddr4map.end()){
				idbgx(Dbg::ipc, "found session for buffer");
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
		
		}break;
		
		case Buffer::ConnectingType:{
			COLLECT_DATA_0(d.statistics.receivedConnecting);
			SocketAddressPair4	inaddr(_rsap);
			int					baseport(Session::parseConnectingBuffer(buf));
			
			idbgx(Dbg::ipc, "connecting buffer with baseport "<<baseport);
			
			if(baseport >= 0){
				Session *ps(new Session(inaddr, baseport, d.rservice.keepAliveTimeout()));
				if(d.rservice.acceptSession(ps)){	
					COLLECT_DATA_0(d.statistics.failedAcceptSession);
					delete ps;
				}
			}else{
				COLLECT_DATA_0(d.statistics.receivedConnectingError);
			}
			Buffer::deallocate(buf.release());
		}break;
		
		case Buffer::AcceptingType:{
			COLLECT_DATA_0(d.statistics.receivedAccepting);
			int baseport = Session::parseAcceptedBuffer(buf);
			
			idbgx(Dbg::ipc, "accepting buffer with baseport "<<baseport);
			
			if(baseport >= 0){
				
				BaseAddress4T					ba(_rsap, baseport);
				Data::BaseAddr4MapT::iterator	bit(d.baseaddr4map.find(ba));
				if(bit != d.baseaddr4map.end()){
					Data::SessionStub	&rss(d.sessionvec[bit->second]);
					if(rss.psession){
						rss.psession->completeConnect(_rstub, _rsap.port());
						//register in peer map
						d.peeraddr4map[rss.psession->peerAddress4()] = bit->second;
						//the connector has at least some updates to send
						if(!rss.inexeq){
							d.sessionexecq.push(bit->second);
							rss.inexeq = true;
						}
					}
				}
			}else{
				COLLECT_DATA_0(d.statistics.receivedAcceptingError);
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
		Context::the().sigctx.connectionuid.idx = ts.sessionidx;
		Context::the().sigctx.connectionuid.uid = rss.uid;
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
	
	if(_sig & fdt::OUTDONE){
		cassert(d.sendq.size());
		COLLECT_DATA_1(d.statistics.maxSendQueueSize, d.sendq.size());
		Data::SendBuffer	&rsb(d.sendq.front());
		Data::SessionStub	&rss(d.sessionvec[rsb.sessionidx]);
		
		Context::the().sigctx.connectionuid.idx = rsb.sessionidx;
		Context::the().sigctx.connectionuid.uid = rss.uid;
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
				Context::the().sigctx.connectionuid.idx = rsb.sessionidx;
				Context::the().sigctx.connectionuid.uid = rss.uid;
				rss.psession->prepareContext(Context::the());
				
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
//return ok if the talker should be signaled
int Talker::pushSignal(
	DynamicPointer<Signal> &_psig,
	const SerializationTypeIdT &_rtid,
	const ConnectionUid &_rconid,
	uint32 _flags
){
	COLLECT_DATA_0(d.statistics.signaled);
	d.sigq.push(Data::SignalData(_psig, _rtid, _rconid.idx, _rconid.uid, _flags));
	return d.sigq.size() == 1 ? NOK : OK;
}

//----------------------------------------------------------------------
//The talker's mutex should be locked
//Return's the new process connector's id
void Talker::pushSession(Session *_pses, ConnectionUid &_rconid, bool _exists){
	vdbgx(Dbg::ipc, "exists "<<_exists);
	if(_exists){
		++_rconid.uid;
	}else{
		if(d.freesessionstack.size()){
			vdbgx(Dbg::ipc, "");
			_rconid.idx = d.freesessionstack.top().first;
			_rconid.uid = d.freesessionstack.top().second;
			d.freesessionstack.pop();
		}else{
			vdbgx(Dbg::ipc, "");
			cassert(d.nextsessionidx < (uint16)0xffff);
			_rconid.idx = d.nextsessionidx;
			_rconid.uid = 0;
			++d.nextsessionidx;//TODO: it must be reseted somewhere!!!
		}
	}
	d.newsessionvec.push_back(Data::SessionPairT(_pses, _rconid.idx));
}
//----------------------------------------------------------------------
void Talker::doInsertNewSessions(){
	for(Data::SessionPairVectorT::const_iterator it(d.newsessionvec.begin()); it != d.newsessionvec.end(); ++it){
		vdbgx(Dbg::ipc, "newsession idx = "<<it->second<<" session vector size = "<<d.sessionvec.size());
		
		if(it->second >= d.sessionvec.size()){
			d.sessionvec.resize(it->second + 1);
		}
		Data::SessionStub &rss(d.sessionvec[it->second]);
		Context::the().sigctx.connectionuid.idx = it->second;
		Context::the().sigctx.connectionuid.uid = rss.uid;
		
		if(rss.psession == NULL){
			rss.psession = it->first;
			rss.psession->prepare();
			d.baseaddr4map[rss.psession->peerBaseAddress4()] = it->second;
			if(rss.psession->isConnected()){
				d.peeraddr4map[rss.psession->peerAddress4()] = it->second;
			}
			if(!rss.inexeq){
				d.sessionexecq.push(it->second);
				rss.inexeq = true;
			}
		}else{
			rss.psession->prepareContext(Context::the());
			if(!rss.psession->isAccepting()){
				//a reconnect
				d.peeraddr4map.erase(rss.psession->peerAddress4());
				rss.psession->reconnect(it->first);
				++rss.uid;
				d.peeraddr4map[rss.psession->peerAddress4()] = it->second;
				if(!rss.inexeq){
					d.sessionexecq.push(it->second);
					rss.inexeq = true;
				}
			}
			Context::the().sigctx.connectionuid.idx = 0xffff;
			Context::the().sigctx.connectionuid.uid = 0xffff;
			it->first->prepareContext(Context::the());
			delete it->first;
		}
	}
	d.newsessionvec.clear();
	d.nextsessionidx = d.sessionvec.size();
}
//----------------------------------------------------------------------
void Talker::doDispatchSignals(){
	//dispatch signals before disconnecting sessions
	while(d.sigq.size()){
		cassert(d.sigq.front().sessionidx < d.sessionvec.size());
		Data::SignalData	&rsd(d.sigq.front());
		Data::SessionStub	&rss(d.sessionvec[rsd.sessionidx]);
		const uint32		flags(rsd.flags);
		
		Context::the().sigctx.connectionuid.idx = rsd.sessionidx;
		Context::the().sigctx.connectionuid.uid = rss.uid;
		rss.psession->prepareContext(Context::the());
		if(
			rss.psession && 
			(
				!(flags & Service::SameConnectorFlag) ||
				rss.uid == rsd.sessionuid
			)
		){
			
			rss.psession->pushSignal(rsd.psig, rsd.tid, flags);
			if(!rss.inexeq){
				d.sessionexecq.push(rsd.sessionidx);
				rss.inexeq = true;
			}
		}
		
		d.sigq.pop();
	}
}
//----------------------------------------------------------------------
//this should be called under ipc service's mutex lock
void Talker::disconnectSessions(){
	Manager 		&rm(Manager::the());
	Locker<Mutex>	lock(rm.mutex(*this));
	//delete sessions
	
	for(Data::UInt16VectorT::const_iterator it(d.closingsessionvec.begin()); it != d.closingsessionvec.end(); ++it){
		Data::SessionStub &rss(d.sessionvec[*it]);
		vdbgx(Dbg::ipc, "disconnecting sessions "<<(void*)rss.psession);
		cassert(rss.psession);
		
		Context::the().sigctx.connectionuid.idx = *it;
		Context::the().sigctx.connectionuid.uid = rss.uid;
		rss.psession->prepareContext(Context::the());
		
		if(rss.psession->isDisconnected()){
			idbgx(Dbg::ipc, "deleting session "<<(void*)rss.psession<<" on pos "<<*it);
			d.rservice.disconnectSession(rss.psession);
			//unregister from base and peer:
			if(rss.psession->peerAddress4().addr){
				d.peeraddr4map.erase(rss.psession->peerAddress4());
			}
			if(
				rss.psession->peerBaseAddress4().first.addr
			){
				d.baseaddr4map.erase(rss.psession->peerBaseAddress4());
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
int Talker::accept(foundation::Visitor &){
	return BAD;
}
//======================================================================
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
	rdbgx(Dbg::ipc, "Statistics:\r\n"<<*this);
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
	return _ros;
}
}//namespace
#endif
}//namespace ipc
}//namesapce foundation

