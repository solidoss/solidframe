// solid/system/error.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/solid_config.hpp"

#include <ostream>
#include <system_error>
#include <vector>

namespace solid {

typedef std::error_condition ErrorConditionT;
typedef std::error_code      ErrorCodeT;
typedef std::error_category  ErrorCategoryT;

ErrorCodeT last_system_error();

struct ErrorStub {
    ErrorStub(
        int                   _value    = -1,
        ErrorCategoryT const* _category = nullptr,
        unsigned              _line     = -1,
        const char*           _file     = nullptr)
        : value(_value)
        , category(_category)
        , line(_line)
        , file(_file)
    {
    }

    ErrorStub(
        ErrorCodeT const& _code,
        unsigned          _line = -1,
        const char*       _file = nullptr)
        : value(_code.value())
        , category(&_code.category())
        , line(_line)
        , file(_file)
    {
    }

    ErrorCodeT errorCode() const
    {
        return ErrorCodeT(value, category ? *category : std::system_category());
    }

    int                   value;
    ErrorCategoryT const* category;
    unsigned              line;
    const char*           file;
};

typedef std::vector<ErrorStub> ErrorVectorT;

extern const ErrorCodeT error_not_implemented;
extern const ErrorCodeT error_system;
extern const ErrorCodeT error_thread_started;

} //namespace solid
