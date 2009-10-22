/* Implementation file processconnector.cpp
	
	Copyright 2007, 2008 Valentin Palade 
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

#include "system/debug.hpp"
#include "system/socketaddress.hpp"
#include "system/specific.hpp"
#include "utility/queue.hpp"
#include "algorithm/serialization/binary.hpp"
#include "algorithm/serialization/idtypemap.hpp"
#include "foundation/signal.hpp"
#include "foundation/manager.hpp"
#include "foundation/ipc/ipcservice.hpp"
#include "processconnector.hpp"
#include "iodata.hpp"

using namespace std;

/*
NOTE 1: Design keep alive:
* no Keep Alive Buffers (KABs) are sent if there are signals and/or buffer to be sent
* KAB needs to use incremental buf id and it needs to be updated and resent
*
 1) in pushSentBuffer: when there is no signal to be sent and/or there are no buffer
 pending to be sent and there are signals waiting for responses, an null buffer is
 scheduled for timeout, indicating to a KAB with a static position (0) in outbufs
 2) when the null buffer is back, check again if there are signals to be sent
 and/or pending buffers else sent the KAB
 3) when KAB is back, wait for updates with timeout exactly as for the other signal buffers
NOTE 2: retransmit timeout coputation
We want to use the network according to its speed. See Data::computeRetransmitTimeout.


*/

namespace fdt = foundation;

namespace foundation{
namespace ipc{

struct BufCmp{
	bool operator()(const Buffer &_rbuf1, const Buffer &_rbuf2)const{
		return _rbuf1.id() > _rbuf2.id();
	}
};

typedef serialization::IdTypeMap					IdTypeMap;

//========	StaticData	===========================================================

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
	typedef std::vector<ulong> ULongVectorTp;
	ULongVectorTp	toutvec;
};

//========	ProcessConnector	===========================================================
struct ProcessConnector::Data{
	enum Flags{
		Locked			= (LastPublicFlag << 1),
		EnqueueStatus	= (LastPublicFlag << 2),
	};
	enum States{
		Connecting = 0,//Connecting must be first see isConnecting
		Accepting,
		WaitAccept,
		Connected,
		Disconnecting
	};
	enum{
		LastBufferId = 0xffffffff - 5,
		UpdateBufferId = 0xffffffff,//the id of a buffer containing only updates
		MaxSignalBufferCount = 32,//continuous buffers sent for a signal
		MaxSendSignalQueueSize = 16,//max count of signals sent in paralell
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
	
	typedef BinSerializer						BinSerializerTp;
	typedef BinDeserializer						BinDeserializerTp;
	
	struct OutWaitSignal{
		OutWaitSignal(
			uint32 _bufid,
			const DynamicPointer<Signal>& _rsig,
			uint32 _flags,
			uint32 _id
		):bufid(_bufid), sig(_rsig), pser(NULL), flags(_flags), id(_id), uid(0){}
		~OutWaitSignal(){
			cassert(!pser);
		}
		bool operator<(const OutWaitSignal &_owc)const{
			if(sig.ptr()){
				if(_owc.sig.ptr()){
				}else return true;
			}else return false;
			//TODO: optimize!!
			if(id < _owc.id){
				return (_owc.id - id) < (0xffffffff/2);
			}else if(id > _owc.id){
				return (id - _owc.id) >= (0xffffffff/2);
			}else return false;
		}
		uint32				bufid;
		DynamicPointer<Signal> sig;
		BinSerializerTp		*pser;
		uint32				flags;
		uint32				id;
		uint32				uid;
	};
	
	typedef std::pair<Buffer, uint16>			OutBufferPairTp;
	typedef std::vector<OutBufferPairTp>		OutBufferVectorTp;
	typedef Stack<uint16>						OutFreePosStackTp;
	typedef std::priority_queue<Buffer,std::vector<Buffer>,BufCmp>	
												BufferPriorityQueueTp;
	typedef std::pair<DynamicPointer<Signal>, uint32>
												CmdPairTp;
	typedef Queue<CmdPairTp>					CmdQueueTp;
	typedef std::deque<OutWaitSignal>			OutCmdVectorTp;
	typedef Queue<uint32>						ReceivedIdsQueueTp;
	typedef std::pair<const SockAddrPair*, int>	BaseProcAddr;
	typedef std::pair<Signal*, BinDeserializerTp*>
												RecvCmdPairTp;
	typedef Queue<RecvCmdPairTp>				RecvCmdQueueTp;
	typedef Queue<SignalUid>					SendCmdQueueTp;
	
	Data(const Inet4SockAddrPair &_raddr, uint32 _keepalivetout);
	Data(const Inet4SockAddrPair &_raddr, int _baseport, uint32 _keepalivetout);
	~Data();
	std::pair<uint16, uint16> insertSentBuffer(Buffer &);
	std::pair<uint16, uint16> getSentBuffer(int _bufpos);
	void incrementExpectedId();
	void incrementSendId();
	BinSerializerTp* popSerializer();
	void pushSerializer(BinSerializerTp*);
	BinDeserializerTp* popDeserializer();
	void pushDeserializer(BinDeserializerTp*);
	//save signals to be resent in case of disconnect
	SignalUid pushOutWaitSignal(
		uint32 _bufid,
		DynamicPointer<Signal> &_sig,
		uint32 _flags,
		uint32 _id
	);
	OutWaitSignal& waitSignal(const SignalUid &_siguid);
	const OutWaitSignal& waitSignal(const SignalUid &_siguid)const;
	OutWaitSignal& waitFrontSignal();
	const OutWaitSignal& waitFrontSignal()const;
	OutWaitSignal& waitBackSignal();
	const OutWaitSignal& waitBackSignal()const;
	//eventually pops signals associated to a buffer
	void popOutWaitSignals(uint32 _bufid, const ConnectorUid &_rconid);
	//pops a signal waiting for a response
	void popOutWaitSignal(const SignalUid &_rsiguid);
	OutBufferPairTp& keepAliveBuffer(){
		cassert(outbufs.size());
		return outbufs[0];
	}
	void prepareKeepAlive();
	bool canSendKeepAlive(const TimeSpec &_tpos);
	uint computeRetransmitTimeout(const uint _retrid, const uint32 _bufid);
//data:	
	uint32					expectedid;
	uint16					retrpos;//see note 2
	int16					state;
	uint16					flags;
	int16					bufjetons;
	uint16					crtsigbufcnt;
	uint32					sendid;
	SocketAddress			addr;
	SockAddrPair			pairaddr;
	BaseProcAddr			baseaddr;
	
	TimeSpec				lasttimepos;
	OutFreePosStackTp		outfreestk;
	OutBufferVectorTp		outbufs;
	CmdQueueTp				cq;
	RecvCmdQueueTp			rcq;
	SendCmdQueueTp			scq;
	ReceivedIdsQueueTp		rcvidq;
	BufferPriorityQueueTp	inbufq;
	OutCmdVectorTp			outsigv;
	OutFreePosStackTp		outfreesigstk;
	uint32					sndsigid;
	uint32					keepalivetout;
	uint32					respwaitsigcnt;
	TimeSpec				rcvtpos;
};

//==============================================================================

/*static*/ StaticData& StaticData::instance(){
	//TODO: staticproblem
	static StaticData sd;
	return sd;
}

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


ulong StaticData::retransmitTimeout(uint _pos){
	cassert(_pos < toutvec.size());
	return toutvec[_pos];
}
//==============================================================================

ProcessConnector::Data::Data(const Inet4SockAddrPair &_raddr, uint32 _keepalivetout):
	expectedid(1), /*retranstimeout(StartRetransmitTimeout),*/retrpos(0),
	state(Connecting), flags(0), bufjetons(1), 
	crtsigbufcnt(MaxSignalBufferCount),sendid(0),
	addr(_raddr), pairaddr(addr), 
	baseaddr(&pairaddr, addr.port()),sndsigid(0), keepalivetout(_keepalivetout),
	respwaitsigcnt(0)
{
	outbufs.push_back(OutBufferPairTp(Buffer(NULL,0), 0));
}

ProcessConnector::Data::Data(const Inet4SockAddrPair &_raddr, int _baseport, uint32 _keepalivetout):
	expectedid(1), /*retranstimeout(StartRetransmitTimeout),*/retrpos(0),
	state(Accepting), flags(0), bufjetons(3), crtsigbufcnt(MaxSignalBufferCount), sendid(0),
	addr(_raddr), pairaddr(addr), baseaddr(&pairaddr, _baseport),
	sndsigid(0), keepalivetout(_keepalivetout),respwaitsigcnt(0)
{
	outbufs.push_back(OutBufferPairTp(Buffer(NULL,0), 0));
}


ProcessConnector::Data::~Data(){
	for(OutBufferVectorTp::iterator it(outbufs.begin()); it != outbufs.end(); ++it){
		char *pb = it->first.buffer();
		if(pb){ 
			idbgx(Dbg::ipc, "released buffer");
			Specific::pushBuffer(pb, Specific::capacityToId(it->first.bufferCapacity()));
		}
	}
	idbgx(Dbg::ipc, "cq.size = "<<cq.size()<<"scq.size = "<<scq.size());
	
	while(cq.size()){
		cq.front().first->ipcFail(0);
		cq.pop();
	}
	
	while(scq.size()){scq.pop();}
	
	for(OutCmdVectorTp::iterator it(outsigv.begin()); it != outsigv.end(); ++it){
		if(it->pser){
			it->pser->clear();
			pushSerializer(it->pser);
			it->pser = NULL;
		}
		if(it->sig.ptr()){
			++it->uid;
			if(it->flags & Service::WaitResponseFlag && it->flags & Service::SentFlag){
				//the was successfully sent but the response did not arrive
				it->sig->ipcFail(1);
			}else{
				//the signal was not successfully sent
				it->sig->ipcFail(0);
			}
			it->sig.clear();
		}
	}
	while(rcq.size()){
		if(rcq.front().second){
			rcq.front().second->clear();
			pushDeserializer(rcq.front().second);
		}
		delete rcq.front().first;
		rcq.pop();
	}
	idbgx(Dbg::ipc, "done deleting process");
}
std::pair<uint16, uint16> ProcessConnector::Data::insertSentBuffer(Buffer &_rbuf){
	std::pair<uint16, uint16> p;
	cassert(_rbuf.buffer());
	if(outfreestk.size()){
		p.first = outfreestk.top();
		cassert(!outbufs[p.first].first.buffer());
		outbufs[p.first].first = _rbuf;
		p.second = outbufs[p.first].second;
		outfreestk.pop();
	}else{
		p.first = outbufs.size();
		outbufs.push_back(OutBufferPairTp(_rbuf, 0));
		p.second = 0;
	}
	return p;
}
std::pair<uint16, uint16> ProcessConnector::Data::getSentBuffer(int _bufpos){
	cassert(_bufpos < (int)outbufs.size());
	return std::pair<uint16, uint16>(_bufpos, outbufs[_bufpos].second);
}
inline void ProcessConnector::Data::incrementExpectedId(){
	++expectedid;
	if(expectedid > LastBufferId) expectedid = 0;
}
inline void ProcessConnector::Data::incrementSendId(){
	++sendid;
	if(sendid > LastBufferId) sendid = 0;
}
//TODO: use Specific
ProcessConnector::Data::BinSerializerTp* ProcessConnector::Data::popSerializer(){
	BinSerializerTp* p = Specific::tryUncache<BinSerializerTp>();
	if(!p){
		p = new BinSerializerTp;
	}
	return p;
}
void ProcessConnector::Data::pushSerializer(ProcessConnector::Data::BinSerializerTp* _p){
	cassert(_p->empty());
	Specific::cache(_p);
}
ProcessConnector::Data::BinDeserializerTp* ProcessConnector::Data::popDeserializer(){
	BinDeserializerTp* p = Specific::tryUncache<BinDeserializerTp>();
	if(!p){
		p = new BinDeserializerTp;
	}
	return p;
}
void ProcessConnector::Data::pushDeserializer(ProcessConnector::Data::BinDeserializerTp* _p){
	cassert(_p->empty());
	Specific::cache(_p);
}

inline ProcessConnector::Data::OutWaitSignal& ProcessConnector::Data::waitSignal(const SignalUid &_siguid){
	cassert(_siguid.idx < outsigv.size() && outsigv[_siguid.idx].uid == _siguid.uid);
	return outsigv[_siguid.idx];
}
inline const ProcessConnector::Data::OutWaitSignal& ProcessConnector::Data::waitSignal(const SignalUid &_siguid)const{
	cassert(_siguid.idx < outsigv.size() && outsigv[_siguid.idx].uid == _siguid.uid);
	return outsigv[_siguid.idx];
}

inline ProcessConnector::Data::OutWaitSignal& ProcessConnector::Data::waitFrontSignal(){
	return waitSignal(scq.front());
}
inline const ProcessConnector::Data::OutWaitSignal& ProcessConnector::Data::waitFrontSignal()const{
	return waitSignal(scq.front());
}

inline ProcessConnector::Data::OutWaitSignal& ProcessConnector::Data::waitBackSignal(){
	return waitSignal(scq.back());
}
inline const ProcessConnector::Data::OutWaitSignal& ProcessConnector::Data::waitBackSignal()const{
	return waitSignal(scq.back());
}


SignalUid ProcessConnector::Data::pushOutWaitSignal(uint32 _bufid, DynamicPointer<Signal> &_sig, uint32 _flags, uint32 _id){
	_flags &= ~Service::SentFlag;
	_flags &= ~Service::WaitResponseFlag;
	if(this->outfreesigstk.size()){
		OutWaitSignal &owc = outsigv[outfreesigstk.top()];
		owc.bufid = _bufid;
		owc.sig = _sig;
		owc.flags = _flags;
		uint32 idx = outfreesigstk.top();
		outfreesigstk.pop();
		return SignalUid(idx, owc.uid);
	}else{
		outsigv.push_back(OutWaitSignal(_bufid, _sig, _flags, _id));
		return SignalUid(outsigv.size() - 1, 0/*uid*/);
	}
}
void ProcessConnector::Data::popOutWaitSignals(uint32 _bufid, const ConnectorUid &_rconid){
	for(OutCmdVectorTp::iterator it(outsigv.begin()); it != outsigv.end(); ++it){
		if(it->bufid == _bufid && it->sig.ptr()){
			it->bufid = 0;
			cassert(!it->pser);
			if(it->flags & Service::WaitResponseFlag){
				//let it leave a little longer
				idbgx(Dbg::ipc, "signal waits for response "<<(it - outsigv.begin())<<','<<it->uid);
				it->flags |= Service::SentFlag;
			}else{
				++it->uid;
				it->sig.clear();
			}
		}
	}
}
void ProcessConnector::Data::popOutWaitSignal(const SignalUid &_rsiguid){
	if(_rsiguid.idx < outsigv.size() && outsigv[_rsiguid.idx].uid == _rsiguid.uid){
		idbgx(Dbg::ipc, "signal received response "<<_rsiguid.idx<<','<<_rsiguid.uid);
		outsigv[_rsiguid.idx].sig.clear();
		--respwaitsigcnt;
	}
}
bool ProcessConnector::Data::canSendKeepAlive(const TimeSpec &_tpos){
	idbgx(Dbg::ipc, "keepalivetout = "<<keepalivetout<<" respwaitsigcnt = "<<respwaitsigcnt<<" rcq = "<<rcq.size()<<" cq = "<<cq.size()<<" scq = "<<scq.size());
	idbgx(Dbg::ipc, "_tpos = "<<_tpos.seconds()<<','<<_tpos.nanoSeconds()<<" rcvtpos = "<<rcvtpos.seconds()<<','<<rcvtpos.nanoSeconds());
	return keepalivetout && respwaitsigcnt &&
		(rcq.empty() || rcq.size() == 1 && !rcq.front().first) && 
		cq.empty() && scq.empty() && _tpos >= rcvtpos;
}
void ProcessConnector::Data::prepareKeepAlive(){
	//cassert(outbufs.empty());
	idbgx(Dbg::ipc,"");
	Buffer b(Specific::popBuffer(Specific::sizeToId(Buffer::minSize())), Specific::idToCapacity(Specific::sizeToId(Buffer::minSize())));
	b.reset();
	b.type(Buffer::KeepAliveType);
	outbufs[0] = OutBufferPairTp(b, 0);
}

inline uint ProcessConnector::Data::computeRetransmitTimeout(const uint _retrid, const uint32 _bufid){
	if(!(_bufid & StaticData::RefreshIndex)){
		//recalibrate the retrpos
		retrpos = 0;
	}
	if(_retrid > retrpos){
		retrpos = _retrid;
		return StaticData::instance().retransmitTimeout(retrpos);
	}else{
		return StaticData::instance().retransmitTimeout(retrpos + _retrid);
	}
}

//*******************************************************************************

ProcessConnector::~ProcessConnector(){
	idbgx(Dbg::ipc, "");
	delete &d;
}

void ProcessConnector::init(){
}
void ProcessConnector::prepare(){
	//add the keepalive buffer to outvec
	d.prepareKeepAlive();
}

// static
int ProcessConnector::parseAcceptedBuffer(Buffer &_rbuf){
	if(_rbuf.dataSize() == sizeof(int32)){
		int32 *pp((int32*)_rbuf.data());
		return (int)ntohl((uint32)*pp);
	}
	return BAD;
}
// static
int ProcessConnector::parseConnectingBuffer(Buffer &_rbuf){
	if(_rbuf.dataSize() == sizeof(int32)){
		int32 *pp((int32*)_rbuf.data());
		return (int)ntohl((uint32)*pp);
	}
	return BAD;
}
	
ProcessConnector::ProcessConnector(
	const Inet4SockAddrPair &_raddr,
	uint32 _keepalivetout
):d(*(new Data(_raddr, _keepalivetout))){
	idbgx(Dbg::ipc, "");
}

//ProcessConnector::ProcessConnector(BinMapper &_rm, const Inet6SockAddrPair &_raddr):d(*(new Data(_rm, _raddr))){}
ProcessConnector::ProcessConnector(
	const Inet4SockAddrPair &_raddr,
	int _baseport,
	uint32 _keepalivetout
):d(*(new Data(_raddr, _baseport, _keepalivetout))){
	idbgx(Dbg::ipc, "");
}


//TODO: ensure that really no signal is dropped on reconnect
// not even those from Dropped buffers!!!
void ProcessConnector::reconnect(ProcessConnector *_ppc){
	idbgx(Dbg::ipc, "reconnecting - clearing process data "<<(void*)_ppc);
	//first we reset the peer addresses
	if(_ppc){
		d.addr = _ppc->d.addr;
		d.pairaddr = d.addr;
		d.state = Data::Accepting;
		d.bufjetons = 3;
	}else{
		d.state = Data::Connecting;
		d.bufjetons = 1;
	}
	//clear the receive queue
	while(d.rcq.size()){
		delete d.rcq.front().first;
		if(d.rcq.front().second){
			d.rcq.front().second->clear();
			d.pushDeserializer(d.rcq.front().second);
		}
		d.rcq.pop();
	}
	//
	for(int sz = d.cq.size(); sz; --sz){
		if(!(d.cq.front().second & Service::SameConnectorFlag)) {
			idbgx(Dbg::ipc, "signal scheduled for resend");
			d.cq.push(d.cq.front());
		}else{
			idbgx(Dbg::ipc, "signal not scheduled for resend");
		}
		d.cq.pop();
	}
	uint32 cqsz = d.cq.size();
	//we must put into the cq all signal in the exact oreder they were sent
	//to do so we must push into cq, the signals from scq and waitsigv
	//ordered by their id
	for(int sz = d.scq.size(); sz; --sz){
		Data::OutWaitSignal &outsig(d.waitFrontSignal());
		if(outsig.pser){
			outsig.pser->clear();
			d.pushSerializer(outsig.pser);
			outsig.pser = NULL;
		}
		cassert(outsig.sig.ptr());
		//NOTE: on reconnect the responses, or signals sent using ConnectorUid are dropped
		if(!(outsig.flags & Service::SameConnectorFlag)){
			if(outsig.flags & Service::WaitResponseFlag && outsig.flags & Service::SentFlag){
				//if the signal was sent and were waiting for response - were not sending twice
				outsig.sig->ipcFail(1);
				outsig.sig.clear();
			}
			//we can try resending the signal
		}else{
			idbgx(Dbg::ipc, "signal not scheduled for resend");
			if(outsig.flags & Service::WaitResponseFlag && outsig.flags & Service::SentFlag){
				outsig.sig->ipcFail(1);
			}else{
				outsig.sig->ipcFail(0);
			}
			outsig.sig.clear();
			++outsig.uid;
		}
		d.scq.pop();
	}
	//now we need to sort d.outsigv
	std::sort(d.outsigv.begin(), d.outsigv.end());
	
	//then we push into cq the signals from outcmv
	for(Data::OutCmdVectorTp::const_iterator it(d.outsigv.begin()); it != d.outsigv.end(); ++it){
		if(it->sig.ptr()){
			d.cq.push(Data::CmdPairTp(it->sig, it->flags));
		}else break;
	}
	//clear out sig vector and stack
	d.outsigv.clear();
	while(d.outfreesigstk.size()){
		d.outfreesigstk.pop();
	}
	
	//put the signals already in the cq to the end of cq
	while(cqsz--){
		d.cq.push(d.cq.front());
		d.cq.pop();
	}
	
	d.crtsigbufcnt = Data::MaxSignalBufferCount;
	d.expectedid = 1;
	d.respwaitsigcnt = 0;
	d.sendid = 0;
	//clear rcvidq - updates
	while(d.rcvidq.size()){
		d.rcvidq.pop();
	}
	//clear the inbufq - out-of-order received buffers
	while(d.inbufq.size()){
		char *buf = d.inbufq.top().buffer();
		Specific::pushBuffer(buf, Specific::capacityToId(d.inbufq.top().bufferCapacity()));
		d.inbufq.pop();
	}
	//clear sent buffers
	//we also want that the connect buffer will put on outbufs[0] so we complicate the algorithm a little bit
	while(d.outfreestk.size()){//first we clear the free stack
		d.outfreestk.pop();
	}
	for(Data::OutBufferVectorTp::iterator it(d.outbufs.begin()); it != d.outbufs.end(); ++it){
		if(it->first.buffer()){
			char *buf = it->first.buffer();
			Specific::pushBuffer(buf, Specific::capacityToId(it->first.bufferCapacity()));
			++it->second;
			it->first.reinit();
		}
	}
	//TODO: it fires when in valgrind both ends
	cassert(d.outbufs.size() >= 2);
	d.prepareKeepAlive();
	d.outfreestk.push(1);
}

bool ProcessConnector::isConnected()const{
	return d.state > Data::Connecting;
}
bool ProcessConnector::isDisconnecting()const{
	return d.state == Data::Disconnecting;
}

bool ProcessConnector::isConnecting()const{
	return d.state == Data::Connecting;
}


bool ProcessConnector::isAccepting()const{
	return d.state == Data::Accepting;
}

void ProcessConnector::completeConnect(int _pairport){
	if(d.state == Data::Connected) return;
	d.state = Data::Connected;
	d.addr.port(_pairport);
	cassert(d.outbufs.size() > 1);
	//free the connecting buffer
	cassert(d.outbufs[1].first.buffer());
	//_rs.collectBuffer(d.outbufs[0].first, _riod);
	char *pb = d.outbufs[1].first.buffer();
	Specific::pushBuffer(pb, Specific::capacityToId(d.outbufs[1].first.bufferCapacity()));
	d.bufjetons = 3;
	++d.outbufs[1].second;
	d.outbufs[1].first.reinit();//clear the buffer
	d.outfreestk.push(1);
	d.rcvidq.push(0);
}

const Inet4SockAddrPair* ProcessConnector::peerAddr4()const{
	const SockAddrPair *p = &(d.pairaddr);
	return reinterpret_cast<const Inet4SockAddrPair*>(p);
}

const std::pair<const Inet4SockAddrPair*, int>* ProcessConnector::baseAddr4()const{
	const Data::BaseProcAddr *p = &(d.baseaddr);
	return reinterpret_cast<const std::pair<const Inet4SockAddrPair*, int>*>(p);
}

// const Inet6SockAddrPair* ProcessConnector::pairAddr6()const{
// 	return reinterpret_cast<Inet6SockAddrPair*>(&d.pairaddr);
// }
// 
// const std::pair<const Inet6SockAddrPair*, int>* ProcessConnector::baseAddr6()const{
// 	return reinterpret_cast<std::pair<const Inet6SockAddrPair*, int>*>(&d.baseaddr);
// }


int ProcessConnector::pushSignal(DynamicPointer<Signal> &_rsig, uint32 _flags){
	//_flags &= ~Data::ProcessReservedFlags;
	d.cq.push(Data::CmdPairTp(_rsig, _flags));
	if(d.cq.size() == 1) return NOK;
	return OK;
}

bool ProcessConnector::moveToNextInBuffer(){
	while(d.inbufq.size() && (d.inbufq.top().id() < d.expectedid)){
		idbgx(Dbg::ipc, "inbufq.size() = "<<d.inbufq.size()<<" d.inbufq.top().id() = "<<d.inbufq.top().id()<<" d.expectedid "<<d.expectedid); 

		Buffer b(d.inbufq.top());
		char *pb = d.inbufq.top().buffer();
		Specific::pushBuffer(pb, Specific::capacityToId(d.inbufq.top().bufferCapacity()));
		d.inbufq.pop();
	}
	if(d.inbufq.size()){
		idbgx(Dbg::ipc, "inbufq.size() = "<<d.inbufq.size()<<" d.inbufq.top().id() = "<<d.inbufq.top().id()<<" d.expectedid "<<d.expectedid);
		return d.inbufq.top().id() == d.expectedid;
	}
	return false;
}

int ProcessConnector::pushReceivedBuffer(Buffer &_rbuf, const ConnectorUid &_rconid, const TimeSpec &_tpos){
	idbgx(Dbg::ipc, "rcvbufid = "<<_rbuf.id()<<" expected id "<<d.expectedid<<" inbufq.size = "<<d.inbufq.size()<<" flags = "<<_rbuf.flags());
	d.rcvtpos = _tpos;
	if(_rbuf.id() != d.expectedid){
		if(_rbuf.id() < d.expectedid){
			//the peer doesnt know that we've already received the buffer
			//add it as update
			d.rcvidq.push(_rbuf.id());
			idbgx(Dbg::ipc, "already received buffer - resend update");
			return NOK;//we have to send updates
		}else if(_rbuf.id() <= Data::LastBufferId){
			if(_rbuf.updatesCount()){//we have updates
				freeSentBuffers(_rbuf, _rconid);
			}
			//try to keep the buffer for future parsing
			if(d.inbufq.size() < 4){//TODO: use a variable instead of inline value
				d.rcvidq.push(_rbuf.id());//for peer updates
				d.inbufq.push(_rbuf);
				_rbuf.reinit();
				idbgx(Dbg::ipc, "keep the buffer for future parsing");
				return NOK;//we have to send updates
			}else{
				idbgx(Dbg::ipc, "to many buffers out-of-order");
				return OK;
			}
		}else if(_rbuf.id() == Data::UpdateBufferId){//a buffer containing only updates
			_rbuf.updatesCount();
			idbgx(Dbg::ipc, "received buffer containing only updates");
			if(freeSentBuffers(_rbuf, _rconid)) return NOK;
			return OK;
		}
	}else{
		//the expected buffer
		d.rcvidq.push(_rbuf.id());
		if(_rbuf.updatesCount()){
			freeSentBuffers(_rbuf, _rconid);
		}
		if(_rbuf.type() == Buffer::DataType){
			parseBuffer(_rbuf, _rconid);
		}
		d.incrementExpectedId();//move to the next buffer
		//while(d.inbufq.top().id() == d.expectedid){
		while(moveToNextInBuffer()){
			Buffer b(d.inbufq.top());
			d.inbufq.pop();
			//this is already done on receive
			if(_rbuf.type() == Buffer::DataType){
				parseBuffer(b, _rconid);
			}
			char *pb = b.buffer();
			Specific::pushBuffer(pb, Specific::capacityToId(b.bufferCapacity()));
			d.incrementExpectedId();//move to the next buffer
		}
		return NOK;//we have something to send
	}
	return OK;
}
/*
Prevent the following situation to occur:
A buffer is received with updates, buffers for which pushSent was not called
*/
int ProcessConnector::pushSentBuffer(SendBufferData &_rbuf, const TimeSpec &_tpos, bool &_reusebuf){
	if(_rbuf.b.buffer()){
		if(_rbuf.b.id() > Data::LastBufferId){
			//cassert(_rbuf.b.id() == Data::UpdateBufferId);
			switch(_rbuf.b.id()){
				case Data::UpdateBufferId:{
					idbgx(Dbg::ipc, "sent updates - collecting buffer");
					char *pb = _rbuf.b.buffer();
					Specific::pushBuffer(pb, Specific::capacityToId(_rbuf.b.bufferCapacity()));
					//++d.bufjetons;
					return NOK;//maybe we have something to send
					}
				default:
					cassert(false);
			}
		}else{
			idbgx(Dbg::ipc, "sent bufid = "<<_rbuf.b.id()<<" bufpos = "<<_rbuf.bufpos<<" retransmitid "<<_rbuf.b.retransmitId()<<" buf = "<<(void*)_rbuf.b.buffer()<<" buffercap = "<<_rbuf.b.bufferCapacity()<<" flags = "<<_rbuf.b.flags()<<" type = "<<(int)_rbuf.b.type());
			std::pair<uint16, uint16>  p;
			const uint retransid = _rbuf.b.retransmitId();
			if(!retransid){
				//cassert(_rbuf.bufpos < 0);
				p = d.insertSentBuffer(_rbuf.b);	//_rbuf.bc will contain the buffer index, and
													//_rbuf.dl will contain the buffer uid
				idbgx(Dbg::ipc, "inserted buffer on pos "<<p.first<<','<<p.second);
				cassert(p.first);//it cannot be the keepalive buffer
			}else{
				//cassert(_rbuf.bufpos >= 0);
				p = d.getSentBuffer(_rbuf.bufpos);
				idbgx(Dbg::ipc, "get buffer from pos "<<p.first<<','<<p.second);
			}
			++p.first;//adjust the index
			//_rbuf.b.retransmitId(_rbuf.retransmitId() + 1);
			uint tmptimeout = d.computeRetransmitTimeout(retransid, _rbuf.b.id());
			idbgx(Dbg::ipc, "p.first "<<p.first<<" p.second "<<p.second<<" retransid = "<<retransid<<" tmptimeout = "<<tmptimeout);
			//reuse _rbuf for retransmission timeout
			_rbuf.b.pb = NULL;
			_rbuf.b.bc = p.first;
			_rbuf.b.dl = p.second;
			_rbuf.bufpos = p.first;
			_rbuf.timeout = _tpos;
			_rbuf.timeout +=  tmptimeout;//d.retranstimeout;//miliseconds retransmission timeout
			_reusebuf = true;
			idbgx(Dbg::ipc, "prepare waitbuf b.cap "<<_rbuf.b.bufferCapacity()<<" b.dl "<<_rbuf.b.dl);
			cassert(sizeof(uint32) <= sizeof(size_t));
			return OK;
		}
	}else{//a timeout occured
		idbgx(Dbg::ipc, "timeout occured _rbuf.bc "<<_rbuf.b.bc<<" rbuf.dl "<<_rbuf.b.dl<<" d.outbufs.size() = "<<d.outbufs.size());
		cassert(d.state != Data::Disconnecting);
		if(_rbuf.b.bc){//for a sent buffer
			cassert(_rbuf.b.bc <= d.outbufs.size());
			int bufpos(_rbuf.b.bc - 1);
			Data::OutBufferPairTp &rob(d.outbufs[bufpos]);
			idbgx(Dbg::ipc, "rob.second = "<<rob.second<<" b.dl = "<<_rbuf.b.dl<<" bufpos = "<<bufpos);
			if(rob.first.buffer() && rob.second == _rbuf.b.dl){
				//we must resend this buffer
				idbgx(Dbg::ipc, "resending buffer "<<(bufpos)<<" retransmit id = "<<rob.first.retransmitId()<<" buf = "<<(void*)rob.first.buffer());
				if(bufpos){
					rob.first.print();
					rob.first.retransmitId(rob.first.retransmitId() + 1);
					if(rob.first.retransmitId() > StaticData::DataRetransmitCount){
						if(rob.first.type() == Buffer::ConnectingType){
							if(rob.first.retransmitId() > StaticData::ConnectRetransmitCount){//too many resends for connect type
								idbgx(Dbg::ipc, "preparing to disconnect process");
								cassert(d.state != Data::Disconnecting);
								d.state = Data::Disconnecting;
								return BAD;//disconnecting
							}
						}else{//too many resends for a data type
							//reconnecting
							reconnect(NULL);
							return BAD;//have something to send
						}
					}
				}else{//keepalive buffer
					//we are scheduled to send the keepalive buffer
					//rob will point to keepalive buffer
					if(rob.first.retransmitId() > StaticData::DataRetransmitCount){
						idbgx(Dbg::ipc, "keep alive buffer - too many retransmits");
						//reconnecting
						reconnect(NULL);
						return BAD;//have something to send
					}else if(rob.first.retransmitId() > 1){
						idbgx(Dbg::ipc, "keep alive buffer - retransmitting");
						rob.first.retransmitId(rob.first.retransmitId() + 1);
					}else if(rob.first.retransmitId() == 1){
						if(d.canSendKeepAlive(_tpos)){//check again
							idbgx(Dbg::ipc, "keep alive buffer - start transmit");
							rob.first.retransmitId(2);
							//set the buffer id:
							rob.first.id(d.sendid);
							d.incrementSendId();
						}else{
							rob.first.retransmitId(0);
							return OK;
						}
					}else return OK;
				}
				_rbuf.b = rob.first;
				_rbuf.paddr = &d.pairaddr;
				_rbuf.bufpos = bufpos;
				_rbuf.timeout = _tpos;
				d.lasttimepos = _tpos;
				_reusebuf = true;
			}else{
				idbgx(Dbg::ipc, "nothing to resend");
			}
		}else{//for something else
			//TODO: e.g. use it for sending update data with certain timeout
		}
	}
	return OK;
}

/*
NOTE: VERY IMPORTANT
	because were using socket address as SockAddrPair and not SocketAddress, the process MUST not be
	destroyed if there are buffers pending for sending (outstk.size < outbufs.size)
*/

int ProcessConnector::processSendSignals(SendBufferData &_rsb, const TimeSpec &_tpos, int _baseport){
	idbgx(Dbg::ipc, "bufjetons = "<<d.bufjetons);
	if(d.bufjetons || d.state != Data::Connected || d.rcvidq.size()){
		idbgx(Dbg::ipc, "d.rcvidq.size() = "<<d.rcvidq.size());
		bool written = false;
		Buffer &rbuf(_rsb.b);
		rbuf.reset();
		//here we keep per buffer waiting signals - 
		//signals that in case of a peer disconnection detection
		//can safely be resend
		SignalUid	waitsigs[Data::MaxSendSignalQueueSize];
		SignalUid	*pcrtwaitsig(waitsigs - 1);
		
		if(d.state == Data::Connected){
			rbuf.type(Buffer::DataType);
			//first push the updates if any
			while(d.rcvidq.size() && rbuf.updatesCount() < 8){
				rbuf.pushUpdate(d.rcvidq.front());
				written = true;
				d.rcvidq.pop();
			}
			if(d.bufjetons){//send data only if we have jetons
				//then push the signals
				idbgx(Dbg::ipc, "d.cq.size() = "<<d.cq.size()<<" d.scq.size = "<<d.scq.size());
				//fill scq
				while(d.cq.size() && d.scq.size() < Data::MaxSendSignalQueueSize){
					uint32 flags = d.cq.front().second;
					cassert(d.cq.front().first.ptr());
					d.scq.push(d.pushOutWaitSignal(0, d.cq.front().first, flags, d.sndsigid++));
					Data::OutWaitSignal &rosig(d.waitBackSignal());
					switch(rosig.sig->ipcPrepare(d.scq.back())){
						case OK://signal doesnt wait for response
							rosig.flags &= ~Service::WaitResponseFlag;
							break;
						case NOK://signal wait for response
							++d.respwaitsigcnt;
							rosig.flags |= Service::WaitResponseFlag;
							break;
						default:
							cassert(false);
					}
					d.cq.pop();
				}
				Data::BinSerializerTp	*pser = NULL;
				
				//at most we can send Data::MaxSendSignalQueueSize signals within a single buffer
				
				while(d.scq.size()){
					if(d.crtsigbufcnt){
						Data::OutWaitSignal &rosig(d.waitFrontSignal());
						if(rosig.pser){//a continued signal
							if(d.crtsigbufcnt == Data::MaxSignalBufferCount){
								idbgx(Dbg::ipc, "oldsignal");
								rbuf.dataType(Buffer::OldSignal);
							}else{
								idbgx(Dbg::ipc, "continuedsignal");
								rbuf.dataType(Buffer::ContinuedSignal);
							}
						}else{//a new commnad
							idbgx(Dbg::ipc, "newsignal");
							rbuf.dataType(Buffer::NewSignal);
							if(pser){
								rosig.pser = pser;
								pser = NULL;
							}else{
								rosig.pser = d.popSerializer();
							}
							Signal *psig(rosig.sig.ptr());
							rosig.pser->push(psig);
						}
						--d.crtsigbufcnt;
						idbgx(Dbg::ipc, "d.crtsigbufcnt = "<<d.crtsigbufcnt);
						written = true;
						
						int rv = rosig.pser->run(rbuf.dataEnd(), rbuf.dataFreeSize());
						
						cassert(rv >= 0);//TODO: deal with the situation!
						
						rbuf.dataSize(rbuf.dataSize() + rv);
						if(rosig.pser->empty()){//finished with this signal
							idbgx(Dbg::ipc, "donesignal");
							if(pser) d.pushSerializer(pser);
							pser = rosig.pser;
							pser->clear();
							rosig.pser = NULL;
							idbgx(Dbg::ipc, "cached wait signal");
							++pcrtwaitsig;
							*pcrtwaitsig = d.scq.front();
							d.scq.pop();
							//we dont want to switch to an old signal
							d.crtsigbufcnt = Data::MaxSignalBufferCount - 1;
							if(rbuf.dataFreeSize() < 16) break;
							break;
						}else{
							break;
						}
					}else if(d.scq.size() == 1){//only one signal
						d.crtsigbufcnt = Data::MaxSignalBufferCount - 1;
					}else{
						d.scq.push(d.scq.front());
						//d.waitFrontSignal().pser = NULL;
						d.scq.pop();
						d.crtsigbufcnt = Data::MaxSignalBufferCount;
					}
				}//while
				if(pser) d.pushSerializer(pser);
			}//if bufjetons
		}else if(d.state == Data::Connecting){
			rbuf.type(Buffer::ConnectingType);
			int32 *pi = (int32*)rbuf.dataEnd();
			*pi = htonl(_baseport);
			rbuf.dataSize(rbuf.dataSize() + sizeof(int32));
			//d.cq.pop();
			d.state = Data::WaitAccept;
			written = true;
		}else if(d.state == Data::Accepting){
			rbuf.type(Buffer::AcceptingType);
			int32 *pi = (int32*)rbuf.dataEnd();
			*pi = htonl(_baseport);
			rbuf.dataSize(rbuf.dataSize() + sizeof(int32));
			//d.cq.pop();
			d.state = Data::Connected;
			written = true;
		}
		if(written){//send the buffer
			_rsb.timeout = _tpos;//TODO: d.lasttimepos can be uninitialized
			//TODO: use a variable instead of 0
			//_rsb.timeout += 10;//10 miliseconds
			//if(_rsb.timeout <= _tpos)
			//d.lasttimepos = _tpos;
			//else
			d.lasttimepos = _rsb.timeout;
			if(rbuf.dataSize()){
				rbuf.id(d.sendid);
				d.incrementSendId();
				--d.bufjetons;
				//now cache all waiting signals based on their last buffer id
				//so that whe this buffer is sent, only then the signal can be deleted
				while(waitsigs <= pcrtwaitsig){
					d.waitSignal(*pcrtwaitsig).bufid= rbuf.id();
					--pcrtwaitsig;
				}
			}else{
				rbuf.id(Data::UpdateBufferId);
			}
			idbgx(Dbg::ipc, "sending buffer id = "<<rbuf.id());
			_rsb.paddr = &d.pairaddr;
			return NOK;//this was the last data to send reenter for keep alive
		}else{
			Data::OutBufferPairTp &rob(d.keepAliveBuffer());
			//if the keep alive buffer is not already being sent
			// and we can send a keepalive buffer
			// we schedule it for resending
			// retransmitId() == 0 buffer sent
			// retransmitId() == 1 first scheduled for resent
			// retransmitId() == 2 first sent
			idbgx(Dbg::ipc, "keep alive buffer - retransmitid = "<<rob.first.retransmitId());
			if(!rob.first.retransmitId() && d.canSendKeepAlive(_tpos)){
				//schedule sending the keepalive buffer
				idbgx(Dbg::ipc, "keep alive buffer - schedule");
				Data::OutBufferPairTp &rob(d.keepAliveBuffer());
				rob.first.retransmitId(1);
				++rob.second;//
				Specific::pushBuffer(_rsb.b.pb, Specific::capacityToId(_rsb.b.bufferCapacity()));
				_rsb.b.pb = NULL;
				_rsb.b.bc = 1;
				_rsb.b.dl = rob.second;
				_rsb.bufpos = 1;
				_rsb.timeout = _tpos;
				_rsb.timeout += d.keepalivetout;
				return OK;//we have something to send
			}
			//nothing to send
		}
	}
	return BAD;//nothing to send
}

//TODO: optimize!!
bool ProcessConnector::freeSentBuffers(Buffer &_rbuf, const ConnectorUid &_rconid){
	bool b = false;
	idbgx(Dbg::ipc, "beg update count "<<_rbuf.updatesCount()<<" sent count = "<<d.outbufs.size());
	for(uint32 i(0); i < _rbuf.updatesCount(); ++i){
		for(Data::OutBufferVectorTp::iterator it(d.outbufs.begin()); it != d.outbufs.end(); ++it){
			if(it->first.buffer() && _rbuf.update(i) == it->first.id()){//done with this one
				if(it != d.outbufs.begin()){
					d.popOutWaitSignals(it->first.id(), _rconid);
					b = true;
					char *pb = it->first.buffer();
					Specific::pushBuffer(pb, Specific::capacityToId(it->first.bufferCapacity()));
					++d.bufjetons;
					++it->second;
					it->first.reinit();//clear the buffer
					d.outfreestk.push((uint16)(it - d.outbufs.begin()));
				}else{//the keep alive buffer:
					it->first.retransmitId(0);//we can now initiate the sending of keepalive buffer
					b = true;
				}
			}
		}
	}
	idbgx(Dbg::ipc, "end update count "<<_rbuf.updatesCount()<<" sent count = "<<d.outbufs.size());
	return b;
}
void ProcessConnector::parseBuffer(Buffer &_rbuf, const ConnectorUid &_rconid){
	const char *bpos = _rbuf.data();
	int			blen = _rbuf.dataSize();
	int rv;
// 	cassert(d.rcq.size());
// 	cassert(d.rcq.front().second);
	int 		firstblen = blen - 1;
	idbgx(Dbg::ipc, "bufferid = "<<_rbuf.id());
	while(blen > 2){
		idbgx(Dbg::ipc, "blen = "<<blen);
		uint8	datatype = *bpos;
		++bpos;
		--blen;
		switch(datatype){
			case Buffer::ContinuedSignal:
				idbgx(Dbg::ipc, "continuedsignal");
				cassert(blen == firstblen);
				if(!d.rcq.front().first){
					d.rcq.pop();
				}
				//we cannot have a continued signal on the same buffer
				break;
			case Buffer::NewSignal:
				idbgx(Dbg::ipc, "newsignal");
				if(d.rcq.size()){
					idbgx(Dbg::ipc, "switch to new rcq.size = "<<d.rcq.size());
					if(d.rcq.front().first){//the previous signal didnt end, we reschedule
						d.rcq.push(d.rcq.front());
						d.rcq.front().first = NULL;
					}
					d.rcq.front().second = d.popDeserializer();
					d.rcq.front().second->push(d.rcq.front().first);
				}else{
					idbgx(Dbg::ipc, "switch to new rcq.size = 0");
					d.rcq.push(Data::RecvCmdPairTp(NULL, d.popDeserializer()));
					d.rcq.front().second->push(d.rcq.front().first);
				}
				break;
			case Buffer::OldSignal:
				idbgx(Dbg::ipc, "oldsignal");
				cassert(d.rcq.size() > 1);
				if(d.rcq.front().first){
					d.rcq.push(d.rcq.front());
				}
				d.rcq.pop();
				break;
			default:{
				cassert(false);
			}
		}
		rv = d.rcq.front().second->run(bpos, blen);
		cassert(rv >= 0);
		blen -= rv;
		if(d.rcq.front().second->empty()){//done one signal.
			SignalUid siguid(0xffffffff, 0xffffffff);
			if(d.rcq.front().first->ipcReceived(siguid, _rconid))
				delete d.rcq.front().first;
			idbgx(Dbg::ipc, "donesignal "<<siguid.idx<<','<<siguid.uid);
			if(siguid.idx != 0xffffffff && siguid.uid != 0xffffffff){//a valid signal waiting for response
				d.popOutWaitSignal(siguid);
			}
			d.pushDeserializer(d.rcq.front().second);
			//we do not pop it because in case of a new signal,
			//we must know if the previous signal terminate
			//so we dont mistakingly reschedule another signal!!
			d.rcq.front().first = NULL;
			d.rcq.front().second = NULL;
		}
	}
}


}//namespace ipc
}//namespace foundation
