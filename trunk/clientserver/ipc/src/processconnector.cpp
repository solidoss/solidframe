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
#include "system/debug.hpp"
#include "system/socketaddress.hpp"
#include "system/specific.hpp"
#include "utility/queue.hpp"
#include "processconnector.hpp"
#include "iodata.hpp"
#include "algorithm/serialization/binary.hpp"
#include "algorithm/serialization/idtypemap.hpp"
#include "core/command.hpp"
#include "core/server.hpp"
#include "ipc/ipcservice.hpp"

#include <iostream>
using namespace std;

namespace cs = clientserver;

namespace clientserver{
namespace ipc{

struct BufCmp{
	bool operator()(const Buffer &_rbuf1, const Buffer &_rbuf2)const{
		return _rbuf1.id() > _rbuf2.id();
	}
};

typedef serialization::IdTypeMap					IdTypeMap;

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
		LastBufferId = 0xffffffff - 1,
		UpdateBufferId = 0xffffffff,//the id of a buffer containing only updates
		MaxCommandBufferCount = 32,//continuous buffers sent for a command
		MaxSendCommandQueueSize = 16,//max count of commands sent in paralell
		DataRetransmitCount = 8,
		ConnectRetransmitCount = 16
	};
	struct BinSerializer:serialization::bin::Serializer{
		BinSerializer():serialization::bin::Serializer(IdTypeMap::the()){}
		static unsigned specificCount(){return MaxSendCommandQueueSize;}
		void specificRelease(){}
	};

	struct BinDeserializer:serialization::bin::Deserializer{
		BinDeserializer():serialization::bin::Deserializer(IdTypeMap::the()){}
		static unsigned specificCount(){return MaxSendCommandQueueSize;}
		void specificRelease(){}
	};

	typedef std::pair<Buffer, uint16>			OutBufferPairTp;
	typedef std::vector<OutBufferPairTp>		OutBufferVectorTp;
	typedef Stack<uint16>						OutFreePosStackTp;
	typedef std::priority_queue<Buffer,std::vector<Buffer>,BufCmp>	
												BufferPriorityQueueTp;
	typedef std::pair<cs::CmdPtr<Command>, uint32>
												CmdPairTp;
	typedef Queue<CmdPairTp>					CmdQueueTp;
	struct OutWaitCommand{
		OutWaitCommand(
			uint32 _bufid,
			const cs::CmdPtr<Command>& _rcmd,
			uint32 _flags,
			uint32 _id
		):bufid(_bufid), cmd(_rcmd), flags(_flags), id(_id){}
		bool operator<(const OutWaitCommand &_owc)const{
			if(cmd.ptr()){
				if(_owc.cmd.ptr()){
				}else return true;
			}else{
// 				if(_owc.cmd.ptr()){
// 					return false;
// 				}else return true;
				return false;
			}
			//TODO: optimize!!
			if(id < _owc.id){
				return (_owc.id - id) < (0xffffffff/2);
			}else if(id > _owc.id){
				return (id - _owc.id) >= (0xffffffff/2);
			}else return false;
		}
		uint32				bufid;
		cs::CmdPtr<Command> cmd;
		uint32				flags;
		uint32				id;
	};
	typedef std::vector<OutWaitCommand>			OutCmdVectorTp;
	typedef Queue<uint32>						ReceivedIdsQueueTp;
	typedef BinSerializer						BinSerializerTp;
	typedef BinDeserializer						BinDeserializerTp;
	typedef std::pair<const SockAddrPair*, int>	BaseProcAddr;
	typedef std::pair<Command*, BinDeserializerTp*>
												RecvCmdPairTp;
	typedef Queue<RecvCmdPairTp>				RecvCmdQueueTp;
	struct SendCommand{
		SendCommand(
			const cs::CmdPtr<Command>& _rpcmd,
			uint32 _flags,
			uint32	_id
		):pcmd(_rpcmd), flags(_flags), id(_id), pser(NULL){}
		~SendCommand(){
			cassert(!pser);
		}
		cs::CmdPtr<Command> pcmd;
		uint32				flags;
		uint32				id;
		BinSerializerTp		*pser;
	};
	typedef Queue<SendCommand>					SendCmdQueueTp;
	
	Data(const Inet4SockAddrPair &_raddr);
	Data(const Inet4SockAddrPair &_raddr, int _baseport);
	~Data();
	std::pair<uint16, uint16> insertSentBuffer(Buffer &);
	std::pair<uint16, uint16> getSentBuffer(int _bufpos);
	void incrementExpectedId();
	void incrementSendId();
	BinSerializerTp* popSerializer();
	void pushSerializer(BinSerializerTp*);
	BinDeserializerTp* popDeserializer();
	void pushDeserializer(BinDeserializerTp*);
	//save commands to be resent in case of disconnect
	void pushOutWaitCommand(uint32 _bufid, cs::CmdPtr<Command> &_cmd, uint32 _flags, uint32 _id);
	void popOutWaitCommands(uint32 _bufid);
//data:	
	uint32					expectedid;
	int32					retranstimeout;
	int16					state;
	uint16					flags;
	int16					bufjetons;
	uint16					crtcmdbufcnt;
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
	OutCmdVectorTp			outcmdv;
	OutFreePosStackTp		outfreecmdstk;
	uint32					sndcmdid;
};


ProcessConnector::Data::Data(const Inet4SockAddrPair &_raddr):
	expectedid(1), retranstimeout(300),
	state(Connecting), flags(0), bufjetons(1), 
	crtcmdbufcnt(MaxCommandBufferCount),sendid(0),
	addr(_raddr), pairaddr(addr), 
	baseaddr(&pairaddr, addr.port()),sndcmdid(0){
}

// ProcessConnector::Data::Data(BinMapper &_rm, const Inet6SockAddrPair &_raddr, int _tkrid, int _procid):
// 	ser(_rm), des(_rm), pincmd(NULL), expectedid(1), id(_procid), retranstimeout(1000),
// 	lockcnt(0), state(Connecting), flags(0), bufjetons(1), sendid(0), tkrid(_tkrid),
// 	addr(_raddr), pairaddr(addr), baseaddr(&pairaddr, addr.port()){
// }

ProcessConnector::Data::Data(const Inet4SockAddrPair &_raddr, int _baseport):
	expectedid(1), retranstimeout(300),
	state(Accepting), flags(0), bufjetons(3), crtcmdbufcnt(MaxCommandBufferCount), sendid(0),
	addr(_raddr), pairaddr(addr), baseaddr(&pairaddr, _baseport),sndcmdid(0){
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
	while(cq.size()) cq.pop();
	while(scq.size()){
		if(scq.front().pser){
			scq.front().pser->clear();
			pushSerializer(scq.front().pser);
			scq.front().pser = NULL;
		}
		scq.pop();
	}
	while(rcq.size()){
		if(rcq.front().second){
			rcq.front().second->clear();
			pushDeserializer(rcq.front().second);
		}
		delete rcq.front().first;
		rcq.pop();
	}
}
std::pair<uint16, uint16> ProcessConnector::Data::insertSentBuffer(Buffer &_rbuf){
	std::pair<uint16, uint16> p;
	cassert(_rbuf.buffer());
	if(outfreestk.size()){
		p.first = outfreestk.top();
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

void ProcessConnector::Data::pushOutWaitCommand(uint32 _bufid, cs::CmdPtr<Command> &_cmd, uint32 _flags, uint32 _id){
	if(this->outfreecmdstk.size()){
		OutWaitCommand &owc = outcmdv[outfreecmdstk.top()];
		owc.bufid = _bufid;
		owc.cmd = _cmd;
		owc.flags = _flags;
		outfreecmdstk.pop();
	}else{
		this->outcmdv.push_back(OutWaitCommand(_bufid, _cmd, _flags, _id));
	}
}
void ProcessConnector::Data::popOutWaitCommands(uint32 _bufid){
	for(OutCmdVectorTp::iterator it(outcmdv.begin()); it != outcmdv.end(); ++it){
		if(it->bufid == _bufid && it->cmd.ptr()){
			it->bufid = 0;
			it->cmd.clear();
		}
	}
}

//*******************************************************************************

ProcessConnector::~ProcessConnector(){
	delete &d;
}

void ProcessConnector::init(){
}
// static
int ProcessConnector::parseAcceptedBuffer(Buffer &_rbuf){
	if(_rbuf.dataSize() == sizeof(int32)){
		int32 *pp((int32*)_rbuf.data());
		return (int)ntohl((uint32)*pp);
	}
	return -1;
}
// static
int ProcessConnector::parseConnectingBuffer(Buffer &_rbuf){
	if(_rbuf.dataSize() == sizeof(int32)){
		int32 *pp((int32*)_rbuf.data());
		return (int)ntohl((uint32)*pp);
	}
	return -1;
}
	
ProcessConnector::ProcessConnector(const Inet4SockAddrPair &_raddr):d(*(new Data(_raddr))){}
//ProcessConnector::ProcessConnector(BinMapper &_rm, const Inet6SockAddrPair &_raddr):d(*(new Data(_rm, _raddr))){}
ProcessConnector::ProcessConnector(const Inet4SockAddrPair &_raddr, int _baseport):d(*(new Data(_raddr, _baseport))){}
	
//TODO: ensure that really no command is dropped on reconnect
// not even those from Dropped buffers!!!
void ProcessConnector::reconnect(ProcessConnector *_ppc){
	idbgx(Dbg::ipc, "reconnecting - clearing process data");
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
			idbgx(Dbg::ipc, "command scheduled for resend");
			d.cq.push(d.cq.front());
		}else{
			idbgx(Dbg::ipc, "command not scheduled for resend");
		}
		d.cq.pop();
	}
	uint32 cqsz = d.cq.size();
	//we must put into the cq all command in the exact oreder they were sent
	//to do so we must push into cq, the commands from scq and waitcmdv 
	//ordered by their id
	for(int sz = d.scq.size(); sz; --sz){
		if(d.scq.front().pser){
			d.scq.front().pser->clear();
			d.pushSerializer(d.scq.front().pser);
			d.scq.front().pser = NULL;
		}
		//NOTE: on reconnect the responses, or commands sent using ConnectorUid are dropped
		if(!(d.scq.front().flags & Service::SameConnectorFlag)){
			d.pushOutWaitCommand(0, d.scq.front().pcmd, d.scq.front().flags, d.scq.front().id);
		}else{
			idbgx(Dbg::ipc, "command not scheduled for resend");
		}
		d.scq.pop();
	}
	
	//now we need to sort d.outcmdv
	std::sort(d.outcmdv.begin(), d.outcmdv.end());
	
	//then we push into cq the commands from outcmv
	for(Data::OutCmdVectorTp::const_iterator it(d.outcmdv.begin()); it != d.outcmdv.end(); ++it){
		if(it->cmd.ptr()){
			d.cq.push(Data::CmdPairTp(it->cmd, it->flags));
		}else break;
	}
	//clear out cmd vector and stack
	d.outcmdv.clear();
	while(d.outfreecmdstk.size()){
		d.outfreecmdstk.pop();
	}
	//put the commands already in the cq to the end of cq
	while(cqsz--){
		d.cq.push(d.cq.front());
		d.cq.pop();
	}
	
	d.crtcmdbufcnt = Data::MaxCommandBufferCount;
	d.expectedid = 1;
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
	//we also want that the connect buffer will put on outbufs[0] so we complicate the algorithm a litle bit
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
		int pos = it - d.outbufs.begin();
		if(pos)	d.outfreestk.push(pos);
	}
	if(d.outbufs.size()){
		d.outfreestk.push(0);
	}
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

void ProcessConnector::completeConnect(int _pairport){
	d.state = Data::Connected;
	d.addr.port(_pairport);
	//free the connecting buffer
	cassert(d.outbufs[0].first.buffer());
	//_rs.collectBuffer(d.outbufs[0].first, _riod);
	char *pb = d.outbufs[0].first.buffer();
	Specific::pushBuffer(pb, Specific::capacityToId(d.outbufs[0].first.bufferCapacity()));
	d.bufjetons = 3;
	++d.outbufs[0].second;
	d.outbufs[0].first.reinit();//clear the buffer
	d.outfreestk.push(0);
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


int ProcessConnector::pushCommand(clientserver::CmdPtr<Command> &_rcmd, uint32 _flags){
	//_flags &= ~Data::ProcessReservedFlags;
	d.cq.push(Data::CmdPairTp(_rcmd, _flags));
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

int ProcessConnector::pushReceivedBuffer(Buffer &_rbuf, const ConnectorUid &_rcodid){
	idbgx(Dbg::ipc, "rcvbufid = "<<_rbuf.id()<<" expected id "<<d.expectedid<<" inbufq.size = "<<d.inbufq.size()<<" flags = "<<_rbuf.flags());
	if(_rbuf.id() != d.expectedid){
		if(_rbuf.id() < d.expectedid){
			//the peer doesnt know that we've already received the buffer
			//add it as update
			d.rcvidq.push(_rbuf.id());
			idbgx(Dbg::ipc, "already received buffer - resend update");
			return NOK;//we have to send updates
		}else if(_rbuf.id() != Data::UpdateBufferId){
			if(_rbuf.updatesCount()){//we have updates
				freeSentBuffers(_rbuf);
			}
			//try to keep the buffer for future parsing
			if(d.inbufq.size() < 4){
				d.rcvidq.push(_rbuf.id());//for peer updates
				d.inbufq.push(_rbuf);
				_rbuf.reinit();
				idbgx(Dbg::ipc, "keep the buffer for future parsing");
				return NOK;//we have to send updates
			}else{
				idbgx(Dbg::ipc, "to many buffers out-of-order");
				return OK;
			}
		}else{//a buffer containing only updates
			_rbuf.updatesCount();
			idbgx(Dbg::ipc, "received buffer containing only updates");
			if(freeSentBuffers(_rbuf)) return NOK;
			return OK;
		}
	}else{
		//the expected buffer
		d.rcvidq.push(_rbuf.id());
		if(_rbuf.updatesCount()){
			freeSentBuffers(_rbuf);
		}
		parseBuffer(_rbuf, _rcodid);
		d.incrementExpectedId();//move to the next buffer
		//while(d.inbufq.top().id() == d.expectedid){
		while(moveToNextInBuffer()){
			Buffer b(d.inbufq.top());
			d.inbufq.pop();
			//this is already done on receive
			parseBuffer(b, _rcodid);
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
			cassert(_rbuf.b.id() == Data::UpdateBufferId);
			idbgx(Dbg::ipc, "sent updates - collecting buffer");
			char *pb = _rbuf.b.buffer();
			Specific::pushBuffer(pb, Specific::capacityToId(_rbuf.b.bufferCapacity()));
			//++d.bufjetons;
			return NOK;//maybe we have something to send
		}else{
			idbgx(Dbg::ipc, "sent bufid = "<<_rbuf.b.id()<<" bufpos = "<<_rbuf.bufpos<<" retransmitid "<<_rbuf.b.retransmitId()<<" buf = "<<(void*)_rbuf.b.buffer()<<" buffercap = "<<_rbuf.b.bufferCapacity()<<" flags = "<<_rbuf.b.flags());
			std::pair<uint16, uint16>  p;
			if(!_rbuf.b.retransmitId()){
				//cassert(_rbuf.bufpos < 0);
				p = d.insertSentBuffer(_rbuf.b);	//_rbuf.bc will contain the buffer index, and
													//_rbuf.dl will contain the buffer uid
			}else{
				//cassert(_rbuf.bufpos >= 0);
				p = d.getSentBuffer(_rbuf.bufpos);
			}
			++p.first;
			//_rbuf.b.retransmitId(_rbuf.retransmitId() + 1);
			idbgx(Dbg::ipc, "p.first "<<p.first<<" p.second "<<p.second);
			//reuse _rbuf for retransmission timeout
			_rbuf.b.pb = NULL;
			_rbuf.b.bc = p.first;
			_rbuf.b.dl = p.second;
			_rbuf.bufpos = p.first;
			idbgx(Dbg::ipc, "prepare waitbuf b.cap "<<_rbuf.b.bufferCapacity()<<" b.dl "<<_rbuf.b.dl);
			cassert(sizeof(uint32) <= sizeof(size_t));
			_rbuf.timeout = _tpos;
			_rbuf.timeout += d.retranstimeout;//miliseconds retransmission timeout
			_reusebuf = true;
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
				rob.first.print();
				rob.first.retransmitId(rob.first.retransmitId() + 1);
				if(rob.first.retransmitId() > Data::DataRetransmitCount){
					if(rob.first.type() == Buffer::ConnectingType){
						if(rob.first.retransmitId() > Data::ConnectRetransmitCount){//too many resends for connect type
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

int ProcessConnector::processSendCommands(SendBufferData &_rsb, const TimeSpec &_tpos, int _baseport){
	idbgx(Dbg::ipc, "bufjetons = "<<d.bufjetons);
	if(d.bufjetons || d.state != Data::Connected || d.rcvidq.size()){
		idbgx(Dbg::ipc, "d.rcvidq.size() = "<<d.rcvidq.size());
		bool written = false;
		Buffer &rbuf(_rsb.b);
		rbuf.reset();
		//here we keep per buffer waiting commands - 
		//commands that in case of a peer disconnection detection
		//can safely be resend
		struct WaitCmd{
			cs::CmdPtr<Command>	cmd;
			uint32				flags;
			uint32				id;
		}waitcmds[Data::MaxSendCommandQueueSize], *pcrtwaitcmd(waitcmds - 1);
			
		if(d.state == Data::Connected){
			rbuf.type(Buffer::DataType);
			//first push the updates if any
			while(d.rcvidq.size() && rbuf.updatesCount() < 8){
				rbuf.pushUpdate(d.rcvidq.front());
				written = true;
				d.rcvidq.pop();
			}
			if(d.bufjetons){//send data only if we have jetons
				//then push the commands
				idbgx(Dbg::ipc, "d.cq.size() = "<<d.cq.size()<<" d.scq.size = "<<d.scq.size());
				//fill scq
				while(d.cq.size() && d.scq.size() < Data::MaxSendCommandQueueSize){
					uint32 flags = d.cq.front().second;
					cassert(d.cq.front().first.ptr());
					d.scq.push(Data::SendCommand(d.cq.front().first, flags, d.sndcmdid++));
					d.cq.pop();
				}
				Data::BinSerializerTp	*pser = NULL;
				
				//at most we can send Data::MaxSendCommandQueueSize commands within a single buffer
				
				while(d.scq.size()){
					if(d.crtcmdbufcnt){
						if(d.scq.front().pser){//a continued command
							if(d.crtcmdbufcnt == Data::MaxCommandBufferCount){
								idbgx(Dbg::ipc, "oldcommand");
								rbuf.dataType(Buffer::OldCommand);
							}else{
								idbgx(Dbg::ipc, "continuedcommand");
								rbuf.dataType(Buffer::ContinuedCommand);
							}
						}else{//a new commnad
							idbgx(Dbg::ipc, "newcommand");
							rbuf.dataType(Buffer::NewCommand);
							if(pser){
								d.scq.front().pser = pser;
								pser = NULL;
							}else{
								d.scq.front().pser = d.popSerializer();
							}
							Command *pcmd(d.scq.front().pcmd.ptr());
							d.scq.front().pser->push(pcmd);
						}
						--d.crtcmdbufcnt;
						idbgx(Dbg::ipc, "d.crtcmdbufcnt = "<<d.crtcmdbufcnt);
						written = true;
						
						int rv = d.scq.front().pser->run(rbuf.dataEnd(), rbuf.dataFreeSize());
						
						cassert(rv >= 0);//TODO: deal with the situation!
						
						rbuf.dataSize(rbuf.dataSize() + rv);
						if(d.scq.front().pser->empty()){//finished with this command
							idbgx(Dbg::ipc, "donecommand");
							if(pser) d.pushSerializer(pser);
							pser = d.scq.front().pser;
							pser->clear();
							d.scq.front().pser = NULL;
							if(!(d.scq.front().flags & Service::SameConnectorFlag)){
								//if the command can be resent, we cache it
								idbgx(Dbg::ipc, "cached wait command");
								++pcrtwaitcmd;
								pcrtwaitcmd->cmd = d.scq.front().pcmd;
								pcrtwaitcmd->flags = d.scq.front().flags;
								pcrtwaitcmd->id = d.scq.front().id;
							}
							d.scq.pop();
							//we dont want to switch to an old command
							d.crtcmdbufcnt = Data::MaxCommandBufferCount - 1;
							if(rbuf.dataFreeSize() < 16) break;
							break;
						}else{
							break;
						}
					}else if(d.scq.size() == 1){//only one command
						d.crtcmdbufcnt = Data::MaxCommandBufferCount - 1;
					}else{
						d.scq.push(d.scq.front());
						d.scq.front().pser = NULL;
						d.scq.pop();
						d.crtcmdbufcnt = Data::MaxCommandBufferCount;
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
			_rsb.timeout = d.lasttimepos;//TODO: d.lasttimepos can be uninitialized
			//TODO: use a variable instead of 0
			_rsb.timeout += 0;//10 miliseconds
			if(_rsb.timeout <= _tpos)
				d.lasttimepos = _tpos;
			else
				d.lasttimepos = _rsb.timeout;
			if(rbuf.dataSize()){
				rbuf.id(d.sendid);
				d.incrementSendId();
				--d.bufjetons;
				//now cache all waiting commands based on their last buffer id
				//so that whe this buffer is sent, only then the command can be deleted
				while(waitcmds <= pcrtwaitcmd){
					d.pushOutWaitCommand(rbuf.id(), pcrtwaitcmd->cmd, pcrtwaitcmd->flags, pcrtwaitcmd->id);
					--pcrtwaitcmd;
				}
			}else{
				rbuf.id(Data::UpdateBufferId);
			}
			idbgx(Dbg::ipc, "sending buffer id = "<<rbuf.id());
			_rsb.paddr = &d.pairaddr;
			if(!d.scq.size() && d.rcvidq.size())
				return OK;
			return NOK;
		}
	}
	return BAD;//nothing to send
}

//TODO: optimize!!
bool ProcessConnector::freeSentBuffers(Buffer &_rbuf){
	bool b = false;
	for(uint32 i(0); i < _rbuf.updatesCount(); ++i){
		idbgx(Dbg::ipc, "compare update "<<_rbuf.update(i)<<" out bufs sz = "<<d.outbufs.size());
		for(Data::OutBufferVectorTp::iterator it(d.outbufs.begin()); it != d.outbufs.end(); ++it){
			if(it->first.buffer() && _rbuf.update(i) == it->first.id()){//done with this one
				d.popOutWaitCommands(it->first.id());
				b = true;
				char *pb = it->first.buffer();
				Specific::pushBuffer(pb, Specific::capacityToId(it->first.bufferCapacity()));
				++d.bufjetons;
				++it->second;
				it->first.reinit();//clear the buffer
				d.outfreestk.push((uint16)(it - d.outbufs.begin()));
			}
		}
	}
	return b;
}
void ProcessConnector::parseBuffer(Buffer &_rbuf, const ConnectorUid &_rcodid){
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
			case Buffer::ContinuedCommand:
				idbgx(Dbg::ipc, "continuedcommand");
				assert(blen == firstblen);
				if(!d.rcq.front().first){
					d.rcq.pop();
				}
				//we cannot have a continued command on the same buffer
				break;
			case Buffer::NewCommand:
				idbgx(Dbg::ipc, "newcommand");
				if(d.rcq.size()){
					idbgx(Dbg::ipc, "switch to new rcq.size = "<<d.rcq.size());
					if(d.rcq.front().first){//the previous command didnt end, we reschedule
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
			case Buffer::OldCommand:
				idbgx(Dbg::ipc, "oldcommand");
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
		if(d.rcq.front().second->empty()){//done one command.
			idbgx(Dbg::ipc, "donecommand");
			
			if(d.rcq.front().first->received(_rcodid))
				delete d.rcq.front().first;
			
			d.pushDeserializer(d.rcq.front().second);
			//we do not pop it because in case of a new command,
			//we must know if the previous command terminate
			//so we dont mistakingly reschedule another command!!
			d.rcq.front().first = NULL;
			d.rcq.front().second = NULL;
		}
	}
}


}//namespace ipc
}//namespace clientserver
