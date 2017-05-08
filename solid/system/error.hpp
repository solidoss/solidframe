// solid/system/error.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/solid_config.hpp"

#ifdef SOLID_USE_CPP11
#define ERROR_NS std
#else
#define ERROR_NS boost::system
#endif

#ifdef SOLID_USE_CPP11
#include <system_error>
#else
#include "boost/system/error_code.hpp"
#endif

#include <ostream>
#include <vector>

namespace solid {

typedef ERROR_NS::error_condition ErrorConditionT;
typedef ERROR_NS::error_code      ErrorCodeT;
typedef ERROR_NS::error_category  ErrorCategoryT;

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
        return ErrorCodeT(value, category ? *category : ERROR_NS::system_category());
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
