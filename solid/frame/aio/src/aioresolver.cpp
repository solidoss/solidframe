// solid/frame/aio/src/aioresolver.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/frame/aio/aioresolver.hpp"
#include "solid/utility/dynamicpointer.hpp"

#include <memory>

namespace solid {
namespace frame {
namespace aio {

//-----------------------------------------------------------------------------
ResolveData DirectResolve::doRun()
{
    return synchronous_resolve(host.c_str(), srvc.c_str(), flags, family, type, proto);
}
//-----------------------------------------------------------------------------
void ReverseResolve::doRun(std::string& _rhost, std::string& _rsrvc)
{
    synchronous_resolve(_rhost, _rsrvc, addr, flags);
}
//-----------------------------------------------------------------------------
} // namespace aio
} //namespace frame
} //namespace solid
