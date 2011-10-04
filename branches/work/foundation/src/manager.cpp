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
#include "system/condition.hpp"
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
//---------------------------------------------------------
typedef ObjectPointer<Service>	ServicePointerT;
typedef ObjectPointer<Object>	ObjectPointerT;
//---------------------------------------------------------
struct DummySelector: SelectorBase{
	void raise(uint32 _objidx){}
};
//---------------------------------------------------------
class MasterService: public Dynamic<MasterService, Service>{
public:
	MasterService():BaseT(true, 0, 0, 8){}
	/*virtual*/ int doStop(ulong _evs, TimeSpec &_rtout){
		expectedCount(1);
		return Service::doStop(_evs, _rtout);
	}
};
//---------------------------------------------------------
struct Manager::Data{
	enum State {
		Stopped,
		Starting,
		Running,
		Stopping,
		MustStop,
	};
	struct ObjectTypeStub{
		ObjectTypeStub():objidx(invalid_uid().first){}
		IndexT			objidx;
	};
	
	typedef std::vector<SchedulerBase*>			SchedulerVectorT;
	
	struct SchedulerTypeStub{
		SchedulerVectorT	schvec;
	};
	struct ServiceStub{
		ServiceStub():tpid(-1), schcbk(NULL), schidx(-1){}
		ServicePointerT		ptr;
		uint				tpid;
		SchedCbkT			schcbk;
		uint				schidx;
	};
	struct ObjectStub{
		ObjectStub():tpid(-1), schcbk(NULL), schidx(-1){}
		ObjectPointerT		ptr;
		uint				tpid;
		SchedCbkT			schcbk;
		uint				schidx;
	};
	
	
	typedef std::vector<ServiceStub>			ServiceVectorT;
	typedef std::vector<ObjectStub>				ObjectVectorT;
	typedef std::vector<SelectorBase*>			SelectorVectorT;
	typedef std::vector<ObjectTypeStub>			ObjectTypeStubVectorT;
	typedef std::vector<SchedulerTypeStub>		SchedulerTypeStubVectorT;
	
	Data(
		uint _selcnt
	);
	Service& masterService();
	
	IndexT						startdynamicidx;
	IndexT						currentdynamicidx;
	uint						currentservicecount;
	uint						currentobjectcount;
	uint						currentselectoridx;
	mutable uint				currentschedulertypeid;
	uint						currentobjecttypeid;
	const uint					objtpcnt;
	const uint					schtpcnt;
	const uint					schcnt;
	const uint					selcnt;
	State						st;
	ServiceVectorT				svcvec;
	ObjectVectorT				objvec;
	SelectorVectorT				selvec;
	SchedulerTypeStubVectorT	schtpvec;
	ObjectTypeStubVectorT		objtpvec;
	Mutex						mtx;
	Mutex						svcmtx;
	Condition					cnd;
	DummySelector				dummysel;
	MasterService				mastersvc;
};
//---------------------------------------------------------
Manager::Data::Data(
	uint _selcnt
)	:startdynamicidx(0), currentdynamicidx(0), currentservicecount(0),
	currentobjectcount(0), currentselectoridx(0),
	currentschedulertypeid(0), currentobjecttypeid(0),
	objtpcnt(1024), schtpcnt(16), schcnt(32), selcnt(_selcnt), st(Stopped){
}
//---------------------------------------------------------
Service& Manager::Data::masterService(){
	return mastersvc;
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
	uint _selcnt
):d(*(new Data(_selcnt))){
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
	d.svcvec.push_back(Data::ServiceStub());
	d.svcvec.front().ptr = &d.mastersvc;
	d.svcvec.front().ptr->id(0, 0);
	d.svcvec.front().ptr->init(&d.svcmtx);
	d.svcvec.front().tpid = Object::staticTypeId();
	//d.svcvec.back().second = Object::staticTypeId();
	//TODO: refactor
	d.svcvec.resize(max_service_count());
	d.objvec.resize(max_service_count());
	d.objtpvec.reserve(d.objtpcnt);
	d.schtpvec.reserve(d.schtpcnt);
	d.selvec.resize(d.selcnt);
}
//---------------------------------------------------------
/*virtual*/ Manager::~Manager(){
	stop();
	d.selvec[0] = NULL;
	for(Data::SelectorVectorT::const_iterator it(d.selvec.begin()); it != d.selvec.end(); ++it){
		cassert(*it == NULL);
	}
	for(Data::ServiceVectorT::iterator it(d.svcvec.begin() + 1); it != d.svcvec.end(); ++it){
		delete it->ptr.release();
	}
	for(Data::ObjectVectorT::iterator it(d.objvec.begin()); it != d.objvec.end(); ++it){
		delete it->ptr.release();
	}
	d.svcvec.front().ptr.release();
	delete &d;
}
//---------------------------------------------------------
void Manager::start(){
	{
		Mutex::Locker	lock(d.mtx);
		bool reenter;
		do{
			reenter = false;
			if(d.st == Data::Stopped){
				d.st = Data::Starting;
			}else if(d.st == Data::Running){
				return;
			}else if(d.st == Data::Starting){
				do{
					d.cnd.wait(d.mtx);
				}while(d.st == Data::Starting);
				reenter = true;
			}else if(d.st == Data::Stopping || d.st == Data::MustStop){
				do{
					d.cnd.wait(d.mtx);
				}while(d.st == Data::Stopping || d.st == Data::MustStop);
				reenter = true;
			}
		}while(reenter);
	}
	
	if(d.svcvec.front().schcbk == NULL){
		THROW_EXCEPTION("At least one ObjectScheduler must be registered");
		return;
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
			vdbgx(Dbg::fdt, "start scheduler of type "<<(it - d.schtpvec.begin())<<" index "<<(sit - it->schvec.begin()));
			(*sit)->start();
		}
	}
	
	(*d.svcvec.front().schcbk)(d.svcvec.front().schidx, d.svcvec.front().ptr.ptr());
	
	d.masterService().start();
	
	d.masterService().insert(d.svcvec.front().ptr.ptr(), 0);
	
	for(
		Data::ServiceVectorT::iterator it(d.svcvec.begin() + 1);
		it != d.svcvec.end();
		++it
	){
		if(!it->ptr) continue;
		d.masterService().insert(it->ptr.ptr(), it - d.svcvec.begin());
		(*it->schcbk)(it->schidx, it->ptr.ptr());
	}
	
	for(
		Data::ObjectVectorT::iterator it(d.objvec.begin() + 1);
		it != d.objvec.end();
		++it
	){
		if(!it->ptr) continue;
		d.masterService().insert(it->ptr.ptr(), it - d.objvec.begin());
		(*it->schcbk)(it->schidx, it->ptr.ptr());
	}
	
	doStart();
	
	{
		Mutex::Locker	lock(d.mtx);
		d.st = Data::Running;
		d.cnd.broadcast();
	}
}
//---------------------------------------------------------
void Manager::stop(bool _waitsignal){
	{
		Mutex::Locker	lock(d.mtx);
		bool reenter;
		do{
			reenter = false;
			if(d.st == Data::Stopped){
				return;
			}else if(d.st == Data::Stopping){
				do{
					d.cnd.wait(d.mtx);
				}while(d.st == Data::Stopping);
				reenter = true;
			}else if(d.st == Data::Starting){
				do{
					d.cnd.wait(d.mtx);
				}while(d.st == Data::Starting);
				reenter = true;
			}else /*if(d.st == Data::Running || d.st == Data::MustStop)*/{
				if(_waitsignal){
					_waitsignal = false;
					while(d.st == Data::Running){
						d.cnd.wait(d.mtx);
					}
					reenter = true;
				}
			}
		}while(reenter);
		d.st = Data::Stopping;
	}
	
	d.masterService().stop(true);
	
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
		if(it->schvec.empty()) continue;
		for(
			Data::SchedulerVectorT::const_iterator sit(it->schvec.begin());
			sit != it->schvec.end();
			++sit
		){
			(*sit)->stop(true);
			delete *sit;
		}
	}
	
	{
		Mutex::Locker	lock(d.mtx);
		d.st = Data::Stopped;
		d.cnd.broadcast();
	}
}
//---------------------------------------------------------
void Manager::signalStop(){
	Mutex::Locker	lock(d.mtx);
	if(d.st == Data::Starting || d.st == Data::Running){
		d.st = Data::MustStop;
		d.cnd.broadcast();
	}
}
//---------------------------------------------------------
void Manager::erase(Object &_robj){
	IndexT	idx(_robj.id());
	IndexT	svcidx(compute_service_id(idx));
	service(svcidx).erase(_robj);
}
//---------------------------------------------------------
void Manager::eraseObject(Object &_robj){
	IndexT	idx(_robj.id());
	IndexT	svcidx(compute_service_id(idx));
	service(svcidx).erase(_robj);
}
//---------------------------------------------------------
bool Manager::signal(ulong _sm){
	bool b = false;
	for(Data::ServiceVectorT::const_iterator it(d.svcvec.begin()); it != d.svcvec.end(); ++it){
		if(it->ptr){
			b = b || it->ptr->signal(_sm);
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
		if(it->ptr){
			DynamicPointer<Signal>	sp(_rsig);
			b = b || it->ptr->signal(sp);
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
	return *d.svcvec[_i].ptr;
}
//---------------------------------------------------------
Object& Manager::object(const IndexT &_i)const{
	cassert(_i < d.objvec.size());
	return *d.objvec[_i].ptr;
}
//---------------------------------------------------------
Service* Manager::servicePointer(const IndexT &_ridx)const{
	if(_ridx < d.svcvec.size()){
		Data::ServiceStub &rsp(d.svcvec[_ridx]);
		return rsp.ptr.ptr();
	}
	return NULL;
}
//---------------------------------------------------------
Object* Manager::objectPointer(const IndexT &_ridx)const{
	if(_ridx < d.svcvec.size()){
		Data::ObjectStub &rop(d.objvec[_ridx]);
		return rop.ptr.ptr();
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
	vdbgx(Dbg::fdt, "prepare thread "<<(void*)_ps);
	Specific::prepareThread();
	requestuidptr.prepareThread();
	if(_ps){
		Mutex::Locker lock(d.mtx);
		if(d.currentselectoridx >= d.selvec.size()){
			THROW_EXCEPTION("Too many open threads - increase manager's selcnt");
			cassert(false);
		}
		_ps->selid = d.currentselectoridx;
		vdbgx(Dbg::fdt, "prepare thread selector idx "<<_ps->selid);
		d.selvec[_ps->selid] = _ps;
		++d.currentselectoridx;
	}
	doPrepareThread();
}
//---------------------------------------------------------
void Manager::unprepareThread(SelectorBase *_ps){
	vdbgx(Dbg::fdt,"");
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
uint Manager::newSchedulerTypeId()const{
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
	uint _objtypeid,
	SchedCbkT _schcbk
 ){
	Mutex::Locker	lock(d.mtx);
	if(d.st != Data::Stopped){
		THROW_EXCEPTION("Error: Register scheduler while not in stopped state");
		return -1;
	}
	
	Data::SchedulerTypeStub	&rstp(safe_at(d.schtpvec, _typeid));
	
	rstp.schvec.push_back(_ps);
	
	if(d.svcvec.front().schcbk == NULL){
		d.svcvec.front().schcbk = _schcbk;
		d.svcvec.front().schidx = 0;
	}
	
	return rstp.schvec.size() - 1;
}
//---------------------------------------------------------
IndexT Manager::doRegisterService(
	Service *_ps,
	const IndexT &_idx,
	SchedCbkT _schcbk,
	uint _schidx
){
	Mutex::Locker			lock(d.mtx);
	uint					tpid(_ps->dynamicTypeId());
	
	if(d.st != Data::Stopped){
		THROW_EXCEPTION("Error: Register service while not in stopped state");
		return invalid_uid().first;
	}
	
	Data::ObjectTypeStub	&rotps(safe_at(d.objtpvec, tpid));
	
	//rotps.pschcbk = _pschcbk;
	
	if(is_valid_index(_idx)){
		if(_idx >= d.startdynamicidx){
			THROW_EXCEPTION_EX("Error: invalid static index for service", _idx);
			return invalid_uid().first;
		}
		Data::ServiceStub &robj(d.svcvec[_idx]);
		robj.ptr = _ps;
		robj.tpid = tpid;
		robj.schcbk = _schcbk;
		robj.schidx = _schidx;
		if(rotps.objidx == -1){
			rotps.objidx = _idx;
		}
		static_cast<Object*>(_ps)->id(0, _idx);
		return _ps->index();
	}else{
		if(d.currentdynamicidx >= max_service_count()){
			THROW_EXCEPTION_EX("Error: invalid dynamic index for service", d.currentdynamicidx);
			return invalid_uid().first;
		}
		
		Data::ServiceStub	&robj(d.svcvec[d.currentdynamicidx]);
		robj.ptr = _ps;
		robj.tpid = tpid;
		robj.schcbk = _schcbk;
		robj.schidx = _schidx;
		if(rotps.objidx == -1){
			rotps.objidx = d.currentdynamicidx;
		}
		static_cast<Object*>(_ps)->id(0, d.currentdynamicidx);
		++d.currentdynamicidx;
		return _ps->index();
	}
}
//---------------------------------------------------------
IndexT Manager::doRegisterObject(
	Object *_po,
	const IndexT &_idx,
	SchedCbkT _schcbk,
	uint _schidx
){
	Mutex::Locker			lock(d.mtx);
	uint					tpid(_po->dynamicTypeId());
	
	if(d.st != Data::Stopped){
		THROW_EXCEPTION("Error: Register service while not in stopped state");
		return -1;
	}
	
	Data::ObjectTypeStub	&rotps(safe_at(d.objtpvec, tpid));
	
	//rotps.pschcbk = _pschcbk;
	
	if(is_valid_index(_idx)){
		if(_idx >= d.startdynamicidx){
			THROW_EXCEPTION_EX("Error: invalid static index for object", _idx);
			return invalid_uid().first;
		}
		
		Data::ObjectStub &robj(d.objvec[_idx]);
		robj.ptr = _po;
		robj.tpid = tpid;
		robj.schcbk = _schcbk;
		robj.schidx = _schidx;
		if(rotps.objidx == -1){
			rotps.objidx = _idx;
		}
		_po->id(0, _idx);
		return _po->index();
	}else{
		if(d.currentdynamicidx >= max_service_count()){
			THROW_EXCEPTION_EX("Error: invalid dynamic index for service", d.currentdynamicidx);
			return invalid_uid().first;
		}
		
		Data::ObjectStub	&robj(d.objvec[d.currentdynamicidx]);
		robj.ptr = _po;
		robj.tpid = tpid;
		robj.schcbk = _schcbk;
		robj.schidx = _schidx;
		if(rotps.objidx == -1){
			rotps.objidx = d.currentdynamicidx;
		}
		_po->id(0, d.currentdynamicidx);
		++d.currentdynamicidx;
		return _po->index();
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
		Data::ObjectStub	&rop(d.objvec[_ridx]);
		if(rop.tpid == _typeid){
			return rop.ptr.ptr();
		}
		if(rop.ptr.ptr() && rop.ptr->isTypeDynamic(_typeid)){
			return rop.ptr.ptr();
		}
	}
	return NULL;
}
//---------------------------------------------------------
Service* Manager::doGetService(uint _typeid, const IndexT &_ridx)const{
	if(_ridx < d.svcvec.size()){
		Data::ServiceStub	&rsp(d.svcvec[_ridx]);
		if(rsp.tpid == _typeid){
			return rsp.ptr.ptr();
		}
		if(rsp.ptr.ptr() && rsp.ptr->isTypeDynamic(_typeid)){
			return rsp.ptr.ptr();
		}
	}
	return NULL;
}
//---------------------------------------------------------
Object* Manager::doGetObject(uint _typeid)const{
	if(_typeid < d.objtpvec.size()){
		Data::ObjectTypeStub	&rts(d.objtpvec[_typeid]);
		if(rts.objidx > 0){
			return d.objvec[rts.objidx].ptr.ptr();
		}
	}
	return NULL;
}
//---------------------------------------------------------
Service* Manager::doGetService(uint _typeid)const{
	if(_typeid < d.objtpvec.size()){
		Data::ObjectTypeStub	&rts(d.objtpvec[_typeid]);
		if(rts.objidx > 0){
			return d.svcvec[rts.objidx].ptr.ptr();
		}
	}
	return NULL;
}
//---------------------------------------------------------
ObjectUidT Manager::insertService(Service *_ps, const IndexT &_ridx){
	return d.masterService().insert(_ps, _ridx);
}
//---------------------------------------------------------
void Manager::eraseService(Service *_ps){
	return d.masterService().erase(*_ps);
}
//---------------------------------------------------------
/*virtual*/ void Manager::doStart(){
}
//---------------------------------------------------------
/*virtual*/ void Manager::doStop(){
}
//---------------------------------------------------------
void Manager::stopObject(const IndexT &_ridx){
	if(_ridx < d.objvec.size()){
		Data::ObjectStub	&rop(d.objvec[_ridx]);
		if(rop.tpid){
			signal(S_KILL | S_RAISE, rop.ptr->uid());
			d.masterService().erase(*rop.ptr);
			//rop.first->id(0, _ridx);
		}
	}
}
//---------------------------------------------------------
ObjectUidT Manager::serviceUid(const IndexT &_idx){
	return make_object_uid(0, _idx, 0);
}
//---------------------------------------------------------
ObjectUidT Manager::objectUid(const IndexT &_idx){
	return make_object_uid(0, _idx, 0);
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
