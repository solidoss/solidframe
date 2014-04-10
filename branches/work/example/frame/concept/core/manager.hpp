// manager.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef CONCEPT_CORE_MANAGER_HPP
#define CONCEPT_CORE_MANAGER_HPP

#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "frame/aio/aioselector.hpp"
#include "frame/objectselector.hpp"
#include "frame/file/filestore.hpp"
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
typedef solid::frame::file::Store<>								FileStoreT;

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
	
	static Manager& the(Manager *_pm = NULL);
	
	void scheduleListener(solid::DynamicPointer<solid::frame::aio::Object> &_objptr);
	void scheduleTalker(solid::DynamicPointer<solid::frame::aio::Object> &_objptr);
	void scheduleAioObject(solid::DynamicPointer<solid::frame::aio::Object> &_objptr);
	void scheduleObject(solid::DynamicPointer<solid::frame::Object> &_objptr);
	
	solid::frame::ipc::Service 	&ipc()const;
	FileStoreT&	fileStore()const;
private:
	struct Data;
	Data	&d;
};


}//namespace concept
#endif
