// solid/utility/poolable.hpp
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

namespace solid {

template <class T>
class IntrusivePtr;
template <class T>
class MutableIntrusivePtr;
template <class T>
class ConstIntrusivePtr;

namespace impl {
template <typename T, template <typename> typename Ptr>
class PoolBase;
template <class T>
class IntrusivePtrBase;
} // namespace impl

template <typename T, template <typename> typename Ptr = MutableIntrusivePtr>
class Pool;

template <typename T, template <typename> typename Ptr = MutableIntrusivePtr>
class Poolable {
    friend class impl::IntrusivePtrBase<T>;
    friend class IntrusivePtr<T>;
    friend class MutableIntrusivePtr<T>;
    friend class ConstIntrusivePtr<T>;
    friend class Pool<T, Ptr>;
    friend class impl::PoolBase<T, Ptr>;
    Pool<T, Ptr>* ppool_ = nullptr;
};

template <class T>
concept PoolableC = (std::is_base_of_v<Poolable<T, IntrusivePtr>, T> or std::is_base_of_v<Poolable<T, MutableIntrusivePtr>, T>) and std::is_final_v<T>;

template <typename T>
struct is_poolable;

template <PoolableC T>
struct is_poolable<T> : std::true_type {
};

template <typename T>
struct is_poolable : std::false_type {
};

template <class T>
inline constexpr bool is_poolable_v = is_poolable<T>::value;
} // namespace solid