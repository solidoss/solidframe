// solid/utility/collapse.hpp
//
// Copyright (c) 2024 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#pragma once
#include "solid/utility/intrusiveptr.hpp"
#include "solid/utility/sharedbuffer.hpp"
#include "solid/utility/typetraits.hpp"

namespace solid {

#ifdef __cpp_concepts
template <typename What>
concept Collapsable = std::is_same_v<What, ConstSharedBuffer> || is_intrusive_ptr_v<What>;

template <Collapsable Ptr>
#else
template <class Ptr>
#endif
inline auto collapse(Ptr& _rptr)
{
    return _rptr.collapse();
}

} // namespace solid