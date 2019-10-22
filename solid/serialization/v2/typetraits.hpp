// solid/serialization/v2/typetraits.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/utility/typetraits.hpp"

namespace solid {
namespace serialization {

//https://stackoverflow.com/questions/12042824/how-to-write-a-type-trait-is-container-or-is-vector
template <typename T, typename _ = void>
struct is_container : std::false_type {
};

template <typename... Ts>
struct is_container_helper {
};

template <typename T>
struct is_container<
    T,
    typename std::conditional<
        false,
        is_container_helper<
            typename T::value_type,
            typename T::size_type,
            typename T::allocator_type,
            typename T::iterator,
            typename T::const_iterator,
            decltype(std::declval<T>().size()),
            decltype(std::declval<T>().begin()),
            decltype(std::declval<T>().end()),
            decltype(std::declval<T>().cbegin()),
            decltype(std::declval<T>().cend())>,
        void>::type> : public std::true_type {
};

template <class F, class... Args>
struct is_callable_helper {
    template <class U>
    static auto test(U* p) -> decltype((*p)(std::declval<Args>()...), void(), std::true_type());
    template <class U>
    static auto test(...) -> decltype(std::false_type());

    static constexpr bool value = decltype(test<F>(0))::value;
    using type                  = decltype(test<F>(0));
};

template <class F, class... Args>
struct is_callable : std::conditional<is_callable_helper<F, Args...>::value, std::true_type, std::false_type>::type {
};

// template <class T>
// struct is_unique_ptr : std::false_type
// {};
//
// template <class T, class D>
// struct is_unique_ptr<std::unique_ptr<T, D>> : std::true_type
// {};

} //namespace serialization
} //namespace solid
