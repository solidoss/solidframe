// alphasteward.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "alphasteward.hpp"
#include "alphamessages.hpp"
#include "core/manager.hpp"
#include "utility/timerqueue.hpp"
#include "utility/stack.hpp"
#include "frame/ipc/ipcservice.hpp"
#include <vector>
#include <deque>


using namespace solid;

namespace concept{
namespace alpha{

typedef solid::DynamicMapper<void, Steward>		DynamicMapperT;
typedef std::vector<solid::DynamicPointer<> >	DynamicPointerVectorT;

namespace{

DynamicMapperT	dm;

Steward* dynamicInit(Steward *_ps){
	dm.insert<RemoteListMessage, Steward>();
	dm.insert<FetchMasterMessage, Steward>();
	dm.insert<FetchSlaveMessage, Steward>();
	return _ps;
}

}//namespace


typedef TimerQueue<size_t>								IndexTimerQueueT;
typedef Stack<size_t>									IndexStackT;
typedef std::deque<DynamicPointer<RemoteListMessage> >	RemoteListDequeT;
struct Steward::Data{
	Manager					&rm;
	DynamicPointerVectorT	dv;
	IndexTimerQueueT		listtq;
	RemoteListDequeT		listdq;
	IndexStackT				listcache;
	Data(Manager &_rmgr):rm(_rmgr){}
};

/*static*/ Steward& Steward::the(Steward *_ps/* = NULL*/){
	static Steward *ps = dynamicInit(_ps);
	return *ps;
}

Steward::Steward(Manager &_rmgr):d(*(new Data(_rmgr))){
	the(this);
}

Steward::~Steward(){
	delete &d;
}

void Steward::sendMessage(solid::DynamicPointer<solid::frame::Message> &_rmsgptr){
	d.rm.notify(_rmsgptr, d.rm.id(*this));
}

/*virtual*/ bool Steward::notify(solid::DynamicPointer<solid::frame::Message> &_rmsgptr){
	d.dv.push_back(DynamicPointer<>(_rmsgptr));
	return Object::notify(frame::S_SIG | frame::S_RAISE);
}

/*virtual*/ void Steward::execute(ExecuteContext &_rexectx){
	if(notified()){//we've received a signal
		DynamicHandler<DynamicMapperT>	dh(dm);
		ulong 							sm(0);
		{
			Locker<Mutex>	lock(d.rm.mutex(*this));
			sm = grabSignalMask(0);//grab all bits of the signal mask
			if(sm & frame::S_KILL){
				_rexectx.close();
				return;
			}
			if(sm & frame::S_SIG){//we have signals
				dh.init(d.dv.begin(), d.dv.end());
				d.dv.clear();
			}
		}
		if(sm & frame::S_SIG){//we've grabed signals, execute them
			for(size_t i = 0; i < dh.size(); ++i){
				dh.handle(*this, i);
			}
		}
	}
	while(d.listtq.isHit(_rexectx.currentTime())){
		const size_t idx = d.listtq.frontValue();
		d.listtq.pop();
		
		doExecute(d.listdq[idx]);
		
		d.listdq[idx].clear();
		d.listcache.push(idx);
	}
	if(d.listtq.size()){
		_rexectx.waitUntil(d.listtq.frontTime());
	}
}

void Steward::dynamicHandle(solid::DynamicPointer<> &_dp){
	cassert(false);
}

void Steward::dynamicHandle(solid::DynamicPointer<RemoteListMessage> &_rmsgptr){
	if(_rmsgptr->tout){
		size_t		idx;
		TimeSpec	ts = Steward::currentTime();
		
		ts += _rmsgptr->tout;
		
		if(d.listcache.size()){
			idx = d.listcache.top();
			d.listcache.pop();
			d.listdq[idx] = _rmsgptr;
		}else{
			idx = d.listdq.size();
			d.listdq.push_back(_rmsgptr);
		}
		
		d.listtq.push(ts, idx);
	}else{
		doExecute(_rmsgptr);
	}
}

void Steward::dynamicHandle(solid::DynamicPointer<FetchMasterMessage> &_rmsgptr){
	
}

void Steward::dynamicHandle(solid::DynamicPointer<FetchSlaveMessage> &_rmsgptr){
	
}

void Steward::doExecute(solid::DynamicPointer<RemoteListMessage> &_rmsgptr){
	fs::directory_iterator				it,end;
	fs::path							pth(_rmsgptr->strpth.c_str()/*, fs::native*/);
	
	RemoteListMessage					&rmsg = *_rmsgptr;
	DynamicPointer<frame::ipc::Message>	msgptr(_rmsgptr);
	
	rmsg.ppthlst = new RemoteList::PathListT;
	rmsg.strpth.clear();
	
	if(!exists( pth ) || !is_directory(pth)){
		d.rm.ipc().sendMessage(msgptr, rmsg.conid);
		return;
	}
	
	try{
		it = fs::directory_iterator(pth);
	}catch ( const std::exception & ex ){
		idbg("dir_iterator exception :"<<ex.what());
		rmsg.err = -1;
		rmsg.strpth = ex.what();
		d.rm.ipc().sendMessage(msgptr, rmsg.conid);
		return;
	}
	
	while(it != end){
		rmsg.ppthlst->push_back(std::pair<String, int64>(it->path().c_str(), -1));
		if(is_directory(*it)){
		}else{
			rmsg.ppthlst->back().second = FileDevice::size(it->path().c_str());
		}
		++it;
	}
	rmsg.err = 0;
	d.rm.ipc().sendMessage(msgptr, rmsg.conid);
}

}//namespace alpha
}//namespace concept
