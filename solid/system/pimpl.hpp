// solid/system/pimpl.hpp
//
// Copyright (c) 2017 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <memory>

namespace solid {

template <typename T>
using PimplT = const std::unique_ptr<T>;

template<typename T, typename... Args>
std::unique_ptr<T> make_pimpl(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

} //namespace solid
