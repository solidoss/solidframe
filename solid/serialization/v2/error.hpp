// solid/serialization/v2/error.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
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
namespace v2 {

extern const ErrorConditionT error_limit_container;
extern const ErrorConditionT error_limit_string;
extern const ErrorConditionT error_limit_stream;
extern const ErrorConditionT error_limit_blob;
extern const ErrorConditionT error_cross_integer;

} //namespace v2
} //namespace serialization
} //namespace solid
