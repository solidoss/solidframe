/* Implementation file manager.cpp
	
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

#include <vector>

#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/specific.hpp"

#include "foundation/manager.hpp"
#include "foundation/service.hpp"
#include "foundation/object.hpp"
#include "foundation/activeset.hpp"
#include "foundation/requestuid.hpp"

/*
Manager &ManagerThread::server(){
	return *reinterpret_cast<Manager*>(ThreadBase::specific(0));
}
void ManagerThread::initSpecific(){
	ThreadBase::specific((void*)NULL);
}
void ManagerThread::destroySpecific(){
	ThreadBase::specific(0, NULL);
}
void ManagerThread::server(Manager *_psrv){
	ThreadBase::specific(0,_psrv);
}
*/
static const unsigned specificPosition(){
	//TODO: staticproblem
	static const unsigned thrspecpos = Thread::specificId();
	return thrspecpos;
}

namespace foundation{

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
// We use the dummy object to ensure that the index within
// ServiceContainer is the same as the index within the manager's
// service vector (i.e. pservice->index());
class ServiceContainer: public Service{
public:
	ServiceContainer():Service(0, 1, 5), pdo(new DummyObject){
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


struct Manager::ServicePtr: ObjectPointer<Service>{
	ServicePtr(Service *_ps = NULL);
	~ServicePtr();
	ServicePtr& operator=(Service *_pobj);
};


Manager::ServicePtr::ServicePtr(Service *_ps):ObjectPointer<Service>(_ps){}

Manager::ServicePtr::~ServicePtr(){
	delete this->release();
}
Manager::ServicePtr& Manager::ServicePtr::operator=(Service *_pobj){
	ptr(_pobj);
	return *this;
}


int ServiceContainer::insert(Object *_po){
	return Service::insert(*_po, 0);
}

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

//-------------------------------------------------------------------

ActiveSet::ActiveSet(){}
ActiveSet::~ActiveSet(){}

//-------------------------------------------------------------------
struct Manager::Data{
	typedef std::vector<ServicePtr> ServiceVectorT;
	typedef std::vector<ActiveSet*> ActiveSetVectorT;
	Data(){}
	ServiceVectorT					sv;
	ActiveSetVectorT				asv;
};
Manager::Manager():d(*(new Data())){
	registerActiveSet(*(new DummySet));
	d.sv.push_back(ServicePtr(new ServiceContainer));
}

Manager::~Manager(){
	d.sv.clear();
	delete d.asv.front();
	delete &d;
}

ServiceContainer & Manager::serviceContainer(){
	return static_cast<ServiceContainer&>(*d.sv.front());
}

Service& Manager::service(uint _i)const{
	return *d.sv[_i];
}

void Manager::stop(bool _wait){
	serviceContainer().clearDummy();
	serviceContainer().stop(_wait);
}

Manager& Manager::the(){
	return *reinterpret_cast<Manager*>(Thread::specific(specificPosition()));
}

uint Manager::registerActiveSet(ActiveSet &_ras){
	_ras.poolid(d.asv.size());
	d.asv.push_back(&_ras);
	return d.asv.size() - 1;
}
int Manager::insertService(Service *_ps){
	serviceContainer().insert(_ps);
	uint idx = _ps->index();//serviceId(*_ps);
	if(idx >= d.sv.size()){
		d.sv.resize(idx + 1);
	}
	d.sv[idx] = ServicePtr(_ps);
	return idx;
}

void Manager::insertObject(Object *_po){
	serviceContainer().insert(_po);
}

void Manager::removeService(Service *_ps){
	serviceContainer().remove(_ps);
}

void Manager::removeObject(Object *_po){
	serviceContainer().remove(_po);
}

int Manager::signalObject(IndexT _fullid, uint32 _uid, ulong _sigmask){
	cassert(Object::computeServiceId(_fullid) < d.sv.size());
	return d.sv[Object::computeServiceId(_fullid)]->signal(
		_fullid,_uid,
		_sigmask);
}
		
int Manager::signalObject(Object &_robj, ulong _sigmask){
	cassert(_robj.serviceid() < d.sv.size());
	return d.sv[_robj.serviceid()]->signal(_robj, _sigmask);
}

int Manager::signalObject(IndexT _fullid, uint32 _uid, DynamicPointer<Signal> &_rsig){
	cassert(Object::computeServiceId(_fullid) < d.sv.size());
	return d.sv[Object::computeServiceId(_fullid)]->signal(
		_fullid,_uid,
		_rsig);
}

int Manager::signalObject(Object &_robj, DynamicPointer<Signal> &_rsig){
	cassert(_robj.serviceid() < d.sv.size());
	return d.sv[_robj.serviceid()]->signal(_robj, _rsig);
}

Mutex& Manager::mutex(Object &_robj)const{
	return d.sv[_robj.serviceid()]->mutex(_robj);
}

uint32  Manager::uid(Object &_robj)const{
	return d.sv[_robj.serviceid()]->uid(_robj);
}

void Manager::raiseObject(Object &_robj){
	uint32 thrid,thrpos;
	_robj.getThread(thrid, thrpos);
	d.asv[thrid>>16]->raise(thrid & 0xffff, thrpos);
}

void Manager::prepareThis(){
	Thread::specific(specificPosition(), this);
}
void Manager::unprepareThis(){
	//Thread::specific(specificPosition(), NULL);
}
void Manager::prepareThread(){
	prepareThis();
	//GlobalMapper::prepareThread(globalMapper());
	Specific::prepareThread();
	requestuidptr.prepareThread();
}
void Manager::unprepareThread(){
	Thread::specific(specificPosition(), NULL);
	//GlobalMapper::unprepareThread();
	//Specific::unprepareThread();
}

SpecificMapper*  Manager::specificMapper(){
//TODO return a function static default local mapper
	return NULL;
}
GlobalMapper* Manager::globalMapper(){
//TODO return a function static default local mapper
	return NULL;
}

unsigned Manager::serviceCount()const{
	return d.sv.size();
}

}//namespace foundation
