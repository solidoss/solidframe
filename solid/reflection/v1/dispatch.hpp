// solid/reflection/v1/dispatch.hpp
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
#include <istream>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

namespace solid {
namespace reflection {
namespace v1 {

enum struct TypeGroupE {
    Unknwon,
    Basic,
    Structure,
    Container,
    Array,
    Bitset,
    Enum,
    SharedPtr,
    UniquePtr,
    IntrusivePtr,
    Tuple,
    IStream,
    OStream,
};

template <class T>
constexpr TypeGroupE type_group()
{
    if constexpr (solid::is_shared_ptr_v<T>) {
        return TypeGroupE::SharedPtr;
    }
    if constexpr (solid::is_intrusive_ptr_v<T>) {
        return TypeGroupE::IntrusivePtr;
    }
    if constexpr (solid::is_unique_ptr_v<T>) {
        return TypeGroupE::UniquePtr;
    } else {
        static_assert(!std::is_pointer_v<T>, "Naked pointer are not supported - use std::shared_ptr or std::unique_ptr");
    }
    if constexpr (std::is_same_v<T, std::string>)
        return TypeGroupE::Basic;
    if constexpr (std::is_enum_v<T>)
        return TypeGroupE::Enum;
    if constexpr (is_bitset_v<T>)
        return TypeGroupE::Bitset;
    else if constexpr (solid::is_container_v<T>)
        return TypeGroupE::Container;
    else if constexpr (std::is_array<T>::value || solid::is_std_array_v<T>)
        return TypeGroupE::Array;
    else if constexpr (std::is_integral_v<T>)
        return TypeGroupE::Basic;
    else if constexpr (is_tuple<T>::value)
        return TypeGroupE::Tuple;
    else if constexpr (std::is_base_of_v<std::istream, T>)
        return TypeGroupE::IStream;
    else if constexpr (std::is_base_of_v<std::ostream, T>)
        return TypeGroupE::OStream;
    else
        return TypeGroupE::Structure;
}

// TODO: maybe you should move the below function unde :: namespace
template <class R, class T1, class T2, class Ctx>
inline void solidReflectV1(R& _rr, std::pair<T1, T2>& _rt, Ctx& _rctx)
{
    if constexpr (std::is_const_v<T1>) {
        _rr.add(*const_cast<std::remove_const_t<T1>*>(&_rt.first), _rctx, 0, "first"); // trick to support std::map::value_type - pair<const Key, Value>
    } else {
        _rr.add(_rt.first, _rctx, 0, "first");
    }
    _rr.add(_rt.second, _rctx, 1, "second");
}

template <class R, class T1, class T2, class Ctx>
inline void solidReflectV1(R& _rr, std::pair<T1, T2> const& _rt, Ctx& _rctx)
{
    _rr.add(_rt.first, _rctx, 0, "first").add(_rt.second, _rctx, 1, "second");
}

template <class R, class Ctx, class Tup, size_t Index = 0>
inline void solidReflectV1Tuple(R& _rr, Tup& _rt, Ctx& _rctx)
{
    if constexpr (Index < std::tuple_size_v<Tup>) {
        _rr.add(std::get<Index>(_rt), _rctx, Index, "tuple_item");
        solidReflectV1Tuple<R, Ctx, Tup, Index + 1>(_rr, _rt, _rctx);
    }
}

template <class R, class Ctx, class... Args>
inline void solidReflectV1(R& _rr, std::tuple<Args...>& _rt, Ctx& _rctx)
{
    solidReflectV1Tuple(_rr, _rt, _rctx);
}

template <class R, class Ctx, class Tup, size_t Index = 0>
inline void solidReflectV1Tuple(R& _rr, Tup const& _rt, Ctx& _rctx)
{
    if constexpr (Index < std::tuple_size_v<Tup>) {
        _rr.add(std::get<Index>(_rt), _rctx, Index, "tuple_item");
        solidReflectV1Tuple<R, Ctx, Tup, Index + 1>(_rr, _rt, _rctx);
    }
}

template <class R, class Ctx, class... Args>
inline void solidReflectV1(R& _rr, std::tuple<Args...> const& _rt, Ctx& _rctx)
{
    solidReflectV1Tuple(_rr, _rt, _rctx);
}

template <class R, class T, class Ctx>
inline void solidReflectV1(R& _rr, T& _rt, Ctx& _rctx)
{
    _rt.solidReflectV1(_rr, _rctx);
}
template <class R, class T, class Ctx>
inline void solidReflectV1(R& _rr, const T& _rt, Ctx& _rctx)
{
    _rt.solidReflectV1(_rr, _rctx);
}

} // namespace v1
} // namespace reflection
} // namespace solid
