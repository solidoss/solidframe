// solid/system/src/error.cpp
//
// Copyright (c) 2013,2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/system/error.hpp"
#include "solid/system/exception.hpp"
#include <sstream>
#ifdef SOLID_ON_WINDOWS
#define NOMINMAX
#include <Windows.h>
#endif

namespace solid {

ErrorCodeT last_system_error()
{
#ifdef SOLID_ON_WINDOWS
    const DWORD err = GetLastError();
    return ErrorCodeT(err, std::system_category());
#else
    return ErrorCodeT(errno, std::system_category());
#endif
}

namespace {
enum {
    ErrorNotImplementedE = 1,
    ErrorSystemE,
    ErrorThreadStartedE
};

class ErrorCategory : public ErrorCategoryT {
public:
    ErrorCategory() {}
    const char* name() const noexcept override
    {
        return "solid";
    }
    std::string message(int _ev) const override;
};

const ErrorCategory category;

std::string ErrorCategory::message(int _ev) const
{
    std::ostringstream oss;

    oss << "(" << name() << ":" << _ev << "): ";

    switch (_ev) {
    case 0:
        oss << "Success";
        break;
    case ErrorNotImplementedE:
        oss << "Functionality not implemented";
        break;
    case ErrorSystemE:
        oss << "System";
        break;
    case ErrorThreadStartedE:
        oss << "Thread started";
        break;
    default:
        oss << "Unknown";
        break;
    }
    return oss.str();
}

} //namespace

/*virtual*/ RuntimeErrorCondition::~RuntimeErrorCondition() noexcept
{
}

/*extern*/ const ErrorCodeT error_not_implemented(ErrorNotImplementedE, category);
/*extern*/ const ErrorCodeT error_system(ErrorSystemE, category);
/*extern*/ const ErrorCodeT error_thread_started(ErrorThreadStartedE, category);

} //namespace solid
