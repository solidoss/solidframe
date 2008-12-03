/* Implementation file ipctalker.cpp
	
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
#include <map>
#include <cerrno>
#include <cstring>

#include "system/timespec.hpp"
#include "system/socketaddress.hpp"
#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/specific.hpp"

#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "core/server.hpp"

#include "ipc/ipcservice.hpp"
#include "ipc/connectoruid.hpp"

#include "ipctalker.hpp"
#include "processconnector.hpp"

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

	Data(Service &_rservice, uint16 _id):rservice(_rservice), nextprocid(0), tkrid(_id){}
	Service				&rservice;
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

Talker::Talker(const SocketDevice &_rsd, Service &_rservice, uint16 _id):	BaseTp(_rsd), d(*new Data(_rservice, _id)){
}
//----------------------------------------------------------------------
Talker::~Talker(){
	char *pb = d.rcvbuf.buffer();
	Specific::pushBuffer(pb, Specific::capacityToId(4096));
	idbgx(Dbg::ipc, "sendq.size = "<<d.sendq.size());
	while(d.sendq.size()){
		pb = d.sendq.top()->b.buffer();
		if(pb){
			idbgx(Dbg::ipc, "released buffer");
			Specific::pushBuffer(pb, Specific::capacityToId(d.sendq.top()->b.bufferCapacity()));
		}
		d.sendq.pop();
	}
	while(d.newprocs.size()){
		delete d.newprocs.top().first;
		d.newprocs.pop();
	}
	for(Data::ProcVectorTp::iterator it(d.procs.begin()); it != d.procs.end(); ++it){
		idbgx(Dbg::ipc, "deleting process "<<(void*)it->first<<" pos = "<<(it - d.procs.begin()));
		delete it->first;
		it->first = NULL;
		
	}
	d.rservice.removeTalker(*this);
	delete &d;
}
//----------------------------------------------------------------------
inline bool Talker::inDone(ulong _sig, const TimeSpec &_rts){
	if(_sig & cs::INDONE){
		//TODO: reject too smaller buffers
		d.rcvbuf.bufferSize(socketRecvSize());
		if(d.rcvbuf.check()){
			idbgx(Dbg::ipc, "received valid buffer size "<<d.rcvbuf.bufferSize()<<" data size "<<d.rcvbuf.dataSize()<<" id "<<d.rcvbuf.id());
			dispatchReceivedBuffer(socketRecvAddr(), _rts);//the address is deeply copied
		}else{
			idbgx(Dbg::ipc, "received invalid buffer size "<<d.rcvbuf.bufferSize()<<" data size "<<d.rcvbuf.dataSize()<<" id "<<d.rcvbuf.id());
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
		idbgx(Dbg::ipc, "disconnecting process "<<(void*)ppc);
		cassert(ppc);
		if(ppc->isDisconnecting()){
			idbgx(Dbg::ipc, "deleting process "<<(void*)ppc<<" on pos "<<d.closes.top());
			d.rservice.disconnectProcess(ppc);
			//unregister from base and peer:
			if(ppc->peerAddr4() && ppc->peerAddr4()->addr){
				d.peerpm4.erase(ppc->peerAddr4());
			}
			if(ppc->baseAddr4() && ppc->baseAddr4()->first && ppc->baseAddr4()->first->addr){
				d.basepm4.erase(ppc->baseAddr4());
			}
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
// 			idbgx(Dbg::ipc, "talker timeout");
// 		if(_sig & cs::ERRDONE)
// 			idbgx(Dbg::ipc, "talker error");
// 		return BAD;
// 	}
	bool nothing = true;
	idbgx(Dbg::ipc, "this = "<<(void*)this<<" &d = "<<(void*)&d);
	if(signaled() || d.closes.size()){
		{
			nothing = false;
			Mutex::Locker	lock(rs.mutex(*this));
			ulong sm = grabSignalMask(0);
			if(sm & cs::S_KILL){
				idbgx(Dbg::ipc, "intalker - dying");
				return BAD;
			}
			idbgx(Dbg::ipc, "intalker - signaled");
			if(sm == cs::S_RAISE){
				_sig |= cs::SIGNALED;
			}else{
				idbgx(Dbg::ipc, "unknown signal");
			}
			//insert new processes
			while(d.newprocs.size()){
				int32 newprocidx(d.newprocs.top().second);
				ProcessConnector *pnewproc(d.newprocs.top().first);
				if((uint32)newprocidx >= d.procs.size()){
					uint32 oldsz = d.procs.size();
					d.procs.resize(newprocidx + 1);
					for(uint32 i = oldsz; i < d.procs.size(); ++i){
						d.procs[i].first = NULL;
						d.procs[i].second = 0;
					}
				}
				if(!d.procs[newprocidx].first){
					d.procs[newprocidx].first = pnewproc;
					pnewproc->prepare();
					//register in base map
					d.basepm4[pnewproc->baseAddr4()] = newprocidx;
					if(pnewproc->isConnected()){
						//register in peer map
						d.peerpm4[pnewproc->peerAddr4()] = newprocidx;
					}
				}else{//a reconnect	
					Data::ProcPairTp &rpp(d.procs[newprocidx]);
					d.peerpm4.erase(rpp.first->peerAddr4());
					rpp.first->reconnect(pnewproc);
					++rpp.second;
					d.peerpm4[rpp.first->peerAddr4()] = newprocidx;
					//TODO: try use Specific for process connectors!!
					//TODO: Ensure that there are no pending sends!!!
					delete pnewproc;
				}
				d.cq.push(newprocidx);
				d.newprocs.pop();
			}
			d.nextprocid = d.procs.size();//TODO: see if its right
			//dispatch commands before deleting processconnectors
			while(d.cmdq.size()){
				cassert(d.cmdq.front().procid < d.procs.size());
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
		nothing = false;
		//this is to ensure the locking order: first service then talker
		d.rservice.disconnectTalkerProcesses(*this);
	}
	//try to recv something
	if(inDone(_sig, _tout) || (!socketHasPendingRecv())){
		nothing  = false;
		//non blocking read some buffers
		int cnt = 0;
		while(cnt++ < 32){
			if(!d.rcvbuf.buffer()){
				cnt += 2;//fewer iterations if we keep storing received buffers
				d.rcvbuf.reinit(Specific::popBuffer(Specific::capacityToId(4096)), 4096);
			}else{
				d.rcvbuf.reset();
			}
			switch(socketRecv(d.rcvbuf.buffer(), d.rcvbuf.bufferCapacity())){
				case BAD:
					idbgx(Dbg::ipc, "socket error "<<strerror(errno));
					cassert(false);
					break;
				case OK:
					d.rcvbuf.bufferSize(socketRecvSize());
					if(d.rcvbuf.check()){
						idbgx(Dbg::ipc, "received valid buffer size "<<d.rcvbuf.bufferSize()<<" data size "<<d.rcvbuf.dataSize()<<" id "<<d.rcvbuf.id());
						dispatchReceivedBuffer(socketRecvAddr(), _tout);//the address is deeply copied
					}else{
						idbgx(Dbg::ipc, "received invalid buffer size "<<d.rcvbuf.bufferSize()<<" data size "<<d.rcvbuf.dataSize()<<" id "<<d.rcvbuf.id());
					}
					break;
				case NOK:
					idbgx(Dbg::ipc, "receive tout");
					goto DoneRecv;
			}
		}
		DoneRecv:;
	}
	if(_sig & cs::OUTDONE){
		nothing = false;
		dispatchSentBuffer(_tout);
	}
	bool mustreenter = processCommands(_tout);
	
	while(d.sendq.size() && !socketHasPendingSend()){
		if(_tout < d.sendq.top()->timeout){
			_tout = d.sendq.top()->timeout;
			idbgx(Dbg::ipc, "return "<<nothing);
			return mustreenter ? OK : NOK;
		}
		nothing = false;
		if(d.sendq.top()->b.buffer() == NULL){
			//simulate sending
			mustreenter = dispatchSentBuffer(_tout);
			continue;
		}
		switch(socketSend(d.sendq.top()->b.buffer(), d.sendq.top()->b.bufferSize(), *d.sendq.top()->paddr)){
			case BAD:
			case OK: 
				mustreenter = dispatchSentBuffer(_tout);
				break;
			case NOK:
				//_tout.set(0);
				idbgx(Dbg::ipc, "return");
				return mustreenter ? OK : NOK;
		}
	}//while
	//if we've sent something or
	//if we did not have a timeout while reading we MUST do an ioUpdate asap.
	if(mustreenter || !socketHasPendingRecv() || d.closes.size()){
		idbgx(Dbg::ipc, "return "<<nothing);
		return OK;
	}
	//_tout.set(0);
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
			cassert(d.nextprocid < (uint16)0xffff);
			_rconid.procid = d.nextprocid;
			_rconid.procuid = 0;
			++d.nextprocid;//TODO: it must be reseted somewhere!!!
		}
	}
	d.newprocs.push(Data::NewProcPairTp(_pc, _rconid.procid));
}
//----------------------------------------------------------------------
//dispatch d.rcvbuf
void Talker::dispatchReceivedBuffer(const SockAddrPair &_rsap, const TimeSpec &_rts){
	cassert(_rsap.family() == AddrInfo::Inet4);
	idbgx(Dbg::ipc, "received buffer:");
	d.rcvbuf.print();

	switch(d.rcvbuf.type()){
		case Buffer::KeepAliveType:
		case Buffer::DataType:{
			idbgx(Dbg::ipc, "data buffer");
			Inet4SockAddrPair					inaddr(_rsap);
			Data::PeerProcAddr4MapTp::iterator	pit(d.peerpm4.find(&inaddr));
			if(pit != d.peerpm4.end()){
				idbgx(Dbg::ipc, "found process for buffer");
				Data::ProcPairTp &rpp(d.procs[pit->second]);
				cassert(rpp.first);
				ConnectorUid conid(d.tkrid, pit->second, rpp.second);
				switch(d.procs[pit->second].first->pushReceivedBuffer(d.rcvbuf, conid, _rts)){
					case BAD:
						cassert(false);
						idbgx(Dbg::ipc, "the processconnector wants to close");
						//TODO: it may crash if uncommenting the below if block
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
			idbgx(Dbg::ipc, "connecting buffer with baseport "<<baseport);
			if(baseport >= 0){
				ProcessConnector *ppc(new ProcessConnector(inaddr, baseport, d.rservice.keepAliveTimeout()));
				if(d.rservice.acceptProcess(ppc)) delete ppc;
			}
		}break;
		case Buffer::AcceptingType:{
			int baseport = ProcessConnector::parseAcceptedBuffer(d.rcvbuf);
			idbgx(Dbg::ipc, "accepting buffer with baseport "<<baseport);
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
			cassert(false);
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
				idbgx(Dbg::ipc, "BAD processSendCommands - should be rare");
				break;
			case NOK://not finished sending
				d.cq.push(procid);
			case OK:
				++sndbufcnt;
				if(psb->b.buffer()){
					optimizeBuffer(psb->b);
				}
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
	}/*else{//TODO:
		cassert(!psb->b.buffer());
		d.sendfs.push(psb);
	}*/
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


