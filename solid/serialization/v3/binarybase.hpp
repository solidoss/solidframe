// solid/serialization/v3/binarybase.hpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <bitset>
#include <memory>
#include <utility>
#include <vector>

#include "solid/serialization/v3/error.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/common.hpp"
#include "solid/reflection/v1/typemap.hpp"

namespace solid {
namespace serialization {
namespace v3 {

extern const LoggerT logger;

namespace binary {

class Base : NonCopyable {
public:
    enum struct ReturnE {
        Done = 0,
        Continue,
        Wait
    };

    const ErrorConditionT& error() const
    {
        return error_;
    }
protected:
    ErrorConditionT error_;
};

} //namespace binary
} //namespace v3
} //namespace serialization
} //namespace solid

