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

#include "solid/reflection/v1/typetraits.hpp"
#include "solid/reflection/v1/reflector.hpp"
#include "solid/reflection/v1/metadata.hpp"

#define SOLID_REFLECT_V1(ser, rthis, ctx)           \
    template <class S, class C>                                     \
    void solidReflectV1(S& _s, C& _rctx) const \
    {                                                               \
        solidReflectV1(_s, *this, _rctx);                  \
    }                                                               \
    template <class S, class C>                                     \
    void solidReflectV1(S& _s, C& _rctx)       \
    {                                                               \
        solidReflectV1(_s, *this, _rctx);                  \
    }                                                               \
    template <class S, class T, class C>                            \
    static void solidReflectV1(S& ser, T& rthis, C& ctx)

namespace solid{
namespace reflection{
using namespace v1;
}//namespace reflection
}//namespace solid
