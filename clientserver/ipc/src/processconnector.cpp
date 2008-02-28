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
#include "core/command.hpp"
#include "core/server.hpp"
#include "ipc/ipcservice.hpp"

namespace cs = clientserver;

namespace clientserver{
namespace ipc{

struct BufCmp{
	bool operator()(const Buffer &_rbuf1, const Buffer &_rbuf2)const{
		return _rbuf1.id() > _rbuf2.id();
	}
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
		LastBufferId = 0xffffffff - 1,
		UpdateBufferId = 0xffffffff,//the id of a buffer containing only updates
		MaxCommandBufferCount = 32,//continuous buffers sent for a command
		MaxSendCommandQueueSize = 16,//max count of commands sent in paralell
		DataRetransmitCount = 8,
		ConnectRetransmitCount = 16
	};
	struct BinSerializer:serialization::bin::Serializer<>{
		BinSerializer(BinMapper &_rbm):serialization::bin::Serializer<>(_rbm){}
		static unsigned specificCount(){return MaxSendCommandQueueSize;}
		void specificRelease(){}
	};

	struct BinDeserializer:serialization::bin::Deserializer<>{
		BinDeserializer(BinMapper &_rbm):serialization::bin::Deserializer<>(_rbm){}
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
	typedef Queue<uint32>						ReceivedIdsQueueTp;
	typedef BinSerializer						BinSerializerTp;
	typedef BinDeserializer						BinDeserializerTp;
	typedef std::pair<const SockAddrPair*, int>	BaseProcAddr;
	typedef std::pair<Command*, BinDeserializerTp*>
												RecvCmdPairTp;
	typedef Queue<RecvCmdPairTp>				RecvCmdQueueTp;
	struct SendCommand{
		SendCommand(const cs::CmdPtr<Command>& _rpcmd, uint32 _flags):pcmd(_rpcmd), flags(_flags), pser(NULL){}
		~SendCommand(){
			cassert(!pser);
		}
		cs::CmdPtr<Command> pcmd;
		uint32				flags;
		BinSerializerTp		*pser;
	};
	typedef Queue<SendCommand>					SendCmdQueueTp;
	
	Data(BinMapper &_rm, const Inet4SockAddrPair &_raddr);
	//Data(BinMapper &_rm, const Inet6SockAddrPair &_raddr);
	Data(BinMapper &_rm, const Inet4SockAddrPair &_raddr, int _baseport);
	//Data(BinMapper &_rm, const Inet6SockAddrPair &_raddr, int _baseport);
	~Data();
	std::pair<uint16, uint16> insertSentBuffer(Buffer &);
	std::pair<uint16, uint16> getSentBuffer(int _bufpos);
	void incrementExpectedId();
	void incrementSendId();
	BinSerializerTp* popSerializer();
	void pushSerializer(BinSerializerTp*);
	BinDeserializerTp* popDeserializer();
	void pushDeserializer(BinDeserializerTp*);
//data:	
	BinMapper				&rbm;
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
};


ProcessConnector::Data::Data(BinMapper &_rm, const Inet4SockAddrPair &_raddr):
	rbm(_rm), expectedid(1), retranstimeout(1000),
	state(Connecting), flags(0), bufjetons(1), crtcmdbufcnt(MaxCommandBufferCount),sendid(0),
	addr(_raddr), pairaddr(addr), baseaddr(&pairaddr, addr.port()){
}

// ProcessConnector::Data::Data(BinMapper &_rm, const Inet6SockAddrPair &_raddr, int _tkrid, int _procid):
// 	ser(_rm), des(_rm), pincmd(NULL), expectedid(1), id(_procid), retranstimeout(1000),
// 	lockcnt(0), state(Connecting), flags(0), bufjetons(1), sendid(0), tkrid(_tkrid),
// 	addr(_raddr), pairaddr(addr), baseaddr(&pairaddr, addr.port()){
// }

ProcessConnector::Data::Data(BinMapper &_rm, const Inet4SockAddrPair &_raddr, int _baseport):
	rbm(_rm), expectedid(1), retranstimeout(1000),
	state(Accepting), flags(0), bufjetons(3), crtcmdbufcnt(MaxCommandBufferCount), sendid(0),
	addr(_raddr), pairaddr(addr), baseaddr(&pairaddr, _baseport){
}

// ProcessConnector::Data::Data(BinMapper &_rm, const Inet6SockAddrPair &_raddr, int _tkrid, int _procid, int _baseport):
// 	ser(_rm), des(_rm), pincmd(NULL), expectedid(1), id(_procid), retranstimeout(1000),
// 	lockcnt(0), state(Connected), flags(0), bufjetons(3), sendid(0), tkrid(_tkrid),
// 	addr(_raddr), pairaddr(addr), baseaddr(&pairaddr, _baseport){
// }

ProcessConnector::Data::~Data(){
	idbg(""<<(void*)this);
	for(OutBufferVectorTp::iterator it(outbufs.begin()); it != outbufs.end(); ++it){
		char *pb = it->first.buffer();
		if(pb){ 
			idbg("released buffer");
			Specific::pushBuffer(pb, Specific::capacityToId(it->first.bufferCapacity()));
		}
	}
	idbg("cq.size = "<<cq.size());
	while(cq.size()) cq.pop();
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
		p = new BinSerializerTp(rbm);
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
		p = new BinDeserializerTp(rbm);
	}
	return p;
}
void ProcessConnector::Data::pushDeserializer(ProcessConnector::Data::BinDeserializerTp* _p){
	cassert(_p->empty());
	Specific::cache(_p);
}

//*******************************************************************************

ProcessConnector::~ProcessConnector(){
	delete &d;
}

void ProcessConnector::init(BinMapper &_rm){
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
	
ProcessConnector::ProcessConnector(BinMapper &_rm, const Inet4SockAddrPair &_raddr):d(*(new Data(_rm, _raddr))){}
//ProcessConnector::ProcessConnector(BinMapper &_rm, const Inet6SockAddrPair &_raddr):d(*(new Data(_rm, _raddr))){}
ProcessConnector::ProcessConnector(BinMapper &_rm, const Inet4SockAddrPair &_raddr, int _baseport):d(*(new Data(_rm, _raddr, _baseport))){}
//ProcessConnector::ProcessConnector(BinMapper &_rm, const Inet6SockAddrPair &_raddr, int _baseport):d(*(new Data(_rm, _raddr, _baseport))){}
	
//TODO: ensure that really no command is dropped on reconnect
// not even those from Dropped buffers!!!
void ProcessConnector::reconnect(ProcessConnector *_ppc){
	idbg("reconnecting - clearing process data");
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
		d.rcq.front().second->clear();
		d.pushDeserializer(d.rcq.front().second);
		d.rcq.pop();
	}
	//clear the send queue
	for(int sz = d.scq.size(); sz; --sz){
		if(d.scq.front().pser){
			d.scq.front().pser->clear();
			d.pushSerializer(d.scq.front().pser);
			d.scq.front().pser = NULL;
		}
		//NOTE: on reconnect the responses, or commands sent using ConnectorUid are dropped
		if(!(d.scq.front().flags & Service::SameConnectorFlag)){
			d.scq.push(d.scq.front());
		}
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
	return reinterpret_cast<Inet4SockAddrPair*>(&d.pairaddr);
}

const std::pair<const Inet4SockAddrPair*, int>* ProcessConnector::baseAddr4()const{
	return reinterpret_cast<std::pair<const Inet4SockAddrPair*, int>*>(&d.baseaddr);
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
		idbg("inbufq.size() = "<<d.inbufq.size()<<" d.inbufq.top().id() = "<<d.inbufq.top().id()<<" d.expectedid "<<d.expectedid); 

		Buffer b(d.inbufq.top());
		char *pb = d.inbufq.top().buffer();
		Specific::pushBuffer(pb, Specific::capacityToId(d.inbufq.top().bufferCapacity()));
		d.inbufq.pop();
	}
	if(d.inbufq.size()){
		idbg("inbufq.size() = "<<d.inbufq.size()<<" d.inbufq.top().id() = "<<d.inbufq.top().id()<<" d.expectedid "<<d.expectedid);
		return d.inbufq.top().id() == d.expectedid;
	}
	return false;
}

int ProcessConnector::pushReceivedBuffer(Buffer &_rbuf, const ConnectorUid &_rcodid){
	idbg("rcvbufid = "<<_rbuf.id()<<" expected id "<<d.expectedid<<" inbufq.size = "<<d.inbufq.size());
	if(_rbuf.id() != d.expectedid){
		if(_rbuf.id() < d.expectedid){
			//the peer doesnt know that we've already received the buffer
			//add it as update
			d.rcvidq.push(_rbuf.id());
			idbg("already received buffer - resend update");
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
				idbg("keep the buffer for future parsing");
				return NOK;//we have to send updates
			}else{
				idbg("to many buffers out-of-order");
				return OK;
			}
		}else{//a buffer containing only updates
			cassert(_rbuf.updatesCount());
			idbg("received buffer containing only updates");
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
	idbg("");
	if(_rbuf.b.buffer()){
		if(_rbuf.b.id() > Data::LastBufferId){
			cassert(_rbuf.b.id() == Data::UpdateBufferId);
			idbg("sent updates - collecting buffer");
			char *pb = _rbuf.b.buffer();
			Specific::pushBuffer(pb, Specific::capacityToId(_rbuf.b.bufferCapacity()));
			++d.bufjetons;
			return NOK;//maybe we have something to send
		}else{
			idbg("sent bufpos = "<<_rbuf.bufpos<<" retransmitid "<<_rbuf.b.retransmitId()<<" buf = "<<(void*)_rbuf.b.buffer()<<" buffercap = "<<_rbuf.b.bufferCapacity());
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
			idbg("p.first "<<p.first<<" p.second "<<p.second);
			//reuse _rbuf for retransmission timeout
			_rbuf.b.pb = NULL;
			_rbuf.b.bc = p.first;
			_rbuf.b.dl = p.second;
			_rbuf.bufpos = p.first;
			idbg("prepare waitbuf b.cap "<<_rbuf.b.bufferCapacity()<<" b.dl "<<_rbuf.b.dl);
			cassert(sizeof(uint32) <= sizeof(size_t));
			_rbuf.timeout = _tpos;
			_rbuf.timeout += d.retranstimeout;//miliseconds retransmission timeout
			_reusebuf = true;
			return OK;
		}
	}else{//a timeout occured
		idbg("timeout occured _rbuf.bc "<<_rbuf.b.bc<<" rbuf.dl "<<_rbuf.b.dl<<" d.outbufs.size() = "<<d.outbufs.size());
		if(_rbuf.b.bc){//for a sent buffer
			cassert(_rbuf.b.bc <= d.outbufs.size());
			int bufpos(_rbuf.b.bc - 1);
			Data::OutBufferPairTp &rob(d.outbufs[bufpos]);
			idbg("rob.second = "<<rob.second);
			if(rob.first.buffer() && rob.second == _rbuf.b.dl){
				//we must resend this buffer
				idbg("resending buffer "<<(bufpos)<<" retransmit id = "<<rob.first.retransmitId()<<" buf = "<<(void*)rob.first.buffer());
				rob.first.print();
				rob.first.retransmitId(rob.first.retransmitId() + 1);
				if(rob.first.retransmitId() > Data::DataRetransmitCount){
					if(rob.first.type() == Buffer::ConnectingType){
						if(rob.first.retransmitId() > Data::ConnectRetransmitCount){//too many resends for connect type
							idbg("preparing to disconnect process");
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
				idbg("nothing to resend");
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
	idbg("bufjetons = "<<d.bufjetons);
	if(d.bufjetons || d.state != Data::Connected){
		idbg("d.rcvidq.size() = "<<d.rcvidq.size());
		bool written = false;
		Buffer &rbuf(_rsb.b);
		rbuf.reset();
		if(d.state == Data::Connected){
			rbuf.type(Buffer::DataType);
			//first push the updates if any
			while(d.rcvidq.size() && rbuf.updatesCount() < 8){
				rbuf.pushUpdate(d.rcvidq.front());
				written = true;
				d.rcvidq.pop();
			}
			//then push the commands
			idbg("d.cq.size() = "<<d.cq.size()<<" d.scq.size = "<<d.scq.size());
			//fill scq
			while(d.cq.size() && d.scq.size() < Data::MaxSendCommandQueueSize){
				uint32 flags = d.cq.front().second;
				//flags &= ~Data::ContinuedCommand;
				cassert(d.cq.front().first.ptr());
				d.scq.push(Data::SendCommand(d.cq.front().first, flags));
				d.cq.pop();
			}
			Data::BinSerializerTp	*pser = NULL;
			//at most we can send Data::MaxSendCommandQueueSize commands within a single buffer
			while(d.scq.size()){
				if(d.crtcmdbufcnt){
					if(d.scq.front().pser){//a continued command
						if(d.crtcmdbufcnt == Data::MaxCommandBufferCount){
							if(rbuf.flags() & (Buffer::SwitchToNew | Buffer::SwitchToOld)) break;//use the next buffer
							rbuf.flags(rbuf.flags() | Buffer::SwitchToOld);
						}
					}else{//a new commnad
						if(rbuf.flags() & Buffer::SwitchToOld) break;//use the next buffer
						rbuf.flags(rbuf.flags() | Buffer::SwitchToNew);
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
					written = true;
					int rv = d.scq.front().pser->run(rbuf.dataEnd(), rbuf.dataFreeSize());
					cassert(rv >= 0);//TODO: deal with the situation!
					rbuf.dataSize(rbuf.dataSize() + rv);
					if(d.scq.front().pser->empty()){//finished with this command
						if(pser) d.pushSerializer(pser);
						pser = d.scq.front().pser;
						d.scq.front().pser = NULL;
						d.scq.pop();
						d.crtcmdbufcnt = Data::MaxCommandBufferCount;
						if(rbuf.dataFreeSize() < 64) break;
					}else{
						break;
					}
				}else if(d.scq.size() == 1){
					d.crtcmdbufcnt = Data::MaxCommandBufferCount - 1;//small trick so that it wouldnt switchtoold
				}else{
					d.scq.push(d.scq.front());
					d.scq.front().pser = NULL;
					d.scq.pop();
					d.crtcmdbufcnt = Data::MaxCommandBufferCount;
				}
			}
			if(pser) d.pushSerializer(pser);
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
			}else{
				rbuf.id(Data::UpdateBufferId);
			}
			_rsb.paddr = &d.pairaddr;
			--d.bufjetons;
			if(!d.scq.size() && d.rcvidq.size()) return OK;
			return NOK;
		}
	}
	return BAD;//nothing to send
}

//TODO: optimize!!
bool ProcessConnector::freeSentBuffers(Buffer &_rbuf){
	bool b = false;
	for(uint32 i(0); i < _rbuf.updatesCount(); ++i){
		idbg("compare update "<<_rbuf.update(i)<<" out bufs sz = "<<d.outbufs.size());
		for(Data::OutBufferVectorTp::iterator it(d.outbufs.begin()); it != d.outbufs.end(); ++it){
			if(it->first.buffer() && _rbuf.update(i) == it->first.id()){//done with this one
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
	//Server &srv(Server::the());
	int rv;
	if(_rbuf.flags() & Buffer::SwitchToNew){
		if(d.rcq.size()){
			d.rcq.push(d.rcq.front());
			d.rcq.front().first = NULL;
			d.rcq.front().second = d.popDeserializer();
			d.rcq.front().second->push(d.rcq.front().first);
		}else{
			d.rcq.push(Data::RecvCmdPairTp(NULL, d.popDeserializer()));
			d.rcq.front().second->push(d.rcq.front().first);
		}
	}else if(_rbuf.flags() & Buffer::SwitchToOld){
		cassert(d.rcq.size() > 1);
		d.rcq.push(d.rcq.front());
		d.rcq.pop();
	}
	cassert(d.rcq.size());
	cassert(d.rcq.front().second);
	while(blen){
		rv = d.rcq.front().second->run(bpos, blen);
		cassert(rv >= 0);
		blen -= rv;
		if(d.rcq.front().second->empty()){//done one command.
			if(d.rcq.front().first->received(_rcodid)) delete d.rcq.front().first;
// 			std::pair<uint32, uint32> to = d.rcq.front().first->to();
// 			srv.signalObject(to.first, to.second, d.rcq.front().first);
			d.rcq.front().first = NULL;
			if(blen){
				d.rcq.front().second->push(d.rcq.front().first);
			}else{
				d.pushDeserializer(d.rcq.front().second);
				d.rcq.pop();
				break;
			}
		}
	}
}

}//namespace ipc
}//namespace clientserver
