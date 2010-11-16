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

namespace foundation{

class	Service;
class	Pool;
class	Object;
class	ActiveSet;
class	Visitor;
class	ServiceContainer;
class	Signal;

class Manager;
struct ManagerStub{
	void prepareThread();
	void unprepareThread();
	uint registerActiveSet(ActiveSet &_ras);
	
	Manager* operator->(){return pmanager;}
	
	Manager	*pmanager;
private:
	ManagerStub(Manager *_pman = NULL):pmanager(_pman){}
};

class Manager{
public:
	virtual ~Manager();
	static Manager& the();
	
	void start();
	void stop(bool _wait = true);
	
	int signalObject(IndexT _fullid, uint32 _uid, ulong _sigmask);
	
	int signalObject(Object &_robj, ulong _sigmask);
	
	int signalObject(IndexT _fullid, uint32 _uid, DynamicPointer<Signal> &_rsig);
	
	int signalObject(Object &_robj, DynamicPointer<Signal> &_rsig);
	
	void raiseObject(const Object &_robj);
	
	Mutex& mutex(const Object &_robj)const;
	
	uint32  uid(const Object &_robj)const;
	
	template<class T>
	typename T::ServiceT& service(const T &_robj){
		return static_cast<typename T::ServiceT&>(service(_robj.serviceid()));
	}
	
	virtual SpecificMapper*  specificMapper();
	
	virtual GlobalMapper* globalMapper();

protected:
	struct ThisGuard{
		ThisGuard(Manager *_pm);
		~ThisGuard();
	};
	
	virtual void doPrepareThread();
	virtual void doUnprepareThread();
	
	uint insertService(Service *_ps, uint _pos = 0);
	void insertObject(Object *_po);
	
	void removeObject(Object *_po);
	
	unsigned serviceCount()const;
	
	Service& service(uint _i = 0)const;
	
	Manager();
	
	ManagerStub createStub();
private:
	friend ManagerStub;
	
	void prepareThread();
	void unprepareThread();
	
	uint registerActiveSet(ActiveSet &_ras);
	
	void prepareThis();
	void unprepareThis();
	
	ServiceContainer & serviceContainer();
	
	Manager(const Manager&);
	Manager& operator=(const Manager&);
private:
	class ServicePtr;
	struct Data;
	Data &d;
};

inline void ManagerStub::prepareThread(){
	pmanager->prepareThread();
}
inline void  ManagerStub::unprepareThread(){
	pmanager->unprepareThread();
}
inline uint ManagerStub::registerActiveSet(ActiveSet &_ras){
	return pmanager->registerActiveSet(_ras);
}

inline ManagerStub Manager::createStub(){
	return ManagerStub(this);
}

}//namespace
#endif
