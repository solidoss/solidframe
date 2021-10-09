// solid/reflection/v1/reflection.hpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/reflection/v1/metadata.hpp"
#include "solid/reflection/v1/ostreamreflection.hpp"
#include "solid/reflection/v1/reflector.hpp"
#include "solid/reflection/v1/typetraits.hpp"

#define SOLID_REFLECT_V1(reflector, rthis, context)           \
    template <class Reflector, class Context>                 \
    void solidReflectV1(Reflector& _rr, Context& _rctx) const \
    {                                                         \
        solidReflectV1(_rr, *this, _rctx);                    \
    }                                                         \
    template <class Reflector, class Context>                 \
    void solidReflectV1(Reflector& _rr, Context& _rctx)       \
    {                                                         \
        solidReflectV1(_rr, *this, _rctx);                    \
    }                                                         \
    template <class Reflector, class T, class Context>        \
    static void solidReflectV1(Reflector& reflector, T& rthis, Context& context)

namespace solid {
namespace reflection {
using namespace v1;
} //namespace reflection
} //namespace solid
