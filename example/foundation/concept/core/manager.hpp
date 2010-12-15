/* Declarations file manager.hpp
	
	Copyright 2007, 2008 Valentin Palade 
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

#ifndef EXAMPLE_CONCEPT_MANAGER_HPP
#define EXAMPLE_CONCEPT_MANAGER_HPP

#include "foundation/manager.hpp"
#include "foundation/scheduler.hpp"
#include "foundation/aio/aioselector.hpp"
#include "foundation/objectselector.hpp"
#include "common.hpp"


namespace foundation{
namespace ipc{
class Service;
}
namespace file{
class Manager;
}
}


namespace concept{

typedef foundation::Scheduler<foundation::aio::Selector>	AioSchedulerT;
typedef foundation::Scheduler<foundation::ObjectSelector>	SchedulerT;


//! A proof of concept server
/*!
	This is a proof of concept server and should be taken accordingly.
	
	It tries to give an ideea of how the foundation::Manager should be used.
	
	To complicate things a little, it allso keeps a map service name to service,
	so that services can be more naturally accessed by their names.
*/
class Manager: public foundation::Manager{
public:
	Manager();
	
	~Manager();
	
	static Manager& the(){return static_cast<Manager&>(foundation::Manager::the());}
	
	template <typename O>
	typename O::ServiceT & service(O &_ro){
		return static_cast<typename O::ServiceT&>(foundation::Manager::service(_ro.serviceId()));
	}
	
	void signalService(const IndexT &_ridx, DynamicPointer<foundation::Signal> &_rsig);
	
	ObjectUidT readSignalExecuterUid()const;
	
	ObjectUidT writeSignalExecuterUid()const;
	
	foundation::ipc::Service 	&ipc()const;
	foundation::file::Manager	&fileManager()const;
private:
	struct Data;
	Data	&d;
};

inline Manager& m(){
	return Manager::the();
}

}//namespace concept
#endif
