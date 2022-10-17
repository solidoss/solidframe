// solid/reflection/v1/typetraits.hpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#pragma once

#include "solid/utility/typetraits.hpp"

namespace solid {
namespace reflection {
namespace v1 {

template <typename T, typename _ = void>
struct is_reflective : std::false_type {
};

template <typename T, typename _ = void>
struct is_input_reflector : std::false_type {
};

template <typename... Ts>
struct is_input_reflector_helper {
};

template <typename T>
struct is_input_reflector<
    T,
    typename std::conditional<
        false,
        is_input_reflector_helper<
            typename T::is_input_reflector>,
        void>::type> : public std::true_type {
};

template <typename T, typename _ = void>
struct is_output_reflector : std::false_type {
};

template <typename... Ts>
struct is_output_reflector_helper {
};

template <typename T>
struct is_output_reflector<
    T,
    typename std::conditional<
        false,
        is_output_reflector_helper<
            typename T::is_output_reflector>,
        void>::type> : public std::true_type {
};

template <class... Args>
struct is_empty_pack;

template <>
struct is_empty_pack<> : std::true_type {
};

template <class... Args>
struct is_empty_pack : std::false_type {
};

template <typename>
struct is_tuple : std::false_type {
};

template <typename... T>
struct is_tuple<std::tuple<T...>> : std::true_type {
};

template <class T>
inline constexpr bool is_tuple_v = is_tuple<T>::value;

template <typename T>
struct compacted {
    using value_type = T;
    T& value_;

    compacted(T& _value)
        : value_(_value)
    {
    }
};

template <typename T>
struct is_compacted;

template <typename>
struct is_compacted : std::false_type {
};

template <typename T>
struct is_compacted<compacted<T>> : std::true_type {
};

template <class T>
inline constexpr bool is_compacted_v = is_compacted<T>::value;

} // namespace v1
} // namespace reflection
} // namespace solid
