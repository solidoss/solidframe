// frame/ipc/src/ipcservice.cpp
//
// Copyright (c) 2007, 2008, 2010 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <vector>
#include <cstring>
#include <utility>
#include <algorithm>

#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/socketdevice.hpp"
#include "system/specific.hpp"
#include "system/exception.hpp"

#include "utility/queue.hpp"
#include "utility/binaryseeker.hpp"

#include "frame/common.hpp"
#include "frame/manager.hpp"

#include "frame/aio/openssl/opensslsocket.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/ipc/ipcconnectionuid.hpp"
#include "frame/ipc/ipcmessage.hpp"

#include "ipctalker.hpp"
#include "ipcnode.hpp"
#include "ipclistener.hpp"
#include "ipcsession.hpp"
#include "ipcpacket.hpp"
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
	
	typedef std::unordered_map<
		const GatewayRelayAddress4T,
		size_t,
		SocketAddressHash,
		SocketAddressEqual
	>	GatewayRelayAddr4MapT;
	typedef std::unordered_map<
		const GatewayRelayAddress6T,
		size_t,
		SocketAddressHash,
		SocketAddressEqual
	>	GatewayRelayAddr6MapT;
	
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
	
	
	typedef std::map<
		const GatewayRelayAddress4T,
		size_t, 
		SocketAddressCompare
	>	GatewayRelayAddr4MapT;
	
	typedef std::map<
		const GatewayRelayAddress6T,
		size_t,
		SocketAddressCompare
	>	GatewayRelayAddr6MapT;
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
	
	struct RelayAddress4Stub{
		RelayAddress4Stub():nid(0xffffffff), idx(0xffffffff){}
		RelayAddress4Stub(
			SocketAddressInet4	const &_addr,
			uint32 _rnid,
			uint32 _idx
		): addr(_addr), nid(_rnid), idx(_idx){}
		
		SocketAddressInet4		addr;
		uint32					nid;
		uint32					idx;
	};
	
	typedef std::vector<TalkerStub>						TalkerStubVectorT;
	typedef std::vector<NodeStub>						NodeStubVectorT;
	typedef std::deque<RelayAddress4Stub>				RelayAddress4DequeT;
	
	typedef Queue<uint32>								Uint32QueueT;
	typedef Stack<size_t>								SizeStackT;
	
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
	uint32						crtgwidx;
	SocketAddress				firstaddr;
	TalkerStubVectorT			tkrvec;
	NodeStubVectorT				nodevec;
	SessionAddr4MapT			sessionaddr4map;
	SessionRelayAddr4MapT		sessionrelayaddr4map;
	
	GatewayRelayAddr4MapT		gwrelayaddrmap;//local to remote
	RelayAddress4DequeT			gwrelayaddrdeq;
	SizeStackT					gwfreestk;
	RelayAddressPointerVectorT	gwnetid2addrvec;
	RelayAddressPointerVectorT	gwaddr2netidvec;
	
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
	tkrcrt(0), nodecrt(0), baseport(-1), crtgwidx(0)
{
	timestamp.currentRealTime();
}

Service::Data::~Data(){
}

//=======	Service		===============================================

/*static*/ const char* Service::errorText(int _err){
	switch(_err){
		case NoError:
			return "No Error";
		case GenericError:
			return "Generic Error";
		case NoGatewayError:
			return "No Gateway Error";
		case UnsupportedSocketFamilyError:
			return "Unsupported socket family Error";
		case NoConnectionError:
			return "No such connection error";
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

struct RelayAddressNetIdLess{
	bool operator()(Configuration::RelayAddress const *_a1, Configuration::RelayAddress const *_a2)const{
		return _a1->networkid < _a2->networkid;
	}
};

struct RelayAddressAddrLess{
	bool operator()(Configuration::RelayAddress const *_a1, Configuration::RelayAddress const *_a2)const{
		//TODO: add support for IPv6
		return _a1->address.address4() < _a2->address.address4();
	}
};


bool Service::reconfigure(const Configuration &_rcfg){
	Locker<Mutex>	lock(mutex());
	
	if(_rcfg == d.config){
		return true;
	}
	
	unsafeReset(lock);
	
	d.config = _rcfg;
	
	if(
		d.config.baseaddr.isInet4()
	){
		
		d.firstaddr = d.config.baseaddr;
		
	
		SocketDevice	sd;
		
		sd.create(SocketInfo::Inet4, SocketInfo::Datagram, 0);
		sd.makeNonBlocking();
		sd.bind(d.config.baseaddr);
		
		if(!sd.ok()){
			return false;
		}
		
		SocketAddress sa;
		
		if(!sd.localAddress(sa)){
			return false;
		}
		
		d.baseport = sa.port();
		if(configuration().node.timeout){
		}else{
			const uint32	dataresendcnt = configuration().session.dataretransmitcount;
			const uint32	connectresendcnt = configuration().session.connectretransmitcount;
			uint32			nodetimeout = 0;
			nodetimeout = Session::computeResendTime(dataresendcnt) + Session::computeResendTime(connectresendcnt);
			nodetimeout += d.config.session.responsekeepalive;
			nodetimeout += d.config.session.keepalive;
			nodetimeout += d.config.session.relayresponsekeepalive;
			nodetimeout += d.config.session.relaykeepalive;
			nodetimeout *= 2;//we need to be sure that the keepalive on note 
			idbgx(Debug::ipc, "nodetimeout = "<<nodetimeout);
			d.config.node.timeout = nodetimeout / 1000;
		}
		d.gwnetid2addrvec.clear();
		d.gwaddr2netidvec.clear();
		for(
			Configuration::RelayAddressVectorT::const_iterator it(configuration().relayaddrvec.begin());
			it != configuration().relayaddrvec.end();
			++it
		){
			d.gwnetid2addrvec.push_back(&(*it));
			d.gwaddr2netidvec.push_back(&(*it));
		}
		{
			RelayAddressNetIdLess	cmp;
			std::sort(d.gwnetid2addrvec.begin(), d.gwnetid2addrvec.end(), cmp);
		}
		{
			RelayAddressAddrLess	cmp;
			std::sort(d.gwaddr2netidvec.begin(), d.gwaddr2netidvec.end(), cmp);
		}
		
		//Locker<Mutex>	lock(serviceMutex());
		cassert(!d.tkrvec.size());//only the first tkr must be inserted from outside
		Talker			*ptkr(new Talker(sd, *this, 0));
		ObjectUidT		objuid = this->unsafeRegisterObject(*ptkr);

		d.tkrvec.push_back(Data::TalkerStub(objuid));
		d.tkrq.push(0);
		controller().scheduleTalker(ptkr);
		if(d.config.acceptaddr.port() > 0){
			sd.create(SocketInfo::Inet4, SocketInfo::Stream, 0);
			sd.makeNonBlocking();
			sd.prepareAccept(d.config.acceptaddr, 1000);
			if(!sd.ok()){
				return false;
			}
			Listener 		*plsn = new Listener(*this, sd, RelayType);
			this->unsafeRegisterObject(*plsn);
			controller().scheduleListener(plsn);
		}
		
		return true;
	}else{
		return false;
	}
}
//---------------------------------------------------------------------
bool Service::sendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const ConnectionUid &_rconid,//the id of the process connector
	uint32	_flags
){
	cassert(_rconid.tkridx < d.tkrvec.size());
	
	Locker<Mutex>		lock(mutex());
	const IndexT		fullid(d.tkrvec[_rconid.tkridx].uid.first);
	Locker<Mutex>		lock2(this->mutex(fullid));
	Talker				*ptkr(static_cast<Talker*>(this->object(fullid)));
	
	cassert(ptkr);
	
	if(ptkr == NULL){
		//TODO: set specific error NoConnectionError;
		return false;
	}
	
	if(ptkr->pushMessage(_rmsgptr, SERIALIZATION_INVALIDID, _rconid, _flags | SameConnectorFlag)){
		//the talker must be notified
		if(ptkr->notify(frame::S_RAISE)){
			manager().raise(*ptkr);
		}
	}
	return true;
}
//---------------------------------------------------------------------
void Service::doSendEvent(
	const ConnectionUid &_rconid,//the id of the process connectors
	int32 _event,
	uint32 _flags
){
	Locker<Mutex>		lock(mutex());
	const IndexT		fullid(d.tkrvec[_rconid.tkridx].uid.first);
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
bool Service::sendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SerializationTypeIdT &_rtid,
	const ConnectionUid &_rconid,//the id of the process connector
	uint32	_flags
){
	cassert(_rconid.tkridx < d.tkrvec.size());
	if(_rconid.tkridx >= d.tkrvec.size()){
		//TODO: specific NoConnectionError;
		return false;
	}
	
	Locker<Mutex>		lock(mutex());
	const IndexT		fullid(d.tkrvec[_rconid.tkridx].uid.first);
	Locker<Mutex>		lock2(this->mutex(fullid));
	Talker				*ptkr(static_cast<Talker*>(this->object(fullid)));
	
	cassert(ptkr);
	if(ptkr == NULL){
		//TODO: specific NoConnectionError;
		return false;
	}
	
	if(ptkr->pushMessage(_rmsgptr, _rtid, _rconid, _flags | SameConnectorFlag)){
		//the talker must be notified
		if(ptkr->notify(frame::S_RAISE)){
			manager().raise(*ptkr);
		}
	}
	return true;
}
//---------------------------------------------------------------------
int Service::basePort()const{
	return d.baseport;
}
//---------------------------------------------------------------------
uint32 Service::computeNetworkId(
	const SocketAddressStub &_rsa_dest,
	const uint32 _netid_dest
)const{
	if(_netid_dest == LocalNetworkId || _netid_dest == configuration().localnetid){
		return LocalNetworkId;
	}else if(_netid_dest == InvalidNetworkId){
		return controller().computeNetworkId(_rsa_dest);
	}else{
		return _netid_dest;
	}
}
//---------------------------------------------------------------------
bool Service::isLocalNetwork(
	const SocketAddressStub &_rsa_dest,
	const uint32 _netid_dest
)const{
	return computeNetworkId(_rsa_dest, _netid_dest) == LocalNetworkId;
}
//---------------------------------------------------------------------
bool Service::isGateway()const{
	//TODO: find better check
	return configuration().relayaddrvec.size() != 0;
}
//---------------------------------------------------------------------
bool Service::doSendMessage(
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
	){
		//TODO: specific UnsupportedSocketFamilyError;
		return false;
	}
	
	const uint32 netid = computeNetworkId(_rsa_dest, _netid_dest);
	
	if(netid == LocalNetworkId){
		return doSendMessageLocal(_rmsgptr, _rtid, _pconid, _rsa_dest, netid, _flags);
	}else{
		return doSendMessageRelay(_rmsgptr, _rtid, _pconid, _rsa_dest, netid, _flags);
	}
}
//---------------------------------------------------------------------
bool Service::doSendMessageLocal(
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
			const IndexT		fullid(d.tkrvec[conid.tkridx].uid.first);
			Locker<Mutex>		lock2(this->mutex(fullid));
			Talker				*ptkr(static_cast<Talker*>(this->object(fullid)));
			
			cassert(conid.tkridx < d.tkrvec.size());
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
		//TODO: specific UnsupportedSocketFamilyError;
		return false;
	}
	return true;
}
//---------------------------------------------------------------------
bool Service::doSendMessageRelay(
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
		
		vdbgx(Debug::ipc, " addr = "<<sa<<" baseport = "<<_rsap.port()<<" netid = "<<_netid_dest);
		
		Data::SessionRelayAddr4MapT::iterator	it(d.sessionrelayaddr4map.find(addr));
		
		if(it != d.sessionrelayaddr4map.end()){
		
			vdbgx(Debug::ipc, "");
			
			ConnectionUid		conid(it->second);
			const IndexT		fullid(d.tkrvec[conid.tkridx].uid.first);
			Locker<Mutex>		lock2(this->mutex(fullid));
			Talker				*ptkr(static_cast<Talker*>(this->object(fullid)));
			
			cassert(conid.tkridx < d.tkrvec.size());
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
			if(configuration().gatewayaddrvec.empty()){
				//TODO: specific NoGatewayError;
				return false;
			}
			
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
			Session				*pses(new Session(*this, _netid_dest, sa, d.crtgwidx));
			ConnectionUid		conid(tkridx);
			
			d.crtgwidx = (d.crtgwidx + 1) % d.config.gatewayaddrvec.size();
			
			ptkr->pushSession(pses, conid);
			
			vdbgx(Debug::ipc, ""<<pses->peerRelayAddress4().first.first);
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
		//TODO: specific UnsupportedSocketFamilyError;
		return false;
	}
	return true;
}
//---------------------------------------------------------------------
int Service::createTalker(IndexT &_tkrfullid, uint32 &_tkruid){
	
	if(d.tkrvec.size() >= d.config.talker.maxcnt){
		vdbgx(Debug::ipc, "maximum talker count reached "<<d.tkrvec.size());
		return -1;
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
	return -1;
}
//---------------------------------------------------------------------
int Service::createNode(IndexT &_nodepos, uint32 &_nodeuid){
	
	if(d.nodevec.size() >= d.config.node.maxcnt){
		vdbgx(Debug::ipc, "maximum node count reached "<<d.nodevec.size());
		return -1;
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
	return -1;
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
bool Service::acceptSession(const SocketAddress &_rsa, const ConnectData &_rconndata){
	specific_error_clear();
	if(_rconndata.type == ConnectData::BasicType){
		return doAcceptBasicSession(_rsa, _rconndata);
	}else if(isLocalNetwork(_rconndata.receiveraddress, _rconndata.receivernetworkid)){
		return doAcceptRelaySession(_rsa, _rconndata);
	}else if(isGateway()){
		return doAcceptGatewaySession(_rsa, _rconndata);
	}else{
		SPECIFIC_ERROR_PUSH2(NoGatewayError, ERROR_NS::generic_category());
		return false;
	}
}
//---------------------------------------------------------------------
bool Service::doAcceptBasicSession(const SocketAddress &_rsa, const ConnectData &_rconndata){
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
			const IndexT	tkrfullid(d.tkrvec[it->second.tkridx].uid.first);
			Locker<Mutex>	lock2(this->mutex(tkrfullid));
			Talker			*ptkr(static_cast<Talker*>(this->object(tkrfullid)));
			
			vdbgx(Debug::ipc, "");
			
			ptkr->pushSession(pses, it->second, true);
			
			if(ptkr->notify(frame::S_RAISE)){
				manager().raise(*ptkr);
			}
			return true;
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
	return true;
}
//---------------------------------------------------------------------
//accept session on destination, receiver
bool Service::doAcceptRelaySession(const SocketAddress &_rsa, const ConnectData &_rconndata){
	Locker<Mutex>				lock(mutex());
	SocketAddressInet4			inaddr(_rsa);
	Session						*pses = new Session(
		*this,
		_rconndata.sendernetworkid,
		inaddr,
		_rconndata,
		d.crtgwidx
	);
	
	d.crtgwidx = (d.crtgwidx + 1) % d.config.gatewayaddrvec.size();
	{
		//TODO: see if the locking is ok!!!
		
		Data::SessionRelayAddr4MapT::iterator	it(d.sessionrelayaddr4map.find(pses->peerRelayAddress4()));
		
		if(it != d.sessionrelayaddr4map.end()){
			//a connection still exists
			const IndexT	tkrfullid(d.tkrvec[it->second.tkridx].uid.first);
			Locker<Mutex>	lock2(this->mutex(tkrfullid));
			Talker			*ptkr(static_cast<Talker*>(this->object(tkrfullid)));
			
			vdbgx(Debug::ipc, "");
			
			ptkr->pushSession(pses, it->second, true);
			
			if(ptkr->notify(frame::S_RAISE)){
				manager().raise(*ptkr);
			}
			return true;
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
	return true;
}
//---------------------------------------------------------------------
bool Service::doAcceptGatewaySession(const SocketAddress &_rsa, const ConnectData &_rconndata){
	typedef Data::GatewayRelayAddr4MapT::const_iterator GatewayRelayAddr4MapConstIteratorT;
	
	Locker<Mutex>						lock(mutex());
	
	
	SocketAddressInet4					addr(_rsa);
	GatewayRelayAddress4T				gwaddr(addr, _rconndata.relayid);
	
	GatewayRelayAddr4MapConstIteratorT	it(d.gwrelayaddrmap.find(gwaddr));
	
	if(d.gwrelayaddrmap.end() != it){
		//maybe a resent packet
		const Data::RelayAddress4Stub	&ras(d.gwrelayaddrdeq[it->second]);
		IndexT							nodefullid;
		
		nodefullid = d.nodevec[ras.nid].uid.first;
		
		Locker<Mutex>					lock2(this->mutex(nodefullid));
		Node							*pnode(static_cast<Node*>(this->object(nodefullid)));
		
		pnode->pushSession(_rsa, _rconndata, ras.idx);
		
		if(pnode->notify(frame::S_RAISE)){
			manager().raise(*pnode);
		}
	}else{
		int				nodeidx(allocateNodeForSession());
		IndexT			nodefullid;
		uint32			nodeuid;
		
		if(nodeidx >= 0){
			//the node exists
			nodefullid = d.nodevec[nodeidx].uid.first;
			nodeuid = d.nodevec[nodeidx].uid.second;
		}else{
			//create new node
			nodeidx = createNode(nodefullid, nodeuid);
			if(nodeidx < 0){
				nodeidx = allocateNodeForSession(true/*force*/);
			}
			nodefullid = d.nodevec[nodeidx].uid.first;
			nodeuid = d.nodevec[nodeidx].uid.second;
		}
		
		Locker<Mutex>	lock2(this->mutex(nodefullid));
		Node			*pnode(static_cast<Node*>(this->object(nodefullid)));
		
		cassert(pnode);
		
		vdbgx(Debug::ipc, "");
		
		
		uint32 idx = pnode->pushSession(_rsa, _rconndata);
		
		if(d.gwfreestk.size()){
			size_t ofs = d.gwfreestk.top();
			d.gwfreestk.pop();
			d.gwrelayaddrdeq[ofs].addr = addr;
			d.gwrelayaddrdeq[ofs].nid = nodeidx;
			d.gwrelayaddrdeq[ofs].idx = idx;
			
			GatewayRelayAddress4T	gwa(d.gwrelayaddrdeq[ofs].addr, _rconndata.relayid);
			d.gwrelayaddrmap[gwa] = ofs;
		}else{
			d.gwrelayaddrdeq.push_back(Data::RelayAddress4Stub(addr, nodeidx, idx));
			GatewayRelayAddress4T	gwa(d.gwrelayaddrdeq.back().addr, _rconndata.relayid);
			d.gwrelayaddrmap[gwa] = d.gwrelayaddrdeq.size() - 1;
		}
		
		if(pnode->notify(frame::S_RAISE)){
			manager().raise(*pnode);
		}
	}
	return true;
}
//---------------------------------------------------------------------
void Service::disconnectSession(const SocketAddressInet &_addr, uint32 _relayid){
	typedef Data::GatewayRelayAddr4MapT::iterator GatewayRelayAddr4MapConstIteratorT;
	
	SocketAddressInet4					addr(_addr);
	GatewayRelayAddress4T				gwaddr(addr, _relayid);
	
	GatewayRelayAddr4MapConstIteratorT	it(d.gwrelayaddrmap.find(gwaddr));
	if(it != d.gwrelayaddrmap.end()){
		d.gwfreestk.push(it->second);
		d.gwrelayaddrmap.erase(it);
	}
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
void Service::disconnectTalkerSessions(Talker &_rtkr, TalkerStub &_rts){
	Locker<Mutex>	lock(mutex());
	_rtkr.disconnectSessions(_rts);
}
//---------------------------------------------------------------------
void Service::disconnectNodeSessions(Node &_rn){
	Locker<Mutex>	lock(mutex());
	_rn.disconnectSessions();
}
//---------------------------------------------------------------------
void Service::disconnectSession(Session *_pses){
	{
		uint32				netid = LocalNetworkId;
		SocketAddressInet	addr;
		if(_pses->isRelayType()){
			const RelayAddress4T peeraddr = _pses->peerRelayAddress4();
			d.sessionrelayaddr4map.erase(peeraddr);
			addr = peeraddr.first.first;
			addr.port(peeraddr.first.second);
			netid = peeraddr.second;			
		}else{
			const BaseAddress4T peeraddr = _pses->peerBaseAddress4();
			d.sessionaddr4map.erase(peeraddr);
			addr = peeraddr.first;
			addr.port(peeraddr.second);
		}
		controller().onDisconnect(addr, netid);
	}
	//Use:Context::the().msgctx.connectionuid.tkridx
	const int			tkridx(Context::the().msgctx.connectionuid.tkridx);
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
void Service::insertConnection(
	SocketDevice &_rsd,
	aio::openssl::Context *_pctx,
	bool _secure
){
	int				nodeidx(allocateNodeForSocket());
	IndexT			nodefullid;
	uint32			nodeuid;
		
	if(nodeidx >= 0){
		//the node exists
		nodefullid = d.nodevec[nodeidx].uid.first;
		nodeuid = d.nodevec[nodeidx].uid.second;
	}else{
		//create new node
		nodeidx = createNode(nodefullid, nodeuid);
		if(nodeidx < 0){
			nodeidx = allocateNodeForSocket(true/*force*/);
		}
		nodefullid = d.nodevec[nodeidx].uid.first;
		nodeuid = d.nodevec[nodeidx].uid.second;
	}
		
	Locker<Mutex>	lock2(this->mutex(nodefullid));
	Node			*pnode(static_cast<Node*>(this->object(nodefullid)));
	
	cassert(pnode);
	
	vdbgx(Debug::ipc, "");
	
	SocketAddress		sa;
	
	_rsd.remoteAddress(sa);
	
	SocketAddressInet	sai(sa);
	
	const size_t		off = address2NetIdFind(sai);
	
	if(off != address2NetIdVectorSize()){
		pnode->pushConnection(_rsd, off, _pctx, _secure);
	
		if(pnode->notify(frame::S_RAISE)){
			manager().raise(*pnode);
		}
	}
}

namespace{
struct RelayAddressNetIdCompare{
	int operator()(Configuration::RelayAddress const *_a1, uint32 _networkid)const{
		if(_a1->networkid < _networkid){
			return -1;
		}else if(_a1->networkid > _networkid){
			return 1;
		}
		return 0;
	}
};

struct RelayAddressAddrCompare{
	bool operator()(Configuration::RelayAddress const *_a1, SocketAddressInet const &_address)const{
		//TODO: add support for IPv6
		if(_a1->address.address4() < _address.address4()){
			return -1;
		}else if(_address.address4() < _a1->address.address4()){
			return 1;
		}
		return 0;
	}
};
}//namespace

size_t	Service::netId2AddressFind(uint32 _netid)const{
	static BinarySeeker<RelayAddressNetIdCompare> bs;
	BinarySeekerResultT r = bs.first(d.gwnetid2addrvec.begin(), d.gwnetid2addrvec.end(), _netid);
	if(r.first){
		return r.second;
	}else{
		return d.gwnetid2addrvec.size();
	}
}
size_t	Service::netId2AddressVectorSize()const{
	return d.gwnetid2addrvec.size();
}

size_t	Service::address2NetIdFind(SocketAddressInet const&_addr)const{
	static BinarySeeker<RelayAddressAddrCompare> bs;
	BinarySeekerResultT r = bs.first(d.gwaddr2netidvec.begin(), d.gwaddr2netidvec.end(), _addr);
	if(r.first){
		return r.second;
	}else{
		return d.gwaddr2netidvec.size();
	}
}
size_t	Service::address2NetIdVectorSize()const{
	return d.gwaddr2netidvec.size();
}
Configuration::RelayAddress const&	Service::netId2AddressAt(const size_t _off)const{
	cassert(_off < d.gwnetid2addrvec.size());
	return *d.gwnetid2addrvec[_off];
}
Configuration::RelayAddress const&	Service::address2NetIdAt(const size_t _off)const{
	cassert(_off < d.gwaddr2netidvec.size());
	return *d.gwaddr2netidvec[_off];
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
/*virtual*/ bool Controller::compressPacket(
	PacketContext &_rpc,
	const uint32 _bufsz,
	char* &_rpb,
	uint32 &_bl
){
	return false;
}

/*virtual*/ bool Controller::decompressPacket(
	PacketContext &_rpc,
	char* &_rpb,
	uint32 &_bl
){
	return true;
}

char * Controller::allocateBuffer(PacketContext &_rpc, uint32 &_cp){
	const uint	mid(Specific::capacityToIndex(_cp ? _cp : Packet::Capacity));
	char		*newbuf(Packet::allocate());
	_cp = Packet::Capacity - _rpc.offset;
	if(_rpc.reqbufid != (uint)-1){
		THROW_EXCEPTION("Requesting more than one buffer");
		return NULL;
	}
	_rpc.reqbufid = mid;
	return newbuf + _rpc.offset;
}

/*virtual*/ bool Controller::onMessageReceive(
	DynamicPointer<Message> &_rmsgptr,
	ConnectionContext const &_rctx
){
	_rmsgptr->ipcOnReceive(_rctx, _rmsgptr);
	return true;
}

/*virtual*/ uint32 Controller::onMessagePrepare(
	Message &_rmsg,
	ConnectionContext const &_rctx
){
	return _rmsg.ipcOnPrepare(_rctx);
}

/*virtual*/ void Controller::onMessageComplete(
	Message &_rmsg,
	ConnectionContext const &_rctx,
	int _error
){
	_rmsg.ipcOnComplete(_rctx, _error);
}

AsyncE Controller::authenticate(
	DynamicPointer<Message> &_rmsgptr,
	ipc::MessageUid &_rmsguid,
	uint32 &_rflags,
	SerializationTypeIdT &_rtid
){
	//use: ConnectionContext::the().connectionuid!!
	return AsyncError;//by default no authentication
}

void Controller::sendEvent(
	Service &_rs,
	const ConnectionUid &_rconid,//the id of the process connectors
	int32 _event,
	uint32 _flags
){
	_rs.doSendEvent(_rconid, _event, _flags);
}

/*virtual*/ uint32 Controller::computeNetworkId(
	const SocketAddressStub &_rsa_dest
)const{
	return LocalNetworkId;
}
/*virtual*/ void Controller::onDisconnect(const SocketAddressInet &_raddr, const uint32 _netid){
	vdbgx(Debug::ipc, _netid<<':'<<_raddr);
}

//------------------------------------------------------------------
//		BasicController
//------------------------------------------------------------------

BasicController::BasicController(
	AioSchedulerT &_rsched,
	const uint32 _flags,
	const uint32 _resdatasz
):Controller(_flags, _resdatasz), rsched_t(_rsched), rsched_l(_rsched), rsched_n(_rsched){
	vdbgx(Debug::ipc, "");
}

BasicController::BasicController(
	AioSchedulerT &_rsched_t,
	AioSchedulerT &_rsched_l,
	AioSchedulerT &_rsched_n,
	const uint32 _flags,
	const uint32 _resdatasz
):Controller(_flags, _resdatasz), rsched_t(_rsched_t), rsched_l(_rsched_l), rsched_n(_rsched_n){
	vdbgx(Debug::ipc, "");
}

BasicController::~BasicController(){
	vdbgx(Debug::ipc, "");
}
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

//------------------------------------------------------------------
//		Message
//------------------------------------------------------------------

/*virtual*/ Message::~Message(){
	
}
/*virtual*/ void Message::ipcOnReceive(ConnectionContext const &_ripcctx, MessagePointerT &_rmsgptr){
}
/*virtual*/ uint32 Message::ipcOnPrepare(ConnectionContext const &_ripcctx){
	return 0;
}
/*virtual*/ void Message::ipcOnComplete(ConnectionContext const &_ripcctx, int _error){
	
}

struct HandleMessage{
	void operator()(Service::SerializerT &_rs, Message &_rt, const ConnectionContext &_rctx){
		vdbgx(Debug::ipc, "reset message state");
		if(_rt.ipcState() >= 2){
			_rt.ipcResetState();
		}
		
	}
	void operator()(Service::DeserializerT &_rs, Message &_rt, const ConnectionContext &_rctx){
		vdbgx(Debug::ipc, "increment message state");
		++_rt.ipcState();
	}
};

void Service::Handle::beforeSerialization(Service::SerializerT &_rs, Message *_pt, const ConnectionContext &_rctx){
	MessageUid	&rmsguid = _pt->ipcRequestMessageUid();
	vdbgx(Debug::ipc, ""<<rmsguid.idx<<','<<rmsguid.uid);
	_rs.pushHandle<HandleMessage>(_pt, "handle_state");
	_rs.push(_pt->ipcState(), "state").push(rmsguid.idx, "msg_idx").push(rmsguid.uid, "msg_uid");
}
void Service::Handle::beforeSerialization(Service::DeserializerT &_rs, Message *_pt, const ConnectionContext &_rctx){
	vdbgx(Debug::ipc, "");
	MessageUid	&rmsguid = _pt->ipcRequestMessageUid();
	_rs.pushHandle<HandleMessage>(_pt, "handle_state");
	_rs.push(_pt->ipcState(), "state").push(rmsguid.idx, "msg_idx").push(rmsguid.uid, "msg_uid");
}

//------------------------------------------------------------------
//		ConnectionContext
//------------------------------------------------------------------

ConnectionContext::MessagePointerT& ConnectionContext::requestMessage(const Message &_rmsg)const{
	static MessagePointerT msgptr;
	if(psession){
		ConnectionContext::MessagePointerT *pmsgptr = psession->requestMessageSafe(_rmsg.ipcRequestMessageUid());
		if(pmsgptr){
			return *pmsgptr;
		}else{
			msgptr.clear();
			return msgptr;
		}
	}else{
		msgptr.clear();
		return msgptr;
	}
}

}//namespace ipc
}//namespace frame
}//namespace solid


