/* Implementation file manager.cpp
	
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

#include <vector>

#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/specific.hpp"

#include "foundation/manager.hpp"
#include "foundation/service.hpp"
#include "foundation/object.hpp"
#include "foundation/activeset.hpp"
#include "foundation/requestuid.hpp"

//#define NSINGLETON_MANAGER
//-------------------------------------------------------------------------------------
#ifdef NSINGLETON_MANAGER
static const unsigned specificPosition(){
	//TODO: staticproblem
	static const unsigned thrspecpos = Thread::specificId();
	return thrspecpos;
}
#endif
//-------------------------------------------------------------------------------------

namespace foundation{
//-------------------------------------------------------------------------------------
struct Manager::Data{
	
};
//-------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------

/*static*/ Manager& Manager::the(){
	
}

Manager::Manager(){
}

/*virtual*/ Manager::~Manager(){
	
}
void Manager::start(){
	
}

void Manager::stop(bool _wait=){
	
}

int Manager::signalObject(IndexT _fullid, uint32 _uid, ulong _sigmask){
	
}

int Manager::signalObject(Object &_robj, ulong _sigmask){
	
}

int Manager::signalObject(IndexT _fullid, uint32 _uid, DynamicPointer<Signal> &_rsig){
	
}

int Manager::signalObject(Object &_robj, DynamicPointer<Signal> &_rsig){
	
}

void Manager::raiseObject(const Object &_robj){
	
}

Mutex& Manager::mutex(const Object &_robj)const{
	
}

uint32  Manager::uid(const Object &_robj)const{
	
}

/*virtual*/ SpecificMapper*  Manager::specificMapper(){
	
}

/*virtual*/ GlobalMapper* Manager::globalMapper(){
	
}

/*virtual*/ void Manager::doPrepareThread(){
	
}
/*virtual*/ void Manager::doUnprepareThread(){
	
}

uint Manager::insertService(Service *_ps, uint _pos){
	
}

void Manager::insertObject(Object *_po){
	
}

void Manager::removeObject(Object *_po){
	
}


Service& Manager::service(uint _i)const{
	
}

void Manager::prepareThread(){
}

void Manager::unprepareThread(){
	
}

uint Manager::registerActiveSet(ActiveSet &_ras){
	
}

void Manager::prepareThis(){
	
}
void Manager::unprepareThis(){
	
}

ServiceContainer & Manager::serviceContainer();
//-------------------------------------------------------------------------------------
Manager::ThisGuard::ThisGuard(Manager *_pm){
#ifdef NSINGLETON_MANAGER
	_pm->prepareThis();
#endif
}
Manager::ThisGuard::~ThisGuard(){
#ifdef NSINGLETON_MANAGER
	Manager::the().unprepareThis();
#endif
}

//-------------------------------------------------------------------------------------


}//namespace foundation
