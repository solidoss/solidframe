/* Implementation file ipcservice.cpp
	
	Copyright 2007, 2008, 2010 Valentin Palade 
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
#include <cstring>
#include <ostream>

#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/socketdevice.hpp"
#include "system/specific.hpp"

#include "foundation/objectpointer.hpp"
#include "foundation/common.hpp"
#include "foundation/manager.hpp"

#include "foundation/ipc/ipcservice.hpp"
#include "foundation/ipc/ipcconnectionuid.hpp"

#include "ipctalker.hpp"
#include "iodata.hpp"
#include "ipcsession.hpp"

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
	
	typedef std::pair<uint32, uint32>					UInt32PairT;
	typedef std::map<
		const Session::Addr4PairT*, 
		ConnectionUid, 
		Inet4AddrPtrCmp
		>												SessionAddr4MapT;
	
	typedef std::map<
		const Session::Addr6PairT*,
		ConnectionUid,
		Inet6AddrPtrCmp
		>												SessionAddr6MapT;
	typedef std::vector<UInt32PairT>					UInt32PairVectorT;
	
	Data();
	
	~Data();	
	
	int						sespertkrcnt;
	uint32					sessioncnt;
	int						baseport;
	SocketAddress			firstaddr;
	UInt32PairVectorT		tkrvec;
	SessionAddr4MapT		sessionaddr4map;
	uint32 					keepalivetout;
};

//=======	ServiceData		===========================================

Service::Data::Data():sespertkrcnt(10), sessioncnt(0), baseport(-1){
}

Service::Data::~Data(){
}

//=======	Service		===============================================

Service::Service(uint32 _keepalivetout):d(*(new Data)){
	//d.maxtkrcnt = 2;//TODO: make it configurable
	d.keepalivetout = _keepalivetout;
}
//---------------------------------------------------------------------
Service::~Service(){
	delete &d;
}
//---------------------------------------------------------------------
uint32 Service::keepAliveTimeout()const{
	return d.keepalivetout;
}
//---------------------------------------------------------------------
int Service::sendSignal(
	const ConnectionUid &_rconid,//the id of the process connector
	DynamicPointer<Signal> &_psig,//the signal to be sent
	uint32	_flags
){
	cassert(_rconid.sessionidx < d.tkrvec.size());
	
	Mutex::Locker		lock(*mutex());
	Data::UInt32PairT	&rtp(d.tkrvec[_rconid.id]);
	Mutex::Locker		lock2(this->mutex(rtp.first, rtp.second));
	Talker				*ptkr(static_cast<Talker*>(this->object(rtp.first, rtp.second)));
	
	cassert(ptkr);
	
	if(ptkr->pushSignal(_psig, _rconid, _flags | SameConnectorFlag)){
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
	const SockAddrPair &_rsap,
	DynamicPointer<Signal> &_psig,//the signal to be sent
	ConnectionUid *_pconid,
	uint32	_flags
){
	
	if(
		_rsap.family() != AddrInfo::Inet4 && 
		_rsap.family() != AddrInfo::Inet6
	) return -1;
	
	Mutex::Locker	lock(*mutex());
	
	if(_rsap.family() == AddrInfo::Inet4){
		
		Inet4SockAddrPair 					inaddr(_rsap);
		Session::Addr4PairT					baddr(&inaddr, inaddr.port());
		Data::SessionAddr4MapT::iterator	it(d.sessionaddr4map.find(&baddr));
		
		if(it != d.sessionaddr4map.end()){
		
			vdbgx(Dbg::ipc, "");
			
			ConnectionUid		conid(it->second);
			Data::UInt32PairT	&rtp(d.tkrvec[conid.id]);
			Mutex::Locker		lock2(this->mutex(rtp.first, rtp.second));
			Talker				*ptkr(static_cast<Talker*>(this->object(rtp.first, rtp.second)));
			
			cassert(conid.id < d.tkrvec.size());
			cassert(ptkr);
			
			if(ptkr->pushSignal(_psig, conid, _flags)){
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
			
			int16	tkrid(computeTalkerForNewSession());
			uint32	tkrpos;
			uint32	tkruid;
			
			if(tkrid >= 0){
				//the talker exists
				tkrpos = d.tkrvec[tkrid].first;
				tkruid = d.tkrvec[tkrid].second;
			}else{
				//create new talker
				tkrid = createNewTalker(tkrpos, tkruid);
				if(tkrid < 0){
					return BAD;
				}
			}
			
			Mutex::Locker		lock2(this->mutex(tkrpos, tkruid));
			Talker				*ptkr(static_cast<Talker*>(this->object(tkrpos, tkruid)));
			cassert(ptkr);
			Session				*pses(new Session(inaddr, d.keepalivetout));
			ConnectionUid		conid(tkrid);
			
			vdbgx(Dbg::ipc, "");
			ptkr->pushSession(pses, conid);
			d.sessionaddr4map[pses->baseAddr4()] = conid;
			
			ptkr->pushSignal(_psig, conid, _flags);
			
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
int16 Service::computeTalkerForNewSession(){
	++d.sessioncnt;
	if(d.sessioncnt % d.sespertkrcnt) return d.sessioncnt / d.sespertkrcnt;
	return -1;
}
//---------------------------------------------------------------------
int Service::acceptSession(Session *_pses){
	Mutex::Locker	lock(*mutex());
	{
		//TODO: try to think if the locking is ok!!!
		
		Data::SessionAddr4MapT::iterator	it(d.sessionaddr4map.find(_pses->baseAddr4()));
		
		if(it != d.sessionaddr4map.end()){
			//a connection still exists
			uint32			tkrpos(d.tkrvec[it->second.sessionidx].first);
			uint32			tkruid(d.tkrvec[it->second.sessionidx].second);
			Mutex::Locker	lock2(this->mutex(tkrpos, tkruid));
			Talker			*ptkr(static_cast<Talker*>(this->object(tkrpos, tkruid)));
			
			vdbgx(Dbg::ipc, "");
			
			ptkr->pushSession(_pses, it->second, true);
			
			if(ptkr->signal(fdt::S_RAISE)){
				Manager::the().raiseObject(*ptkr);
			}
			return OK;
		}
	}
	int16	tkrid(computeTalkerForNewSession());
	uint32	tkrpos,tkruid;
	
	if(tkrid >= 0){
		//the talker exists
		tkrpos = d.tkrvec[tkrid].first;
		tkruid = d.tkrvec[tkrid].second;
	}else{
		//create new talker
		tkrid = createNewTalker(tkrpos, tkruid);
		if(tkrid < 0) return BAD;
	}
	
	Mutex::Locker	lock2(this->mutex(tkrpos, tkruid));
	Talker			*ptkr(static_cast<Talker*>(this->object(tkrpos, tkruid)));
	cassert(ptkr);
	ConnectionUid	conid(tkrid, 0xffff, 0xffff);
	
	vdbgx(Dbg::ipc, "");
	
	ptkr->pushSession(_pses, conid);
	d.sessionaddr4map[_pses->baseAddr4()] = conid;
	
	if(ptkr->signal(fdt::S_RAISE)){
		Manager::the().raiseObject(*ptkr);
	}
	return OK;
}
//---------------------------------------------------------------------
void Service::connectSession(const Inet4SockAddrPair &_raddr){
	Mutex::Locker	lock(*mutex());
	int16			tkrid(computeTalkerForNewSession());
	uint32			tkrpos;
	uint32			tkruid;
	
	if(tkrid >= 0){
		//the talker exists
		tkrpos = d.tkrvec[tkrid].first;
		tkruid = d.tkrvec[tkrid].second;
	}else{
		//create new talker
		tkrid = createNewTalker(tkrpos, tkruid);
		if(tkrid < 0){
			return;
		}
	}
	
	Mutex::Locker		lock2(this->mutex(tkrpos, tkruid));
	Talker				*ptkr(static_cast<Talker*>(this->object(tkrpos, tkruid)));
	cassert(ptkr);
	Session				*pses(new Session(_raddr, d.keepalivetout));
	ConnectionUid		conid(tkrid);
	
	vdbgx(Dbg::ipc, "");
	ptkr->pushSession(pses, conid);
	d.sessionaddr4map[pses->baseAddr4()] = conid;
	
	if(ptkr->signal(fdt::S_RAISE)){
		Manager::the().raiseObject(*ptkr);
	}
}
//---------------------------------------------------------------------
void Service::disconnectTalkerSessions(Talker &_rtkr){
	Mutex::Locker	lock(*mutex());
	_rtkr.disconnectSessions();
}
//---------------------------------------------------------------------
void Service::disconnectSession(Session *_pses){
	d.sessionaddr4map.erase(_pses->baseAddr4());
}
//---------------------------------------------------------------------
int16 Service::createNewTalker(uint32 &_tkrpos, uint32 &_tkruid){
	
	if(d.tkrvec.size() > 30000) return BAD;
	
	int16			tkrid(d.tkrvec.size());
	SocketDevice	sd;
	
	d.firstaddr.port(d.firstaddr.port() + tkrid);
	
	sd.create(d.firstaddr.family(), AddrInfo::Datagram, 0);
	sd.bind(d.firstaddr);
	d.firstaddr.port(d.firstaddr.port() - tkrid);
	
	if(sd.ok()){
		
		Talker *ptkr(new Talker(sd, *this, tkrid));
		
		if(this->doInsert(*ptkr, this->index())){
			delete ptkr;
			return BAD;
		}
		
		d.tkrvec.push_back(Data::UInt32PairT((_tkrpos = ptkr->id()), (_tkruid = this->uid(*ptkr))));
		doPushTalkerInPool(ptkr);
	
	}else{
		return BAD;
	}
	return tkrid;
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
//---------------------------------------------------------------------
int Service::insertTalker(
	const AddrInfoIterator &_rai,
	const char *_node,
	const char *_svc
){	
	SocketDevice	sd;
	sd.create(_rai);
	sd.bind(_rai);
	
	if(!sd.ok()) return BAD;
	
	Mutex::Locker	lock(*mutex());
	cassert(!d.tkrvec.size());//only the first tkr must be inserted from outside
	Talker			*ptkr(new Talker(sd, *this, 0));
	
	if(this->doInsert(*ptkr, this->index())){
		delete ptkr;
		return BAD;
	}
	
	d.firstaddr = _rai;
	d.baseport = d.firstaddr.port();
	d.tkrvec.push_back(Data::UInt32PairT(ptkr->id(), this->uid(*ptkr)));
	//_rm.pushJob((fdt::udp::Talker*)ptkr);
	doPushTalkerInPool(ptkr);
	return OK;
}
//---------------------------------------------------------------------
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
//---------------------------------------------------------------------
int Service::removeTalker(Talker& _rtkr){
	this->remove(_rtkr);
	return OK;
}
//---------------------------------------------------------------------
int Service::removeConnection(Connection &_rcon){
//	this->remove(_rcon);
	return OK;
}
//---------------------------------------------------------------------
int Service::execute(ulong _sig, TimeSpec &_rtout){
	idbgx(Dbg::ipc, "serviceexec");
	if(signaled()){
		ulong sm;
		{
			Mutex::Locker	lock(*mutex());
			sm = grabSignalMask(1);
		}
		if(sm & fdt::S_KILL){
			idbgx(Dbg::ipc, "killing service "<<this->id());
			this->stop(true);
			Manager::the().removeService(this);
			return BAD;
		}
	}
	return NOK;
}

//=======	Buffer		=============================================
bool Buffer::check()const{
	cassert(bc >= 32);
	//TODO:
	if(this->pb){
		if(header().size() < sizeof(Header)) return false;
		if(header().size() > ReadCapacity) return false;
		return true;
	}
	return false;
}
//---------------------------------------------------------------------
/*static*/ char* Buffer::allocateDataForReading(){
	return Specific::popBuffer(Specific::capacityToId(ReadCapacity));
}
//---------------------------------------------------------------------
/*static*/ void Buffer::deallocateDataForReading(char *_buf){
	Specific::pushBuffer(_buf, Specific::capacityToId(ReadCapacity));
}
//---------------------------------------------------------------------
void Buffer::clear(){
	if(pb){
		Specific::pushBuffer(pb, Specific::capacityToId(bc));
		pb = NULL;
		bc = 0;
	}
}
//---------------------------------------------------------------------
Buffer::~Buffer(){
	clear();
}
//---------------------------------------------------------------------
void Buffer::optimize(uint16 _cp){
	const uint32	bufsz(this->bufferSize());
	const uint		id(Specific::sizeToId(bufsz));
	const uint		mid(Specific::capacityToId(_cp ? _cp : Buffer::capacityForReading()));
	if(mid > id){
		uint32 datasz = this->dataSize();//the size
		
		char *newbuf(Specific::popBuffer(id));
		memcpy(newbuf, this->buffer(), bufsz);//copy to the new buffer
		
		char *pb = this->release();
		Specific::pushBuffer(pb, mid);
		
		this->pb = newbuf;
		this->dl = datasz;
		this->bc = Specific::idToCapacity(id);
	}
}
//---------------------------------------------------------------------
std::ostream& operator<<(std::ostream &_ros, const Buffer &_rb){
	_ros<<"BUFFER ver = "<<(int)_rb.header().version;
	_ros<<" id = "<<_rb.header().id;
	_ros<<" retransmit = "<<_rb.header().retransid;
	_ros<<" type = ";
	switch(_rb.header().type){
		case Buffer::KeepAliveType: _ros<<"KeepAliveType";break;
		case Buffer::DataType: _ros<<"DataType";break;
		case Buffer::ConnectingType: _ros<<"ConnectingType";break;
		case Buffer::AcceptingType: _ros<<"AcceptingType";break;
		case Buffer::Unknown: _ros<<"Unknown";break;
		default: _ros<<"[INVALID TYPE]";
	}
	_ros<<" buffer_cp = "<<_rb.bc;
	_ros<<" datalen = "<<_rb.dl;
	_ros<<" updatescnt = "<<_rb.header().updatescnt;
	_ros<<" updates [";
	for(int i = 0; i < _rb.header().updatescnt; ++i){
		_ros<<_rb.update(i)<<',';
	}
	_ros<<']';
	return _ros;
	
}
}//namespace ipc
}//namespace foundation

