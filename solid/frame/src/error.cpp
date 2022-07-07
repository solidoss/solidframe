// solid/frame/src/error.cpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/frame/error.hpp"
#include <sstream>

namespace solid {
namespace frame {

namespace {

enum {
    ErrorTimerCancelE = 1,
    ErrorAlreadyE,
};

class ErrorCategory : public ErrorCategoryT {
public:
    ErrorCategory() {}
    const char* name() const noexcept override
    {
        return "solid::frame::aio";
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
    case ErrorTimerCancelE:
        oss << "Timer: canceled";
        break;
    case ErrorAlreadyE:
        oss << "Opperation already in progress";
        break;
    default:
        oss << "Unknown";
        break;
    }
    return oss.str();
}

} // namespace

/*extern*/ const ErrorConditionT error_timer_cancel(ErrorTimerCancelE, category);
/*extern*/ const ErrorConditionT error_already(ErrorAlreadyE, category);

} // namespace frame
} // namespace solid
