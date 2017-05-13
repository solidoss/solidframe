// solid/frame/src/service.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <algorithm>
#include <cstring>
#include <deque>
#include <vector>

#include "solid/system/debug.hpp"
#include "solid/system/exception.hpp"
#include <condition_variable>

#include "solid/system/mutualstore.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"

#include "solid/frame/common.hpp"
#include "solid/frame/manager.hpp"
#include "solid/frame/object.hpp"
#include "solid/frame/service.hpp"

namespace solid {
namespace frame {

Service::Service(
    UseServiceShell _force_shell)
    : rm(_force_shell.rmanager)
    , idx(static_cast<size_t>(InvalidIndex()))
    , running(false)
{
    rm.registerService(*this);
    vdbgx(Debug::frame, "" << this);
}

Service::~Service()
{
    vdbgx(Debug::frame, "" << this);
    stop(true);
    rm.unregisterService(*this);
    vdbgx(Debug::frame, "" << this);
}

void Service::notifyAll(Event const& _revt)
{
    rm.notifyAll(*this, _revt);
}

bool Service::start()
{
    return rm.startService(*this);
}

void Service::stop(const bool _wait)
{
    rm.stopService(*this, _wait);
}

std::mutex& Service::mutex(const ObjectBase& _robj) const
{
    return rm.mutex(_robj);
}

std::mutex& Service::mutex() const
{
    return rm.mutex(*this);
}

ObjectIdT Service::registerObject(ObjectBase& _robj, ReactorBase& _rr, ScheduleFunctionT& _rfct, ErrorConditionT& _rerr)
{
    return rm.registerObject(*this, _robj, _rr, _rfct, _rerr);
}

// void Service::unsafeStop(Locker<Mutex> &_rlock, bool _wait){
//  const size_t    svcidx = idx.load(/*std::memory_order_seq_cst*/);
//  rm.doWaitStopService(svcidx, _rlock, true);
// }

} //namespace frame
} //namespace solid
