/* Implementation file ipctalker.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "udp/station.h"
#include <queue>
#include <map>
#include "system/timespec.h"
#include "system/socketaddress.h"
#include "system/debug.h"
#include "system/specific.h"
#include "utility/queue.h"
#include "utility/stack.h"
#include "core/server.h"
#include "ipc/ipcservice.h"
#include "ipctalker.h"
#include "processconnector.h"
#include "ipc/connectoruid.h"

#include <cerrno>

namespace cs = clientserver;

namespace clientserver{
namespace ipc{

struct Talker::Data{
	struct CommandData{
		CommandData(clientserver::CmdPtr<Command> &_pcmd, uint16 _procid, uint16 _procuid, uint32 _flags):
			pcmd(_pcmd), procid(_procid), procuid(_procuid), flags(_flags){}
		clientserver::CmdPtr<Command> pcmd;
		uint16	procid;
		uint16	procuid;
		uint32	flags;
	};
	struct SendBufferDataCmp{
		bool operator()(const SendBufferData* const &_rpon1,const SendBufferData* const &_rpon2)const{
			return _rpon1->timeout > _rpon2->timeout;
		}
	};
	typedef std::pair<ProcessConnector*, int32>			NewProcPairTp;
	typedef std::pair<ProcessConnector*, uint16>		ProcPairTp;
	typedef std::vector<ProcPairTp>						ProcVectorTp;
	
	typedef std::map<const Inet4SockAddrPair*, 
						uint32, Inet4AddrPtrCmp>		PeerProcAddr4MapTp;//TODO: test if a hash map is better
	typedef std::pair<const Inet4SockAddrPair*, int>	BaseProcAddr4;
	typedef std::map<const BaseProcAddr4*, uint32, Inet4AddrPtrCmp>
														BaseProcAddr4MapTp;
	
	typedef std::map<const Inet6SockAddrPair*, 
						uint32, Inet6AddrPtrCmp>		PeerProcAddr6MapTp;//TODO: test if a hash map is better
	typedef std::pair<const Inet6SockAddrPair*, int>	BaseProcAddr6;
	typedef std::map<const BaseProcAddr6*, uint32, Inet6AddrPtrCmp>
														BaseProcAddr6MapTp;

	
	typedef Queue<uint32>								CmdQueueTp;
	typedef Stack<uint32>								CloseStackTp;
	typedef Stack<std::pair<uint16, uint16> >			FreePosStackTp;//first is the pos second is the uid
	typedef Queue<CommandData>							CommandQueueTp;
	typedef std::priority_queue<SendBufferData*, std::vector<SendBufferData*>, SendBufferDataCmp>	
														SendQueueTp;
	typedef Stack<SendBufferData>						SendStackTp;
	typedef Stack<SendBufferData*>						SendFreeStackTp;
	typedef Stack<NewProcPairTp>						NewProcStackTp;

	Data(Service &_rservice, BinMapper &_rmapper, uint16 _id):rservice(_rservice), rmapper(_rmapper), nextprocid(0), tkrid(_id){}
	Service				&rservice;
	BinMapper 			&rmapper;
	uint32				nextprocid;
	uint16				tkrid;
	Buffer				rcvbuf;
	ProcVectorTp		procs;
	PeerProcAddr4MapTp	peerpm4;
	BaseProcAddr4MapTp	basepm4;
	FreePosStackTp		procfs;
	CmdQueueTp			cq;
	CommandQueueTp		cmdq;
	SendQueueTp			sendq;
	SendStackTp			sends;
	SendFreeStackTp		sendfs;
	NewProcStackTp		newprocs;
	CloseStackTp		closes;//a stack with talkers which want to close
};

//======================================================================

Talker::Talker(cs::udp::Station *_pst, Service &_rservice, BinMapper &_rmapper, uint16 _id):	BaseTp(_pst), d(*new Data(_rservice, _rmapper, _id)){
}
//----------------------------------------------------------------------
Talker::~Talker(){
	char *pb = d.rcvbuf.buffer();
	Specific::pushBuffer(pb, Specific::capacityToId(4096));
	idbg("sendq.size = "<<d.sendq.size());
	while(d.sendq.size()){
		pb = d.sendq.top()->b.buffer();
		if(pb){
			idbg("released buffer");
			Specific::pushBuffer(pb, Specific::capacityToId(d.sendq.top()->b.bufferCapacity()));
		}
		d.sendq.pop();
	}
	while(d.newprocs.size()){
		delete d.newprocs.top().first;
		d.newprocs.pop();
	}
	for(Data::ProcVectorTp::iterator it(d.procs.begin()); it != d.procs.end(); ++it){
		idbg("deleting process "<<(void*)it->first<<" pos = "<<(it - d.procs.begin()));
		delete it->first;
		it->first = NULL;
		
	}
	d.rservice.removeTalker(*this);
	delete &d;
}
//----------------------------------------------------------------------
inline bool Talker::inDone(ulong _sig){
	if(_sig & cs::INDONE){
		//TODO: reject too smaller buffers
		d.rcvbuf.bufferSize(station().recvSize());
		if(d.rcvbuf.check()){
			idbg("received valid buffer size "<<d.rcvbuf.bufferSize()<<" data size "<<d.rcvbuf.dataSize()<<" id "<<d.rcvbuf.id());
			dispatchReceivedBuffer(station().recvAddr());//the address is deeply copied
		}else{
			idbg("received invalid buffer size "<<d.rcvbuf.bufferSize()<<" data size "<<d.rcvbuf.dataSize()<<" id "<<d.rcvbuf.id());
		}
		return true;
	}else return false;
}
//this should be called under ipc service's mutex lock
void Talker::disconnectProcesses(){
	Server &rs = Server::the();
	Mutex::Locker	lock(rs.mutex(*this));
	//delete processconnectors
	while(d.closes.size()){
		ProcessConnector *ppc = d.procs[d.closes.top()].first;
		idbg("disconnecting process "<<(void*)ppc);
		assert(ppc);
		if(ppc->isDisconnecting()){
			idbg("deleting process "<<(void*)ppc<<" on pos "<<d.closes.top());
			d.rservice.disconnectProcess(ppc);
			delete ppc;
			d.procs[d.closes.top()].first = NULL;
			uint16 uidv = ++d.procs[d.closes.top()].second;
			d.procfs.push(std::pair<uint16, uint16>(d.closes.top(), uidv));
		}
		d.closes.pop();
	}
}
//----------------------------------------------------------------------
int Talker::execute(ulong _sig, TimeSpec &_tout){
	Server &rs = Server::the();
// 	if(_sig & (cs::TIMEOUT | cs::ERRDONE)){
// 		if(_sig & cs::TIMEOUT)
// 			idbg("talker timeout");
// 		if(_sig & cs::ERRDONE)
// 			idbg("talker error");
// 		return BAD;
// 	}
	if(signaled() || d.closes.size()){
		{
			Mutex::Locker	lock(rs.mutex(*this));
			ulong sm = grabSignalMask(0);
			if(sm & cs::S_KILL){
				idbg("intalker - dying");
				return BAD;
			}
			idbg("intalker - signaled");
			if(sm == cs::S_RAISE){
				_sig |= cs::SIGNALED;
			}else{
				idbg("unknown signal");
			}
			//insert new processes
			while(d.newprocs.size()){
				if((uint32)d.newprocs.top().second >= d.procs.size()){
					uint32 oldsz = d.procs.size();
					d.procs.resize(d.newprocs.top().second + 1);
					for(uint32 i = oldsz; i < d.procs.size(); ++i){
						d.procs[i].first = NULL;
						d.procs[i].second = 0;
					}
				}
				if(!d.procs[d.newprocs.top().second].first){
					d.procs[d.newprocs.top().second].first = d.newprocs.top().first;
					//register in base map
					d.basepm4[d.newprocs.top().first->baseAddr4()] = d.newprocs.top().second;
					if(d.newprocs.top().first->isConnected()){
						//register in peer map
						d.peerpm4[d.newprocs.top().first->peerAddr4()] = d.newprocs.top().second;
					}
				}else{//a reconnect	
					Data::ProcPairTp &rpp(d.procs[d.newprocs.top().second]);
					d.peerpm4.erase(rpp.first->peerAddr4());
					rpp.first->reconnect(d.newprocs.top().first);
					++rpp.second;
					d.peerpm4[rpp.first->peerAddr4()] = d.newprocs.top().second;
					//TODO: try use Specific for process connectors!!
					//TODO: Ensure that there are no pending sends!!!
					delete d.newprocs.top().first;
				}
				d.cq.push(d.newprocs.top().second);
				d.newprocs.pop();
			}
			//dispatch commands before deleting processconnectors
			while(d.cmdq.size()){
				assert(d.cmdq.front().procid < d.procs.size());
				Data::ProcPairTp	&rpp(d.procs[d.cmdq.front().procid]);
				uint32				flags(d.cmdq.front().flags);
				if(rpp.first && (!(flags & Service::SameConnectorFlag) || rpp.second == d.cmdq.front().procuid)){
					rpp.first->pushCommand(d.cmdq.front().pcmd, d.cmdq.front().flags);
					d.cq.push(d.cmdq.front().procid);
				}
				d.cmdq.pop();
			}
		}
	}
	if(d.closes.size()){
		//this is to ensure the locking order: first service then talker
		d.rservice.disconnectTalkerProcesses(*this);
	}
	//try to recv something
	if(inDone(_sig) || (!station().recvPending())){
		//non blocking read some buffers
		int cnt = 0;
		while(cnt++ < 16){
			if(!d.rcvbuf.buffer()){
				cnt += 2;//fewer iterations if we keep storing received buffers
				d.rcvbuf.reinit(Specific::popBuffer(Specific::capacityToId(4096)), 4096);
			}else{
				d.rcvbuf.reset();
			}
			switch(station().recvFrom(d.rcvbuf.buffer(), d.rcvbuf.bufferCapacity())){
				case BAD:
					idbg("socket error "<<strerror(errno));
					assert(false);
					break;
				case OK:
					d.rcvbuf.bufferSize(station().recvSize());
					if(d.rcvbuf.check()){
						idbg("received valid buffer size "<<d.rcvbuf.bufferSize()<<" data size "<<d.rcvbuf.dataSize()<<" id "<<d.rcvbuf.id());
						dispatchReceivedBuffer(station().recvAddr());//the address is deeply copied
					}else{
						idbg("received invalid buffer size "<<d.rcvbuf.bufferSize()<<" data size "<<d.rcvbuf.dataSize()<<" id "<<d.rcvbuf.id());
					}
					break;
				case NOK:
					idbg("receive tout");
					goto DoneRecv;
			}
		}
		DoneRecv:;
	}
	if(_sig & cs::OUTDONE){
		dispatchSentBuffer(_tout);
	}
	bool mustreenter = processCommands(_tout);
	
	while(d.sendq.size() && !station().sendPendingCount()){
		if(_tout < d.sendq.top()->timeout){
			_tout = d.sendq.top()->timeout - _tout;
			idbg("return");
			return mustreenter ? OK : NOK;
		}
		if(d.sendq.top()->b.buffer() == NULL){
			//simulate sending
			mustreenter = dispatchSentBuffer(_tout);
			continue;
		}
		switch(station().sendTo(d.sendq.top()->b.buffer(), d.sendq.top()->b.bufferSize(), *d.sendq.top()->paddr)){
			case BAD:
			case OK: 
				mustreenter = dispatchSentBuffer(_tout);
				break;
			case NOK:
				_tout.set(0);
				idbg("return");
				return mustreenter ? OK : NOK;
		}
	}//while
	//if we've sent something or
	//if we did not have a timeout while reading we MUST do an ioUpdate asap.
	if(mustreenter || !station().recvPending() || d.closes.size()){
		idbg("return");
		return OK;
	}
	_tout.set(0);
	return NOK;
}
//----------------------------------------------------------------------
int Talker::execute(){
	return BAD;
}
//----------------------------------------------------------------------
int Talker::accept(clientserver::Visitor &){
	return BAD;
}
//----------------------------------------------------------------------
//The talker's mutex should be locked
//return ok if the talker should be signaled
int Talker::pushCommand(clientserver::CmdPtr<Command> &_pcmd, const ConnectorUid &_rconid, uint32 _flags){
	d.cmdq.push(Data::CommandData(_pcmd, _rconid.procid, _rconid.procuid, _flags));
	return d.cmdq.size() == 1 ? NOK : OK;
}
//----------------------------------------------------------------------
//The talker's mutex should be locked
//Return's the new process connector's id
void Talker::pushProcessConnector(ProcessConnector *_pc, ConnectorUid &_rconid, bool _exists){
	if(_exists){
		++_rconid.procuid;
	}else{
		if(d.procfs.size()){
			_rconid.procid = d.procfs.top().first;
			_rconid.procuid = d.procfs.top().second;
			d.procfs.pop();
		}else{
			assert(d.nextprocid < (uint16)0xffff);
			_rconid.procid = d.nextprocid;
			_rconid.procuid = 0;
			++d.nextprocid;
		}
	}
	d.newprocs.push(Data::NewProcPairTp(_pc, _rconid.procid));
}
//----------------------------------------------------------------------
//dispatch d.rcvbuf
void Talker::dispatchReceivedBuffer(const SockAddrPair &_rsap){
	assert(_rsap.family() == AddrInfo::Inet4);
	idbg("received buffer:");
	d.rcvbuf.print();

	switch(d.rcvbuf.type()){
		case Buffer::DataType:{
			idbg("data buffer");
			Inet4SockAddrPair					inaddr(_rsap);
			Data::PeerProcAddr4MapTp::iterator	pit(d.peerpm4.find(&inaddr));
			if(pit != d.peerpm4.end()){
				idbg("found process for buffer");
				Data::ProcPairTp &rpp(d.procs[pit->second]);
				assert(rpp.first);
				ConnectorUid conid(d.tkrid, pit->second, rpp.second);
				switch(d.procs[pit->second].first->pushReceivedBuffer(d.rcvbuf, conid)){
					case BAD:
						idbg("the processconnector wants to close");
						if((rpp.first->isConnecting())){
							d.peerpm4.erase(rpp.first->peerAddr4());
							d.cq.push(pit->second);
						}else{
							d.closes.push(pit->second);
						}
					case OK:
						break;
					case NOK:
						d.cq.push(pit->second);
						break;
				}
			}
		}break;
		case Buffer::ConnectingType:{
			Inet4SockAddrPair		inaddr(_rsap);
			int baseport = ProcessConnector::parseConnectingBuffer(d.rcvbuf);
			idbg("connecting buffer with baseport "<<baseport);
			if(baseport >= 0){
				ProcessConnector *ppc(new ProcessConnector(d.rmapper, inaddr, baseport));
				if(d.rservice.acceptProcess(ppc)) delete ppc;
			}
		}break;
		case Buffer::AcceptingType:{
			int baseport = ProcessConnector::parseAcceptedBuffer(d.rcvbuf);
			idbg("accepting buffer with baseport "<<baseport);
			if(baseport >= 0){
				Inet4SockAddrPair					inaddr(_rsap);
				Data::BaseProcAddr4					ppa(&inaddr, baseport);
				Data::BaseProcAddr4MapTp::iterator	bit(d.basepm4.find(&ppa));
				ProcessConnector *ppc = NULL;
				if(bit != d.basepm4.end() && (ppc = d.procs[bit->second].first)){
					ppc->completeConnect(inaddr.port());
					//register in peer map
					d.peerpm4[ppc->peerAddr4()] = bit->second;
					//the connector has a command to send!!
					d.cq.push(bit->second);
				}
			}
		}break;
		default:
			assert(false);
	}
}
//----------------------------------------------------------------------
//process commands to be sent d.cq
//return true if the cq is not empty
bool Talker::processCommands(const TimeSpec &_rts){
	int sndbufcnt = 0;
	SendBufferData *psb = NULL;
	while(d.cq.size() && sndbufcnt < 16){
		uint32 procid = d.cq.front(); d.cq.pop();
		if(procid >= d.procs.size()) continue;
		ProcessConnector *ppc(d.procs[procid].first);
		//only a buffer for every process
		//SendBufferData defined in processconnector.h
		if(!psb){
			if(d.sendfs.size()){
				psb = d.sendfs.top();
				d.sendfs.pop();
			}else{
				d.sends.push(SendBufferData());
				psb = &d.sends.top();
			}
			psb->b.reinit(Specific::popBuffer(Specific::capacityToId(4096)), 4096);
		}
		switch(ppc->processSendCommands(*psb, _rts, d.rservice.basePort())){
			case BAD:
				//TODO: should very rarely pass here
				idbg("BAD processSendCommands - should be rare");
				break;
			case NOK://not finished sending
				d.cq.push(procid);
			case OK:
				++sndbufcnt;
				optimizeBuffer(psb->b);
				psb->procid = procid;
				d.sendq.push(psb);
				psb = NULL;
				break;
		}
	}
	if(psb){
		char *pb = psb->b.buffer();
		Specific::pushBuffer(pb, Specific::capacityToId(4096));
		d.sendfs.push(psb);
	}
	return d.cq.size();
}
//----------------------------------------------------------------------
bool Talker::dispatchSentBuffer(const TimeSpec &_rts){
	SendBufferData *psb(d.sendq.top());
	ProcessConnector *ppc(d.procs[psb->procid].first);
	bool	renqueuebuf = false;
	switch(ppc->pushSentBuffer(*psb, _rts, renqueuebuf)){
		case BAD:
			if(ppc->isConnecting()){
				//the process tries to reconnect
				d.peerpm4.erase(ppc->peerAddr4());
				++d.procs[psb->procid].second;
				d.cq.push(psb->procid);
			}else{
				d.closes.push(psb->procid);
			}
		case OK:
			break;
		case NOK:
			//the process has something to send
			d.cq.push(psb->procid);
			break;
	}
	d.sendq.pop();
	if(renqueuebuf){
		d.sendq.push(psb);
	}else{
		d.sendfs.push(psb);
	}
	return d.cq.size();
}
//----------------------------------------------------------------------
void Talker::optimizeBuffer(Buffer &_rbuf){
	uint id(Specific::sizeToId(_rbuf.bufferSize()));
	uint mid(Specific::capacityToId(4096));
	if(mid > id){
		uint32 datasz = _rbuf.dataSize();//the size
		uint32 buffsz = _rbuf.bufferSize();
		Buffer b(Specific::popBuffer(id), Specific::idToCapacity(id));//create a new smaller buffer
		memcpy(b.buffer(), _rbuf.buffer(), buffsz);//copy to the new buffer
		b.dataSize(datasz);
		char *pb = _rbuf.buffer();
		Specific::pushBuffer(pb, mid);
		_rbuf = b;
	}
}
//======================================================================
}//namespace ipc
}//namesapce clientserver


