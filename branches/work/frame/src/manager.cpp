/* Implementation file manager.cpp
	
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

#include <vector>
#include <deque>
#include <atomic>

#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/specific.hpp"
#include "system/exception.hpp"

#include "frame/manager.hpp"
#include "frame/message.hpp"

namespace solid{
namespace frame{

typedef std::atomic<size_t>			AtomicSizeT;
struct ObjectStub{
	
};
typedef std::deque<ObjectStub>		ObjectVectorT;


struct ServiceStub{

	size_t		objvecsz;
};

typedef std::vector<ServiceStub>	ServiceVectorT;

struct Manager::Data{
	Data(
		const size_t _svcprovisioncp
	):svcprovisioncp(_svcprovisioncp){
		svcvec.resize(_svcprovisioncp);
	}
	
	AtomicSizeT		marcker;
	AtomicSizeT		svcvecsz;
	const size_t	svcprovisioncp;
	Mutex			mtx;
	Condition		cnd;
	ServiceVectorT	svcvec;
};

/*static*/ Manager& Manager::specific(){
	
}
	
Manager::Manager(
	const size_t _svcprovisioncp,
	int _objpermutbts, int _mutrowsbts, int _mutcolsbts
):d(*(new Data(_svcprovisioncp))){
}

/*virtual*/ Manager::~Manager(){
	stop();
	delete &d;
}

void Manager::stop(){
	
}
//void start();
//void stop(bool _wait = true);

bool	Manager::registerService(Service &_rs){
	Locker<Mutex>	lock(d.mtx);
}

ObjectUidT	Manager::registerObject(Object &_ro){
	Locker<Mutex>	lock(d.mtx);
}

bool Manager::notify(ulong _sm, const ObjectUidT &_ruid){
	size_t		svcidx;
	size_t		objidx;
	
	
// 	_ruid.split(svcidx, objidx, d.marker.load());
// 	
// 	if(svcidx < d.svcprovisioncp){
// 		//lock free at service level
// 		if(svcidx < d.svcvecsz.load()){
// 			ServiceStub &rss(d.svcvec[svcidx]);
// 		}
// 	}else{
// 		
// 	}
}

/*virtual*/ SpecificMapper*  Manager::specificMapper(){
	return NULL;
}
	
/*virtual*/ GlobalMapper* Manager::globalMapper(){
	return NULL;
}

/*virtual*/ void Manager::doPrepareThread(){
	
}
/*virtual*/ void Manager::doUnprepareThread(){
	
}

bool Manager::notify(MessagePointerT &_rmsgptr, const ObjectUidT &_ruid){
	
}

void Manager::raise(const Object &_robj){
	
}

Mutex& Manager::mutex(const Object &_robj)const{
	
}

ObjectUidT  Manager::id(const Object &_robj)const{
	
}

Mutex& Manager::serviceMutex(const Service &_rsvc){
	
}
ObjectUidT Manager::registerServiceObject(const Service &_rsvc, Object &_robj){
	
}
Object* Manager::nextServiceObject(const Service &_rsvc, VisitContext &_rctx){
	
}

IndexT Manager::computeThreadId(const IndexT &_selidx, const IndexT &_objidx){
	return 0;
}
void Manager::prepareThread(SelectorBase *_ps){
	
}
void Manager::unprepareThread(SelectorBase *_ps){
	
}

}//namespace frame
}//namespace solid