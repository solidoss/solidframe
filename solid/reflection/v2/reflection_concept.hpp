// solid/reflection/v2/reflection_concept.hpp
//
// Copyright (c) 2026 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <memory>
#include <optional>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace solid::reflection::v2 {

using namespace std::literals;

enum class TypeIdE : std::uint8_t {
    Boolean,
    Integral,
    String,
    Reflectable,
    Array,
    KeyContainer,
    KeyValueContainer,
    Container,
    Callable,
    IStream,
    OStream,
    IOStream,
    Pointer,
    Bitset,
    BitVector,
    Tuple,
    Optional,
    Variant,
    Enum,
};

namespace detail {
struct AnyType {
    template <typename T>
    operator T() const;

    template <typename T>
    operator T&() const;
};

template <typename T>
concept CallableC = requires(T fn) {
    requires(requires { fn(AnyType{}, AnyType{}); });
};

template <typename T>
concept StringC = std::is_convertible_v<T, std::string_view>;

template <typename T>
concept ArrayC = std::is_array_v<T>;

template <typename T>
struct is_std_array : std::false_type {};
template <typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <typename T>
struct is_unique_ptr : std::false_type {
};

template <typename T>
struct is_unique_ptr<std::unique_ptr<T>> : std::true_type {
};

template <class T>
struct is_shared_ptr : std::false_type {
};

template <typename T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {
};

template <class T>
inline constexpr bool is_unique_ptr_v = is_unique_ptr<T>::value;

template <class T>
inline constexpr bool is_shared_ptr_v = is_shared_ptr<T>::value;

template <typename T>
struct is_bitset : std::false_type {
};

template <size_t Sz>
struct is_bitset<std::bitset<Sz>> : std::true_type {
};

template <class T>
inline constexpr bool is_bitset_v = is_bitset<T>::value;

template <typename T>
concept StdArrayC = is_std_array<std::decay_t<T>>::value;

template <typename T>
concept BitsetC = is_bitset<std::decay_t<T>>::value;

template <typename T>
struct is_bit_vector : std::false_type {
};

template <typename Alloc>
struct is_bit_vector<std::vector<bool, Alloc>> : std::true_type {
};

template <class T>
inline constexpr bool is_bit_vector_v = is_bit_vector<T>::value;

template <typename T>
concept BitVectorC = is_bit_vector<std::decay_t<T>>::value;

template <typename T, typename Enable = void>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template <class T>
inline constexpr bool is_optional_v = is_optional<T>::value;

template <typename T>
concept OptionalC = is_optional<std::decay_t<T>>::value;

template <typename T, typename Enable = void>
struct is_variant : std::false_type {};

template <typename... T>
struct is_variant<std::variant<T...>> : std::true_type {};

template <class T>
inline constexpr bool is_variant_v = is_variant<T>::value;

template <typename T>
concept VariantC = is_variant<std::decay_t<T>>::value;

template <typename T>
concept EnumC = std::is_enum_v<std::decay_t<T>>;

// A field id may be spelled either as an integral constant or as an enumerator
// (e.g. an enum class listing a struct's fields); the metadata descriptors
// always carry it as the size_t it static_casts to.
template <typename T>
concept FieldIdC = std::is_integral_v<T> || std::is_enum_v<T>;

// An enum is "named" when it provides ADL-reachable
//   std::string_view          to_string(E)
//   std::optional<E>          from_string(std::type_identity<E>, std::string_view)
// The std::type_identity tag on from_string is what lets ADL find the user's
// overload: its associated namespaces include those of E (the string_view
// argument alone would only pull in namespace std).
template <typename T>
concept NamedEnumC = EnumC<T> && requires(std::decay_t<T> e, std::string_view sv) {
    { to_string(e) } -> std::convertible_to<std::string_view>;
    { from_string(std::type_identity<std::decay_t<T>>{}, sv) }
    -> std::convertible_to<std::optional<std::decay_t<T>>>;
};

template <typename T>
concept KeyContainerC = requires(T& t) {
    typename T::key_type;
    typename T::value_type;
    typename T::iterator;
    typename T::const_iterator;
    t.begin();
    t.end();
    { t.size() } -> std::convertible_to<std::size_t>;
    { t.empty() } -> std::convertible_to<bool>;
} && std::same_as<typename T::key_type, typename T::value_type>;

template <typename T>
concept KeyValueContainerC = requires(T& t) {
    typename T::key_type;
    typename T::value_type;
    typename T::mapped_type;
    typename T::iterator;
    typename T::const_iterator;
    t.begin();
    t.end();
    { t.size() } -> std::convertible_to<std::size_t>;
    { t.empty() } -> std::convertible_to<bool>;
} && !KeyContainerC<T>;

template <typename T>
concept ContainerC = requires(T& t) {
    typename T::value_type;
    typename T::iterator;
    typename T::const_iterator;
    t.begin();
    t.end();
    { t.size() } -> std::convertible_to<std::size_t>;
    { t.empty() } -> std::convertible_to<bool>;
} && !StringC<T> && !StdArrayC<T> && !BitVectorC<T> && !KeyContainerC<T> && !KeyValueContainerC<T>;

template <typename T>
concept AnyContainerC = KeyValueContainerC<T> || KeyContainerC<T> || ContainerC<T>;

template <typename T>
concept IntrusiveReflectableC = requires(T& t, std::nullopt_t& ctx) {
    t.solidReflectV2([](auto&&) {}, ctx);
    static_cast<T const&>(t).solidReflectV2([](auto&&) {}, ctx);
};

template <typename Val>
constexpr TypeIdE to_type_id()
{
    using ValT = std::decay_t<Val>;
    if constexpr (std::is_same_v<ValT, bool>) {
        return TypeIdE::Boolean;
    } else if constexpr (std::is_integral_v<ValT>) {
        return TypeIdE::Integral;
    } else if constexpr (std::is_enum_v<ValT>) {
        return TypeIdE::Enum;
    } else if constexpr (StringC<ValT>) {
        return TypeIdE::String;
    } else if constexpr (std::is_base_of_v<std::iostream, ValT>) {
        return TypeIdE::IOStream;
    } else if constexpr (std::is_base_of_v<std::ostream, ValT>) {
        return TypeIdE::OStream;
    } else if constexpr (std::is_base_of_v<std::istream, ValT>) {
        return TypeIdE::IStream;
    } else if constexpr (StdArrayC<ValT>) {
        return TypeIdE::Container;
    } else if constexpr (BitsetC<ValT>) {
        return TypeIdE::Bitset;
    } else if constexpr (BitVectorC<ValT>) {
        return TypeIdE::BitVector;
    } else if constexpr (KeyContainerC<ValT>) {
        return TypeIdE::KeyContainer;
    } else if constexpr (KeyValueContainerC<ValT>) {
        return TypeIdE::KeyValueContainer;
    } else if constexpr (ContainerC<ValT>) {
        return TypeIdE::Container;
    } else if constexpr (is_shared_ptr_v<ValT> or is_unique_ptr_v<ValT>) {
        return TypeIdE::Pointer;
    } else if constexpr (is_optional_v<ValT>) {
        return TypeIdE::Optional;
    } else if constexpr (is_variant_v<ValT>) {
        return TypeIdE::Variant;
    }
    return TypeIdE::Reflectable;
}

} // namespace detail

} // namespace solid::reflection::v2
