// frame/ipc/src/ipcservice.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <vector>
#include <cstring>
#include <utility>
#include <algorithm>

#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/socketdevice.hpp"
#include "system/specific.hpp"
#include "system/exception.hpp"

#include "utility/queue.hpp"
#include "utility/binaryseeker.hpp"

#include "frame/common.hpp"
#include "frame/manager.hpp"

#include "frame/aio/aioresolver.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/ipc/ipccontext.hpp"
#include "frame/ipc/ipcmessage.hpp"
#include "frame/ipc/ipcconfiguration.hpp"

#include "system/mutualstore.hpp"
#include "system/atomic.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "ipcutility.hpp"
#include "ipcsession.hpp"
#include "ipclistener.hpp"


#include <vector>
#include <deque>

#ifdef HAS_CPP11
#define CPP11_NS std
#include <unordered_map>
#else
#include "boost/unordered_map.hpp"
#define CPP11_NS boost
#endif

namespace solid{
namespace frame{
namespace ipc{
//=============================================================================
typedef CPP11_NS::unordered_map<const char*, size_t>	NameMapT;

struct SessionStub{
	SessionStub():uid(0){}
	void clear(){
		name.clear();
		sessionuid = ObjectUidT();
		uid = -1;
	}
	std::string		name;
	ObjectUidT 		sessionuid;
	uint32			uid;
};

typedef std::deque<SessionStub>							SessionDequeT;
typedef Stack<size_t>									SizeStackT;


struct Service::Data{
	
	Mutex			mtx;
	NameMapT		namemap;
	SessionDequeT	sessiondq;
	SizeStackT		cachestk;
	Configuration	config;
};
//=============================================================================

Service::Service(
	frame::Manager &_rm, frame::Event const &_revt
):BaseT(_rm, _revt), d(*(new Data)){}
	
//! Destructor
Service::~Service(){
	delete &d;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::reconfigure(Configuration const& _rcfg){
	ServiceProxy	sp(*this);
	_rcfg.regfnc(sp);
	return ErrorConditionT();
}
//-----------------------------------------------------------------------------
struct PushMessageVisitorF{
	PushMessageVisitorF(
		MessagePointerT	&_rmsgptr,
		ConnectionUid const  &_rconuid_in,
		ConnectionUid	*_pconuid_out,
		ulong			_flags,
		Event const &	_revt
	):rmsgptr(_rmsgptr), rconuid_in(_rconuid_in), pconuid_out(_pconuid_out), flags(_flags), raise_event(_revt){}
	
	
	bool operator()(ObjectBase &_robj, ReactorBase &_rreact){
		if(static_cast<Session&>(_robj).pushMessage(rmsgptr, rconuid_in, pconuid_out, flags)){
			_rreact.raise(_robj.runId(), raise_event);
		}
		
		return true;
	}
	
	MessagePointerT		&rmsgptr;
	ConnectionUid const	&rconuid_in;
	ConnectionUid		*pconuid_out;
	ulong				flags;
	Event				raise_event;
};



struct OnRelsolveF{
	Manager		&rm;
	ObjectUidT	objuid;
	Event 		event;
	
	OnRelsolveF(
		Manager &_rm,
		const ObjectUidT &_robjuid,
		const Event &_revent
	):rm(_rm), objuid(_robjuid), event(_revent){}
	
	void operator()(AddressVectorT &_raddrvec){
		event.msgptr = new ResolveMessage(_raddrvec);
		rm.notify(objuid, event);
	}
};

ErrorConditionT Service::doSendMessage(
	const char *_session_name,
	const ConnectionUid	&_rconuid_in,
	MessagePointerT &_rmsgptr,
	ConnectionUid *_pconuid_out,
	ulong _flags
){
	solid::ErrorConditionT		err;
	Locker<Mutex>				lock(d.mtx);
	size_t						idx;
	if(_session_name){
		NameMapT::const_iterator	it = d.namemap.find(_session_name);
		
		if(it != d.namemap.end()){
			idx = it->second;
		}else{
			if(d.cachestk.size()){
				idx = d.cachestk.top();
				d.cachestk.pop();
			}else{
				idx = d.sessiondq.size();
				d.sessiondq.push_back(SessionStub());
			}
			SessionStub 					&rss(d.sessiondq[idx]);
			
			rss.name = _session_name;
			d.namemap[rss.name.c_str()] = idx;
			
			DynamicPointer<aio::Object>		objptr(new Session(idx));
			
			rss.sessionuid = d.config.scheduler().startObject(objptr, *this, d.config.start_event, err);
			if(err){
				rss.clear();
				d.cachestk.push(idx);
				return err;
			}
			//resolve the name
			ResolveCompleteFunctionT	cbk(OnRelsolveF(manager(), rss.sessionuid, d.config.raise_event));
			d.config.resolve_fnc(rss.name, cbk);
		}
	}else if(_rconuid_in.ssnidx < d.sessiondq.size() && _rconuid_in.ssnuid == d.sessiondq[_rconuid_in.ssnidx].uid){
		idx = _rconuid_in.ssnidx;
	}else{
		err.assign(-1, err.category());//TODO
		return err;
	}
	SessionStub 			&rss(d.sessiondq[idx]);
	PushMessageVisitorF		fnc(_rmsgptr, _rconuid_in, _pconuid_out, _flags, d.config.raise_event);
	bool 					rv = manager().visit(rss.sessionuid, fnc);
	
	if(!rv){
		err.assign(-1, err.category());//TODO
	}
	
	return err;
}
//-----------------------------------------------------------------------------
bool Service::isEventStart(Event const&_revent){
	return false;
}
//-----------------------------------------------------------------------------
bool Service::isEventStop(Event const&_revent){
	return false;
}
//-----------------------------------------------------------------------------
void Service::receiveConnection(SocketDevice &_rsd){
	
}
//=============================================================================
//-----------------------------------------------------------------------------
/*virtual*/ Message::~Message(){
	
}
//-----------------------------------------------------------------------------
//=============================================================================
struct ResolveF{
	ResolveCompleteFunctionT	cbk;
	
	void operator()(ResolveData &_rrd, ERROR_NS::error_code const &_rerr){
		AddressVectorT		addrvec;
		if(!_rerr){
			for(auto it = _rrd.begin(); it != _rrd.end(); ++it){
				addrvec.push_back(SocketAddressStub(it));
			}
		}
		cbk(addrvec);
	}
};

void ResolverF::operator()(const std::string&_name, ResolveCompleteFunctionT& _cbk){
	std::string		tmp;
	const char 		*hst_name;
	const char		*svc_name;
	
	size_t off = _name.rfind(':');
	if(off != std::string::npos){
		tmp = _name.substr(0, off);
		hst_name = tmp.c_str();
		svc_name = _name.c_str() + off + 1;
		if(!svc_name[0]){
			svc_name = default_service.c_str();
		}
	}else{
		hst_name = _name.c_str();
		svc_name = default_service.c_str();
	}
	
	ResolveF		fnc;
	
	fnc.cbk = std::move(_cbk);
	
	rresolver.requestResolve(fnc, hst_name, svc_name);
}
//=============================================================================
}//namespace ipc
}//namespace frame
}//namespace solid


