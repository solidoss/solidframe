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
#include "foundation/schedulerbase.hpp"
#include "foundation/requestuid.hpp"

namespace foundation{
//---------------------------------------------------------
struct Manager::Data{
	
};
//---------------------------------------------------------

//---------------------------------------------------------

#ifdef NSINGLETON_MANAGER
static const unsigned specificPosition(){
	//TODO: staticproblem
	static const unsigned thrspecpos = Thread::specificId();
	return thrspecpos;
}
Manager& Manager::the(){
	return *reinterpret_cast<Manager*>(Thread::specific(specificPosition()));
}
#else
namespace{
	Manager *globalpm(NULL);
}
/*static*/ Manager& Manager::the(){
	return *globalpm;
}
#endif
//---------------------------------------------------------
Manager::Manager():d(*(new Data)){
#ifndef NSINGLETON_MANAGER
	globalpm = this;
#endif
	
}
//---------------------------------------------------------
/*virtual*/ Manager::~Manager(){
	stop(true);
	//stop all schedulers
	
	
	delete &d;
}
//---------------------------------------------------------
void Manager::start(){
}
//---------------------------------------------------------
void Manager::stop(bool _wait){
}
//---------------------------------------------------------
bool Manager::signal(ulong _sm){
}
//---------------------------------------------------------
bool Manager::signal(ulong _sm, const ObjectUidT &_ruid){
}
//---------------------------------------------------------
bool Manager::signal(ulong _sm, IndexT _fullid, uint32 _uid){
}
//---------------------------------------------------------
bool Manager::signal(ulong _sm, const Object &_robj){
}
//---------------------------------------------------------
bool Manager::signal(DynamicSharedPointer<Signal> &_rsig){
}
//---------------------------------------------------------
bool Manager::signal(DynamicPointer<Signal> &_rsig, const ObjectUidT &_ruid){
}
//---------------------------------------------------------
bool Manager::signal(DynamicPointer<Signal> &_rsig, IndexT _fullid, uint32 _uid){
}
//---------------------------------------------------------
bool Manager::signal(DynamicPointer<Signal> &_rsig, const Object &_robj){
}
//---------------------------------------------------------
void Manager::raiseObject(const Object &_robj){
}
//---------------------------------------------------------
Mutex& Manager::mutex(const Object &_robj)const{
}
//---------------------------------------------------------
uint32  Manager::uid(const Object &_robj)const{
}
//---------------------------------------------------------
Service& Manager::service(const IndexT &_i)const{
}
//---------------------------------------------------------
Object& Manager::object(const IndexT &_i)const{
}
//---------------------------------------------------------
/*virtual*/ SpecificMapper*  Manager::specificMapper(){
}
//---------------------------------------------------------

/*virtual*/ GlobalMapper* Manager::globalMapper(){
}
//---------------------------------------------------------
/*virtual*/ void Manager::doPrepareThread(){
}
//---------------------------------------------------------
/*virtual*/ void Manager::doUnprepareThread(){
}
//---------------------------------------------------------
unsigned Manager::serviceCount()const{
}
//---------------------------------------------------------
void Manager::prepareThread(SelectorBase *_ps){
}
//---------------------------------------------------------
void Manager::unprepareThread(SelectorBase *_ps){
}
//---------------------------------------------------------
void Manager::prepareThis(){
}
//---------------------------------------------------------
void Manager::unprepareThis(){
}
//---------------------------------------------------------
uint Manager::newSchedulerTypeId(){
}
//---------------------------------------------------------
uint Manager::newServiceTypeId(){
}
//---------------------------------------------------------
uint Manager::newObjectTypeId(){
}
//---------------------------------------------------------
uint Manager::doRegisterScheduler(SchedulerBase *_ps, uint _typeid){
}
//---------------------------------------------------------
ObjectUidT Manager::doRegisterService(
	Service *_ps,
	const IndexT &_idx,
	ScheduleCbkT _pschcbk,
	uint _schedidx
){
	
}
//---------------------------------------------------------
ObjectUidT Manager::doRegisterObject(
	Object *_po,
	const IndexT &_idx,
	ScheduleCbkT _pschcbk,
	uint _schedidx
){
	
}
//---------------------------------------------------------
SchedulerBase* Manager::doGetScheduler(uint _typeid, uint _idx)const{
}
//---------------------------------------------------------
Object* Manager::doGetObject(uint _typeid, const IndexT &_ridx)const{
}
//---------------------------------------------------------
Service* Manager::doGetService(uint _typeid, const IndexT &_ridx)const{
}
//---------------------------------------------------------
Object* Manager::doGetObject(uint _typeid)const{
}
//---------------------------------------------------------
Service* Manager::doGetService(uint _typeid)const{
}
//---------------------------------------------------------
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
//---------------------------------------------------------
}//namespace foundation
