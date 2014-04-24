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
#include "core/messages.hpp"
#include "utility/timerqueue.hpp"
#include "utility/stack.hpp"
#include "frame/ipc/ipcservice.hpp"
#include "frame/file/filestore.hpp"
#include "frame/file/filestream.hpp"
#include "system/debug.hpp"
#include <vector>
#include <deque>


using namespace solid;
using namespace std;

std::streampos stream_size(std::istream &_rios){
	std::streampos pos = _rios.tellg();
	_rios.seekg(0, _rios.end);
	std::streampos endpos = _rios.tellg();
	_rios.seekg(pos);
	return endpos;
}

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
	dm.insert<FilePointerMessage, Steward>();
	return _ps;
}

}//namespace


typedef TimerQueue<size_t>								IndexTimerQueueT;
typedef Stack<size_t>									IndexStackT;
typedef std::deque<DynamicPointer<RemoteListMessage> >	RemoteListDequeT;
typedef std::pair<
	DynamicPointer<FetchMasterMessage>, uint32
>														FetchPairT;
typedef std::deque<FetchPairT>							FetchDequeT;

struct Steward::Data{
	Manager					&rm;
	DynamicPointerVectorT	dv;
	IndexTimerQueueT		listtq;
	RemoteListDequeT		listdq;
	IndexStackT				listcache;
	
	IndexTimerQueueT		fetchtq;
	FetchDequeT				fetchdq;
	IndexStackT				fetchcache;
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
		solid::ulong 							sm(0);
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

struct OpenCbk{
	frame::ObjectUidT	uid;
	size_t				fetchidx;
	uint32				fetchuid;
	OpenCbk(){}
	OpenCbk(
		const frame::ObjectUidT &_robjuid,
		size_t _fetchidx,
		uint32 _fetchuid
	):uid(_robjuid), fetchidx(_fetchidx), fetchuid(_fetchuid){}
	
	void operator()(
		frame::file::Store<> &,
		frame::file::FilePointerT &_rptr,
		ERROR_NS::error_code err
	){
		idbg("");
		frame::Manager 				&rm = frame::Manager::specific();
		frame::file::FilePointerT	emptyptr;
		frame::MessagePointerT		msgptr(new FilePointerMessage(err ? emptyptr : _rptr, fetchidx, fetchuid));
		rm.notify(msgptr, uid);
	}
};

void Steward::dynamicHandle(solid::DynamicPointer<FetchMasterMessage> &_rmsgptr){
	idbg("");
	size_t		idx;
	uint32		uid = 0;
	if(d.fetchcache.size()){
		idx = d.fetchcache.top();
		d.fetchcache.pop();
		d.fetchdq[idx].first = _rmsgptr;
		uid = d.fetchdq[idx].second;
	}else{
		idx = d.fetchdq.size();
		d.fetchdq.push_back(FetchPairT(_rmsgptr, 0));
	}
	d.rm.fileStore().requestOpenFile(OpenCbk(d.rm.id(*this), idx, uid), d.fetchdq[idx].first->fname, FileDevice::ReadOnlyE);
}

void Steward::dynamicHandle(solid::DynamicPointer<FilePointerMessage> &_rmsgptr){
	idbg("");
	const size_t	msgidx = _rmsgptr->reqidx;
	const uint32	msguid = _rmsgptr->requid;
	if(msgidx < d.fetchdq.size() && d.fetchdq[msgidx].second == msguid){
		
		solid::DynamicPointer<FetchMasterMessage>	&rmsgptr = d.fetchdq[msgidx].first;
		
		rmsgptr->fileptr = _rmsgptr->ptr;
		
		if(!rmsgptr->fileptr.empty()){
			d.rm.fileStore().uniqueToShared(rmsgptr->fileptr);
			idbg((void*)this<<" send first stream");
			FetchSlaveMessage						*pmsg(new FetchSlaveMessage);
			DynamicPointer<frame::ipc::Message>		msgptr(pmsg);
			ERROR_NS::error_code					err;
			
			pmsg->ipcResetState(rmsgptr->ipcState());
			rmsgptr->filesz = rmsgptr->fileptr->size();
			
			pmsg->tov = rmsgptr->fromv;
			pmsg->filesz = rmsgptr->filesz;
			pmsg->streamsz = rmsgptr->streamsz;
			if(pmsg->streamsz > pmsg->filesz){
				pmsg->streamsz = pmsg->filesz;
			}
			pmsg->msguid = solid::frame::UidT(msgidx, msguid);
			pmsg->requid = rmsgptr->requid;
			pmsg->fuid = rmsgptr->tmpfuid;
			idbg("filesz = "<<rmsgptr->filesz<<" inpos = "<<rmsgptr->filepos);
			rmsgptr->filesz -= pmsg->streamsz;
			pmsg->filepos = rmsgptr->filepos;
			rmsgptr->filepos += pmsg->streamsz;
			{
				frame::file::FilePointerT	fptr = d.rm.fileStore().shared(rmsgptr->fileptr.id(), err);
				pmsg->ios.device(fptr);
			}
 			d.rm.ipc().sendMessage(msgptr, rmsgptr->conid);
			if(rmsgptr->filesz){
				idbg(" filesz = "<<rmsgptr->filesz);
				return;
			}
		}else{
			FetchSlaveMessage 						*pmsg = new FetchSlaveMessage;
			DynamicPointer<frame::ipc::Message>		msgptr(pmsg);
			pmsg->tov = rmsgptr->fromv;
			pmsg->filesz = -1;
			d.rm.ipc().sendMessage(msgptr, rmsgptr->conid);
		}
		doClearFetch(msgidx);
	}
}

void Steward::dynamicHandle(solid::DynamicPointer<FetchSlaveMessage> &_rmsgptr){
	idbg("");
	const size_t	msgidx = _rmsgptr->msguid.first;
	const uint32	msguid = _rmsgptr->msguid.first;
	if(msgidx < d.fetchdq.size() && d.fetchdq[msgidx].second == msguid){
		FetchSlaveMessage							*pmsg(_rmsgptr.get());
		DynamicPointer<frame::ipc::Message>			msgptr(_rmsgptr);
		solid::DynamicPointer<FetchMasterMessage>	&rmsgptr = d.fetchdq[msgidx].first;
		ERROR_NS::error_code						err;
		
		{
			frame::file::FilePointerT	fptr = d.rm.fileStore().shared(rmsgptr->fileptr.id(), err);
			pmsg->ios.device(fptr);
		}
		
		pmsg->filepos = rmsgptr->filepos;
		if(pmsg->streamsz > rmsgptr->filesz){
			pmsg->streamsz = rmsgptr->filesz;
		}
		
		rmsgptr->filepos += pmsg->streamsz;
		rmsgptr->filesz -= pmsg->streamsz;
		
		d.rm.ipc().sendMessage(msgptr, rmsgptr->conid);
		
		if(!rmsgptr->filesz){
			idbg(" filesz = "<<rmsgptr->filesz);
			doClearFetch(msgidx);
		}
	}
}


void Steward::doExecute(solid::DynamicPointer<RemoteListMessage> &_rmsgptr){
	idbg("");
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

void Steward::doClearFetch(const size_t _idx){
	d.fetchdq[_idx].first.clear();
	d.fetchcache.push(_idx);
}
}//namespace alpha
}//namespace concept
