// solid/utility/functiontraits.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <cstddef>
#include <type_traits>

namespace solid {

// from https://stackoverflow.com/questions/7943525/is-it-possible-to-figure-out-the-parameter-type-and-return-type-of-a-lambda
// and thrill/common/function_traits.hpp: http://project-thrill.org/docs/master/function__traits_8hpp_source.html

template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())> {
};

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...) const> {
    static constexpr size_t arity = sizeof...(Args);
    using result_type             = ReturnType;

    using arguments = std::tuple<Args...>;

    template <size_t i>
    using argument = typename std::tuple_element<i, arguments>::type;
};

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...)> : public function_traits<ReturnType (ClassType::*)(Args...) const> {
};

template <typename ReturnType, typename... Args>
struct function_traits<ReturnType (*)(Args...)> {
    static constexpr size_t arity = sizeof...(Args);
    using result_type             = ReturnType;

    using arguments = std::tuple<Args...>;

    template <size_t i>
    using argument = typename std::tuple_element<i, arguments>::type;
};

} // namespace solid
