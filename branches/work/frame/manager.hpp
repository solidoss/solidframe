// frame/manager.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_MANAGER_HPP
#define SOLID_FRAME_MANAGER_HPP

#include "frame/common.hpp"
#include "frame/event.hpp"
#include "system/mutex.hpp"
#include "system/error.hpp"
#include "utility/dynamicpointer.hpp"
#include "utility/functor.hpp"

namespace solid{
namespace frame{
class	Manager;
class	Service;
class	ObjectBase;
class	SchedulerBase;
class	ReactorBase;

template <class Obj, class Sch>
struct ScheduleObjectF{
	DynamicPointer<Obj>	&robjptr;
	Sch					&rsch;
	
	ScheduleObjectF(DynamicPointer<Obj> &_robjptr, Sch &_rsch):robjptr(_robjptr), rsch(_rsch){}
	
	ErrorConditionT operator()(){
		return rsch.schedule(robjptr);
	}
};

struct EventNotifier{
	EventNotifier(
		Manager &_rm, Event const &_revt, const size_t _sigmsk = 0
	):rm(_rm), evt(_revt), sigmsk(_sigmsk){}
	
	Manager			&rm;
	Event			evt;
	const size_t	sigmsk;
	
	void operator()(ObjectBase &_robj);
};


class Manager{
	typedef FunctorReference<ErrorConditionT>	ObjectScheduleFunctorT;
public:
	static Manager& specific();
	
	Manager(
		const size_t _svcprovisioncp = 1024,
		const size_t _selprovisioncp = 1024,
		uint _objpermutbts = 6,
		uint _mutrowsbts = 4,
		uint _mutcolsbts = 8
	);
	
	virtual ~Manager();

	void stop(Event const &_revt);
	
	bool registerService(
		Service &_rsvc,
		uint _objpermutbts = 0
	);
	void unregisterService(Service &_rsvc);
	
	template <class Obj, class Sch>
	ObjectUidT	registerObject(DynamicPointer<Obj> &_robjptr, Sch &_rsch, Event const &_revt){
		ScheduleObjectF<Obj, Sch>	fnc(_robjptr, _rsch);
		ObjectScheduleFunctorT		fctor(fnc);
		return doRegisterObject(*_robjptr, fctor);
	}
	
	bool notify(ObjectUidT const &_ruid, Event const &_e, const size_t _sigmsk = 0);

	bool notifyAll(Event const &_e, const size_t _sigmsk = 0);
	
	Mutex& mutex(const ObjectBase &_robj)const;
	
	Service& service(const ObjectBase &_robj)const;
	
	ObjectUidT  id(const ObjectBase &_robj)const;
	
	IndexT	reactorId(IndexT _idx)const;
	
protected:
	size_t serviceCount()const;
	
private:
	friend class Service;
	friend class ObjectBase;
	
	
	typedef FunctorReference<void, ObjectBase&>	ObjectVisitFunctorT;
	
	void unregisterObject(ObjectBase &_robj);
	
	ObjectUidT  unsafeId(const ObjectBase &_robj)const;
	
	Mutex& serviceMutex(const Service &_rsvc)const;
	
	ObjectUidT registerServiceObject(const Service &_rsvc, ObjectBase &_robj, ObjectScheduleFunctorT &_rfct);
	ObjectUidT doRegisterObject(ObjectBase &_robj, ObjectScheduleFunctorT &_rfct);
	
	template <typename F>
	bool forEachServiceObject(const Service &_rsvc, F &_f){
		ObjectVisitFunctorT fctor(_f);
		return doForEachServiceObject(_rsvc, fctor);
	}
	
	friend class ReactorBase;
	friend class SchedulerBase;
	friend struct EventNotifier;
	
	bool raise(const ObjectBase &_robj, Event const &_re);
	
	Mutex& mutex(const IndexT &_rfullid)const;
	ObjectBase* unsafeObject(const IndexT &_rfullid)const;
	
	IndexT computeThreadId(const IndexT &_selidx, const IndexT &_objidx);
	bool prepareThread(ReactorBase *_ps = NULL);
	void unprepareThread(ReactorBase *_ps = NULL);
	
	void resetService(Service &_rsvc);
	void stopService(Service &_rsvc, bool _wait);
	
	virtual bool doPrepareThread();
	virtual void doUnprepareThread();
	//ObjectUidT doRegisterServiceObject(const IndexT _svcidx, Object &_robj);
	ObjectUidT doUnsafeRegisterServiceObject(const IndexT _svcidx, ObjectBase &_robj, ObjectScheduleFunctorT &_rfct);
	
	bool doForEachServiceObject(const Service &_rsvc, ObjectVisitFunctorT &_fctor);
	bool doForEachServiceObject(const size_t _svcidx, ObjectVisitFunctorT &_fctor);
	void doWaitStopService(const size_t _svcidx, Locker<Mutex> &_rlock, bool _wait);
	bool doRegisterService(
		Service &_rsvc,
		uint _objpermutbts = 0
	);
	void doResetService(const size_t _svcidx, Locker<Mutex> &_rlock);
private:
	struct Data;
	Data	&d;
};

#ifndef NINLINES
#include "frame/manager.ipp"
#endif

}//namespace frame
}//namespace solid
#endif

