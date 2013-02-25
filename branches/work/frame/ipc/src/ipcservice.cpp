/* Implementation file ipcservice.cpp
	
	Copyright 2007, 2008, 2010 Valentin Palade 
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

#include <vector>
#include <cstring>
#include <utility>

#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/socketdevice.hpp"
#include "system/specific.hpp"
#include "system/exception.hpp"

#include "utility/queue.hpp"

#include "frame/objectpointer.hpp"
#include "frame/common.hpp"
#include "frame/manager.hpp"

#include "frame/aio/openssl/opensslsocket.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/ipc/ipcconnectionuid.hpp"

#include "ipctalker.hpp"
#include "ipcnode.hpp"
#include "ipclistener.hpp"
#include "ipcsession.hpp"
#include "ipcbuffer.hpp"
#include "ipcutility.hpp"

#ifdef HAS_CPP11
#include <unordered_map>
#else
#include <map>
#endif

namespace fdt = foundation;

namespace foundation{
namespace ipc{

//*******	Service::Data	******************************************************************

struct Service::Data{
	struct SessionStub{
		SessionStub(Session*	_pses = NULL, uint32 _uid = 0):pses(_pses), uid(_uid){}
		Session*	pses;
		uint32		uid;
	};
#ifdef HAS_CPP11
	typedef std::unordered_map<
		const BaseAddress4T,
		ConnectionUid,
		SocketAddressHash,
		SocketAddressEqual
	>	SessionAddr4MapT;
	typedef std::unordered_map<
		const BaseAddress6T,
		ConnectionUid,
		SocketAddressHash,
		SocketAddressEqual
	>	SessionAddr6MapT;
	
	typedef std::unordered_map<
		const RelayAddress4T,
		ConnectionUid,
		SocketAddressHash,
		SocketAddressEqual
	>	SessionRelayAddr4MapT;
	typedef std::unordered_map<
		const RelayAddress6T,
		ConnectionUid,
		SocketAddressHash,
		SocketAddressEqual
	>	SessionRelayAddr6MapT;
#else
	typedef std::map<
		const BaseAddress4T,
		ConnectionUid, 
		SocketAddressCompare
	>	SessionAddr4MapT;
	
	typedef std::map<
		const BaseAddress6T,
		ConnectionUid,
		SocketAddressCompare
	>	SessionAddr6MapT;
	
	typedef std::map<
		const RelayAddress4T,
		ConnectionUid, 
		SocketAddressCompare
	>	SessionRelayAddr4MapT;
	
	typedef std::map<
		const RelayAddress6T,
		ConnectionUid,
		SocketAddressCompare
	>	SessionRelayAddr6MapT;
#endif	
	struct TalkerStub{
		TalkerStub():cnt(0){}
		TalkerStub(const ObjectUidT &_ruid, uint32 _cnt = 0):uid(_ruid), cnt(_cnt){}
		ObjectUidT	uid;
		uint32		cnt;
	};
	
	struct NodeStub{
		NodeStub():sesscnt(0), sockcnt(0){}
		NodeStub(
			const ObjectUidT &_ruid,
			uint32 _sesscnt = 0,
			uint32 _sockcnt = 0
		):uid(_ruid), sesscnt(_sesscnt), sockcnt(_sockcnt){}
		ObjectUidT	uid;
		uint32		sesscnt;
		uint32		sockcnt;
	};
	typedef std::vector<TalkerStub>						TalkerStubVectorT;
	typedef std::vector<NodeStub>						NodeStubVectorT;
	typedef Queue<uint32>								Uint32QueueT;
	
	Data(
		const DynamicPointer<Controller> &_rctrlptr,
		const uint32 _tkrsescnt,
		const uint32 _tkrmaxcnt,
		const uint32 _nodesescnt,
		const uint32 _nodesockcnt,
		const uint32 _nodemaxcnt
	);
	
	~Data();
	
	uint32 sessionCount()const{
		return sessionaddr4map.size();
	}
	
	DynamicPointer<Controller>	ctrlptr;
	const uint32				tkrsescnt;
	const uint32				tkrmaxcnt;
	const uint32				nodesescnt;
	const uint32				nodesockcnt;
	const uint32				nodemaxcnt;
	uint32						tkrcrt;
	uint32						nodecrt;
	int							baseport;
	SocketAddress				firstaddr;
	TalkerStubVectorT			tkrvec;
	NodeStubVectorT				nodevec;
	SessionAddr4MapT			sessionaddr4map;
	SessionRelayAddr4MapT		sessionrelayaddr4map;
	Uint32QueueT				tkrq;
	Uint32QueueT				sessnodeq;
	Uint32QueueT				socknodeq;
	TimeSpec					timestamp;
};

//=======	ServiceData		===========================================

Service::Data::Data(
	const DynamicPointer<Controller> &_rctrlptr,
	const uint32 _tkrsescnt,
	const uint32 _tkrmaxcnt,
	const uint32 _nodesescnt,
	const uint32 _nodesockcnt,
	const uint32 _nodemaxcnt
):
	ctrlptr(_rctrlptr), tkrsescnt(_tkrsescnt), tkrmaxcnt(_tkrmaxcnt),
	nodesescnt(_nodesescnt), nodesockcnt(_nodesockcnt), nodemaxcnt(_nodemaxcnt),
	tkrcrt(0), nodecrt(0), baseport(-1)
{
	timestamp.currentRealTime();
}

Service::Data::~Data(){
}

//=======	Service		===============================================

/*static*/ const char* Service::errorText(int _err){
	switch(_err){
		case BAD:
			return "Generic Error";
		case NoError:
			return "No Error";
		case NoGatewayError:
			return "No Gateway Error";
		default:
			return "Unknown Error";
	}
}

/*static*/ Service& Service::the(){
	return *m().service<Service>();
}
/*static*/ Service& Service::the(const IndexT &_ridx){
	return *m().service<Service>(_ridx);
}

Service::Service(
	const DynamicPointer<Controller> &_rctrlptr,
	const uint32 _tkrsescnt,
	const uint32 _tkrmaxcnt,
	const uint32 _nodesescnt,
	const uint32 _nodesockcnt,
	const uint32 _nodemaxcnt
):d(*(new Data(_rctrlptr, _tkrsescnt, _tkrmaxcnt, _nodesescnt, _nodesockcnt, _nodemaxcnt))){
	registerObjectType<Talker>(this);
}
//---------------------------------------------------------------------
Service::~Service(){
	delete &d;
}
//---------------------------------------------------------------------
const TimeSpec& Service::timeStamp()const{
	return d.timestamp;
}
//---------------------------------------------------------------------
int Service::sendSignal(
	DynamicPointer<Signal> &_psig,//the signal to be sent
	const ConnectionUid &_rconid,//the id of the process connector
	uint32	_flags
){
	cassert(_rconid.tid < d.tkrvec.size());
	
	Locker<Mutex>		lock(serviceMutex());
	IndexT				idx(Manager::the().computeIndex(d.tkrvec[_rconid.tid].uid.first));
	Locker<Mutex>		lock2(this->mutex(idx));
	Talker				*ptkr(static_cast<Talker*>(this->objectAt(idx)));
	
	cassert(ptkr);
	
	if(ptkr->pushSignal(_psig, SERIALIZATION_INVALIDID, _rconid, _flags | SameConnectorFlag)){
		//the talker must be signaled
		if(ptkr->signal(fdt::S_RAISE)){
			Manager::the().raiseObject(*ptkr);
		}
	}
	return OK;
}
//---------------------------------------------------------------------
void Service::doSendEvent(
	const ConnectionUid &_rconid,//the id of the process connectors
	int32 _event,
	uint32 _flags
){
	Locker<Mutex>		lock(serviceMutex());
	IndexT				idx(Manager::the().computeIndex(d.tkrvec[_rconid.tid].uid.first));
	Locker<Mutex>		lock2(this->mutex(idx));
	Talker				*ptkr(static_cast<Talker*>(this->objectAt(idx)));
	
	cassert(ptkr);
	
	if(ptkr->pushEvent(_rconid, _event, _flags)){
		//the talker must be signaled
		if(ptkr->signal(fdt::S_RAISE)){
			Manager::the().raiseObject(*ptkr);
		}
	}
}
//---------------------------------------------------------------------
int Service::sendSignal(
	DynamicPointer<Signal> &_psig,//the signal to be sent
	const SerializationTypeIdT &_rtid,
	const ConnectionUid &_rconid,//the id of the process connector
	uint32	_flags
){
	cassert(_rconid.tid < d.tkrvec.size());
	
	Locker<Mutex>		lock(serviceMutex());
	IndexT				idx(Manager::the().computeIndex(d.tkrvec[_rconid.tid].uid.first));
	Locker<Mutex>		lock2(this->mutex(idx));
	Talker				*ptkr(static_cast<Talker*>(this->objectAt(idx)));
	
	cassert(ptkr);
	
	if(ptkr->pushSignal(_psig, _rtid, _rconid, _flags | SameConnectorFlag)){
		//the talker must be signaled
		if(ptkr->signal(fdt::S_RAISE)){
			Manager::the().raiseObject(*ptkr);
		}
	}
	return OK;
}
//---------------------------------------------------------------------
int Service::basePort()const{
	return d.baseport;
}
//---------------------------------------------------------------------
int Service::doSendSignal(
	DynamicPointer<Signal> &_psig,//the signal to be sent
	const SerializationTypeIdT &_rtid,
	ConnectionUid *_pconid,
	const SocketAddressStub &_rsa_dest,
	const uint32 _netid_dest,
	uint32	_flags
){
	
	if(
		_rsa_dest.family() != SocketInfo::Inet4/* && 
		_rsap.family() != SocketAddressInfo::Inet6*/
	) return BAD;
	
	if(controller().isLocalNetwork(_rsa_dest, _netid_dest)){
		return doSendSignalLocal(_psig, _rtid, _pconid, _rsa_dest, _netid_dest, _flags);
	}else{
		return doSendSignalRelay(_psig, _rtid, _pconid, _rsa_dest, _netid_dest, _flags);
	}
}
//---------------------------------------------------------------------
int Service::doSendSignalLocal(
	DynamicPointer<Signal> &_psig,//the signal to be sent
	const SerializationTypeIdT &_rtid,
	ConnectionUid *_pconid,
	const SocketAddressStub &_rsap,
	const uint32 /*_netid_dest*/,
	uint32	_flags
){
	Locker<Mutex>	lock(serviceMutex());
	
	if(_rsap.family() == SocketInfo::Inet4){
		const SocketAddressInet4			sa(_rsap);
		const BaseAddress4T					baddr(sa, _rsap.port());
		
		Data::SessionAddr4MapT::iterator	it(d.sessionaddr4map.find(baddr));
		
		if(it != d.sessionaddr4map.end()){
		
			vdbgx(Dbg::ipc, "");
			
			ConnectionUid		conid(it->second);
			IndexT				idx(Manager::the().computeIndex(d.tkrvec[conid.tid].uid.first));
			Locker<Mutex>		lock2(this->mutex(idx));
			Talker				*ptkr(static_cast<Talker*>(this->objectAt(idx)));
			
			cassert(conid.tid < d.tkrvec.size());
			cassert(ptkr);
			
			if(ptkr->pushSignal(_psig, _rtid, conid, _flags)){
				//the talker must be signaled
				if(ptkr->signal(fdt::S_RAISE)){
					Manager::the().raiseObject(*ptkr);
				}
			}
			if(_pconid){
				*_pconid = conid;
			}
			return OK;
		
		}else{//the connection/session does not exist
			vdbgx(Dbg::ipc, "");
			
			int16	tkrid(allocateTalkerForSession());
			IndexT	tkrpos;
			uint32	tkruid;
			
			if(tkrid >= 0){
				//the talker exists
				tkrpos = d.tkrvec[tkrid].uid.first;
				tkruid = d.tkrvec[tkrid].uid.second;
			}else{
				//create new talker
				tkrid = createTalker(tkrpos, tkruid);
				if(tkrid < 0){
					tkrid = allocateTalkerForSession(true/*force*/);
				}
				tkrpos = d.tkrvec[tkrid].uid.first;
				tkruid = d.tkrvec[tkrid].uid.second;
			}
			
			tkrpos = Manager::the().computeIndex(tkrpos);
			Locker<Mutex>		lock2(this->mutex(tkrpos));
			Talker				*ptkr(static_cast<Talker*>(this->objectAt(tkrpos)));
			Session				*pses(new Session(sa));
			ConnectionUid		conid(tkrid);
			
			cassert(ptkr);
			
			vdbgx(Dbg::ipc, "");
			ptkr->pushSession(pses, conid);
			d.sessionaddr4map[pses->peerBaseAddress4()] = conid;
			
			ptkr->pushSignal(_psig, _rtid, conid, _flags);
			
			if(ptkr->signal(fdt::S_RAISE)){
				Manager::the().raiseObject(*ptkr);
			}
			
			if(_pconid){
				*_pconid = conid;
			}
			return OK;
		}
	}else{//inet6
		cassert(false);
		//TODO:
	}
	return OK;
}
//---------------------------------------------------------------------
int Service::doSendSignalRelay(
	DynamicPointer<Signal> &_psig,//the signal to be sent
	const SerializationTypeIdT &_rtid,
	ConnectionUid *_pconid,
	const SocketAddressStub &_rsap,
	const uint32 _netid_dest,
	uint32	_flags
){
	Locker<Mutex>	lock(serviceMutex());
	
	if(_rsap.family() == SocketInfo::Inet4){
		const SocketAddressInet4				sa(_rsap);
		const RelayAddress4T					addr(BaseAddress4T(sa, _rsap.port()), _netid_dest);
		
		Data::SessionRelayAddr4MapT::iterator	it(d.sessionrelayaddr4map.find(addr));
		
		if(it != d.sessionrelayaddr4map.end()){
		
			vdbgx(Dbg::ipc, "");
			
			ConnectionUid		conid(it->second);
			IndexT				idx(Manager::the().computeIndex(d.tkrvec[conid.tid].uid.first));
			Locker<Mutex>		lock2(this->mutex(idx));
			Talker				*ptkr(static_cast<Talker*>(this->objectAt(idx)));
			
			cassert(conid.tid < d.tkrvec.size());
			cassert(ptkr);
			
			if(ptkr->pushSignal(_psig, _rtid, conid, _flags)){
				//the talker must be signaled
				if(ptkr->signal(fdt::S_RAISE)){
					Manager::the().raiseObject(*ptkr);
				}
			}
			if(_pconid){
				*_pconid = conid;
			}
			return OK;
		
		}else{//the connection/session does not exist
			vdbgx(Dbg::ipc, "");
			
			int16	tkrid(allocateTalkerForSession());
			IndexT	tkrpos;
			uint32	tkruid;
			
			if(tkrid >= 0){
				//the talker exists
				tkrpos = d.tkrvec[tkrid].uid.first;
				tkruid = d.tkrvec[tkrid].uid.second;
			}else{
				//create new talker
				tkrid = createTalker(tkrpos, tkruid);
				if(tkrid < 0){
					tkrid = allocateTalkerForSession(true/*force*/);
				}
				tkrpos = d.tkrvec[tkrid].uid.first;
				tkruid = d.tkrvec[tkrid].uid.second;
			}
			
			tkrpos = Manager::the().computeIndex(tkrpos);
			
			Locker<Mutex>		lock2(this->mutex(tkrpos));
			Talker				*ptkr(static_cast<Talker*>(this->objectAt(tkrpos)));
			cassert(ptkr);
			Session				*pses(new Session(_netid_dest, sa));
			ConnectionUid		conid(tkrid);
			
			vdbgx(Dbg::ipc, "");
			ptkr->pushSession(pses, conid);
			d.sessionrelayaddr4map[pses->peerRelayAddress4()] = conid;
			
			ptkr->pushSignal(_psig, _rtid, conid, _flags);
			
			if(ptkr->signal(fdt::S_RAISE)){
				Manager::the().raiseObject(*ptkr);
			}
			
			if(_pconid){
				*_pconid = conid;
			}
			return OK;
		}
	}else{//inet6
		cassert(false);
		//TODO:
	}
	return OK;
}
//---------------------------------------------------------------------
int Service::createTalker(IndexT &_tkrpos, uint32 &_tkruid){
	
	if(d.tkrvec.size() >= d.tkrmaxcnt){
		vdbgx(Dbg::ipc, "maximum talker count reached "<<d.tkrvec.size());
		return BAD;
	}
	
	int16			tkrid(d.tkrvec.size());
	
	SocketDevice	sd;
	uint			oldport(d.firstaddr.port());
	
	d.firstaddr.port(0);//bind to any available port
	sd.create(d.firstaddr.family(), SocketInfo::Datagram, 0);
	sd.bind(d.firstaddr);

	if(sd.ok()){
		d.firstaddr.port(oldport);
		vdbgx(Dbg::ipc, "Successful created talker");
		Talker		*ptkr(new Talker(sd, *this, tkrid));
		ObjectUidT	objuid(this->insertLockless(ptkr));
		
		d.tkrq.push(d.tkrvec.size());
		d.tkrvec.push_back(objuid);
		controller().scheduleTalker(ptkr);
		return tkrid;
	}else{
		edbgx(Dbg::ipc, "Could not bind to random port");
	}
	d.firstaddr.port(oldport);
	return BAD;
}
//---------------------------------------------------------------------
int Service::createNode(IndexT &_nodepos, uint32 &_nodeuid){
	
	if(d.nodevec.size() >= d.nodemaxcnt){
		vdbgx(Dbg::ipc, "maximum node count reached "<<d.nodevec.size());
		return BAD;
	}
	
	int16			nodeid(d.nodevec.size());
	
	SocketDevice	sd;
	uint			oldport(d.firstaddr.port());
	
	d.firstaddr.port(0);//bind to any available port
	sd.create(d.firstaddr.family(), SocketInfo::Datagram, 0);
	sd.bind(d.firstaddr);

	if(sd.ok()){
		d.firstaddr.port(oldport);
		vdbgx(Dbg::ipc, "Successful created node");
		Node		*pnode(new Node(sd, *this, nodeid));
		ObjectUidT	objuid(this->insertLockless(pnode));
		
		d.sessnodeq.push(d.nodevec.size());
		d.socknodeq.push(d.nodevec.size());
		d.nodevec.push_back(objuid);
		controller().scheduleNode(pnode);
		return nodeid;
	}else{
		edbgx(Dbg::ipc, "Could not bind to random port");
	}
	d.firstaddr.port(oldport);
	return BAD;
}
//---------------------------------------------------------------------
int Service::allocateTalkerForSession(bool _force){
	if(!_force){
		if(d.tkrq.size()){
			int 				rv(d.tkrq.front());
			Data::TalkerStub	&rts(d.tkrvec[rv]);
			++rts.cnt;
			if(rts.cnt == d.tkrsescnt){
				d.tkrq.pop();
			}
			vdbgx(Dbg::ipc, "non forced allocate talker: "<<rv<<" sessions per talker "<<rts.cnt);
			return rv;
		}
		vdbgx(Dbg::ipc, "non forced allocate talker failed");
		return -1;
	}else{
		int					rv(d.tkrcrt);
		Data::TalkerStub	&rts(d.tkrvec[rv]);
		++rts.cnt;
		cassert(d.tkrq.empty());
		++d.tkrcrt;
		d.tkrcrt %= d.tkrmaxcnt;
		vdbgx(Dbg::ipc, "forced allocate talker: "<<rv<<" sessions per talker "<<rts.cnt);
		return rv;
	}
}
//---------------------------------------------------------------------
int Service::allocateNodeForSession(bool _force){
	if(!_force){
		if(d.sessnodeq.size()){
			const int 		rv(d.sessnodeq.front());
			Data::NodeStub	&rns(d.nodevec[rv]);
			++rns.sesscnt;
			if(rns.sesscnt == d.nodesescnt){
				d.sessnodeq.pop();
			}
			vdbgx(Dbg::ipc, "non forced allocate node: "<<rv<<" sessions per node "<<rns.sesscnt);
			return rv;
		}
		vdbgx(Dbg::ipc, "non forced allocate node failed");
		return -1;
	}else{
		const int		rv(d.nodecrt);
		Data::NodeStub	&rns(d.nodevec[rv]);
		++rns.sesscnt;
		cassert(d.sessnodeq.empty());
		++d.nodecrt;
		d.nodecrt %= d.nodemaxcnt;
		vdbgx(Dbg::ipc, "forced allocate node: "<<rv<<" sessions per node "<<rns.sesscnt);
		return rv;
	}
}
//---------------------------------------------------------------------
int Service::allocateNodeForSocket(bool _force){
	if(!_force){
		if(d.socknodeq.size()){
			const int 		rv(d.socknodeq.front());
			Data::NodeStub	&rns(d.nodevec[rv]);
			++rns.sockcnt;
			if(rns.sockcnt == d.nodesockcnt){
				d.socknodeq.pop();
			}
			vdbgx(Dbg::ipc, "non forced allocate node: "<<rv<<" sockets per node "<<rns.sockcnt);
			return rv;
		}
		vdbgx(Dbg::ipc, "non forced allocate node failed");
		return -1;
	}else{
		const int		rv(d.nodecrt);
		Data::NodeStub	&rns(d.nodevec[rv]);
		++rns.sockcnt;
		cassert(d.socknodeq.empty());
		++d.nodecrt;
		d.nodecrt %= d.nodemaxcnt;
		vdbgx(Dbg::ipc, "forced allocate node: "<<rv<<" socket per node "<<rns.sockcnt);
		return rv;
	}
}
//---------------------------------------------------------------------
int Service::acceptSession(const SocketAddress &_rsa, const ConnectData &_rconndata){
	if(_rconndata.type == ConnectData::BasicType){
		return doAcceptBasicSession(_rsa, _rconndata);
	}else if(controller().isLocalNetwork(_rconndata.receiveraddress, _rconndata.receivernetworkid)){
		return doAcceptRelaySession(_rsa, _rconndata);
	}else if(controller().isGateway()){
		return doAcceptGatewaySession(_rsa, _rconndata);
	}else{
		return NoGatewayError;
	}
}
//---------------------------------------------------------------------
int Service::doAcceptBasicSession(const SocketAddress &_rsa, const ConnectData &_rconndata){
	Locker<Mutex>		lock(serviceMutex());
	SocketAddressInet4	inaddr(_rsa);
	Session				*pses = new Session(
		inaddr,
		_rconndata
	);
	
	{
		//TODO: see if the locking is ok!!!
		
		Data::SessionAddr4MapT::iterator	it(d.sessionaddr4map.find(pses->peerBaseAddress4()));
		
		if(it != d.sessionaddr4map.end()){
			//a connection still exists
			IndexT			tkrpos(Manager::the().computeIndex(d.tkrvec[it->second.tid].uid.first));
			Locker<Mutex>	lock2(this->mutex(tkrpos));
			Talker			*ptkr(static_cast<Talker*>(this->objectAt(tkrpos)));
			
			vdbgx(Dbg::ipc, "");
			
			ptkr->pushSession(pses, it->second, true);
			
			if(ptkr->signal(fdt::S_RAISE)){
				Manager::the().raiseObject(*ptkr);
			}
			return OK;
		}
	}
	
	int		tkrid(allocateTalkerForSession());
	IndexT	tkrpos;
	uint32	tkruid;
	
	if(tkrid >= 0){
		//the talker exists
		tkrpos = d.tkrvec[tkrid].uid.first;
		tkruid = d.tkrvec[tkrid].uid.second;
	}else{
		//create new talker
		tkrid = createTalker(tkrpos, tkruid);
		if(tkrid < 0){
			tkrid = allocateTalkerForSession(true/*force*/);
		}
		tkrpos = d.tkrvec[tkrid].uid.first;
		tkruid = d.tkrvec[tkrid].uid.second;
	}
	
	tkrpos = Manager::the().computeIndex(tkrpos);
	
	Locker<Mutex>	lock2(this->mutex(tkrpos));
	Talker			*ptkr(static_cast<Talker*>(this->objectAt(tkrpos)));
	ConnectionUid	conid(tkrid, 0xffff, 0xffff);
	
	cassert(ptkr);
	vdbgx(Dbg::ipc, "");
	
	ptkr->pushSession(pses, conid);
	d.sessionaddr4map[pses->peerBaseAddress4()] = conid;
	
	if(ptkr->signal(fdt::S_RAISE)){
		Manager::the().raiseObject(*ptkr);
	}
	return OK;
}
//---------------------------------------------------------------------
//accept session on destination, receiver
int Service::doAcceptRelaySession(const SocketAddress &_rsa, const ConnectData &_rconndata){
	Locker<Mutex>				lock(serviceMutex());
	SocketAddressInet4			inaddr(_rsa);
	Session						*pses = new Session(
		_rconndata.sendernetworkid,
		inaddr,
		_rconndata
	);
	{
		//TODO: see if the locking is ok!!!
		
		Data::SessionRelayAddr4MapT::iterator	it(d.sessionrelayaddr4map.find(pses->peerRelayAddress4()));
		
		if(it != d.sessionrelayaddr4map.end()){
			//a connection still exists
			IndexT			tkrpos(Manager::the().computeIndex(d.tkrvec[it->second.tid].uid.first));
			Locker<Mutex>	lock2(this->mutex(tkrpos));
			Talker			*ptkr(static_cast<Talker*>(this->objectAt(tkrpos)));
			
			vdbgx(Dbg::ipc, "");
			
			ptkr->pushSession(pses, it->second, true);
			
			if(ptkr->signal(fdt::S_RAISE)){
				Manager::the().raiseObject(*ptkr);
			}
			return OK;
		}
	}
	int		tkrid(allocateTalkerForSession());
	IndexT	tkrpos;
	uint32	tkruid;
	
	if(tkrid >= 0){
		//the talker exists
		tkrpos = d.tkrvec[tkrid].uid.first;
		tkruid = d.tkrvec[tkrid].uid.second;
	}else{
		//create new talker
		tkrid = createTalker(tkrpos, tkruid);
		if(tkrid < 0){
			tkrid = allocateTalkerForSession(true/*force*/);
		}
		tkrpos = d.tkrvec[tkrid].uid.first;
		tkruid = d.tkrvec[tkrid].uid.second;
	}
	
	tkrpos = Manager::the().computeIndex(tkrpos);
	
	Locker<Mutex>	lock2(this->mutex(tkrpos));
	Talker			*ptkr(static_cast<Talker*>(this->objectAt(tkrpos)));
	ConnectionUid	conid(tkrid, 0xffff, 0xffff);
	
	cassert(ptkr);
	
	vdbgx(Dbg::ipc, "");
	
	ptkr->pushSession(pses, conid);
	d.sessionrelayaddr4map[pses->peerRelayAddress4()] = conid;
	
	if(ptkr->signal(fdt::S_RAISE)){
		Manager::the().raiseObject(*ptkr);
	}
	return OK;
}
//---------------------------------------------------------------------
int Service::doAcceptGatewaySession(const SocketAddress &_rsa, const ConnectData &_rconndata){
	
	return BAD;
}
//---------------------------------------------------------------------
void Service::connectSession(const SocketAddressInet4 &_raddr){
	Locker<Mutex>	lock(serviceMutex());
	int				tkrid(allocateTalkerForSession());
	IndexT			tkrpos;
	uint32			tkruid;
	
	if(tkrid >= 0){
		//the talker exists
		tkrpos = d.tkrvec[tkrid].uid.first;
		tkruid = d.tkrvec[tkrid].uid.second;
	}else{
		//create new talker
		tkrid = createTalker(tkrpos, tkruid);
		if(tkrid < 0){
			tkrid = allocateTalkerForSession(true/*force*/);
		}
		tkrpos = d.tkrvec[tkrid].uid.first;
		tkruid = d.tkrvec[tkrid].uid.second;
	}
	tkrpos = Manager::the().computeIndex(tkrpos);
	
	Locker<Mutex>	lock2(this->mutex(tkrpos));
	Talker			*ptkr(static_cast<Talker*>(this->objectAt(tkrpos)));
	Session			*pses(new Session(_raddr));
	ConnectionUid	conid(tkrid);
	
	cassert(ptkr);
	
	vdbgx(Dbg::ipc, "");
	ptkr->pushSession(pses, conid);
	d.sessionaddr4map[pses->peerBaseAddress4()] = conid;
	
	if(ptkr->signal(fdt::S_RAISE)){
		Manager::the().raiseObject(*ptkr);
	}
}
//---------------------------------------------------------------------
void Service::disconnectTalkerSessions(Talker &_rtkr){
	Locker<Mutex>	lock(serviceMutex());
	_rtkr.disconnectSessions();
}
//---------------------------------------------------------------------
void Service::disconnectSession(Session *_pses){
	if(_pses->isRelayType()){
		d.sessionrelayaddr4map.erase(_pses->peerRelayAddress4());
	}else{
		d.sessionaddr4map.erase(_pses->peerBaseAddress4());
	}
	//Use:Context::the().sigctx.connectionuid.tid
	int					tkrid(Context::the().sigctx.connectionuid.tid);
	Data::TalkerStub	&rts(d.tkrvec[tkrid]);
	
	--rts.cnt;
	
	vdbgx(Dbg::ipc, "disconnected session for talker "<<tkrid<<" session count per talker = "<<rts.cnt);
	
	if(rts.cnt < d.tkrsescnt){
		d.tkrq.push(tkrid);
	}
}
//---------------------------------------------------------------------
int Service::insertConnection(
	const SocketDevice &_rsd,
	const Types _type,
	foundation::aio::openssl::Context */*_pctx*/,
	bool _secure
){
	if(_type == PlainType){
		THROW_EXCEPTION("Not implemented yet!!!");
	}else if(_type == RelayType){
		
	}else{
		THROW_EXCEPTION("Not implemented yet!!!");
	}
/*	Connection *pcon = new Connection(_pch, 0);
	if(this->insert(*pcon, _serviceid)){
		delete pcon;
		return BAD;
	}
	_rm.pushJob((fdt::tcp::Connection*)pcon);*/
	return OK;
}
//---------------------------------------------------------------------
int Service::insertListener(
	const ResolveIterator &_rai,
	const Types _type,
	bool _secure
){
	SocketDevice sd;
	sd.create(_rai);
	sd.makeNonBlocking();
	sd.prepareAccept(_rai, 100);
	if(!sd.ok()){
		return BAD;
	}
	
	foundation::aio::openssl::Context *pctx = NULL;
	if(_secure){
		pctx = foundation::aio::openssl::Context::create();
	}
	if(pctx){
		const char *pcertpath = NULL;//certificate_path();
		
		pctx->loadCertificateFile(pcertpath);
		pctx->loadPrivateKeyFile(pcertpath);
	}
	
	Listener 	*plis = new Listener(sd, _type, pctx);
	ObjectUidT	objuid(this->insert(plis));
	
	controller().scheduleTalker(plis);
	return OK;
}
//---------------------------------------------------------------------
int Service::insertTalker(
	const ResolveIterator &_rai
){	
	SocketDevice	sd;
	sd.create(_rai);
	sd.bind(_rai);
	
	if(!sd.ok()) return BAD;
	SocketAddress sa;
	if(sd.localAddress(sa) != OK){
		return BAD;
	}
	//Locker<Mutex>	lock(serviceMutex());
	cassert(!d.tkrvec.size());//only the first tkr must be inserted from outside
	Talker			*ptkr(new Talker(sd, *this, 0));
	
	ObjectUidT		objuid(this->insert(ptkr));
	
	Locker<Mutex>	lock(serviceMutex());
	
	d.firstaddr = _rai;
	d.baseport = sa.port();
	d.tkrvec.push_back(Data::TalkerStub(objuid));
	d.tkrq.push(0);
	controller().scheduleTalker(ptkr);
	return OK;
}
//---------------------------------------------------------------------
int Service::insertConnection(
	const ResolveIterator &_rai
){
	
/*	Connection *pcon = new Connection(_pch, _node, _svc);
	if(this->insert(*pcon, _serviceid)){
		delete pcon;
		return BAD;
	}
	_rm.pushJob((fdt::tcp::Connection*)pcon);*/
	return OK;
}
//---------------------------------------------------------------------
void Service::insertObject(Talker &_ro, const ObjectUidT &_ruid){
	vdbgx(Dbg::ipc, "inserting talker");
}
//---------------------------------------------------------------------
void Service::eraseObject(const Talker &_ro){
	vdbgx(Dbg::ipc, "erasing talker");
}
bool Service::checkAcceptData(const SocketAddress &/*_rsa*/, const AcceptData &_raccdata){
	return _raccdata.timestamp_s == timeStamp().seconds() && _raccdata.timestamp_n == timeStamp().nanoSeconds();
}

//---------------------------------------------------------------------
//			Controller
//---------------------------------------------------------------------

Controller& Service::controller(){
	return *d.ctrlptr;
}
//---------------------------------------------------------------------
const Controller& Service::controller()const{
	return *d.ctrlptr;
}
//------------------------------------------------------------------
//		Controller
//------------------------------------------------------------------
/*virtual*/ Controller::~Controller(){
	
}
/*virtual*/ bool Controller::compressBuffer(
	BufferContext &_rbc,
	const uint32 _bufsz,
	char* &_rpb,
	uint32 &_bl
){
	return false;
}
/*virtual*/ bool Controller::decompressBuffer(
	BufferContext &_rbc,
	char* &_rpb,
	uint32 &_bl
){
	return true;
}
char * Controller::allocateBuffer(BufferContext &_rbc, uint32 &_cp){
	const uint	mid(Specific::capacityToIndex(_cp ? _cp : Buffer::Capacity));
	char		*newbuf(Buffer::allocate());
	_cp = Buffer::Capacity - _rbc.offset;
	if(_rbc.reqbufid != (uint)-1){
		THROW_EXCEPTION("Requesting more than one buffer");
		return NULL;
	}
	_rbc.reqbufid = mid;
	return newbuf + _rbc.offset;
}
bool Controller::receive(
	Signal *_psig,
	ipc::SignalUid &_rsiguid
){
	_psig->ipcReceive(_rsiguid);
	return true;
}

int Controller::authenticate(
	DynamicPointer<Signal> &_sigptr,
	ipc::SignalUid &_rsiguid,
	uint32 &_rflags,
	SerializationTypeIdT &_rtid
){
	//use: ConnectionContext::the().connectionuid!!
	return BAD;//by default no authentication
}

/*virtual*/ uint32 Controller::localNetworkId()const{
	return LocalNetworkId;
}
bool Controller::isLocalNetwork(
	const SocketAddressStub &,
	const uint32 _netid_dest
)const{
	return _netid_dest == LocalNetworkId || _netid_dest == localNetworkId();
}

void Controller::sendEvent(
	Service &_rs,
	const ConnectionUid &_rconid,//the id of the process connectors
	int32 _event,
	uint32 _flags
){
	_rs.doSendEvent(_rconid, _event, _flags);
}


/*virtual*/ SocketAddressStub Controller::gatewayAddress(
	const uint _idx,
	const uint32 _netid_dest,
	const SocketAddressStub &_rsas_dest
){
	return SocketAddressStub();
}

//retval:
// -1 : wait for asynchrounous event and retry
// 0: no gateway
// > 0: the count
/*virtual*/ int Controller::gatewayCount(
	const uint32 _netid_dest,
	const SocketAddressStub &_rsas_dest
)const{
	return 0;
}

//called on the gateway to find out where to connect for relaying data to _rsas_dest
/*virtual*/ const SocketAddress& Controller::relayAddress(
	const uint32 _netid_dest,
	const SocketAddressStub &_rsas_dest
){
	const SocketAddress *psa = NULL;
	return *psa;
}

/*virtual*/ uint32 Controller::relayCount(
	const uint32 _netid_dest,
	const SocketAddressStub &_rsas_dest
)const{
	return 0;
}

}//namespace ipc
}//namespace foundation

