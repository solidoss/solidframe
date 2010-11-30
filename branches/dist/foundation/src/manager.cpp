/* Implementation file manager.cpp
	
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
#include <deque>

#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/specific.hpp"
#include "system/exception.hpp"

#include "foundation/manager.hpp"
#include "foundation/service.hpp"
#include "foundation/object.hpp"
#include "foundation/schedulerbase.hpp"
#include "foundation/selectorbase.hpp"
#include "foundation/requestuid.hpp"
#include "foundation/signal.hpp"

namespace foundation{

typedef ObjectPointer<Service>	ServicePointerT;
typedef ObjectPointer<Object>	ObjectPointerT;

struct DummySelector: SelectorBase{
	void raise(uint32 _objidx){}
};

//---------------------------------------------------------
struct Manager::Data{
	struct ObjectTypeStub{
		ObjectTypeStub():pschcbk(NULL), schidx(0), objidx(-1){}
		ScheduleCbkT	pschcbk;
		uint			schidx;
		IndexT			objidx;
	};
	
	typedef std::vector<SchedulerBase*>			SchedulerVectorT;
	
	struct SchedulerTypeStub{
		SchedulerVectorT	schvec;
	};
	
	typedef std::pair<ServicePointerT, uint>	ServicePairT;
	typedef std::pair<ObjectPointerT, uint>		ObjectPairT;
	
	typedef std::vector<ServicePairT>			ServiceVectorT;
	typedef std::vector<ObjectPairT>			ObjectVectorT;
	typedef std::vector<SelectorBase*>			SelectorVectorT;
	typedef std::vector<ObjectTypeStub>			ObjectTypeStubVectorT;
	typedef std::vector<SchedulerTypeStub>		SchedulerTypeStubVectorT;
	
	Data(
		uint _objtpcnt,
		uint _schtpcnt,
		uint _schcnt,
		uint _selcnt
	);
	Service& masterService();
	
	IndexT						startdynamicidx;
	IndexT						currentdynamicidx;
	uint						currentservicecount;
	uint						currentobjectcount;
	uint						currentselectoridx;
	uint						currentschedulertypeid;
	uint						currentobjecttypeid;
	const uint					objtpcnt;
	const uint					schtpcnt;
	const uint					schcnt;
	const uint					selcnt;
	ServiceVectorT				svcvec;
	ObjectVectorT				objvec;
	SelectorVectorT				selvec;
	SchedulerTypeStubVectorT	schtpvec;
	ObjectTypeStubVectorT		objtpvec;
	Mutex						mtx;
	DummySelector				dummysel;
};
//---------------------------------------------------------
Manager::Data::Data(
	uint _objtpcnt,
	uint _schtpcnt,
	uint _schcnt,
	uint _selcnt
)	:startdynamicidx(0), currentdynamicidx(0), currentservicecount(0),
	currentobjectcount(0), currentselectoridx(0),
	currentschedulertypeid(0), currentobjecttypeid(0),
	objtpcnt(_objtpcnt), schtpcnt(_schtpcnt), schcnt(_schcnt), selcnt(_selcnt){
}
//---------------------------------------------------------
Service& Manager::Data::masterService(){
	return *svcvec[0].first;
}
//---------------------------------------------------------

#ifdef NSINGLETON_MANAGER
static const unsigned specificPosition(){
	//TODO: staticproblem
	static const unsigned thrspecpos = Thread::specificId();
	return thrspecpos;
}
Manager& Manager::the(){
	return *reinterpret_cast<Manager*>(Thread::specific(specificPosition()));
}
#else
namespace{
	Manager *globalpm(NULL);
}
/*static*/ Manager& Manager::the(){
	return *globalpm;
}
#endif
//---------------------------------------------------------
Manager::Manager(
	const IndexT &_startdynamicidx,
	uint _objtpcnt,
	uint _schtpcnt,
	uint _schcnt,
	uint _selcnt
):d(*(new Data(_objtpcnt, _schtpcnt, _schcnt, _selcnt))){
#ifndef NSINGLETON_MANAGER
	globalpm = this;
#endif
	d.startdynamicidx = _startdynamicidx;
	if(d.startdynamicidx > max_service_count()){
		d.startdynamicidx = max_service_count();
	}
	if(d.startdynamicidx == 0){
		d.startdynamicidx = 1;
	}
	d.currentdynamicidx = d.startdynamicidx;
	d.selvec.push_back(&d.dummysel);
	d.svcvec.push_back(Data::ServicePairT());
	d.svcvec.back().first = new Service;
	d.svcvec.back().second = Object::staticTypeId();
	//TODO: refactor
	d.svcvec.resize(max_service_count());
	d.objvec.resize(max_service_count());
	d.objtpvec.resize(d.objtpcnt);
	d.schtpvec.resize(d.schtpcnt);
	d.selvec.resize(d.selcnt);
}
//---------------------------------------------------------
/*virtual*/ Manager::~Manager(){
	stop(true);
	//stop all schedulers
	for(
		Data::SchedulerTypeStubVectorT::const_iterator it(d.schtpvec.begin());
		it != d.schtpvec.end();
		++it
	){
		if(it->schvec.empty())continue;
		for(
			Data::SchedulerVectorT::const_iterator sit(it->schvec.begin());
			sit != it->schvec.end();
			++sit
		){
			(*sit)->stop(false);
		}
	}
	for(
		Data::SchedulerTypeStubVectorT::const_iterator it(d.schtpvec.begin());
		it != d.schtpvec.end();
		++it
	){
		if(it->schvec.empty())continue;
		for(
			Data::SchedulerVectorT::const_iterator sit(it->schvec.begin());
			sit != it->schvec.end();
			++sit
		){
			(*sit)->stop(true);
			delete *sit;
		}
	}
	d.selvec[0] = NULL;
	for(Data::SelectorVectorT::const_iterator it(d.selvec.begin()); it != d.selvec.end(); ++it){
		cassert(*it == NULL);
	}
	delete &d;
}
//---------------------------------------------------------
void Manager::start(){
	Mutex::Locker	lock(d.mtx);
	if(Object::staticTypeId() < d.objtpvec.size()){
		if(!d.objtpvec[Object::staticTypeId()].pschcbk){
			THROW_EXCEPTION("At least one object selector must be registered");
		}
	}
	for(
		Data::SchedulerTypeStubVectorT::const_iterator it(d.schtpvec.begin());
		it != d.schtpvec.end();
		++it
	){
		if(it->schvec.empty())continue;
		for(
			Data::SchedulerVectorT::const_iterator sit(it->schvec.begin());
			sit != it->schvec.end();
			++sit
		){
			(*sit)->start();
		}
	}
	for(Data::ServiceVectorT::const_iterator it(d.svcvec.begin()); it != d.svcvec.end(); ++it){
		if(it->first){
			it->first->start();
			Data::ObjectTypeStub &rts(d.objtpvec[it->second]);
			(*rts.pschcbk)(rts.schidx, it->first.ptr());
		}
	}
	for(Data::ObjectVectorT::const_iterator it(d.objvec.begin()); it != d.objvec.end(); ++it){
		if(it->first){
			Data::ObjectTypeStub &rts(d.objtpvec[it->second]);
			(*rts.pschcbk)(rts.schidx, it->first.ptr());
		}
	}
}
//---------------------------------------------------------
void Manager::stop(bool _wait){
	Mutex::Locker	lock(d.mtx);
	for(Data::ServiceVectorT::const_iterator it(d.svcvec.begin()); it != d.svcvec.end(); ++it){
		if(it->first){
			it->first->stop(false);
		}
	}
	
	if(!_wait) return;
	
	for(Data::ServiceVectorT::const_iterator it(d.svcvec.begin()); it != d.svcvec.end(); ++it){
		if(it->first){
			it->first->stop(true);
		}
	}
}
//---------------------------------------------------------
bool Manager::signal(ulong _sm){
	bool b = false;
	for(Data::ServiceVectorT::const_iterator it(d.svcvec.begin()); it != d.svcvec.end(); ++it){
		if(it->first){
			b = b || it->first->signal(_sm);
		}
	}
	return b;
}
//---------------------------------------------------------
bool Manager::signal(ulong _sm, const ObjectUidT &_ruid){
	const IndexT	svcidx(compute_service_id(_ruid.first));
	Service			*ps(servicePointer(svcidx));
	if(ps){
		return ps->signal(_sm, _ruid);
	}
	return false;
}
//---------------------------------------------------------
bool Manager::signal(ulong _sm, IndexT _fullid, uint32 _uid){
	const IndexT	svcidx(compute_service_id(_fullid));
	Service			*ps(servicePointer(svcidx));
	if(ps){
		return ps->signal(_sm, _fullid, _uid);
	}
	return false;
}
//---------------------------------------------------------
// bool Manager::signal(ulong _sm, const Object &_robj){
// }
//---------------------------------------------------------
bool Manager::signal(DynamicSharedPointer<Signal> &_rsig){
	bool b(false);
	for(Data::ServiceVectorT::const_iterator it(d.svcvec.begin()); it != d.svcvec.end(); ++it){
		if(it->first){
			DynamicPointer<Signal>	sp(_rsig);
			b = b || it->first->signal(sp);
		}
	}
	return b;
}
//---------------------------------------------------------
bool Manager::signal(DynamicPointer<Signal> &_rsig, const ObjectUidT &_ruid){
	const IndexT	svcidx(compute_service_id(_ruid.first));
	Service			*ps(servicePointer(svcidx));
	if(ps){
		return ps->signal(_rsig, _ruid);
	}
	return false;
}
//---------------------------------------------------------
bool Manager::signal(DynamicPointer<Signal> &_rsig, IndexT _fullid, uint32 _uid){
	const IndexT	svcidx(compute_service_id(_fullid));
	Service			*ps(servicePointer(svcidx));
	if(ps){
		return ps->signal(_rsig, _fullid, _uid);
	}
	return false;
}
//---------------------------------------------------------
// bool Manager::signal(DynamicPointer<Signal> &_rsig, const Object &_robj){
// }
//---------------------------------------------------------
void Manager::raiseObject(const Object &_robj){
	d.selvec[_robj.thrid]->raise(_robj.thrpos);
}
//---------------------------------------------------------
Mutex& Manager::mutex(const Object &_robj)const{
	return service(_robj.serviceId()).mutex(_robj);
}
//---------------------------------------------------------
uint32  Manager::uid(const Object &_robj)const{
	return service(_robj.serviceId()).uid(_robj.index());
}
//---------------------------------------------------------
Service& Manager::service(const IndexT &_i)const{
	cassert(_i < d.svcvec.size());
	return *d.svcvec[_i].first;
}
//---------------------------------------------------------
Object& Manager::object(const IndexT &_i)const{
	cassert(_i < d.objvec.size());
	return *d.objvec[_i].first;
}
//---------------------------------------------------------
Service* Manager::servicePointer(const IndexT &_ridx)const{
	if(_ridx < d.svcvec.size()){
		Data::ServicePairT &rsp(d.svcvec[_ridx]);
		return rsp.first.ptr();
	}
	return NULL;
}
//---------------------------------------------------------
Object* Manager::objectPointer(const IndexT &_ridx)const{
	if(_ridx < d.svcvec.size()){
		Data::ObjectPairT &rop(d.objvec[_ridx]);
		return rop.first.ptr();
	}
	return NULL;
}
//---------------------------------------------------------
/*virtual*/ SpecificMapper*  Manager::specificMapper(){
	//TODO
	return NULL;
}
//---------------------------------------------------------

/*virtual*/ GlobalMapper* Manager::globalMapper(){
	//TODO
	return NULL;
}
//---------------------------------------------------------
/*virtual*/ void Manager::doPrepareThread(){
}
//---------------------------------------------------------
/*virtual*/ void Manager::doUnprepareThread(){
}
//---------------------------------------------------------
unsigned Manager::serviceCount()const{
	return d.currentservicecount;
}
//---------------------------------------------------------
void Manager::prepareThread(SelectorBase *_ps){
	prepareThis();
	//GlobalMapper::prepareThread(globalMapper());
	Specific::prepareThread();
	requestuidptr.prepareThread();
	if(_ps){
		cassert(d.currentselectoridx < d.selvec.size());
		_ps->selid = d.currentselectoridx;
		d.selvec[_ps->selid] = _ps;
		++d.currentselectoridx;
	}
	doPrepareThread();
}
//---------------------------------------------------------
void Manager::unprepareThread(SelectorBase *_ps){
	unprepareThis();
	if(_ps){
		d.selvec[_ps->selid] = NULL;
	}
	doUnprepareThread();
}
//---------------------------------------------------------
void Manager::prepareThis(){
#ifdef NSINGLETON_MANAGER
	Thread::specific(specificPosition(), this);
#endif	
}
//---------------------------------------------------------
void Manager::unprepareThis(){
#ifdef NSINGLETON_MANAGER
	Thread::specific(specificPosition(), NULL);
#endif
}
//---------------------------------------------------------
uint Manager::newSchedulerTypeId(){
	Mutex::Locker	lock(d.mtx);
	++d.currentschedulertypeid;
	return d.currentschedulertypeid;
}
//---------------------------------------------------------
uint Manager::newServiceTypeId(){
	return newObjectTypeId();
}
//---------------------------------------------------------
uint Manager::newObjectTypeId(){
	Mutex::Locker	lock(d.mtx);
	++d.currentobjecttypeid;
	return d.currentobjecttypeid;
}
//---------------------------------------------------------
uint Manager::doRegisterScheduler(
	SchedulerBase *_ps,
	uint _typeid,
	ScheduleCbkT _pschcbk,
	uint _objtypeid
 ){
	Mutex::Locker	lock(d.mtx);
	if(_typeid >= d.schtpcnt){
		THROW_EXCEPTION_EX("Scheduler dynamic type id too big", _typeid);
		return -1;
	}
	
	Data::SchedulerTypeStub	&rstp(d.schtpvec[_typeid]);
	if(rstp.schvec.empty()){
		//TODO
		rstp.schvec.reserve(d.schcnt);
	}
	rstp.schvec.push_back(_ps);
	
	Data::ObjectTypeStub	&rotps(safe_at(d.objtpvec, _objtypeid));
	
	rotps.pschcbk = _pschcbk;
	rotps.schidx = rstp.schvec.size() - 1;
	rotps.objidx = -1;
	
	return rotps.schidx;
}
//---------------------------------------------------------
ObjectUidT Manager::doRegisterService(
	Service *_ps,
	const IndexT &_idx,
	ScheduleCbkT _pschcbk,
	uint _schedidx
){
	Mutex::Locker			lock(d.mtx);
	uint					tpid(_ps->dynamicTypeId());
	
	if(tpid >= d.objtpcnt){
		THROW_EXCEPTION_EX("Object dynamic type id too big", tpid);
		return invalid_uid();
	}
	
	Data::ObjectTypeStub	&rotps(safe_at(d.objtpvec, tpid));
	
	//rotps.pschcbk = _pschcbk;
	
	if(is_valid_index(_idx)){
		if(_idx >= d.startdynamicidx){
			return invalid_uid();
		}
		
		ObjectUidT			objuid(d.masterService().insert(_ps, d.currentdynamicidx));
		
		if(is_valid_uid(objuid)){
			Data::ServicePairT &robj(d.svcvec[_idx]);
			robj.first = _ps;
			robj.second = tpid;
			rotps.pschcbk = _pschcbk;
			rotps.schidx = _schedidx;
			if(rotps.objidx == -1){
				rotps.objidx = _idx;
			}
		}
		return objuid;
	}else{
		if(d.currentdynamicidx >= max_service_count()){
			return invalid_uid();
		}
		
		ObjectUidT			objuid(d.masterService().insert(_ps, d.currentdynamicidx));
		
		if(is_valid_uid(objuid)){
			Data::ServicePairT	&robj(d.svcvec[d.currentdynamicidx]);
			robj.first = _ps;
			robj.second = tpid;
			rotps.pschcbk = _pschcbk;
			rotps.schidx = _schedidx;
			if(rotps.objidx == -1){
				rotps.objidx = d.currentdynamicidx;
			}
			++d.currentdynamicidx;
		}
		return objuid;
	}
}
//---------------------------------------------------------
ObjectUidT Manager::doRegisterObject(
	Object *_po,
	const IndexT &_idx,
	ScheduleCbkT _pschcbk,
	uint _schedidx
){
	Mutex::Locker			lock(d.mtx);
	uint					tpid(_po->dynamicTypeId());
	
	if(tpid >= d.objtpcnt){
		THROW_EXCEPTION_EX("Object dynamic type id too big", tpid);
		return invalid_uid();
	}
	Data::ObjectTypeStub	&rotps(safe_at(d.objtpvec, tpid));
	
	//rotps.pschcbk = _pschcbk;
	
	if(is_valid_index(_idx)){
		if(_idx >= d.startdynamicidx){
			return invalid_uid();
		}
		
		ObjectUidT		objuid(d.masterService().insert(_po, d.currentdynamicidx));
		
		if(is_valid_uid(objuid)){
			Data::ObjectPairT &robj(d.objvec[_idx]);
			robj.first = _po;
			robj.second = tpid;
			rotps.pschcbk = _pschcbk;
			rotps.schidx = _schedidx;
			if(rotps.objidx == -1){
				rotps.objidx = _idx;
			}
		}
		return objuid;
	}else{
		if(d.currentdynamicidx >= max_service_count()){
			return invalid_uid();
		}
		
		ObjectUidT			objuid(d.masterService().insert(_po, d.currentdynamicidx));
		
		if(is_valid_uid(objuid)){
			Data::ObjectPairT	&robj(d.objvec[d.currentdynamicidx]);
			robj.first = _po;
			robj.second = tpid;
			rotps.pschcbk = _pschcbk;
			rotps.schidx = _schedidx;
			if(rotps.objidx == -1){
				rotps.objidx = d.currentdynamicidx;
			}
			++d.currentdynamicidx;
		}
		return objuid;
	}
}
//---------------------------------------------------------
SchedulerBase* Manager::doGetScheduler(uint _typeid, uint _idx)const{
	if(_typeid < d.schtpvec.size()){
		Data::SchedulerTypeStub &rsts(d.schtpvec[_typeid]);
		if(_idx < rsts.schvec.size()){
			return rsts.schvec[_idx];
		}
	}
	return NULL;
}
//---------------------------------------------------------
Object* Manager::doGetObject(uint _typeid, const IndexT &_ridx)const{
	if(_ridx < d.objvec.size()){
		Data::ObjectPairT	&rop(d.objvec[_ridx]);
		if(rop.second == _typeid){
			return rop.first.ptr();
		}
	}
	return NULL;
}
//---------------------------------------------------------
Service* Manager::doGetService(uint _typeid, const IndexT &_ridx)const{
	if(_ridx < d.svcvec.size()){
		Data::ServicePairT	&rsp(d.svcvec[_ridx]);
		if(rsp.second == _typeid){
			return rsp.first.ptr();
		}
		if(rsp.first.ptr() && rsp.first->isTypeDynamic(_typeid)){
			return rsp.first.ptr();
		}
	}
	return NULL;
}
//---------------------------------------------------------
Object* Manager::doGetObject(uint _typeid)const{
	if(_typeid < d.objtpvec.size()){
		Data::ObjectTypeStub	&rts(d.objtpvec[_typeid]);
		if(rts.objidx > 0){
			return d.objvec[rts.objidx].first.ptr();
		}
	}
	return NULL;
}
//---------------------------------------------------------
Service* Manager::doGetService(uint _typeid)const{
	if(_typeid < d.objtpvec.size()){
		Data::ObjectTypeStub	&rts(d.objtpvec[_typeid]);
		if(rts.objidx > 0){
			return d.svcvec[rts.objidx].first.ptr();
		}
	}
	return NULL;
}
//---------------------------------------------------------
Manager::ThisGuard::ThisGuard(Manager *_pm){
#ifdef NSINGLETON_MANAGER
	_pm->prepareThis();
#endif
}
Manager::ThisGuard::~ThisGuard(){
#ifdef NSINGLETON_MANAGER
	Manager::the().unprepareThis();
#endif
}
//---------------------------------------------------------
}//namespace foundation
