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

namespace solid{
namespace frame{
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
		const DynamicPointer<Controller> &_rctrlptr
	);
	
	~Data();
	
	uint32 sessionCount()const{
		return sessionaddr4map.size();
	}
	
	DynamicPointer<Controller>	ctrlptr;
	Configuration				config;
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
	const DynamicPointer<Controller> &_rctrlptr
):
	ctrlptr(_rctrlptr),
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

Service::Service(
	Manager &_rm,
	const DynamicPointer<Controller> &_rctrlptr
):BaseT(_rm), d(*(new Data(_rctrlptr))){
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
int Service::reconfigure(const Configuration &_rcfg){
	Locker<Mutex>	lock(mutex());
	
	if(_rcfg == d.config){
		return OK;
	}
	
	unsafeReset(lock);
	
	d.config = _rcfg;
	
	if(
		d.config.baseaddr.isInet4()
	){
		SocketDevice	sd;
		sd.create(SocketInfo::Inet4, SocketInfo::Datagram, 0);
		sd.bind(d.config.baseaddr);
		
		if(!sd.ok()){
			return BAD;
		}
		
		SocketAddress sa;
		
		if(sd.localAddress(sa) != OK){
			return BAD;
		}
		//Locker<Mutex>	lock(serviceMutex());
		cassert(!d.tkrvec.size());//only the first tkr must be inserted from outside
		Talker			*ptkr(new Talker(sd, *this, 0));
		
		ObjectUidT		objuid(this->unsafeRegisterObject(*ptkr));
		
		d.firstaddr = d.config.baseaddr;
		d.baseport = sa.port();
		d.tkrvec.push_back(Data::TalkerStub(objuid));
		d.tkrq.push(0);
		controller().scheduleTalker(ptkr);
		return OK;
	}else{
		return BAD;
	}
}
//---------------------------------------------------------------------
void Service::sendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const ConnectionUid &_rconid,//the id of the process connector
	uint32	_flags
){
	cassert(_rconid.tid < d.tkrvec.size());
	
	Locker<Mutex>		lock(mutex());
	const IndexT		fullid(d.tkrvec[_rconid.tid].uid.first);
	Locker<Mutex>		lock2(this->mutex(fullid));
	Talker				*ptkr(static_cast<Talker*>(this->object(fullid)));
	
	cassert(ptkr);
	
	if(ptkr->pushMessage(_rmsgptr, SERIALIZATION_INVALIDID, _rconid, _flags | SameConnectorFlag)){
		//the talker must be notified
		if(ptkr->notify(frame::S_RAISE)){
			manager().raise(*ptkr);
		}
	}
}
//---------------------------------------------------------------------
void Service::doSendEvent(
	const ConnectionUid &_rconid,//the id of the process connectors
	int32 _event,
	uint32 _flags
){
	Locker<Mutex>		lock(mutex());
	const IndexT		fullid(d.tkrvec[_rconid.tid].uid.first);
	Locker<Mutex>		lock2(this->mutex(fullid));
	Talker				*ptkr(static_cast<Talker*>(this->object(fullid)));
	
	cassert(ptkr);
	
	if(ptkr->pushEvent(_rconid, _event, _flags)){
		//the talker must be notified
		if(ptkr->notify(frame::S_RAISE)){
			manager().raise(*ptkr);
		}
	}
}
//---------------------------------------------------------------------
void Service::sendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SerializationTypeIdT &_rtid,
	const ConnectionUid &_rconid,//the id of the process connector
	uint32	_flags
){
	cassert(_rconid.tid < d.tkrvec.size());
	
	Locker<Mutex>		lock(mutex());
	const IndexT		fullid(d.tkrvec[_rconid.tid].uid.first);
	Locker<Mutex>		lock2(this->mutex(fullid));
	Talker				*ptkr(static_cast<Talker*>(this->object(fullid)));
	
	cassert(ptkr);
	
	if(ptkr->pushMessage(_rmsgptr, _rtid, _rconid, _flags | SameConnectorFlag)){
		//the talker must be notified
		if(ptkr->notify(frame::S_RAISE)){
			manager().raise(*ptkr);
		}
	}
}
//---------------------------------------------------------------------
int Service::basePort()const{
	return d.baseport;
}
//---------------------------------------------------------------------
void Service::doSendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SerializationTypeIdT &_rtid,
	ConnectionUid *_pconid,
	const SocketAddressStub &_rsa_dest,
	const uint32 _netid_dest,
	uint32	_flags
){
	
	if(
		_rsa_dest.family() != SocketInfo::Inet4/* && 
		_rsap.family() != SocketAddressInfo::Inet6*/
	)return;
	
	if(controller().isLocalNetwork(_rsa_dest, _netid_dest)){
		doSendMessageLocal(_rmsgptr, _rtid, _pconid, _rsa_dest, _netid_dest, _flags);
	}else{
		doSendMessageRelay(_rmsgptr, _rtid, _pconid, _rsa_dest, _netid_dest, _flags);
	}
}
//---------------------------------------------------------------------
void Service::doSendMessageLocal(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SerializationTypeIdT &_rtid,
	ConnectionUid *_pconid,
	const SocketAddressStub &_rsap,
	const uint32 /*_netid_dest*/,
	uint32	_flags
){
	Locker<Mutex>	lock(mutex());
	
	if(_rsap.family() == SocketInfo::Inet4){
		const SocketAddressInet4			sa(_rsap);
		const BaseAddress4T					baddr(sa, _rsap.port());
		
		Data::SessionAddr4MapT::iterator	it(d.sessionaddr4map.find(baddr));
		
		if(it != d.sessionaddr4map.end()){
		
			vdbgx(Debug::ipc, "");
			
			ConnectionUid		conid(it->second);
			const IndexT		fullid(d.tkrvec[conid.tid].uid.first);
			Locker<Mutex>		lock2(this->mutex(fullid));
			Talker				*ptkr(static_cast<Talker*>(this->object(fullid)));
			
			cassert(conid.tid < d.tkrvec.size());
			cassert(ptkr);
			
			if(ptkr->pushMessage(_rmsgptr, _rtid, conid, _flags)){
				//the talker must be notified
				if(ptkr->notify(frame::S_RAISE)){
					manager().raise(*ptkr);
				}
			}
			if(_pconid){
				*_pconid = conid;
			}
		}else{//the connection/session does not exist
			vdbgx(Debug::ipc, "");
			
			int16	tkridx(allocateTalkerForSession());
			IndexT	tkrfullid;
			uint32	tkruid;
			
			if(tkridx >= 0){
				//the talker exists
				tkrfullid = d.tkrvec[tkridx].uid.first;
				tkruid = d.tkrvec[tkridx].uid.second;
			}else{
				//create new talker
				tkridx = createTalker(tkrfullid, tkruid);
				if(tkridx < 0){
					tkridx = allocateTalkerForSession(true/*force*/);
				}
				tkrfullid = d.tkrvec[tkridx].uid.first;
				tkruid = d.tkrvec[tkridx].uid.second;
			}
			
			Locker<Mutex>		lock2(this->mutex(tkrfullid));
			Talker				*ptkr(static_cast<Talker*>(this->object(tkrfullid)));
			Session				*pses(new Session(*this, sa));
			ConnectionUid		conid(tkridx);
			
			cassert(ptkr);
			
			vdbgx(Debug::ipc, "");
			ptkr->pushSession(pses, conid);
			d.sessionaddr4map[pses->peerBaseAddress4()] = conid;
			
			ptkr->pushMessage(_rmsgptr, _rtid, conid, _flags);
			
			if(ptkr->notify(frame::S_RAISE)){
				manager().raise(*ptkr);
			}
			
			if(_pconid){
				*_pconid = conid;
			}
		}
	}else{//inet6
		cassert(false);
		//TODO:
	}
}
//---------------------------------------------------------------------
void Service::doSendMessageRelay(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SerializationTypeIdT &_rtid,
	ConnectionUid *_pconid,
	const SocketAddressStub &_rsap,
	const uint32 _netid_dest,
	uint32	_flags
){
	Locker<Mutex>	lock(mutex());
	
	if(_rsap.family() == SocketInfo::Inet4){
		const SocketAddressInet4				sa(_rsap);
		const RelayAddress4T					addr(BaseAddress4T(sa, _rsap.port()), _netid_dest);
		
		Data::SessionRelayAddr4MapT::iterator	it(d.sessionrelayaddr4map.find(addr));
		
		if(it != d.sessionrelayaddr4map.end()){
		
			vdbgx(Debug::ipc, "");
			
			ConnectionUid		conid(it->second);
			const IndexT		fullid(d.tkrvec[conid.tid].uid.first);
			Locker<Mutex>		lock2(this->mutex(fullid));
			Talker				*ptkr(static_cast<Talker*>(this->object(fullid)));
			
			cassert(conid.tid < d.tkrvec.size());
			cassert(ptkr);
			
			if(ptkr->pushMessage(_rmsgptr, _rtid, conid, _flags)){
				//the talker must be notified
				if(ptkr->notify(frame::S_RAISE)){
					manager().raise(*ptkr);
				}
			}
			if(_pconid){
				*_pconid = conid;
			}
		}else{//the connection/session does not exist
			vdbgx(Debug::ipc, "");
			
			int16	tkridx(allocateTalkerForSession());
			IndexT	tkrfullid;
			uint32	tkruid;
			
			if(tkridx >= 0){
				//the talker exists
				tkrfullid = d.tkrvec[tkridx].uid.first;
				tkruid = d.tkrvec[tkridx].uid.second;
			}else{
				//create new talker
				tkridx = createTalker(tkrfullid, tkruid);
				if(tkridx < 0){
					tkridx = allocateTalkerForSession(true/*force*/);
				}
				tkrfullid = d.tkrvec[tkridx].uid.first;
				tkruid = d.tkrvec[tkridx].uid.second;
			}
			
			Locker<Mutex>		lock2(this->mutex(tkrfullid));
			Talker				*ptkr(static_cast<Talker*>(this->object(tkrfullid)));
			cassert(ptkr);
			Session				*pses(new Session(*this, _netid_dest, sa));
			ConnectionUid		conid(tkridx);
			
			vdbgx(Debug::ipc, "");
			ptkr->pushSession(pses, conid);
			d.sessionrelayaddr4map[pses->peerRelayAddress4()] = conid;
			
			ptkr->pushMessage(_rmsgptr, _rtid, conid, _flags);
			
			if(ptkr->notify(frame::S_RAISE)){
				manager().raise(*ptkr);
			}
			
			if(_pconid){
				*_pconid = conid;
			}
		}
	}else{//inet6
		cassert(false);
		//TODO:
	}
}
//---------------------------------------------------------------------
int Service::createTalker(IndexT &_tkrfullid, uint32 &_tkruid){
	
	if(d.tkrvec.size() >= d.config.talker.maxcnt){
		vdbgx(Debug::ipc, "maximum talker count reached "<<d.tkrvec.size());
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
		vdbgx(Debug::ipc, "Successful created talker");
		Talker		*ptkr(new Talker(sd, *this, tkrid));
		ObjectUidT	objuid(this->unsafeRegisterObject(*ptkr));
		
		d.tkrq.push(d.tkrvec.size());
		d.tkrvec.push_back(objuid);
		controller().scheduleTalker(ptkr);
		return tkrid;
	}else{
		edbgx(Debug::ipc, "Could not bind to random port");
	}
	d.firstaddr.port(oldport);
	return BAD;
}
//---------------------------------------------------------------------
int Service::createNode(IndexT &_nodepos, uint32 &_nodeuid){
	
	if(d.nodevec.size() >= d.config.node.maxcnt){
		vdbgx(Debug::ipc, "maximum node count reached "<<d.nodevec.size());
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
		vdbgx(Debug::ipc, "Successful created node");
		Node		*pnode(new Node(sd, *this, nodeid));
		ObjectUidT	objuid(this->unsafeRegisterObject(*pnode));
		
		d.sessnodeq.push(d.nodevec.size());
		d.socknodeq.push(d.nodevec.size());
		d.nodevec.push_back(objuid);
		controller().scheduleNode(pnode);
		return nodeid;
	}else{
		edbgx(Debug::ipc, "Could not bind to random port");
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
			if(rts.cnt == d.config.talker.sescnt){
				d.tkrq.pop();
			}
			vdbgx(Debug::ipc, "non forced allocate talker: "<<rv<<" sessions per talker "<<rts.cnt);
			return rv;
		}
		vdbgx(Debug::ipc, "non forced allocate talker failed");
		return -1;
	}else{
		int					rv(d.tkrcrt);
		Data::TalkerStub	&rts(d.tkrvec[rv]);
		++rts.cnt;
		cassert(d.tkrq.empty());
		++d.tkrcrt;
		d.tkrcrt %= d.config.talker.maxcnt;
		vdbgx(Debug::ipc, "forced allocate talker: "<<rv<<" sessions per talker "<<rts.cnt);
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
			if(rns.sesscnt == d.config.node.sescnt){
				d.sessnodeq.pop();
			}
			vdbgx(Debug::ipc, "non forced allocate node: "<<rv<<" sessions per node "<<rns.sesscnt);
			return rv;
		}
		vdbgx(Debug::ipc, "non forced allocate node failed");
		return -1;
	}else{
		const int		rv(d.nodecrt);
		Data::NodeStub	&rns(d.nodevec[rv]);
		++rns.sesscnt;
		cassert(d.sessnodeq.empty());
		++d.nodecrt;
		d.nodecrt %= d.config.node.maxcnt;
		vdbgx(Debug::ipc, "forced allocate node: "<<rv<<" sessions per node "<<rns.sesscnt);
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
			if(rns.sockcnt == d.config.node.sockcnt){
				d.socknodeq.pop();
			}
			vdbgx(Debug::ipc, "non forced allocate node: "<<rv<<" sockets per node "<<rns.sockcnt);
			return rv;
		}
		vdbgx(Debug::ipc, "non forced allocate node failed");
		return -1;
	}else{
		const int		rv(d.nodecrt);
		Data::NodeStub	&rns(d.nodevec[rv]);
		++rns.sockcnt;
		cassert(d.socknodeq.empty());
		++d.nodecrt;
		d.nodecrt %= d.config.node.maxcnt;
		vdbgx(Debug::ipc, "forced allocate node: "<<rv<<" socket per node "<<rns.sockcnt);
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
	Locker<Mutex>		lock(mutex());
	SocketAddressInet4	inaddr(_rsa);
	Session				*pses = new Session(
		*this,
		inaddr,
		_rconndata
	);
	
	{
		//TODO: see if the locking is ok!!!
		
		Data::SessionAddr4MapT::iterator	it(d.sessionaddr4map.find(pses->peerBaseAddress4()));
		
		if(it != d.sessionaddr4map.end()){
			//a connection still exists
			const IndexT	tkrfullid(d.tkrvec[it->second.tid].uid.first);
			Locker<Mutex>	lock2(this->mutex(tkrfullid));
			Talker			*ptkr(static_cast<Talker*>(this->object(tkrfullid)));
			
			vdbgx(Debug::ipc, "");
			
			ptkr->pushSession(pses, it->second, true);
			
			if(ptkr->notify(frame::S_RAISE)){
				manager().raise(*ptkr);
			}
			return OK;
		}
	}
	
	int		tkridx(allocateTalkerForSession());
	IndexT	tkrfullid;
	uint32	tkruid;
	
	if(tkridx >= 0){
		//the talker exists
		tkrfullid = d.tkrvec[tkridx].uid.first;
		tkruid = d.tkrvec[tkridx].uid.second;
	}else{
		//create new talker
		tkridx = createTalker(tkrfullid, tkruid);
		if(tkridx < 0){
			tkridx = allocateTalkerForSession(true/*force*/);
		}
		tkrfullid = d.tkrvec[tkridx].uid.first;
		tkruid = d.tkrvec[tkridx].uid.second;
	}
	
	Locker<Mutex>	lock2(this->mutex(tkrfullid));
	Talker			*ptkr(static_cast<Talker*>(this->object(tkrfullid)));
	ConnectionUid	conid(tkridx, 0xffff, 0xffff);
	
	cassert(ptkr);
	vdbgx(Debug::ipc, "");
	
	ptkr->pushSession(pses, conid);
	d.sessionaddr4map[pses->peerBaseAddress4()] = conid;
	
	if(ptkr->notify(frame::S_RAISE)){
		manager().raise(*ptkr);
	}
	return OK;
}
//---------------------------------------------------------------------
//accept session on destination, receiver
int Service::doAcceptRelaySession(const SocketAddress &_rsa, const ConnectData &_rconndata){
	Locker<Mutex>				lock(mutex());
	SocketAddressInet4			inaddr(_rsa);
	Session						*pses = new Session(
		*this,
		_rconndata.sendernetworkid,
		inaddr,
		_rconndata
	);
	{
		//TODO: see if the locking is ok!!!
		
		Data::SessionRelayAddr4MapT::iterator	it(d.sessionrelayaddr4map.find(pses->peerRelayAddress4()));
		
		if(it != d.sessionrelayaddr4map.end()){
			//a connection still exists
			const IndexT	tkrfullid(d.tkrvec[it->second.tid].uid.first);
			Locker<Mutex>	lock2(this->mutex(tkrfullid));
			Talker			*ptkr(static_cast<Talker*>(this->object(tkrfullid)));
			
			vdbgx(Debug::ipc, "");
			
			ptkr->pushSession(pses, it->second, true);
			
			if(ptkr->notify(frame::S_RAISE)){
				manager().raise(*ptkr);
			}
			return OK;
		}
	}
	int		tkridx(allocateTalkerForSession());
	IndexT	tkrfullid;
	uint32	tkruid;
	
	if(tkridx >= 0){
		//the talker exists
		tkrfullid = d.tkrvec[tkridx].uid.first;
		tkruid = d.tkrvec[tkridx].uid.second;
	}else{
		//create new talker
		tkridx = createTalker(tkrfullid, tkruid);
		if(tkridx < 0){
			tkridx = allocateTalkerForSession(true/*force*/);
		}
		tkrfullid = d.tkrvec[tkridx].uid.first;
		tkruid = d.tkrvec[tkridx].uid.second;
	}
	
	Locker<Mutex>	lock2(this->mutex(tkrfullid));
	Talker			*ptkr(static_cast<Talker*>(this->object(tkrfullid)));
	ConnectionUid	conid(tkridx, 0xffff, 0xffff);
	
	cassert(ptkr);
	
	vdbgx(Debug::ipc, "");
	
	ptkr->pushSession(pses, conid);
	d.sessionrelayaddr4map[pses->peerRelayAddress4()] = conid;
	
	if(ptkr->notify(frame::S_RAISE)){
		manager().raise(*ptkr);
	}
	return OK;
}
//---------------------------------------------------------------------
int Service::doAcceptGatewaySession(const SocketAddress &_rsa, const ConnectData &_rconndata){
	
	return BAD;
}
//---------------------------------------------------------------------
void Service::connectSession(const SocketAddressInet4 &_raddr){
	Locker<Mutex>	lock(mutex());
	int				tkridx(allocateTalkerForSession());
	IndexT			tkrfullid;
	uint32			tkruid;
	
	if(tkridx >= 0){
		//the talker exists
		tkrfullid = d.tkrvec[tkridx].uid.first;
		tkruid = d.tkrvec[tkridx].uid.second;
	}else{
		//create new talker
		tkridx = createTalker(tkrfullid, tkruid);
		if(tkridx < 0){
			tkridx = allocateTalkerForSession(true/*force*/);
		}
		tkrfullid = d.tkrvec[tkridx].uid.first;
		tkruid = d.tkrvec[tkridx].uid.second;
	}
	
	Locker<Mutex>	lock2(this->mutex(tkrfullid));
	Talker			*ptkr(static_cast<Talker*>(this->object(tkrfullid)));
	Session			*pses(new Session(*this, _raddr));
	ConnectionUid	conid(tkridx);
	
	cassert(ptkr);
	
	vdbgx(Debug::ipc, "");
	ptkr->pushSession(pses, conid);
	d.sessionaddr4map[pses->peerBaseAddress4()] = conid;
	
	if(ptkr->notify(frame::S_RAISE)){
		manager().raise(*ptkr);
	}
}
//---------------------------------------------------------------------
void Service::disconnectTalkerSessions(Talker &_rtkr){
	Locker<Mutex>	lock(mutex());
	_rtkr.disconnectSessions();
}
//---------------------------------------------------------------------
void Service::disconnectSession(Session *_pses){
	if(_pses->isRelayType()){
		d.sessionrelayaddr4map.erase(_pses->peerRelayAddress4());
	}else{
		d.sessionaddr4map.erase(_pses->peerBaseAddress4());
	}
	//Use:Context::the().msgctx.connectionuid.tid
	const int			tkridx(Context::the().msgctx.connectionuid.tid);
	Data::TalkerStub	&rts(d.tkrvec[tkridx]);
	
	--rts.cnt;
	
	vdbgx(Debug::ipc, "disconnected session for talker "<<tkridx<<" session count per talker = "<<rts.cnt);
	
	if(rts.cnt < d.config.talker.sescnt){
		d.tkrq.push(tkridx);
	}
}
//---------------------------------------------------------------------
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
Configuration const& Service::configuration()const{
	return d.config;
}
//---------------------------------------------------------------------
const Controller& Service::controller()const{
	return *d.ctrlptr;
}
//------------------------------------------------------------------
//		Configuration
//------------------------------------------------------------------

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
	Message *_pmsg,
	ipc::MessageUid &_rmsguid
){
	_pmsg->ipcReceive(_rmsguid);
	return true;
}

int Controller::authenticate(
	DynamicPointer<Message> &_rmsgptr,
	ipc::MessageUid &_rmsguid,
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


// /*virtual*/ SocketAddressStub Controller::gatewayAddress(
// 	const uint _idx,
// 	const uint32 _netid_dest,
// 	const SocketAddressStub &_rsas_dest
// ){
// 	return SocketAddressStub();
// }
// 
// //retval:
// // -1 : wait for asynchrounous event and retry
// // 0: no gateway
// // > 0: the count
// /*virtual*/ int Controller::gatewayCount(
// 	const uint32 _netid_dest,
// 	const SocketAddressStub &_rsas_dest
// )const{
// 	return 0;
// }
// 
// //called on the gateway to find out where to connect for relaying data to _rsas_dest
// /*virtual*/ const SocketAddress& Controller::relayAddress(
// 	const uint32 _netid_dest,
// 	const SocketAddressStub &_rsas_dest
// ){
// 	const SocketAddress *psa = NULL;
// 	return *psa;
// }
// 
// /*virtual*/ uint32 Controller::relayCount(
// 	const uint32 _netid_dest,
// 	const SocketAddressStub &_rsas_dest
// )const{
// 	return 0;
// }
//------------------------------------------------------------------
//		BasicController
//------------------------------------------------------------------

BasicController::BasicController(
	AioSchedulerT &_rsched,
	const uint32 _flags,
	const uint32 _resdatasz
):Controller(_flags, _resdatasz), rsched_t(_rsched), rsched_l(_rsched), rsched_n(_rsched){}

BasicController::BasicController(
	AioSchedulerT &_rsched_t,
	AioSchedulerT &_rsched_l,
	AioSchedulerT &_rsched_n,
	const uint32 _flags,
	const uint32 _resdatasz
):Controller(_flags, _resdatasz), rsched_t(_rsched_t), rsched_l(_rsched_l), rsched_n(_rsched_n){}

/*virtual*/ void BasicController::scheduleTalker(frame::aio::Object *_ptkr){
	DynamicPointer<frame::aio::Object> op(_ptkr);
	rsched_t.schedule(op);
}

/*virtual*/ void BasicController::scheduleListener(frame::aio::Object *_plis){
	DynamicPointer<frame::aio::Object> op(_plis);
	rsched_t.schedule(op);
}

/*virtual*/ void BasicController::scheduleNode(frame::aio::Object *_pnod){
	DynamicPointer<frame::aio::Object> op(_pnod);
	rsched_t.schedule(op);
}

}//namespace ipc
}//namespace frame
}//namespace solid


