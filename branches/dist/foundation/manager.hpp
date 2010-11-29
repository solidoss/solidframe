/* Declarations file manager.hpp
	
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

#ifndef FOUNDATION_MANAGER_HPP
#define FOUNDATION_MANAGER_HPP

//#include <vector>

#include "foundation/common.hpp"
#include "foundation/objectpointer.hpp"

#include "utility/dynamicpointer.hpp"

class  Mutex;
class  SpecificMapper;
class  GlobalMapper;
struct AddrInfoIterator;

namespace fdt = foundation;

namespace foundation{

class	Service;
class	Pool;
class	Object;
class	ActiveSet;
class	Visitor;
class	ServiceContainer;
class	Signal;
class	SchedulerBase;
class	SelectorBase;

class Manager{
public:
	static Manager& the();
	
	Manager();
	
	virtual ~Manager();
	
	void start();
	void stop(bool _wait = true);
	
	bool signal(ulong _sm);
	bool signal(ulong _sm, const ObjectUidT &_ruid);
	bool signal(ulong _sm, IndexT _fullid, uint32 _uid);
	bool signal(ulong _sm, const Object &_robj);

	bool signal(DynamicSharedPointer<Signal> &_rsig);
	bool signal(DynamicPointer<Signal> &_rsig, const ObjectUidT &_ruid);
	bool signal(DynamicPointer<Signal> &_rsig, IndexT _fullid, uint32 _uid);
	bool signal(DynamicPointer<Signal> &_rsig, const Object &_robj);
	
	void raiseObject(const Object &_robj);
	
	Mutex& mutex(const Object &_robj)const;
	
	uint32  uid(const Object &_robj)const;
	
	template<class T>
	typename T::ServiceT& service(const T &_robj){
		return static_cast<typename T::ServiceT&>(service(_robj.serviceid()));
	}
	
	virtual SpecificMapper*  specificMapper();
	
	virtual GlobalMapper* globalMapper();
	
	template <class S>
	S* registerScheduler(S *_ps){
		doRegisterScheduler(_ps, schedulerTypeId<S>());
		return _ps;
	}
	
	template <class S>
	S* scheduler(uint _id){
		return static_cast<S*>(doGetScheduler(schedulerTypeId<S>(), _id));
	}
	
	template <class Sch, class S>
	ObjectUidT registerService(
		S *_ps,
		const IndexT &_idx = invalid_uid().second,
		uint _schedidx = 0
	){
		return doRegisterService(_ps, _idx, &sched_cbk<S, Sch>, _schedidx);
	}
	
	template <class Sch, class O>
	ObjectUidT registerObject(
		O *_po,
		const IndexT &_idx = invalid_uid().second,
		uint _schedidx = 0
	){
		return doRegisterObject(_po, _idx, &sched_cbk<O, Sch>, _schedidx);
	}
	
	Service& service(const IndexT &_ridx = 0)const;
	Object& object(const IndexT &_ridx)const;
	
	Service* servicePointer(const IndexT &_ridx = 0)const;
	Object* objectPointer(const IndexT &_ridx)const;
	
	template <class O>
	O* object(const IndexT &_ridx)const{
		return static_cast<O*>(doGetObject(O::staticTypeId(), _ridx));
	}
	template <class O>
	O* service(const IndexT &_ridx)const{
		return static_cast<O*>(doGetService(O::staticTypeId(), _ridx));
	}
	//For cases when there is only one object per type
	template <class O>
	O* object()const{
		return static_cast<O*>(doGetObject(O::staticTypeId()));
	}
	//For cases when there is only one object per type
	template <class O>
	O* service()const{
		return static_cast<O*>(doGetService(O::staticTypeId()));
	}
	
protected:
	struct ThisGuard{
		ThisGuard(Manager *_pm);
		~ThisGuard();
	};
	
	virtual void doPrepareThread();
	virtual void doUnprepareThread();
	
	
	unsigned serviceCount()const;
	
private:
	typedef void (*ScheduleCbkT) (uint, Object *);
	friend class SchedulerBase;
	
	void prepareThread(SelectorBase *_ps = NULL);
	void unprepareThread(SelectorBase *_ps = NULL);
	
	void prepareThis();
	void unprepareThis();
	
	uint newSchedulerTypeId();
	uint newServiceTypeId();
	uint newObjectTypeId();
	
	uint doRegisterScheduler(SchedulerBase *_ps, uint _typeid);
	ObjectUidT doRegisterService(Service *_ps, const IndexT &_idx, ScheduleCbkT _pschcbk, uint _schedidx);
	ObjectUidT doRegisterObject(Object *_po, const IndexT &_idx, ScheduleCbkT _pschcbk, uint _schedidx);
	
	SchedulerBase* doGetScheduler(uint _typeid, uint _idx)const;
	Service* doGetService(uint _typeid, const IndexT &_ridx)const;
	Object* doGetObject(uint _typeid, const IndexT &_ridx)const;
	
	Service* doGetService(uint _typeid)const;
	Object* doGetObject(uint _typeid)const;
	
	template <class O>
	uint schedulerTypeId(){
		static const uint v(newSchedulerTypeId());
		return v;
	}
	template <class O, class Sch>
	static void sched_cbk(uint _idx, Object *_po){
		O *po(static_cast<O*>(_po));
		typename Sch::JobT op(static_cast<typename Sch::ObjectT*>(po));
		Sch::schedule(op, _idx);
	}
	
	Manager(const Manager&);
	Manager& operator=(const Manager&);
private:
	class ServicePtr;
	struct Data;
	Data &d;
};

inline Manager& m(){
	return Manager::the();
}

}//namespace
#endif
