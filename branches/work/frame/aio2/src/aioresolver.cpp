// frame/aio/src/aioresolver.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "frame/aio/aioresolver.hpp"
#include "utility/workpool.hpp"
#include "utility/dynamicpointer.hpp"

#include <memory>

namespace solid{
namespace frame{
namespace aio{

typedef DynamicPointer<ResolveBase>		ResolverPointerT;

struct WorkPoolController;
typedef WorkPool<ResolverPointerT, WorkPoolController>	WorkPoolT;

struct WorkPoolController: WorkPoolControllerBase{
	size_t		maxthrcnt;
	WorkPoolController(size_t _thrcnt):maxthrcnt(_thrcnt){}
	
	bool createWorker(WorkPoolT &_rwp, ushort _wkrcnt){
		if(_wkrcnt <= maxthrcnt){
			_rwp.createSingleWorker()->start();
			return true;
		}else{
			return false;
		}
	}
	void execute(WorkPoolBase &_wp, WorkerBase &, ResolverPointerT & _ptr){
		_ptr->run(_wp.isStopping());
	}
};

struct Resolver::Data{
	Data(size_t _thrcnt):wp(_thrcnt){}
	
	WorkPoolT	wp;
	size_t		thrcnt;
};

Resolver::Resolver(size_t _thrcnt):d(*(new Data(_thrcnt))){
	
	if(_thrcnt == 0){
		d.wp.controller().maxthrcnt = Thread::processorCount();
	}
}

Resolver::~Resolver(){
	delete &d;
}

void Resolver::start(ushort _thrcnt){
	d.wp.start(_thrcnt);
}

void Resolver::doSchedule(ResolveBase *_pb){
	ResolverPointerT ptr(_pb);
	d.wp.push(ptr);
}

//---------------------------------------------------------------
ResolveData DirectResolve::doRun(){
	return synchronous_resolve(host.c_str(), srvc.c_str(), flags, family, type, proto);
}

void ReverseResolve::doRun(std::string &_rhost, std::string &_rsrvc){
	synchronous_resolve(_rhost, _rsrvc, addr, flags);
}

}//namespace aio;
}//namespace frame
}//namespace solid

