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


#include "frame/ipc/ipcservice.hpp"
#include "frame/ipc/ipcsessionuid.hpp"
#include "frame/ipc/ipcmessage.hpp"

#include "system/mutualstore.hpp"
#include "system/atomic.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "ipcutility.hpp"

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

typedef std::pair<size_t, bool>							SizeBoolPairT;
typedef CPP11_NS::unordered_map<size_t, SizeBoolPairT>	MessageTypeIdMapT;

struct Service::Data{
	struct MessageStub{
		MessageStub(){}
		MessageStub(
			MessageFactoryFunctionT&	_rfactoryfnc,
			MessageStoreFunctionT&		_rstorefnc,
			MessageLoadFunctionT&		_rloadfnc,
			MessageReceiveFunctionT&	_rreceivefnc,
			MessagePrepareFunctionT&	_rpreparefnc,
			MessageCompleteFunctionT&	_rcompletefnc
		):	factoryfnc(std::move(_rfactoryfnc)),
			storefnc(std::move(_rstorefnc)),
			loadfnc(std::move(_rloadfnc)),
			receivefnc(std::move(_rreceivefnc)),
			preparefnc(std::move(_rpreparefnc)),
			completefnc(std::move(_rcompletefnc))
		{
		}
		
		MessageFactoryFunctionT		factoryfnc;
		MessageStoreFunctionT		storefnc;
		MessageLoadFunctionT		loadfnc;
		MessageReceiveFunctionT		receivefnc;
		MessagePrepareFunctionT		preparefnc;
		MessageCompleteFunctionT	completefnc;
	};
	
	typedef std::vector<MessageStub>			MessageVectorT;
	
	MessageVectorT		msgvec;
	MessageTypeIdMapT	msgtypeidmap;
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
	return ErrorConditionT();
}
//-----------------------------------------------------------------------------
void Service::doRegisterMessage(
	DynamicIdVectorT const& 	_rtypeidvec,
	MessageFactoryFunctionT&	_rfactoryfnc,
	MessageStoreFunctionT&		_rstorefnc,
	MessageLoadFunctionT&		_rloadfnc,
	MessageReceiveFunctionT&	_rreceivefnc,
	MessagePrepareFunctionT&	_rpreparefnc,
	MessageCompleteFunctionT&	_rcompletefnc,
	size_t	_idx
){
	if(_idx != static_cast<size_t>(-1)){
		_idx = d.msgvec.size();
		d.msgvec.push_back(Data::MessageStub(_rfactoryfnc, _rstorefnc, _rloadfnc, _rreceivefnc, _rpreparefnc, _rcompletefnc));
	}else{
		if(_idx >= d.msgvec.size()){
			d.msgvec.resize(_idx + 1);
		}
		if(FUNCTION_EMPTY(d.msgvec[_idx].factoryfnc)){
			Data::MessageStub &rmsgstub = d.msgvec[_idx];
			rmsgstub.factoryfnc = std::move(_rfactoryfnc);
			rmsgstub.storefnc = std::move(_rstorefnc);
			rmsgstub.loadfnc = std::move(_rloadfnc);
			rmsgstub.receivefnc = std::move(_rreceivefnc);
			rmsgstub.preparefnc = std::move(_rpreparefnc);
			rmsgstub.completefnc = std::move(_rcompletefnc);
		}else{
			THROW_EXCEPTION_EX("Message index already used: ", _idx);
			return;
		}
	}
	for(auto it = _rtypeidvec.begin(); it != _rtypeidvec.end(); ++it){
		auto mapit = d.msgtypeidmap.find(*it);
		if(mapit != d.msgtypeidmap.end()){
			d.msgtypeidmap[*it] = SizeBoolPairT(_idx, it == _rtypeidvec.begin());
		}else{
			//we have a collision
			if(it == _rtypeidvec.begin()){
				//replace the existing entry - we have both a message and one of its ancestors as targeted messages
				d.msgtypeidmap[*it] = SizeBoolPairT(_idx, true);
			}else{
				//remove the existing entry - a common ancestor
				d.msgtypeidmap.erase(mapit);
			}
		}
	}
}
//-----------------------------------------------------------------------------
//=============================================================================
//-----------------------------------------------------------------------------
/*virtual*/ Message::~Message(){
	
}
//-----------------------------------------------------------------------------
//=============================================================================
typedef std::pair<size_t, bool>		SizeBoolPairT;
typedef std::vector<SizeBoolPairT>	SizeBoolVectorT;

struct MessageRegisterProxy::Data{
	SizeBoolVectorT		testvec;
};

MessageRegisterProxy::MessageRegisterProxy():pservice(nullptr), pd(new Data), pass_test(true){}

MessageRegisterProxy::~MessageRegisterProxy(){
	delete pd;
}
bool MessageRegisterProxy::check(DynamicIdVectorT const &_idvec){
	for(auto it = _idvec.begin(); it != _idvec.end(); ++it){
		for(auto tit = pd->testvec.begin(); tit != pd->testvec.end(); ++tit){
			if(tit->first == *it){
				if(it == _idvec.begin()){
					if(tit->second){//also a head
						return false;
					}
					//make head
					tit->second = true;
				}
			}else{
				pd->testvec.push_back(SizeBoolPairT(*it, it == _idvec.begin()));
			}
		}
	}
	return true;
}
	
//=============================================================================
}//namespace ipc
}//namespace frame
}//namespace solid


