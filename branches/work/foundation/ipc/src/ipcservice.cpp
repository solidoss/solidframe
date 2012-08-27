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

#include "foundation/objectpointer.hpp"
#include "foundation/common.hpp"
#include "foundation/manager.hpp"

#include "foundation/ipc/ipcservice.hpp"
#include "foundation/ipc/ipcconnectionuid.hpp"

#include "ipctalker.hpp"
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
#endif	
	struct TalkerStub{
		TalkerStub():cnt(0){}
		TalkerStub(const ObjectUidT &_ruid, uint32 _cnt = 0):uid(_ruid), cnt(_cnt){}
		ObjectUidT	uid;
		uint32		cnt;
	};
	typedef std::vector<TalkerStub>						TalkerStubVectorT;
	typedef Queue<uint32>								Uint32QueueT;
	
	Data(
		uint32 _keepalivetout,
		uint32 _sespertkr,
		uint32 _tkrmaxcnt
	);
	
	~Data();
	
	uint32 sessionCount()const{
		return sessionaddr4map.size();
	}
	
	const uint32 			keepalivetout;
	const uint32			sespertkr;
	const uint32			tkrmaxcnt;
	uint32					tkrcrt;
	int						baseport;
	SocketAddress			firstaddr;
	TalkerStubVectorT		tkrvec;
	SessionAddr4MapT		sessionaddr4map;
	Controller				*pc;
	Uint32QueueT			tkrq;
};

//=======	ServiceData		===========================================

Service::Data::Data(
	uint32 _keepalivetout,
	uint32 _sespertkr,
	uint32 _tkrmaxcnt
):
	keepalivetout(_keepalivetout), sespertkr(_sespertkr), tkrmaxcnt(_tkrmaxcnt),
	tkrcrt(0), baseport(-1), pc(NULL)
{
}

Service::Data::~Data(){
}

//=======	Service		===============================================

/*static*/ Service& Service::the(){
	return *m().service<Service>();
}
/*static*/ Service& Service::the(const IndexT &_ridx){
	return *m().service<Service>(_ridx);
}

Service::Service(
	Controller *_pc,
	uint32 _keepalivetout,
	uint32 _sespertkr,
	uint32 _tkrmaxcnt
):d(*(new Data(_keepalivetout, _sespertkr, _tkrmaxcnt))){
	registerObjectType<Talker>(this);
	d.pc = _pc;
}
//---------------------------------------------------------------------
Service::~Service(){
	if(d.pc && d.pc->release()){
		delete d.pc;
	}
	delete &d;
}
//---------------------------------------------------------------------
uint32 Service::keepAliveTimeout()const{
	return d.keepalivetout;
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
	
	Controller::SocketAddressIterator	it(controller().gatewayIteratorBegin(_rsa_dest, _netid_dest));
	
	if(it == controller().gatewayIteratorEnd()){
		return doSendSignalLocal(_psig, _rtid, _pconid, _rsa_dest, _netid_dest, _flags);
	}else{
		return doSendSignalRelay(_psig, _rtid, _pconid, _rsa_dest, _netid_dest, _flags, it);
	}
}
//---------------------------------------------------------------------
int Service::doSendSignalLocal(
	DynamicPointer<Signal> &_psig,//the signal to be sent
	const SerializationTypeIdT &_rtid,
	ConnectionUid *_pconid,
	const SocketAddressStub &_rsap,
	const uint32 _netid_dest,
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
			
			int16	tkrid(allocateTalkerForNewSession());
			IndexT	tkrpos;
			uint32	tkruid;
			
			if(tkrid >= 0){
				//the talker exists
				tkrpos = d.tkrvec[tkrid].uid.first;
				tkruid = d.tkrvec[tkrid].uid.second;
			}else{
				//create new talker
				tkrid = createNewTalker(tkrpos, tkruid);
				if(tkrid < 0){
					tkrid = allocateTalkerForNewSession(true/*force*/);
				}
				tkrpos = d.tkrvec[tkrid].uid.first;
				tkruid = d.tkrvec[tkrid].uid.second;
			}
			
			tkrpos = Manager::the().computeIndex(tkrpos);
			Locker<Mutex>		lock2(this->mutex(tkrpos));
			Talker				*ptkr(static_cast<Talker*>(this->objectAt(tkrpos)));
			cassert(ptkr);
			Session				*pses(new Session(sa, d.keepalivetout));
			ConnectionUid		conid(tkrid);
			
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
	uint32	_flags,
	const Controller::SocketAddressIterator& _addrit
){
	return BAD;
}
//---------------------------------------------------------------------
int Service::allocateTalkerForNewSession(bool _force){
	if(!_force){
		if(d.tkrq.size()){
			int 				rv(d.tkrq.front());
			Data::TalkerStub	&rts(d.tkrvec[rv]);
			++rts.cnt;
			if(rts.cnt == d.sespertkr){
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
int Service::acceptSession(Session *_pses){
	Locker<Mutex>	lock(serviceMutex());
	{
		//TODO: see if the locking is ok!!!
		
		Data::SessionAddr4MapT::iterator	it(d.sessionaddr4map.find(_pses->peerBaseAddress4()));
		
		if(it != d.sessionaddr4map.end()){
			//a connection still exists
			IndexT			tkrpos(Manager::the().computeIndex(d.tkrvec[it->second.tid].uid.first));
			Locker<Mutex>	lock2(this->mutex(tkrpos));
			Talker			*ptkr(static_cast<Talker*>(this->objectAt(tkrpos)));
			
			vdbgx(Dbg::ipc, "");
			
			ptkr->pushSession(_pses, it->second, true);
			
			if(ptkr->signal(fdt::S_RAISE)){
				Manager::the().raiseObject(*ptkr);
			}
			return OK;
		}
	}
	int		tkrid(allocateTalkerForNewSession());
	IndexT	tkrpos;
	uint32	tkruid;
	
	if(tkrid >= 0){
		//the talker exists
		tkrpos = d.tkrvec[tkrid].uid.first;
		tkruid = d.tkrvec[tkrid].uid.second;
	}else{
		//create new talker
		tkrid = createNewTalker(tkrpos, tkruid);
		if(tkrid < 0){
			tkrid = allocateTalkerForNewSession(true/*force*/);
		}
		tkrpos = d.tkrvec[tkrid].uid.first;
		tkruid = d.tkrvec[tkrid].uid.second;
	}
	
	tkrpos = Manager::the().computeIndex(tkrpos);
	
	Locker<Mutex>	lock2(this->mutex(tkrpos));
	Talker			*ptkr(static_cast<Talker*>(this->objectAt(tkrpos)));
	cassert(ptkr);
	ConnectionUid	conid(tkrid, 0xffff, 0xffff);
	
	vdbgx(Dbg::ipc, "");
	
	ptkr->pushSession(_pses, conid);
	d.sessionaddr4map[_pses->peerBaseAddress4()] = conid;
	
	if(ptkr->signal(fdt::S_RAISE)){
		Manager::the().raiseObject(*ptkr);
	}
	return OK;
}
//---------------------------------------------------------------------
void Service::connectSession(const SocketAddressInet4 &_raddr){
	Locker<Mutex>	lock(serviceMutex());
	int				tkrid(allocateTalkerForNewSession());
	IndexT			tkrpos;
	uint32			tkruid;
	
	if(tkrid >= 0){
		//the talker exists
		tkrpos = d.tkrvec[tkrid].uid.first;
		tkruid = d.tkrvec[tkrid].uid.second;
	}else{
		//create new talker
		tkrid = createNewTalker(tkrpos, tkruid);
		if(tkrid < 0){
			tkrid = allocateTalkerForNewSession(true/*force*/);
		}
		tkrpos = d.tkrvec[tkrid].uid.first;
		tkruid = d.tkrvec[tkrid].uid.second;
	}
	tkrpos = Manager::the().computeIndex(tkrpos);
	
	Locker<Mutex>		lock2(this->mutex(tkrpos));
	Talker				*ptkr(static_cast<Talker*>(this->objectAt(tkrpos)));
	cassert(ptkr);
	Session				*pses(new Session(_raddr, d.keepalivetout));
	ConnectionUid		conid(tkrid);
	
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
	d.sessionaddr4map.erase(_pses->peerBaseAddress4());
	//Use:Context::the().sigctx.connectionuid.tid
	int tkrid(Context::the().sigctx.connectionuid.tid);
	Data::TalkerStub &rts(d.tkrvec[tkrid]);
	--rts.cnt;
	vdbgx(Dbg::ipc, "disconnected session for talker "<<tkrid<<" session count per talker = "<<rts.cnt);
	if(rts.cnt < d.sespertkr){
		d.tkrq.push(tkrid);
	}
}
//---------------------------------------------------------------------
int Service::createNewTalker(IndexT &_tkrpos, uint32 &_tkruid){
	
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
		Talker *ptkr(new Talker(sd, *this, tkrid));
		
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
//---------------------------------------------------------------------
int Service::insertListener(
	const ResolveIterator &_rai
){
/*	test::Listener *plis = new test::Listener(_pst, 100, 0);
	if(this->insert(*plis, _serviceid)){
		delete plis;
		return BAD;
	}	
	_rm.pushJob((fdt::tcp::Listener*)plis);*/
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
//---------------------------------------------------------------------
Controller& Service::controller(){
	return *d.pc;
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
	_psig->ipcReceived(_rsiguid);
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

/*virtual*/ Controller::SocketAddressIterator Controller::gatewayIteratorBegin(
	const SocketAddressStub &_rsas_dest,
	const uint32 _netid_dest
){
	return SocketAddressIterator();
}
/*virtual*/ const Controller::SocketAddressIterator& Controller::gatewayIteratorEnd(){
	static const SocketAddressIterator endit;
	return endit;
}
	
	
/*virtual*/ uint32 Controller::localNetworkId()const{
	return LocalNetworkId;
}

}//namespace ipc
}//namespace foundation

