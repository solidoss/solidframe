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
#include "system/mutex.hpp"
#include "system/error.hpp"
#include "utility/dynamicpointer.hpp"
#include "system/function.hpp"
#include "frame/schedulerbase.hpp"

//#include "utility/functor.hpp"

namespace solid{

struct Event;

namespace frame{

class	Manager;
class	Service;
class	ObjectBase;
class	SchedulerBase;
class	ReactorBase;

struct	ServiceStub;


class Manager{
public:
	Manager(
		const size_t _svcmtxcnt = 0,
		const size_t _objmtxcnt = 0,
		const size_t _objbucketsize = 0
	);
	
	virtual ~Manager();

	void stop();
	
	bool notify(ObjectIdT const &_ruid, Event &&_uevt, const size_t _sigmsk = 0);
	
	//bool notifyAll(Event const &_revt, const size_t _sigmsk = 0);
	
	
	template <class F>
	bool visit(ObjectIdT const &_ruid, F _f){
		ObjectVisitFunctionT fct(_f);
		return doVisit(_ruid, fct);
	}
	
	
	ObjectIdT  id(const ObjectBase &_robj)const;
	
	Service& service(const ObjectBase &_robj)const;
protected:
	size_t serviceCount()const;
	
private:
	friend class Service;
	friend class ObjectBase;
	friend class ReactorBase;
	friend class SchedulerBase;
	
	typedef FUNCTION<bool(ObjectBase&, ReactorBase&)>			ObjectVisitFunctionT;
	
	static bool notify_object(
		ObjectBase &_robj, ReactorBase &_rreact,
		Event const &_revt, const size_t _sigmsk
	);
	
	static bool notify_object(
		ObjectBase &_robj, ReactorBase &_rreact,
		Event &&_uevt, const size_t _sigmsk
	);
	
	bool registerService(Service &_rsvc);
	void unregisterService(Service &_rsvc);
	
	void unregisterObject(ObjectBase &_robj);
	bool disableObjectVisits(ObjectBase &_robj);
	
	ObjectIdT  unsafeId(const ObjectBase &_robj)const;
	
	Mutex& mutex(const Service &_rsvc)const;
	Mutex& mutex(const ObjectBase &_robj)const;
	
	ObjectIdT registerObject(
		const Service &_rsvc,
		ObjectBase &_robj,
		ReactorBase &_rr,
		ScheduleFunctionT &_rfct,
		ErrorConditionT &_rerr
	);
	
	size_t notifyAll(const Service &_rsvc, Event const & _revt, const size_t _sigmsk);
	
	template <typename F>
	size_t forEachServiceObject(const Service &_rsvc, F _f){
		ObjectVisitFunctionT fct(_f);
		return doForEachServiceObject(_rsvc, fct);
	}
	
	bool raise(const ObjectBase &_robj, Event const &_re);

	void stopService(Service &_rsvc, bool _wait);
	bool startService(Service &_rsvc);
	
	
	size_t doForEachServiceObject(const Service &_rsvc, ObjectVisitFunctionT &_rfct);
	size_t doForEachServiceObject(const size_t _chkidx, ObjectVisitFunctionT &_rfct);
	bool doVisit(ObjectIdT const &_ruid, ObjectVisitFunctionT &_fctor);
	void doUnregisterService(ServiceStub &_rss);
private:
	struct Data;
	Data	&d;
};

#ifndef SOLID_HAS_NO_INLINES
#include "frame/manager.ipp"
#endif

}//namespace frame
}//namespace solid
#endif

