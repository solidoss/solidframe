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
struct Service::Data{
	
};
//=============================================================================

Service::Service(
	frame::Manager &_rm, frame::Event const &_revt
):BaseT(_rm, _revt), d(*(new Data)){}
	
//! Destructor
Service::~Service(){
	delete &d;
}

ErrorConditionT Service::reconfigure(Configuration const& _rcfg){
	return ErrorConditionT();
}

//-----------------------------------------------------------------------------
/*virtual*/ Message::~Message(){
	
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid


