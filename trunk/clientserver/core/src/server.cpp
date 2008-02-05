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

#include <vector>

#include "system/cassert.h"
#include "system/debug.h"
#include "system/thread.h"
#include "system/specific.h"

#include "core/server.h"
#include "core/service.h"
#include "core/object.h"
#include "core/activeset.h"
#include "core/filemanager.h"
#include "ipc/ipcservice.h"

/*
Server &ServerThread::server(){
	return *reinterpret_cast<Server*>(ThreadBase::specific(0));
}
void ServerThread::initSpecific(){
	ThreadBase::specific((void*)NULL);
}
void ServerThread::destroySpecific(){
	ThreadBase::specific(0, NULL);
}
void ServerThread::server(Server *_psrv){
	ThreadBase::specific(0,_psrv);
}
*/

static const unsigned serverspecificid = Thread::specificId();

namespace clientserver{

class DummySet: public ActiveSet{
public:
	DummySet(){}
	~DummySet(){}
	void raise(uint _thid){}
	void raise(uint _thid, uint _objid){}
	void poolid(uint){}
};

struct DummyObject: Object{
};
//a service which will hold the other services
//TODO: make it inheritable by users. 
// for service group signalling.
// also on the first position should be either
// the server or the service itself == the root for signalling.
class ServiceContainer: public Service{
public:
	ServiceContainer():Service(0, 1, 16), pdo(new DummyObject){
		Service::mutex(new Mutex);
		insert(pdo);
	}
	~ServiceContainer(){
		delete Service::mutex();
		Service::mutex((Mutex*)NULL);
	}
	//int insert(Service *_ps);
	int insert(Object *_po);
	//void remove(Service *_ps);
	void remove(Object *_po);
	void clearDummy();
private:
	DummyObject *pdo;
};


struct Server::ServicePtr: ObjPtr<Service>{
	ServicePtr(Service *_ps = NULL);
	~ServicePtr();
	ServicePtr& operator=(Service *_pobj);
};


Server::ServicePtr::ServicePtr(Service *_ps):ObjPtr<Service>(_ps){}

Server::ServicePtr::~ServicePtr(){
	delete this->release();
}
Server::ServicePtr& Server::ServicePtr::operator=(Service *_pobj){
	ptr(_pobj);
	return *this;
}

struct Server::ServiceVector: std::vector<ServicePtr>{};
struct Server::ActiveSetVector:std::vector<ActiveSet*>{};

// int ServiceContainer::insert(Service *_ps){
// 	return Service::insert(*_ps, 0);
// }

int ServiceContainer::insert(Object *_po){
	return Service::insert(*_po, 0);
}

// void ServiceContainer::remove(Service *_ps){
// 	Service::remove(*_ps);
// }

void ServiceContainer::remove(Object *_po){
	Service::remove(*_po);
}

void ServiceContainer::clearDummy(){
	if(pdo){
		Service::remove(*pdo);
		delete pdo;
		pdo = NULL;
	}
}

Server::Server(FileManager *_pfm, ipc::Service *_pipcs):servicev(*(new ServiceVector)),asv(*(new ActiveSetVector)), pfm(_pfm), pipcs(_pipcs){
	registerActiveSet(*(new DummySet));
	servicev.push_back(ServicePtr(new ServiceContainer));
}

Server::~Server(){
	//delete servicev.front();
	servicev.clear();
	delete asv.front();
	delete &asv;
	delete &servicev;
	delete pfm.release();
	//delete pipcs;
}

ServiceContainer & Server::serviceContainer(){
	return static_cast<ServiceContainer&>(*servicev.front());
}

Service& Server::service(uint _i)const{
	return *servicev[_i];
}

void Server::stop(bool _wait){
	serviceContainer().clearDummy();
	serviceContainer().stop(*this, _wait);
}

Server& Server::the(){
	return *reinterpret_cast<Server*>(Thread::specific(serverspecificid));
}

// uint Server::serviceId(const Service &_rs)const{
// 	return _rs.index();//the first service must be the ServiceContainer
// }

void Server::fileManager(FileManager *_pfm){
	cassert(!pfm);
	pfm = _pfm;
}
void Server::ipc(ipc::Service *_pipcs){
	pipcs = _pipcs;
}

void Server::removeFileManager(){
	removeObject(&fileManager());
	//psm = NULL;
}

uint Server::registerActiveSet(ActiveSet &_ras){
	_ras.poolid(asv.size());
	asv.push_back(&_ras);
	return asv.size() - 1;
}
int Server::insertService(Service *_ps){
	//servicev.push_back(ServicePtr(_ps));
	//cassert(_ps->state() == Service::Stopped);
	serviceContainer().insert(_ps);
	uint idx = _ps->index();//serviceId(*_ps);
	if(idx >= servicev.size()){
		servicev.resize(idx + 1);
	}
	servicev[idx] = ServicePtr(_ps);
	return idx;
}

void Server::insertObject(Object *_po){
	serviceContainer().insert(_po);
}

void Server::removeService(Service *_ps){
	serviceContainer().remove(_ps);
}

void Server::removeObject(Object *_po){
	serviceContainer().remove(_po);
}

int Server::signalObject(ulong _fullid, ulong _uid, ulong _sigmask){
	cassert(Object::computeServiceId(_fullid) < servicev.size());
	return servicev[Object::computeServiceId(_fullid)]->signal(
		_fullid,_uid,
		*this,
		_sigmask);
}
		
int Server::signalObject(Object &_robj, ulong _sigmask){
	cassert(_robj.serviceid() < servicev.size());
	return servicev[_robj.serviceid()]->signal(_robj, *this, _sigmask);
}

int Server::signalObject(ulong _fullid, ulong _uid, CmdPtr<Command> &_cmd){
	cassert(Object::computeServiceId(_fullid) < servicev.size());
	return servicev[Object::computeServiceId(_fullid)]->signal(
		_fullid,_uid,
		*this,
		_cmd);
}

int Server::signalObject(Object &_robj, CmdPtr<Command> &_cmd){
	cassert(_robj.serviceid() < servicev.size());
	return servicev[_robj.serviceid()]->signal(_robj, *this, _cmd);
}

Mutex& Server::mutex(Object &_robj)const{
	return servicev[_robj.serviceid()]->mutex(_robj);
}

ulong  Server::uid(Object &_robj)const{
	return servicev[_robj.serviceid()]->uid(_robj);
}

void Server::raiseObject(Object &_robj){
	uint thrid,thrpos;
	_robj.getThread(thrid, thrpos);
	idbg("raise thrid "<<thrid<<" thrpos "<<thrpos<<" objid "<<_robj.id());
	asv[thrid>>16]->raise(thrid & 0xffff, thrpos);
}

void Server::prepareThread(){
	Thread::specific(serverspecificid, this);
	//GlobalMapper::prepareThread(globalMapper());
	Specific::prepareThread();
}
void Server::unprepareThread(){
	Thread::specific(serverspecificid, NULL);
	//GlobalMapper::unprepareThread();
	//Specific::unprepareThread();
}

SpecificMapper*  Server::specificMapper(){
//TODO return a function static default local mapper
	return NULL;
}
GlobalMapper* Server::globalMapper(){
//TODO return a function static default local mapper
	return NULL;
}

unsigned Server::serviceCount()const{
	return servicev.size();
}

int Server::insertIpcTalker(
	const AddrInfoIterator &_rai,
	const char*_node,
	const char *_srv
){
	cassert(pipcs);
	return ipc().insertTalker(*this, _rai, _node, _srv);
}

}//namespace clientserver
