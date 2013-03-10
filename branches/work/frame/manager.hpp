/* Declarations file manager.hpp
	
	Copyright 2013 Valentin Palade 
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

#ifndef SOLID_FRAME_MANAGER_HPP
#define SOLID_FRAME_MANAGER_HPP

#include "frame/common.hpp"
#include "system/mutex.hpp"
#include "utility/dynamicpointer.hpp"

class  SpecificMapper;
class  GlobalMapper;
class  SelectorBase;

namespace solid{
namespace frame{

class	Service;
class	Object;
struct	Message;
class	SchedulerBase;
class	SelectorBase;

typedef DynamicSharedPointer<Message>	MessageSharedPointerT;
typedef DynamicPointer<Message>			MessagePointerT;

typedef DynamicSharedPointer<Object>	ObjectSharedPointerT;
typedef DynamicPointer<Object>			ObjectPointerT;

class Manager{
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

	void stop();
	
	bool registerService(
		Service &_rsvc,
		uint _objpermutbts = 0
	);
	void unregisterService(Service &_rsvc);
	
	ObjectUidT	registerObject(Object &_robj);
	
	void unregisterObject(Object &_robj);
	
	bool notify(ulong _sm, const ObjectUidT &_ruid);

	bool notify(MessagePointerT &_rmsgptr, const ObjectUidT &_ruid);
	
	bool notifyAll(ulong _sm);
	
	bool notifyAll(MessageSharedPointerT &_rmsgptr);
	
	
	void raise(const Object &_robj);
	
	Mutex& mutex(const Object &_robj)const;
	
	Service& service(const Object &_robj)const;
	
	ObjectUidT  id(const Object &_robj)const;
	
protected:
	size_t serviceCount()const;
private:
	friend class Service;
	
	struct VisitContext{
		
	};
	Mutex& serviceMutex(const Service &_rsvc);
	ObjectUidT registerServiceObject(const Service &_rsvc, Object &_robj);
	Object* nextServiceObject(const Service &_rsvc, VisitContext &_rctx);
	
	template <typename F>
	bool forEachServiceObject(const Service &_rsvc, F _f){
		Locker<Mutex>	lock(serviceMutex(_rsvc));
		VisitContext	ctx;
		Object			*pobj;
		bool			signaled = false;
		while((pobj = nextServiceObject(_rsvc, ctx)) != NULL){
			_f(*pobj);
			signaled = true;
		}
		return signaled;
	}
	
	friend class SelectorBase;
	friend class SchedulerBase;
	
	IndexT computeThreadId(const IndexT &_selidx, const IndexT &_objidx);
	bool prepareThread(SelectorBase *_ps = NULL);
	void unprepareThread(SelectorBase *_ps = NULL);
	
	void resetService(Service &_rsvc);
	void stopService(Service &_rsvc, bool _wait);
	
	virtual bool doPrepareThread();
	virtual void doUnprepareThread();
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

