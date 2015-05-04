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
#include "frame/schedulerbase.hpp"

namespace solid{
namespace frame{

class	Manager;
class	Service;
class	ObjectBase;
class	SchedulerBase;
class	ReactorBase;

struct	ServiceStub;

template <class Obj, class Sch>
struct ScheduleObjectF{
	DynamicPointer<Obj>	&robjptr;
	Sch					&rsch;
	Event const			&revt;
	
	ScheduleObjectF(
		DynamicPointer<Obj> &_robjptr, Sch &_rsch, Event const &_revt
	):robjptr(_robjptr), rsch(_rsch), revt(_revt){}
	
	ErrorConditionT operator()(){
		return rsch.schedule(robjptr, revt);
	}
};

struct EventNotifierF{
	EventNotifierF(
		Event const &_revt, const size_t _sigmsk = 0
	):evt(_revt), sigmsk(_sigmsk){}
	
	Event			evt;
	const size_t	sigmsk;
	
	bool operator()(ObjectBase &_robj, ReactorBase &_rreact);
};


class Manager{
public:
	Manager(
		const size_t _svcmtxcnt = 0,
		const size_t _objmtxcnt = 0,
		const size_t _objbucketsize = 0
	);
	
	virtual ~Manager();

	void stop();
	
	bool notify(ObjectUidT const &_ruid, Event const &_e, const size_t _sigmsk = 0);

	bool notifyAll(Event const &_e, const size_t _sigmsk = 0);
	
	
	template <class F>
	bool visit(ObjectUidT const &_ruid, F &_rf){
		ObjectVisitFunctorT fctor(_rf);
		return doVisit(_ruid, fctor);
	}
	
	
	ObjectUidT  id(const ObjectBase &_robj)const;
	
	Service& service(const ObjectBase &_robj)const;
protected:
	size_t serviceCount()const;
	
private:
	friend class Service;
	friend class ObjectBase;
	friend class ReactorBase;
	friend class SchedulerBase;
	friend struct EventNotifierF;
	
	typedef FunctorReference<bool, ObjectBase&, ReactorBase&>	ObjectVisitFunctorT;
	
	
	bool registerService(Service &_rsvc);
	void unregisterService(Service &_rsvc);
	
	void unregisterObject(ObjectBase &_robj);
	
	ObjectUidT  unsafeId(const ObjectBase &_robj)const;
	
	Mutex& mutex(const Service &_rsvc)const;
	Mutex& mutex(const ObjectBase &_robj)const;
	
	ObjectUidT registerObject(
		const Service &_rsvc,
		ObjectBase &_robj,
		ReactorBase &_rr,
		ScheduleFunctorT &_rfct,
		ErrorConditionT &_rerr
	);
		
	template <typename F>
	bool forEachServiceObject(const Service &_rsvc, F &_f){
		ObjectVisitFunctorT fctor(_f);
		return doForEachServiceObject(_rsvc, fctor);
	}
	
	bool raise(const ObjectBase &_robj, Event const &_re);

	void stopService(Service &_rsvc, bool _wait);
	
	
	bool doForEachServiceObject(const Service &_rsvc, ObjectVisitFunctorT &_fctor);
	bool doForEachServiceObject(const size_t _chkidx, ObjectVisitFunctorT &_fctor);
	bool doVisit(ObjectUidT const &_ruid, ObjectVisitFunctorT &_fctor);
	void doUnregisterService(ServiceStub &_rss);
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

