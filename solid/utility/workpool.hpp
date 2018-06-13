// solid/utility/workpool_atomic.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once
#include "solid/utility/common.hpp"

#ifdef SOLID_USE_WORKPOOL_MUTEX
#include "solid/utility/workpool_mutex.hpp"
#else
#include "solid/utility/workpool_atomic.hpp"
#endif
