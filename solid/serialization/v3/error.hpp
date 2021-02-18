// solid/serialization/v3/error.hpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/error.hpp"

namespace solid {
namespace serialization {
namespace v3 {

extern const ErrorConditionT error_limit_container;
extern const ErrorConditionT error_limit_string;
extern const ErrorConditionT error_limit_stream;
extern const ErrorConditionT error_limit_blob;
extern const ErrorConditionT error_limit_array;
extern const ErrorConditionT error_cross_integer;
extern const ErrorConditionT error_unknown_type;

} //namespace v3
} //namespace serialization
} //namespace solid

