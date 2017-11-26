// solid/system/function.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/solid_config.hpp"

#include <functional>

#define SOLID_FUNCTION std::function
#define SOLID_FUNCTION_EMPTY(f) (f == nullptr)
#define SOLID_FUNCTION_CLEAR(f) (f = nullptr)
