// solid/serialization/v2/src/serialization.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//


#include "solid/serialization/v2/serialization.hpp"

namespace solid {
namespace serialization {
namespace v2 {
//========================================================================
/*virtual*/ Base::~Base()
{
}
//========================================================================
namespace {

class ErrorCategory : public ErrorCategoryT {
public:
    enum {
        NoTypeE = 1,
        NoCastE,
    };
    ErrorCategory() {}

private:
    const char* name() const noexcept(true)
    {
        return "solid::serialization::TypeIdMap";
    }

    std::string message(int _ev) const
    {
        switch (_ev) {
        case NoTypeE:
            return "Type not registered";
        case NoCastE:
            return "Cast not registered";
        default:
            return "Unknown";
        }
    }
};

const ErrorCategory ec;
} //namespace

/*static*/ ErrorConditionT TypeMapBase::error_no_cast()
{
    return ErrorConditionT(ErrorCategory::NoCastE, ec);
}
/*static*/ ErrorConditionT TypeMapBase::error_no_type()
{
    return ErrorConditionT(ErrorCategory::NoTypeE, ec);
}
/*virtual*/ TypeMapBase::~TypeMapBase() {}
//========================================================================
//========================================================================
} // namespace v2
} // namespace serialization
} // namespace solid
