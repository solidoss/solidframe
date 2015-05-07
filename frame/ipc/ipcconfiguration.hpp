// frame/ipc/ipcconfiguration.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_IPCCONFIGURATION_HPP
#define SOLID_FRAME_IPC_IPCCONFIGURATION_HPP

#include <vector>
#include "system/function.hpp"
#include "system/socketaddress.hpp"
#include "frame/aio/aioreactor.hpp"
#include "frame/aio/aioreactorcontext.hpp"
#include "frame/scheduler.hpp"



namespace solid{
namespace frame{

namespace aio{
class Resolver;
}

namespace ipc{

struct	ServiceProxy;
class	Service;

typedef frame::Scheduler<frame::aio::Reactor> 							AioSchedulerT;

typedef std::vector<SocketAddressInet>									AddressVectorT;
	
typedef FUNCTION<void(ServiceProxy &)>									MessageRegisterFunctionT;
typedef FUNCTION<void(AddressVectorT &)>								ResolveCompleteFunctionT;
typedef FUNCTION<void(const std::string&, ResolveCompleteFunctionT&)>	AsyncResolveFunctionT;

struct Configuration{
	
	Configuration(AioSchedulerT &_rsch): psch(&_rsch){}
	
	Configuration():psch(nullptr){}
	
	template <class F>
	void protocolCallback(F _f);
	AioSchedulerT & scheduler(){
		return *psch;
	}
	
	AioSchedulerT				*psch;
	
	MessageRegisterFunctionT	regfnc;
	Event 						event_start;
	Event						event_raise;
	AsyncResolveFunctionT		resolve_fnc;
};

template <class F>
void Configuration::protocolCallback(F _f){
	regfnc = _f;
}


struct ResolverF{
	aio::Resolver	&rresolver;
	std::string		default_service;
	
	ResolverF(
		aio::Resolver &_rresolver,
		const char *_default_service
	):	rresolver(_rresolver), default_service(_default_service){}
	
	void operator()(const std::string&, ResolveCompleteFunctionT&);
};

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
