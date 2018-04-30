// solid/system/log.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/common.hpp"
#include "solid/system/error.hpp"
#include "solid/system/flags.hpp"
#include <ostream>
#include <string>

namespace solid {

enum struct LogLevels {
    Verbose,
    Info,
    Warning,
    Error,
    LastFlag
};

struct LogCategory {
    static const LogCategory& basic();

    virtual ~LogCategory();
};

using LogFlagsT = Flags<LogLevels>;

template <class Flgs = LogFlagsT>
class Logger {
    const size_t idx_;

public:
    using ThisT = Logger<Flgs>;
    using FlagT = typename Flgs::FlagT;

    Logger(const std::string& _name, const LogCategory& _rcat = LogCategory::basic());

    bool shouldLog(const FlagT _flag) const;

    std::ostream& log(const FlagT _flag, const char* _file, const char* _fnc, int _line) const;
    void          done() const;
};

using LoggerT = Logger<>;

ErrorConditionT log_start(...);

} //namespace solid

#ifndef CRT_FUNCTION_NAME
#ifdef SOLID_ON_WINDOWS
#define CRT_FUNCTION_NAME __func__
#else
#define CRT_FUNCTION_NAME __FUNCTION__
#endif
#endif

#ifdef SOLID_HAS_DEBUG

#else

#define solid_dbg(...)

#endif

#define solid_log(...)
