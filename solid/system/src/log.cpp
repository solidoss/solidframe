// solid/system/src/log.cpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/system/log.hpp"

namespace solid {

//-----------------------------------------------------------------------------
//  LogCategory
//-----------------------------------------------------------------------------
/*static*/ const LogCategory& LogCategory::the()
{
    static const LogCategory lc;
    return lc;
}

//-----------------------------------------------------------------------------
//  Engine
//-----------------------------------------------------------------------------
namespace {

} //namespace

//-----------------------------------------------------------------------------
//  log_start
//-----------------------------------------------------------------------------
const LoggerT basic_logger{"basic"};

ErrorConditionT log_start(std::ostream& _ros, const std::string& _rmask, std::vector<std::string>&& _rmodule_mask_vec)
{
    return ErrorConditionT();
}

ErrorConditionT log_start(
    const char*        _fname,
    const std::string& _rmask, std::vector<std::string>&& _rmodule_mask_vec,
    bool     _buffered,
    uint32_t _respincnt,
    uint64_t _respinsize)
{
    return ErrorConditionT();
}

ErrorConditionT log_start(
    const char*        _addr,
    const char*        _port,
    const std::string& _rmask, std::vector<std::string>&& _rmodule_mask_vec,
    bool _buffered)
{
    return ErrorConditionT();
}

} //namespace solid
