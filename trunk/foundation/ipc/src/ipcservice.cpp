/* Implementation file ipcservice.cpp
	
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

#include <map>
#include <vector>

#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/socketdevice.hpp"

#include "foundation/objectpointer.hpp"
#include "foundation/common.hpp"
#include "foundation/manager.hpp"

#include "foundation/ipc/ipcservice.hpp"
#include "foundation/ipc/connectoruid.hpp"

#include "ipctalker.hpp"
#include "iodata.hpp"
#include "processconnector.hpp"

namespace fdt = foundation;

namespace foundation{
namespace ipc{

//*******	Service::Data	******************************************************************

struct Service::Data{
	struct ProcStub{
		ProcStub(ProcessConnector*	_proc = NULL, uint32 _uid = 0):proc(_proc), uid(_uid){}
		ProcessConnector*	proc;
		uint32				uid;
	};
	typedef std::pair<uint32, uint32>					TkrPairTp;
	//typedef std::pair<uint16, uint16>					ProcPosPairTp;
	typedef std::pair<const Inet4SockAddrPair*, int>	BaseProcAddr4;
	typedef std::map<
		const BaseProcAddr4*, 
		ConnectorUid, 
		Inet4AddrPtrCmp
		>												BaseProcAddr4MapTp;
	
	typedef std::pair<const Inet6SockAddrPair*, int>	BaseProcAddr6;
	typedef std::map<
		const BaseProcAddr6*,
		ConnectorUid,
		Inet6AddrPtrCmp
		>												BaseProcAddr6MapTp;
	typedef std::vector<TkrPairTp>						TalkerVectorTp;
	Data();
	~Data();	
	
	int						procpertkrcnt;
	uint32					proccnt;
	int						baseport;
	SocketAddress			firstaddr;
	TalkerVectorTp			tkrvec;
	BaseProcAddr4MapTp		basepm4;
	uint32 					keepalivetout;
};

//=======	ServiceData		===========================================

Service::Data::Data():procpertkrcnt(10), proccnt(0), baseport(-1){
}
Service::Data::~Data(){
}

//=======	Service		===============================================

Service::Service(uint32 _keepalivetout):d(*(new Data)){
	//d.maxtkrcnt = 2;//TODO: make it configurable
	d.keepalivetout = _keepalivetout;
	ProcessConnector::init();
}

Service::~Service(){
	delete &d;
}
uint32 Service::keepAliveTimeout()const{
	return d.keepalivetout;
}
int Service::sendSignal(
	const ConnectorUid &_rconid,//the id of the process connector
	DynamicPointer<Signal> &_psig,//the signal to be sent
	uint32	_flags
){
	cassert(_rconid.tkrid < d.tkrvec.size());
	Mutex::Locker	lock(*mutex());
	Data::TkrPairTp	&rtp(d.tkrvec[_rconid.tkrid]);
	Mutex::Locker lock2(this->mutex(rtp.first, rtp.second));
	Talker *ptkr = static_cast<Talker*>(this->object(rtp.first, rtp.second));
	cassert(ptkr);
	if(ptkr->pushSignal(_psig, _rconid, _flags | SameConnectorFlag)){
		//the talker must be signaled
		if(ptkr->signal(fdt::S_RAISE)){
			Manager::the().raiseObject(*ptkr);
		}
	}
	return OK;
}

int Service::basePort()const{
	return d.baseport;
}

int Service::doSendSignal(
	const SockAddrPair &_rsap,
	DynamicPointer<Signal> &_psig,//the signal to be sent
	ConnectorUid *_pconid,
	uint32	_flags
){
	if(_rsap.family() != AddrInfo::Inet4 && _rsap.family() != AddrInfo::Inet6) return -1;
	Mutex::Locker	lock(*mutex());
	if(_rsap.family() == AddrInfo::Inet4){
		Inet4SockAddrPair 	inaddr(_rsap);
		Data::BaseProcAddr4	baddr(&inaddr, inaddr.port());
		Data::BaseProcAddr4MapTp::iterator	it(d.basepm4.find(&baddr));
		if(it != d.basepm4.end()){
			idbgx(Dbg::ipc, "");
			ConnectorUid	conid = it->second;
			cassert(conid.tkrid < d.tkrvec.size());
			Data::TkrPairTp	&rtp(d.tkrvec[conid.tkrid]);
			Mutex::Locker lock2(this->mutex(rtp.first, rtp.second));
			Talker *ptkr = static_cast<Talker*>(this->object(rtp.first, rtp.second));
			cassert(ptkr);
			if(ptkr->pushSignal(_psig, conid, _flags)){
				//the talker must be signaled
				if(ptkr->signal(fdt::S_RAISE)){
					Manager::the().raiseObject(*ptkr);
				}
			}
			if(_pconid) *_pconid = conid;
			return OK;
		}else{//the process connector does not exist
			idbgx(Dbg::ipc, "");
			int16 tkrid = computeTalkerForNewProcess();
			uint32 tkrpos,tkruid;
			if(tkrid >= 0){
				//the talker exists
				tkrpos = d.tkrvec[tkrid].first;
				tkruid = d.tkrvec[tkrid].second;
			}else{
				//create new talker
				tkrid = createNewTalker(tkrpos, tkruid);
				if(tkrid < 0) return BAD;
			}
			Mutex::Locker lock2(this->mutex(tkrpos, tkruid));
			Talker *ptkr = static_cast<Talker*>(this->object(tkrpos, tkruid));
			cassert(ptkr);
			ProcessConnector *ppc = new ProcessConnector(inaddr, d.keepalivetout);
			ConnectorUid conid(tkrid);
			ptkr->pushProcessConnector(ppc, conid);
			d.basepm4[ppc->baseAddr4()] = conid;
// 			DynamicPointer<test::Signal> pnullsig(NULL);
// 			ptkr->pushSignal(pnullsig, conid, Buffer::Connecting);
			ptkr->pushSignal(_psig, conid, _flags);
			if(ptkr->signal(fdt::S_RAISE)){
				Manager::the().raiseObject(*ptkr);
			}
			if(_pconid) *_pconid = conid;
			return OK;
		}
	}else{//inet6
		cassert(false);
		//TODO:
	}
	return OK;
}

int16 Service::computeTalkerForNewProcess(){
	++d.proccnt;
	if(d.proccnt % d.procpertkrcnt) return d.proccnt / d.procpertkrcnt;
	return -1;
}

int Service::acceptProcess(ProcessConnector *_ppc){
	Mutex::Locker	lock(*mutex());
	{
		//TODO: try to think if the locking is ok!!!
		Data::BaseProcAddr4MapTp::iterator	it(d.basepm4.find(_ppc->baseAddr4()));
		if(it != d.basepm4.end()){
			//a connection still exists
			uint32 tkrpos,tkruid;
			tkrpos = d.tkrvec[it->second.tkrid].first;
			tkruid = d.tkrvec[it->second.tkrid].second;
			Mutex::Locker lock2(this->mutex(tkrpos, tkruid));
			Talker *ptkr = static_cast<Talker*>(this->object(tkrpos, tkruid));
			ptkr->pushProcessConnector(_ppc, it->second, true);
			if(ptkr->signal(fdt::S_RAISE)){
				Manager::the().raiseObject(*ptkr);
			}
			return OK;
		}
	}
	int16 tkrid = computeTalkerForNewProcess();
	uint32 tkrpos,tkruid;
	if(tkrid >= 0){
		//the talker exists
		tkrpos = d.tkrvec[tkrid].first;
		tkruid = d.tkrvec[tkrid].second;
	}else{
		//create new talker
		tkrid = createNewTalker(tkrpos, tkruid);
		if(tkrid < 0) return BAD;
	}
	Mutex::Locker lock2(this->mutex(tkrpos, tkruid));
	Talker *ptkr = static_cast<Talker*>(this->object(tkrpos, tkruid));
	cassert(ptkr);
	ConnectorUid conid(tkrid, 0xffff, 0xffff);
	ptkr->pushProcessConnector(_ppc, conid);
	d.basepm4[_ppc->baseAddr4()] = conid;
// 	DynamicPointer<test::Signal> pnullsig(NULL);
// 	ptkr->pushSignal(pnullsig, conid, Buffer::Accepted);
	if(ptkr->signal(fdt::S_RAISE)){
		Manager::the().raiseObject(*ptkr);
	}
	return OK;
}

void Service::disconnectTalkerProcesses(Talker &_rtkr){
	Mutex::Locker	lock(*mutex());
	_rtkr.disconnectProcesses();
}

void Service::disconnectProcess(ProcessConnector *_ppc){
	d.basepm4.erase(_ppc->baseAddr4());
}

int16 Service::createNewTalker(uint32 &_tkrpos, uint32 &_tkruid){
	if(d.tkrvec.size() > 30000) return BAD;
	int16 tkrid = d.tkrvec.size();
	d.firstaddr.port(d.firstaddr.port() + tkrid);
	SocketDevice sd;
	sd.create(d.firstaddr.family(), AddrInfo::Datagram, 0);
	sd.bind(d.firstaddr);
	d.firstaddr.port(d.firstaddr.port() - tkrid);
	if(sd.ok()){
		Talker *ptkr = new Talker(sd, *this, tkrid);
		if(this->doInsert(*ptkr, this->index())){
			delete ptkr;
			return BAD;
		}
		d.tkrvec.push_back(Data::TkrPairTp((_tkrpos = ptkr->id()), (_tkruid = this->uid(*ptkr))));
		//Manager::the().pushJob((fdt::udp::Talker*)ptkr);
		doPushTalkerInPool(ptkr);
	}else{
		return BAD;
	}
	return tkrid;
}

int Service::insertConnection(
	const SocketDevice &_rsd
){
/*	Connection *pcon = new Connection(_pch, 0);
	if(this->insert(*pcon, _serviceid)){
		delete pcon;
		return BAD;
	}
	_rm.pushJob((fdt::tcp::Connection*)pcon);*/
	return OK;
}

int Service::insertListener(
	const AddrInfoIterator &_rai
){
/*	test::Listener *plis = new test::Listener(_pst, 100, 0);
	if(this->insert(*plis, _serviceid)){
		delete plis;
		return BAD;
	}	
	_rm.pushJob((fdt::tcp::Listener*)plis);*/
	return OK;
}
int Service::insertTalker(
	const AddrInfoIterator &_rai,
	const char *_node,
	const char *_svc
){	
	SocketDevice sd;
	sd.create(_rai);
	sd.bind(_rai);
	if(!sd.ok()) return BAD;
	Mutex::Locker lock(*mutex());
	cassert(!d.tkrvec.size());//only the first tkr must be inserted from outside
	Talker *ptkr = new Talker(sd, *this, 0);
	if(this->doInsert(*ptkr, this->index())){
		delete ptkr;
		return BAD;
	}
	d.firstaddr = _rai;
	d.baseport = d.firstaddr.port();
	d.tkrvec.push_back(Data::TkrPairTp(ptkr->id(), this->uid(*ptkr)));
	//_rm.pushJob((fdt::udp::Talker*)ptkr);
	doPushTalkerInPool(ptkr);
	return OK;
}

int Service::insertConnection(
	const AddrInfoIterator &_rai,
	const char *_node,
	const char *_svc
){
	
/*	Connection *pcon = new Connection(_pch, _node, _svc);
	if(this->insert(*pcon, _serviceid)){
		delete pcon;
		return BAD;
	}
	_rm.pushJob((fdt::tcp::Connection*)pcon);*/
	return OK;
}

int Service::removeTalker(Talker& _rtkr){
	this->remove(_rtkr);
	return OK;
}

int Service::removeConnection(Connection &_rcon){
//	this->remove(_rcon);
	return OK;
}

// int Service::execute(ulong _sig, TimeSpec &_rtout){
// 	idbgx(Dbg::ipc, "serviceexec");
// 	if(signaled()){
// 		ulong sm;
// 		{
// 			Mutex::Locker	lock(*mut);
// 			sm = grabSignalMask(1);
// 		}
// 		if(sm & fdt::S_KILL){
// 			idbgx(Dbg::ipc, "killing service "<<this->id());
// 			this->stop(Manager::the(), true);
// 			Manager::the().removeService(this);
// 			return BAD;
// 		}
// 	}
// 	return NOK;
// }


//=======	Buffer		=============================================
bool Buffer::check()const{
	cassert(bc >= 32);
	//TODO:
	if(this->pb){
		if(header().size() < sizeof(Header)) return false;
		return true;
	}
	return false;
}
void Buffer::print()const{
	idbgx(Dbg::ipc, "version = "<<(int)header().version<<" id = "<<header().id<<" retransmit = "<<header().retransid<<" flags = "<<header().flags<<" type = "<<(int)header().type<<" updatescnt = "<<header().updatescnt<<" bufcp = "<<bc<<" dl = "<<dl);
	for(int i = 0; i < header().updatescnt; ++i){
		idbgx(Dbg::ipc, "update = "<<update(i));
	}
}

}//namespace ipc
}//namespace foundation

