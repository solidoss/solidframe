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
#include <memory>

namespace solid::frame::aio {

//-----------------------------------------------------------------------------
std::expected<ResolveData, std::error_code> DirectResolve::doRun()
{
    return synchronous_resolve(host, srvc, flags, family, type, proto);
}
//-----------------------------------------------------------------------------
std::expected<ReverseResolveData, std::error_code> ReverseResolve::doRun()
{
    return synchronous_resolve(addr, flags);
}
//-----------------------------------------------------------------------------
} // namespace solid::frame::aio
