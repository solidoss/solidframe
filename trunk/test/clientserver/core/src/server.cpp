/* Implementation file server.cpp
	
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

#include <map>
#include <vector>

#include "system/debug.h"
#include "utility/polycontainer.h"
#include "utility/iostream.h"

#include "algorithm/serialization/binary.h"

#include "core/server.h"
#include "core/service.h"
#include "core/visitor.h"
#include "core/command.h"
#include "core/connection.h"


#include "clientserver/core/selectpool.h"
#include "clientserver/core/execpool.h"
#include "clientserver/core/storagemanager.h"
#include "clientserver/tcp/connectionselector.h"
#include "clientserver/tcp/connection.h"
#include "clientserver/tcp/listenerselector.h"
#include "clientserver/tcp/listener.h"
#include "clientserver/udp/talkerselector.h"
#include "clientserver/udp/talker.h"
#include "clientserver/core/objectselector.h"

#include "clientserver/ipc/ipcservice.h"


#include <iostream>
using namespace std;

namespace clientserver{
class ObjectSelector;
namespace tcp{
class ConnectionSelector;
class ListenerSelector;
}
namespace udp{
class TalkerSelector;
}
}

namespace cs=clientserver;


namespace test{

typedef clientserver::SelectPool<cs::ObjectSelector>			ObjSelPoolTp;
typedef clientserver::SelectPool<cs::tcp::ConnectionSelector>	ConSelPoolTp;
typedef clientserver::SelectPool<cs::tcp::ListenerSelector>		LisSelPoolTp;
typedef clientserver::SelectPool<cs::udp::TalkerSelector>		TkrSelPoolTp;
typedef std::vector<ConSelPoolTp*>								ConSelPoolVecTp;
typedef std::vector<ObjSelPoolTp*>								ObjSelPoolVecTp;

//======= StorageManager:==================================================

struct IStreamCommand: test::Command{
	IStreamCommand(StreamPtr<IStream> &_sptr):sptr(_sptr){}
	int execute(Connection &_pcon);
	StreamPtr<IStream>	sptr;
};

int IStreamCommand::execute(Connection &_rcon){
	return _rcon.receiveIStream(sptr);
}

struct OStreamCommand: test::Command{
	OStreamCommand(StreamPtr<OStream> &_sptr):sptr(_sptr){}
	int execute(Connection &_pcon);
	StreamPtr<OStream>	sptr;
};

int OStreamCommand::execute(Connection &_rcon){
	return _rcon.receiveOStream(sptr);
}

struct IOStreamCommand: test::Command{
	IOStreamCommand(StreamPtr<IOStream> &_sptr):sptr(_sptr){}
	int execute(Connection &_pcon);
	StreamPtr<IOStream>	sptr;
};

int IOStreamCommand::execute(Connection &_rcon){
	return _rcon.receiveIOStream(sptr);
}


class StorageManager: public cs::StorageManager{
public:
	StorageManager(uint32 _maxfcnt = 256):cs::StorageManager(_maxfcnt){}
protected:
	/*virtual*/ void sendStream(StreamPtr<IStream> &_sptr, uint32 _idx, uint32 _uid);
	/*virtual*/ void sendStream(StreamPtr<OStream> &_sptr, uint32 _idx, uint32 _uid);
	/*virtual*/ void sendStream(StreamPtr<IOStream> &_sptr, uint32 _idx, uint32 _uid);
};
void StorageManager::sendStream(StreamPtr<IStream> &_sptr, uint32 _idx, uint32 _uid){
	cs::CmdPtr<cs::Command>	cp(new IStreamCommand(_sptr));
	Server::the().signalObject(_idx, _uid, cp);
}
void StorageManager::sendStream(StreamPtr<OStream> &_sptr, uint32 _idx, uint32 _uid){
	cs::CmdPtr<cs::Command>	cp(new OStreamCommand(_sptr));
	Server::the().signalObject(_idx, _uid, cp);
}
void StorageManager::sendStream(StreamPtr<IOStream> &_sptr, uint32 _idx, uint32 _uid){
	cs::CmdPtr<cs::Command>	cp(new IOStreamCommand(_sptr));
	Server::the().signalObject(_idx, _uid, cp);
}

//======= IpcService ======================================================
class IpcService: public cs::ipc::Service{
public:
	IpcService(serialization::bin::RTTIMapper& _rm):cs::ipc::Service(_rm){}
protected:
	/*virtual*/void pushTalkerInPool(clientserver::Server &_rs, clientserver::udp::Talker *_ptkr);
};

//=========================================================================

struct PoolContainer: public CONSTPOLY4(LisSelPoolTp*, ConSelPoolVecTp, TkrSelPoolTp*, ObjSelPoolVecTp) {
	PoolContainer(Server &_rs);
	~PoolContainer();
};

struct ExtraObjPtr: cs::ObjPtr<cs::Object>{
	ExtraObjPtr(cs::Object *_po);
	~ExtraObjPtr();
	ExtraObjPtr& operator=(cs::Object *_pobj);
};


struct Server::Data{
	typedef std::vector<ExtraObjPtr> ExtraObjectVector;
	typedef std::map<const char*, int, StrLess> ServiceIdxMap;
	Data(Server &_rs):pools(_rs){}
	ExtraObjectVector				eovec;
	ServiceIdxMap					servicemap;
	cs::ipc::Service				*pcs;
	PoolContainer					pools;
	serialization::bin::RTTIMapper	binmapper;
};

template <>
void Server::pushJob(cs::tcp::Listener *_pj, int _pos){
	d.pools.get<LisSelPoolTp*>()->push(cs::ObjPtr<cs::tcp::Listener>(_pj));
}
template <>
void Server::pushJob(cs::udp::Talker *_pj, int _pos){
	d.pools.get<TkrSelPoolTp*>()->push(cs::ObjPtr<cs::udp::Talker>(_pj));
}
template <>
void Server::pushJob(cs::tcp::Connection *_pj, int _pos){
	d.pools.get<ConSelPoolVecTp>()[_pos]->push(cs::ObjPtr<cs::tcp::Connection>(_pj));
}
template <>
void Server::pushJob(cs::Object *_pj, int _pos){
	d.pools.get<ObjSelPoolVecTp>()[_pos]->push(cs::ObjPtr<cs::Object>(_pj));
}

//=====================================================================
void IpcService::pushTalkerInPool(clientserver::Server &_rs, clientserver::udp::Talker *_ptkr){
	static_cast<Server&>(_rs).pushJob(_ptkr);
}
//=====================================================================

PoolContainer::PoolContainer(Server &_rs){
	if(true){	
		ConSelPoolTp	*pp = new ConSelPoolTp(	_rs, 
												10,		//max thread cnt
												50		//max connections per selector
												);		//at most 10 * 4 * 1024 connections
		this->set<ConSelPoolVecTp>().push_back(pp);
		pp->start(1);//start with one worker
	}
	if(true){
		LisSelPoolTp	*pp = new LisSelPoolTp(	_rs, 
												2, 	//max thread cnt
												128	//max listeners per selector 
												);	//at most 128*2 = 256 listeners
		this->set<LisSelPoolTp*>() = pp;
		pp->start(1);//start with one worker
	}
	if(true){
		TkrSelPoolTp	*pp = new TkrSelPoolTp(	_rs, 
												2, 	//max thread cnt
												128	//max listeners per selector 
												);	//at most 128*2 = 256 listeners
		this->set<TkrSelPoolTp*>() = pp;
		pp->start(1);//start with one worker
	}
	if(true){
		ObjSelPoolTp	*pp = new ObjSelPoolTp(	_rs, 
												10,		//max thread cnt
												1024 * 4//max objects per selector
												);		//at most 10 * 4 * 1024 connections
		this->set<ObjSelPoolVecTp>().push_back(pp);
		pp->start(1);//start with one worker
	}
}

PoolContainer::~PoolContainer(){
	this->get<ConSelPoolVecTp>().front()->stop();
	delete this->get<ConSelPoolVecTp>().front();
	this->get<LisSelPoolTp*>()->stop();
	this->get<TkrSelPoolTp*>()->stop();
	this->get<ObjSelPoolVecTp>().front()->stop();
	delete this->get<ObjSelPoolVecTp>().front();
}

int Command::execute(Connection &){
	assert(false);
	return BAD;
}

int Command::execute(Listener &){
	assert(false);
	return BAD;
}

int Connection::receiveIStream(
	StreamPtr<IStream> &,
	const FromPairTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	assert(false);
	return BAD;
}

int Connection::receiveOStream(
	StreamPtr<OStream> &,
	const FromPairTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	assert(false);
	return BAD;
}

int Connection::receiveIOStream(
	StreamPtr<IOStream> &, 
	const FromPairTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	assert(false);
	return BAD;
}

int Connection::receiveString(
	const String &_str, 
	const FromPairTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	assert(false);
	return BAD;
}

ServiceMap& getServiceMap(){
	static ServiceMap sm;
	return sm;
}

void registerService(ServiceCreator _psc, const char* _pname){
	idbg("registering "<<_pname);
	cout<<"registering "<<_pname<<endl;
	getServiceMap()[_pname] = _psc;
}

ExtraObjPtr::ExtraObjPtr(cs::Object *_po):cs::ObjPtr<cs::Object>(_po){}

ExtraObjPtr::~ExtraObjPtr(){
	this->destroy(this->release());
}
ExtraObjPtr& ExtraObjPtr::operator=(cs::Object *_pobj){
	ptr(_pobj);
	return *this;
}

/*
NOTE:
	It should be safe to give reference to 'this' to Data constructor, because
	it will be used in the SelectPool(s) constructor to call registerActiveSet(*this),
	which is defined in the base class of the server which should be initialized first
*/

Server::Server():d(*(new Data(*this))){
	//ppools = new PoolContainer(*this);
	if(true){
		
		//d.eovec.push_back(ExtraObjPtr(new StorageManager(50)));
		this->storage(new StorageManager(50));
		cs::Server::insertObject(&storage());
		this->pushJob((cs::Object*)&storage());
	}
	if(true){
		this->ipc(new IpcService(d.binmapper));
		int pos = cs::Server::insertService(&this->ipc());
		if(pos < 0){
			idbg("unable to register service: "<<"ipc");
		}else{
			idbg("service "<<"ipc"<<" registered on pos "<<pos);
			this->pushJob((cs::Object*)&this->ipc());
			//do not map the ipc!!!
			//d.servicemap["ipc"] = pos;
		}
	}
}

Server::~Server(){
	service(0).stop(*this, true);//Stop all services
	delete &d;
}

serialization::bin::RTTIMapper &Server::binMapper(){
	return d.binmapper;
}

int Server::start(const char *_which){
	if(_which){
		Data::ServiceIdxMap::iterator it(d.servicemap.find(_which));
		if(it != d.servicemap.end()){
			service(it->second).start(*this);
		}
	}else{
		for(Data::ServiceIdxMap::iterator it(d.servicemap.begin());it != d.servicemap.end(); ++it){
			service(it->second).start(*this);
		}
	}
	return OK;
}
int Server::stop(const char *_which){
	if(_which){
		Data::ServiceIdxMap::iterator it(d.servicemap.find(_which));
		if(it != d.servicemap.end()){
			service(it->second).stop(*this);
		}
	}else{
		for(Data::ServiceIdxMap::iterator it(d.servicemap.begin());it != d.servicemap.end(); ++it){
			service(it->second).stop(*this, false);//do not wait for stopping
		}
		//storage().stop(*this, false);
	}
	return OK;
}

int Server::insertService(const char* _nm, Service* _psrvc){
	if(_psrvc != NULL){
		int pos = cs::Server::insertService(_psrvc);
		if(pos < 0){
			idbg("unable to register service: "<<_nm);
		}else{
			idbg("service "<<_nm<<" registered on pos "<<pos);
			//this->get<ObjSelPoolVecTp>()[0]->push(cs::ObjPtr<cs::Object>(_psrvc));
			this->pushJob((cs::Object*)_psrvc);
			d.servicemap[_nm] = pos;
			return OK;
		}
	}else{
		idbg("service NULL");
	}
	return BAD;
}
void Server::removeService(Service *_psrvc){
	cs::Server::removeService(_psrvc);
}

int Server::insertListener(const char* _nm, const AddrInfoIterator &_rai){
	Data::ServiceIdxMap::iterator it(d.servicemap.find(_nm));
	if(it != d.servicemap.end()){
		test::Service &ts = static_cast<test::Service&>(this->service(it->second));
		return ts.insertListener(*this, _rai);
	}else{
		idbg("service not found "<<d.servicemap.size());
		return BAD;
	}
}

int Server::insertTalker(const char* _nm, const AddrInfoIterator &_rai, const char*_node, const char *_srv){
	Data::ServiceIdxMap::iterator it(d.servicemap.find(_nm));
	if(it != d.servicemap.end()){
		test::Service &ts = static_cast<test::Service&>(this->service(it->second));
		return ts.insertTalker(*this, _rai, _node, _srv);
	}else{
		idbg("service not found "<<d.servicemap.size());
		return BAD;
	}
}

int Server::insertConnection(const char* _nm, const AddrInfoIterator &_rai, const char*_node, const char *_srv){
	Data::ServiceIdxMap::iterator it(d.servicemap.find(_nm));
	if(it != d.servicemap.end()){
		test::Service &ts = static_cast<test::Service&>(this->service(it->second));
		return ts.insertConnection(*this, _rai, _node, _srv);
	}else{
		idbg("service not found "<<d.servicemap.size());
		return BAD;
	}
}

int Server::visitService(const char* _nm, Visitor &_rov){
	Data::ServiceIdxMap::iterator it(d.servicemap.find(_nm));
	if(it != d.servicemap.end()){
		//test::Service *ts = static_cast<test::Service*>(this->service(it->second));
		//TODO:
		//ts->visit(_rov);
		return OK;
	}else{
		idbg("service not found "<<d.servicemap.size());
		return BAD;
	}
}

}//namespace test
