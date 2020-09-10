// solid/system/cassert.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/common.hpp"
#include "solid/system/log.hpp"

#ifdef SOLID_HAS_ASSERT

#include <cassert>

#define solid_assert(a) assert((a))
#define solid_assert_log(a, l)                             \
    if (static_cast<bool>(a)) {                            \
    } else {                                               \
        solid_log(l, Exception, "(" #a ") assert failed"); \
        assert((a));                                       \
    }
#define solid_assert_logx(a, l, x)                               \
    if (static_cast<bool>(a)) {                                  \
    } else {                                                     \
        solid_log(l, Exception, "(" #a ") assert failed " << x); \
        assert((a));                                             \
    }

#else
#define solid_assert(a)
#define solid_assert_log(l, a)
#define solid_assert_logx(l,a,x)
#endif
