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
//#include "foundation/service.hpp"

#include "utility/dynamicpointer.hpp"

class  Mutex;
class  SpecificMapper;
class  GlobalMapper;
struct ResolveIterator;

namespace fdt = foundation;

namespace foundation{

class	Service;
class	Object;
class	Signal;
class	SchedulerBase;
class	SelectorBase;
//! The central class of the foundation architecure.
/*!
 * Schedulers, services and service like objects (objects
 * that reside within a master service), must be registered
 * before Manager start.
 * The manager is a singleton and it is inheritable.
 * One can access the manager using Manager::the static function
 * or foundation::m().
 * After start, the manager is solely used to access services
 * and signal objects.
 */
class Manager{
	typedef void (*SchedCbkT) (uint, Object *);
public:
	static Manager& the();
	
	Manager(
		const IndexT &_startdynamicidx = 1,
#ifndef USERVICEBITS
		const uint _servicebits = sizeof(IndexT) == 4 ? 5 : 8,
#endif
		uint _selcnt = 1024
	);
	
	virtual ~Manager();
	
	const IndexT& firstDynamicIndex()const;
	const IndexT& maxServiceCount()const;
	IndexT computeId(const IndexT &_srvidx, const IndexT &_objidx)const;
	UidT makeObjectUid(const IndexT &_srvidx, const IndexT &_objidx, const uint32 _uid)const;
	IndexT computeIndex(const IndexT &_fullid)const;
	IndexT computeServiceId(const IndexT &_fullid)const;
	
	
	void start();
	void stop(bool _waitsignal = false);
	void signalStop();
	
	bool signal(ulong _sm);
	bool signal(ulong _sm, const ObjectUidT &_ruid);
	bool signal(ulong _sm, IndexT _fullid, uint32 _uid);
	//bool signal(ulong _sm, const Object &_robj);

	bool signal(DynamicSharedPointer<Signal> &_rsig);
	bool signal(DynamicPointer<Signal> &_rsig, const ObjectUidT &_ruid);
	bool signal(DynamicPointer<Signal> &_rsig, IndexT _fullid, uint32 _uid);
	//bool signal(DynamicPointer<Signal> &_rsig, const Object &_robj);
	
	void raiseObject(const Object &_robj);
	
	Mutex& mutex(const Object &_robj)const;
	
	uint32  uid(const Object &_robj)const;
	
	template<class T>
	typename T::ServiceT& service(const T &_robj)const{
		return static_cast<typename T::ServiceT&>(service(_robj.serviceId()));
	}
	
	virtual SpecificMapper*  specificMapper();
	
	virtual GlobalMapper* globalMapper();
	
	template <typename S>
	uint registerScheduler(S *_ps){
		typedef typename S::ObjectT ObjectT;
		return doRegisterScheduler(
			_ps,
			schedulerTypeId<S>(),
			ObjectT::staticTypeId(),
			&sched_cbk<ObjectT, S>
		);
	}
	
	template <class S>
	S* scheduler(uint _id)const{
		return static_cast<S*>(doGetScheduler(schedulerTypeId<S>(), _id));
	}
	
	template <typename S, class O>
	IndexT registerService(
		O *_po,
		uint _schidx = 0,
		const IndexT &_idx = invalid_uid().first
	){
		return doRegisterService(_po, _idx, &sched_cbk<O, S>, _schidx);
	}
	
	template <typename S, class O>
	IndexT registerObject(
		O *_po,
		uint _schidx = 0,
		const IndexT &_idx = invalid_uid().first
	){
		return doRegisterObject(_po, _idx, &sched_cbk<O, S>, _schidx);
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
	
	void eraseObject(Object &_robj);
	
	void stopObject(const IndexT &_idx);
	
	ObjectUidT serviceUid(const IndexT &_idx);
	ObjectUidT objectUid(const IndexT &_idx);
protected:
	struct ThisGuard{
		ThisGuard(Manager *_pm);
		~ThisGuard();
	};
	
	virtual void doPrepareThread();
	virtual void doUnprepareThread();
	
	
	unsigned serviceCount()const;
	
	virtual void doStart();
	
	virtual void doStop();
	
private:
	//typedef void (*ScheduleCbkT) (uint, Object *);
	friend class SchedulerBase;
	friend class Service;
	friend class ObjectPointerBase;
	
	void erase(Object &_robj);
	
	void prepareThread(SelectorBase *_ps = NULL);
	void unprepareThread(SelectorBase *_ps = NULL);
	
	void prepareThis();
	void unprepareThis();
	
	uint newSchedulerTypeId()const;
	uint newServiceTypeId();
	uint newObjectTypeId();
	
	uint doRegisterScheduler(SchedulerBase *_ps, uint _typeid, uint _objtypeid, SchedCbkT _schcbk);
	IndexT doRegisterService(Service *_ps, const IndexT &_idx, SchedCbkT _schcbk, uint _schidx);
	IndexT doRegisterObject(Object *_po, const IndexT &_idx, SchedCbkT _schcbk, uint _schidx);
	
	SchedulerBase* doGetScheduler(uint _typeid, uint _idx)const;
	Service* doGetService(uint _typeid, const IndexT &_ridx)const;
	Object* doGetObject(uint _typeid, const IndexT &_ridx)const;
	
	Service* doGetService(uint _typeid)const;
	Object* doGetObject(uint _typeid)const;
	
	uint doStart(Service **_psvctbl, uint _svctblcp); 
	
	template <class O>
	uint schedulerTypeId()const{
		static const uint v(newSchedulerTypeId());
		return v;
	}
	template <class O, typename S>
	static void sched_cbk(uint _idx, Object *_po){
		O *po(static_cast<O*>(_po));
		typename S::JobT op(static_cast<typename S::ObjectT*>(po));
		S::schedule(op, _idx);
	}
	
	ObjectUidT insertService(Service *_ps, const IndexT &_ridx);
	void eraseService(Service *_ps);
	
	Manager(const Manager&);
	Manager& operator=(const Manager&);
private:
	class ServicePtr;
#ifndef USERVICEBITS
	const uint		service_bit_cnt;
	const uint		index_bit_cnt;
	const IndexT	max_srvc_cnt;
#endif
	struct Data;
	Data	&d;
};

inline Manager& m(){
	return Manager::the();
}

#ifndef NINLINES
#include "foundation/manager.ipp"
#endif

}//namespace
#endif

