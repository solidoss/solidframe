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

#include "solid/system/exception.hpp"
#include <condition_variable>

#include "solid/system/mutualstore.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"

#include "solid/frame/actor.hpp"
#include "solid/frame/common.hpp"
#include "solid/frame/manager.hpp"
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
    solid_dbg(logger, Verbose, "" << this);
}

Service::~Service()
{
    stop(true);
    rm.unregisterService(*this);
    solid_dbg(logger, Verbose, "" << this);
}

} //namespace frame
} //namespace solid
