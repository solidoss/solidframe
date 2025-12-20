// solid/utility/anyimpl.hpp
//
// Copyright (c) 2025 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/utility/common.hpp"

namespace solid::any_impl {

enum struct RepresentationE : uintptr_t {
    None = 0,
    Small,
    Big,
};
constexpr uintptr_t representation_mask                    = 3;
constexpr uintptr_t representation_and_flags_mask          = representation_mask;
constexpr uintptr_t reversed_representation_and_flags_mask = ~representation_mask;

inline RepresentationE representation(uintptr_t const _rtti) noexcept
{
    return static_cast<RepresentationE>(_rtti & representation_mask);
}

inline uintptr_t representation(uintptr_t const _rtti, RepresentationE const _repr) noexcept
{
    return (_rtti & (~representation_and_flags_mask)) | to_underlying(_repr);
}

inline uintptr_t representation(void const* _rtti, RepresentationE const _repr) noexcept
{
    return (reinterpret_cast<uintptr_t>(_rtti) & (~representation_and_flags_mask)) | to_underlying(_repr);
}

} // namespace solid::any_impl