// solid/reflection/v2/reflection.hpp
//
// Copyright (c) 2026 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#pragma once

#include "solid/reflection/v2/reflection_concept.hpp"
#include "solid/reflection/v2/reflection_metadata.hpp"

#include <tuple>
#include <utility>

namespace solid::reflection::v2 {

template <typename F, typename S, typename R, typename Ctx>
void reflect(std::pair<F, S>& _rv, R&& _rr, Ctx& _rc, std::string_view = {})
{
    _rr(make<1>("first"sv, _rv.first), _rc);
    _rr(make<2>("second"sv, _rv.second), _rc);
}

template <typename F, typename S, typename R, typename Ctx>
void reflect(std::pair<F, S> const& _rv, R&& _rr, Ctx& _rc, std::string_view = {})
{
    _rr(make<1>("first"sv, _rv.first), _rc);
    _rr(make<2>("second"sv, _rv.second), _rc);
}

namespace detail {
template <class R, class Ctx, class Tup, size_t Index = 0>
void reflect(Tup const& _rt, R& _rr, Ctx& _rctx)
{
    if constexpr (Index < std::tuple_size_v<Tup>) {
        _rr(make<Index>(""sv, std::get<Index>(_rt)), _rctx);
        detail::reflect<R, Ctx, Tup, Index + 1>(_rt, _rr, _rctx);
    }
}

template <class R, class Ctx, class Tup, size_t Index = 0>
void reflect(Tup& _rt, R& _rr, Ctx& _rctx)
{
    if constexpr (Index < std::tuple_size_v<Tup>) {
        _rr(make<Index>(""sv, std::get<Index>(_rt)), _rctx);
        detail::reflect<R, Ctx, Tup, Index + 1>(_rt, _rr, _rctx);
    }
}

} // namespace detail

template <class R, class Ctx, class... Args>
void reflect(std::tuple<Args...>& _rt, R&& _rr, Ctx& _rctx, std::string_view = {})
{
    detail::reflect(_rt, _rr, _rctx);
}

template <class R, class Ctx, class... Args>
void reflect(std::tuple<Args...> const& _rt, R&& _rr, Ctx& _rctx, std::string_view = {})
{
    detail::reflect(_rt, _rr, _rctx);
}

template <detail::IntrusiveReflectableC T, typename R, typename Ctx>
void reflect(T& _rv, R&& _rr, Ctx& _rc, std::string_view = {})
{
    _rv.solidReflectV2(std::forward<R>(_rr), _rc);
}

template <detail::IntrusiveReflectableC T, typename R, typename Ctx>
void reflect(T const& _rv, R&& _rr, Ctx& _rc, std::string_view = {})
{
    _rv.solidReflectV2(std::forward<R>(_rr), _rc);
}

template <typename T, typename R, typename Ctx>
void reflect(T& _rv, R&& _rr, Ctx& _rc, std::string_view _name = {})
{
    _rr(make<0>(_name, _rv), _rc);
}

template <typename T, typename R, typename Ctx>
void reflect(T const& _rv, R&& _rr, Ctx& _rc, std::string_view _name = {})
{
    _rr(make<0>(_name, _rv), _rc);
}

template <typename T>
concept ReflectableC = requires(T& t, std::nullopt_t& ctx) {
    solid::reflection::v2::reflect(t, [](auto&&) {}, ctx, {});
    solid::reflection::v2::reflect(static_cast<T const&>(t), [](auto&&) {}, ctx, {});
};

template <detail::FieldIdC auto Id, typename Val, typename... Args>
auto make(std::string_view _name, Val& _ref, Args&&... _args)
{
    using ValT = std::decay_t<Val>;
    static constexpr size_t id{static_cast<size_t>(Id)};
    static constexpr auto   type_id{detail::to_type_id<ValT>()};

    if constexpr (type_id == TypeIdE::Boolean) {
        return Boolean<id, Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Integral) {
        return Integral<id, Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Enum) {
        if constexpr (sizeof...(Args) != 0) {
            // Caller supplied its own name<->value converters: forward them
            // verbatim. Such an enum needs no to_string/from_string overloads.
            static_assert(sizeof...(Args) == 2,
                "make: an enum takes an optional get_sv/set_sv pair");
            return Enum<id, Val, std::decay_t<Args>...>{_name, _ref, std::forward<Args>(_args)...};
        } else {
            static_assert(detail::NamedEnumC<ValT>,
                "reflected enums must provide ADL to_string(E) and "
                "from_string(std::type_identity<E>, std::string_view)");
            auto get_sv = [&_ref]() -> std::string_view {
                return to_string(_ref);
            };
            // The if constexpr guard keeps the assignment well-formed when _ref is a
            // const reference (the const serialization path instantiates make, and
            // thus this closure, even though it never calls set).
            auto set_sv = [&_ref](std::string_view _sv) -> bool {
                if constexpr (!std::is_const_v<std::remove_reference_t<decltype(_ref)>>) {
                    if (auto val = from_string(std::type_identity<ValT>{}, _sv)) {
                        _ref = *val;
                        return true;
                    }
                }
                return false;
            };
            return Enum<id, Val, decltype(get_sv), decltype(set_sv)>{_name, _ref, get_sv, set_sv};
        }
    } else if constexpr (type_id == TypeIdE::String) {
        return String<id, Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::IOStream) {
        return IOStream<id>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::OStream) {
        return OStream<id>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::IStream) {
        return IStream<id>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Array) {
        return Container<id, Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Bitset) {
        return Container<id, Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::BitVector) {
        return Container<id, Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Container || type_id == TypeIdE::KeyContainer || type_id == TypeIdE::KeyValueContainer) {
        return Container<id, Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Reflectable) {
        return Reflectable<id, Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Pointer) {
        return Pointer<id, Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Optional) {
        return Optional<id, Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Variant) {
        return Variant<id, Val>{_name, _ref};
    }
}

template <detail::FieldIdC auto Id, typename Val>
    requires detail::CallableC<std::decay_t<Val>>
auto make(std::string_view _name, Val&& _ref)
{
    return Callable<static_cast<size_t>(Id), Val>{_name, std::forward<Val>(_ref)};
}

// Function-based counterpart of make: the reflected value is only reachable
// through the accessor closures, no reference is stored. The descriptor kind
// is deduced from the getter's return type.
template <detail::FieldIdC auto Id, typename GetF, typename SetF, typename... ArgsF>
auto makef(std::string_view _name, GetF&& _get, SetF&& _set, ArgsF&&... _args)
{
    using RetT = std::invoke_result_t<GetF>;
    using ValT = std::decay_t<RetT>;
    static constexpr size_t id{static_cast<size_t>(Id)};
    static constexpr auto   type_id{detail::to_type_id<ValT>()};

    if constexpr (type_id == TypeIdE::Boolean) {
        return BooleanF<id, ValT, GetF, SetF>{_name, std::forward<GetF>(_get), std::forward<SetF>(_set)};
    } else if constexpr (type_id == TypeIdE::Integral) {
        return IntegralF<id, ValT, GetF, SetF>{_name, std::forward<GetF>(_get), std::forward<SetF>(_set)};
    } else if constexpr (type_id == TypeIdE::Enum) {
        if constexpr (sizeof...(ArgsF) != 0) {
            // Caller supplied its own name<->value converters: forward them
            // verbatim. Such an enum needs no to_string/from_string overloads.
            static_assert(sizeof...(ArgsF) == 2,
                "makef: an enum takes an optional get_sv/set_sv pair");
            return EnumF<id, ValT, GetF, SetF, std::decay_t<ArgsF>...>{_name, std::forward<GetF>(_get), std::forward<SetF>(_set), std::forward<ArgsF>(_args)...};
        } else {
            static_assert(detail::NamedEnumC<ValT>,
                "reflected enums must provide ADL to_string(E) and "
                "from_string(std::type_identity<E>, std::string_view)");
            auto get_sv = [_get]() -> std::string_view {
                return to_string(_get());
            };
            // Generic in its parameter so the body — and with it the user's
            // setter — is only instantiated when set_sv is actually called: the
            // const (serialize) path instantiates makef, and thus this closure,
            // but must never materialize the setter (see SFR_V2_MAKEF).
            auto set_sv = [_set](std::convertible_to<std::string_view> auto _sv) -> bool {
                if (auto val = from_string(std::type_identity<ValT>{}, std::string_view{_sv})) {
                    _set(*val);
                    return true;
                }
                return false;
            };
            return EnumF<id, ValT, GetF, SetF, decltype(get_sv), decltype(set_sv)>{_name, std::forward<GetF>(_get), std::forward<SetF>(_set), get_sv, set_sv};
        }
    } else if constexpr (type_id == TypeIdE::String) {
        auto view = [_get]() {
            return std::string_view{_get()};
        };
        return StringF<id, ValT, GetF, SetF, decltype(view)>{_name, std::forward<GetF>(_get), std::forward<SetF>(_set), view};
    } else if constexpr (type_id == TypeIdE::Container) {
        // RetT (not ValT): a getter returning a reference to a real container
        // stores that reference; one returning a view (std::span) stores the
        // view by value.
        return ContainerF<id, RetT, GetF, SetF>{_name, std::forward<GetF>(_get), std::forward<SetF>(_set)};
    } else {
        static_assert(type_id == TypeIdE::Boolean, "makef: unsupported value type");
    }
}

template <size_t Id, detail::AnyContainerC ValT>
template <typename T, typename V, typename C, typename... Args>
void Container<Id, ValT>::emplace_back_with(this T& rthis, V&& _rv, C& _rc, Args&&... args)
    requires detail::ContainerC<ValT>
{
    rthis.ref.emplace_back(std::forward<Args>(args)...);
    solid::reflection::v2::reflect(rthis.ref.back(), std::forward<V>(_rv), _rc);
}

// for_each hands the visitor one metadata descriptor per element (make<0> with
// an empty name), so element boundaries stay visible to the visitor and the
// walked elements are never exposed directly.
template <size_t Id, detail::AnyContainerC ValT>
template <typename T, typename V, typename C>
void Container<Id, ValT>::for_each(this T& rthis, V&& _rv, C& _rc)
{
    for (auto& re : rthis.ref) {
        std::invoke(_rv, solid::reflection::v2::make<0>(""sv, re), _rc);
    }
}

template <size_t Id, detail::StdArrayC ValT>
template <typename T, typename V, typename C>
void Container<Id, ValT>::for_each(this T& rthis, V&& _rv, C& _rc)
{
    for (auto& re : rthis.ref) {
        std::invoke(_rv, solid::reflection::v2::make<0>(""sv, re), _rc);
    }
}

template <size_t Id, detail::StdArrayC ValT>
template <typename T, typename V, typename C, typename... Args>
void Container<Id, ValT>::emplace_back_with(this T& rthis, V&& _rv, C& _rc, Args&&... args)
{
    auto& item = rthis.ref.at(rthis.offset++);
    item       = {std::forward<Args>(args)...};
    solid::reflection::v2::reflect(item, std::forward<V>(_rv), _rc);
}

// Bits are visited through a Boolean descriptor over a local copy; a mutation
// made by the visitor is written back on the non-const path.
template <size_t Id, detail::BitsetC ValT>
template <typename T, typename V, typename C>
void Container<Id, ValT>::for_each(this T& rthis, V&& _rv, C& _rc)
{
    for (size_t i = 0; i < rthis.ref.size(); ++i) {
        bool val = rthis.ref[i];
        std::invoke(_rv, solid::reflection::v2::make<0>(""sv, val), _rc);
        if constexpr (!std::is_const_v<ValT>) {
            rthis.ref[i] = val;
        }
    }
}

template <size_t Id, detail::BitVectorC ValT>
template <typename T, typename V, typename C>
void Container<Id, ValT>::for_each(this T& rthis, V&& _rv, C& _rc)
{
    for (size_t i = 0; i < rthis.ref.size(); ++i) {
        bool val = rthis.ref[i];
        std::invoke(_rv, solid::reflection::v2::make<0>(""sv, val), _rc);
        if constexpr (!std::is_const_v<ValT>) {
            rthis.ref[i] = val;
        }
    }
}

template <size_t Id, typename ValT>
template <typename T, typename V, typename C>
bool Pointer<Id, ValT>::with_if(this T& rthis, V&& _rv, C& _rc)
{
    if (rthis.has_value()) {
        solid::reflection::v2::reflect(*rthis.ref, std::forward<V>(_rv), _rc);
        return true;
    }
    return false;
}

template <size_t Id, typename ValT>
template <typename T, typename V, typename C>
bool Optional<Id, ValT>::with_if(this T& rthis, V&& _rv, C& _rc)
{
    if (rthis.has_value()) {
        solid::reflection::v2::reflect(*rthis.ref, std::forward<V>(_rv), _rc);
        return true;
    }
    return false;
}

template <size_t Id, typename ValT>
template <typename T, typename V, typename C>
void Reflectable<Id, ValT>::reflect(this T& rthis, V&& _rv, C& _rc)
{
    solid::reflection::v2::reflect(rthis.ref, std::forward<V>(_rv), _rc, rthis.name);
}

template <detail::FieldIdC auto Id, typename T, typename R, typename Ctx>
void reflect_at(T& rt, R&& _rr, Ctx& _rc)
{
    auto ref = [&_rr](auto&& _field, Ctx& _rc) {
        using FieldT = std::decay_t<decltype(_field)>;
        if constexpr (FieldT::id == static_cast<size_t>(Id)) {
            _rr(std::forward<decltype(_field)>(_field), _rc);
        }
    };
    reflect(rt, ref, _rc);
}

template <size_t Id, typename ValT>
template <typename This, typename T, typename V, typename C, typename... Args>
void Variant<Id, ValT>::emplace_with(this This& rthis, V&& _rv, C& _rc, Args&&... args)
{
    auto& rval{rthis.ref.template emplace<T>(std::forward<Args>(args)...)};
    solid::reflection::v2::reflect(rval, std::forward<V>(_rv), _rc);
}

template <size_t Id, typename ValT>
template <typename This, size_t I, typename V, typename C, typename... Args>
void Variant<Id, ValT>::emplace_with(this This& rthis, V&& _rv, C& _rc, Args&&... args)
{
    auto& rval{rthis.ref.template emplace<I>(std::forward<Args>(args)...)};
    solid::reflection::v2::reflect(rval, std::forward<V>(_rv), _rc);
}

template <size_t Id, typename ValT>
template <typename This, typename V, typename C>
bool Variant<Id, ValT>::with_if(this This& rthis, V&& _rv, C& _rc)
{
    if (rthis.has_value()) {
        std::visit([&_rv, &_rc](auto& rval) {
            solid::reflection::v2::reflect(rval, std::forward<V>(_rv), _rc);
        },
            rthis.ref);
        return true;
    }
    return false;
}

} // namespace solid::reflection::v2

// Reflect a field exposed via accessors: getter `getf()` paired with setter
// `set_<getf>(value)`. The setter lambda is generic and only instantiated when
// actually called, so it is never materialized on the const (serialize) path.
#define SFR_V2_MAKEF(ID, rthis, getf)                          \
    solid::reflection::v2::makef<ID>(                          \
        std::string_view{#getf},                               \
        [&rthis]() -> decltype(auto) { return rthis.getf(); }, \
        [&rthis](auto&&... _val) { rthis.set_##getf(std::forward<decltype(_val)>(_val)...); })
