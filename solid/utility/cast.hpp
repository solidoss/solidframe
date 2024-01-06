// solid/utility/cast.hpp
//
// Copyright (c) 2024 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#pragma once

#include "solid/utility/innerlist.hpp"
#include <memory>

namespace solid {

template <class T, class U>
inline std::shared_ptr<T> static_pointer_cast(const std::shared_ptr<U>& r) noexcept
{
    return std::static_pointer_cast<T>(r);
}

template <class T, class U>
inline std::shared_ptr<T> static_pointer_cast(std::shared_ptr<U>&& r) noexcept
{
    return std::static_pointer_cast<T>(std::move(r));
}

template <class T, class U>
inline std::shared_ptr<T> dynamic_pointer_cast(const std::shared_ptr<U>& r) noexcept
{
    return std::dynamic_pointer_cast<T>(r);
}

template <class T, class U>
inline std::shared_ptr<T> dynamic_pointer_cast(std::shared_ptr<U>&& r) noexcept
{
    return std::dynamic_pointer_cast<T>(std::move(r));
}

} // namespace solid