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

#ifndef CONCEPT_CORE_MANAGER_HPP
#define CONCEPT_CORE_MANAGER_HPP

#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "frame/aio/aioselector.hpp"
#include "frame/objectselector.hpp"
#include "common.hpp"

namespace solid{
namespace frame{

namespace ipc{
class Service;
}//namespace ipc

namespace file{
class Manager;
}//namespace file
}//namespace frame
}//namespace solid


namespace concept{

typedef solid::frame::Scheduler<solid::frame::aio::Selector>	AioSchedulerT;
typedef solid::frame::Scheduler<solid::frame::ObjectSelector>	SchedulerT;


//! A proof of concept server
/*!
	This is a proof of concept server and should be taken accordingly.
	
	It tries to give an ideea of how the foundation::Manager should be used.
	
	To complicate things a little, it allso keeps a map service name to service,
	so that services can be more naturally accessed by their names.
*/
class Manager: public solid::frame::Manager{
public:
	Manager();
	
	~Manager();
	
	void start();
	
	static Manager& the();
	
	void scheduleListener(solid::DynamicPointer<solid::frame::aio::Object> &_objptr);
	void scheduleTalker(solid::DynamicPointer<solid::frame::aio::Object> &_objptr);
	void scheduleAioObject(solid::DynamicPointer<solid::frame::aio::Object> &_objptr);
	void scheduleObject(solid::DynamicPointer<solid::frame::Object> &_objptr);
	
	
	ObjectUidT readMessageStewardUid()const;
	
	ObjectUidT writeMessageStewardUid()const;
	
	solid::frame::ipc::Service 	&ipc()const;
	solid::frame::file::Manager	&fileManager()const;
private:
	struct Data;
	Data	&d;
};


}//namespace concept
#endif
