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

#include "solid/system/configuration_impl.hpp"

#include <tuple>
#include <utility>

#ifdef SOLID_USE_REFLECTION
#include <concepts>
#include <meta>
#include <type_traits>
#endif

namespace solid::reflection::v2 {

#ifdef SOLID_USE_REFLECTION
// C++26 annotations for natively reflected members - the declarative
// counterpart of the descriptor setters used inside solidReflectV2:
//
//     struct Item {
//         [[=annotate::max_size(64)]] std::string name_;
//         [[=annotate::min(1), =annotate::max(100)]] uint32_t count_;
//     };
//
// native_reflect forwards them to the member's descriptor
// (with_max_size, with_min, with_max). Annotations of other
// types are ignored, so members may carry user-defined ones as well.
//
// A member annotated with annotate::skip is not reflected at all - it may
// even be of a type that cannot be reflected (a mutex, a raw pointer...):
//
//     struct Item {
//         std::string                   name_;
//         [[=annotate::skip]] std::mutex mtx_;
//     };
//
// Skipping a member does not renumber the others: ids follow the
// declaration order of all the members.
namespace annotate {

struct skip_t {
    constexpr bool operator==(skip_t const&) const = default;
};

inline constexpr skip_t skip{};

struct max_size_t {
    size_t value;
};

template <std::integral T>
struct min_t {
    T value;
};

template <std::integral T>
struct max_t {
    T value;
};

consteval max_size_t max_size(size_t const _value)
{
    return {_value};
}

template <std::integral T>
consteval min_t<T> min(T const _value)
{
    return {_value};
}

template <std::integral T>
consteval max_t<T> max(T const _value)
{
    return {_value};
}

} // namespace annotate
#endif

template <typename F,
    typename S,
    typename R,
    typename Ctx>
void reflect(
    std::pair<F,
        S>& _rv,
    R&&     _rr,
    Ctx&    _rc,
    std::string_view = {})
{
    _rr(id<1>, make("first"sv, _rv.first), _rc);
    _rr(id<2>, make("second"sv, _rv.second), _rc);
}

template <typename F,
    typename S,
    typename R,
    typename Ctx>
void reflect(
    std::pair<F,
        S> const& _rv,
    R&&           _rr,
    Ctx&          _rc,
    std::string_view = {})
{
    _rr(id<1>, make("first"sv, _rv.first), _rc);
    _rr(id<2>, make("second"sv, _rv.second), _rc);
}

namespace detail {
template <class R,
    class Ctx,
    class Tup,
    size_t Index = 0>
void reflect(
    Tup const& _rt,
    R&         _rr,
    Ctx&       _rctx)
{
    if constexpr (Index < std::tuple_size_v<Tup>) {
        _rr(id<Index>, make(""sv, std::get<Index>(_rt)), _rctx);
        detail::reflect<R, Ctx, Tup, Index + 1>(_rt, _rr, _rctx);
    }
}

template <class R,
    class Ctx,
    class Tup,
    size_t Index = 0>
void reflect(
    Tup& _rt,
    R&   _rr,
    Ctx& _rctx)
{
    if constexpr (Index < std::tuple_size_v<Tup>) {
        _rr(id<Index>, make(""sv, std::get<Index>(_rt)), _rctx);
        detail::reflect<R, Ctx, Tup, Index + 1>(_rt, _rr, _rctx);
    }
}

} // namespace detail

template <class R,
    class Ctx,
    class... Args>
void reflect(
    std::tuple<Args...>& _rt,
    R&&                  _rr,
    Ctx&                 _rctx,
    std::string_view = {})
{
    detail::reflect(_rt, _rr, _rctx);
}

template <class R,
    class Ctx,
    class... Args>
void reflect(
    std::tuple<Args...> const& _rt,
    R&&                        _rr,
    Ctx&                       _rctx,
    std::string_view = {})
{
    detail::reflect(_rt, _rr, _rctx);
}

namespace detail {
#ifdef SOLID_USE_REFLECTION
// A solidReflectV2 written as `this auto&` is inherited, so a derived class
// would satisfy IntrusiveReflectableC through its base and lose its own
// members. With native reflection available, only a class declaring
// solidReflectV2 itself is walked intrusively; a derived one goes through
// native_reflect, which reflects its bases (the base's solidReflectV2
// included) and then its own members.
consteval bool declares_solid_reflect_v2(std::meta::info _type)
{
    for (auto member : std::meta::members_of(_type, std::meta::access_context::unchecked())) {
        if (std::meta::has_identifier(member) && std::meta::identifier_of(member) == "solidReflectV2") {
            return true;
        }
    }
    return false;
}

template <typename T>
concept SelfReflectableC = IntrusiveReflectableC<T> && declares_solid_reflect_v2(^^std::remove_const_t<T>);
#else
template <typename T>
concept SelfReflectableC = IntrusiveReflectableC<T>;
#endif
} // namespace detail

template <detail::SelfReflectableC T,
    typename R,
    typename Ctx>
void reflect(
    T&   _rv,
    R&&  _rr,
    Ctx& _rc,
    std::string_view = {})
{
    _rv.solidReflectV2(std::forward<R>(_rr), _rc);
}

template <detail::SelfReflectableC T,
    typename R,
    typename Ctx>
void reflect(
    T const& _rv,
    R&&      _rr,
    Ctx&     _rc,
    std::string_view = {})
{
    _rv.solidReflectV2(std::forward<R>(_rr), _rc);
}

#ifdef SOLID_USE_REFLECTION
namespace detail {
// Class types without solidReflectV2 are walked through C++26 static
// reflection. Each accessible base class is reflected first through
// reflect() - so a base keeps its own way of being walked (solidReflectV2,
// or native reflection again) - followed by one descriptor per accessible
// non-static data member of the class itself, named after it.
template <typename T>
concept NativeReflectableC = std::is_class_v<T> && !SelfReflectableC<T> && to_type_id<T>() == TypeIdE::Reflectable;

// defined after all the reflect overloads, which it calls for the bases
template <typename T,
    typename R,
    typename Ctx>
void native_reflect(
    T&   _rv,
    R&   _rr,
    Ctx& _rc);
} // namespace detail

template <detail::NativeReflectableC T,
    typename R,
    typename Ctx>
void reflect(
    T&   _rv,
    R&&  _rr,
    Ctx& _rc,
    std::string_view = {})
{
    detail::native_reflect(_rv, _rr, _rc);
}

template <detail::NativeReflectableC T,
    typename R,
    typename Ctx>
void reflect(
    T const& _rv,
    R&&      _rr,
    Ctx&     _rc,
    std::string_view = {})
{
    detail::native_reflect(_rv, _rr, _rc);
}
#endif

// A struct-like type reaching the generic overloads has no way to be walked:
// wrapping it in a Reflectable descriptor would only lead back here
// (Reflectable::reflect -> reflect -> make -> Reflectable ...), so it is
// rejected at compile time unless it is empty.
template <typename T,
    typename R,
    typename Ctx>
void reflect(
    T&               _rv,
    R&&              _rr,
    Ctx&             _rc,
    std::string_view _name = {})
{
    if constexpr (detail::to_type_id<T>() != TypeIdE::Reflectable) {
        _rr(id<0>, make(_name, _rv), _rc);
    } else {
        // empty types (e.g. std::monostate) simply have no fields
        static_assert(std::is_empty_v<T>,
            "type is not reflectable: provide solidReflectV2 (or build with C++26 reflection support)");
    }
}

template <typename T,
    typename R,
    typename Ctx>
void reflect(
    T const&         _rv,
    R&&              _rr,
    Ctx&             _rc,
    std::string_view _name = {})
{
    if constexpr (detail::to_type_id<T>() != TypeIdE::Reflectable) {
        _rr(id<0>, make(_name, _rv), _rc);
    } else {
        // empty types (e.g. std::monostate) simply have no fields
        static_assert(std::is_empty_v<T>,
            "type is not reflectable: provide solidReflectV2 (or build with C++26 reflection support)");
    }
}

template <typename T>
concept ReflectableC = requires(T& t, std::nullopt_t& ctx) {
    solid::reflection::v2::reflect(t, [](auto, auto&&, auto&) {}, ctx, {});
    solid::reflection::v2::reflect(static_cast<T const&>(t), [](auto, auto&&, auto&) {}, ctx, {});
};

template <typename Val,
    typename... Args>
auto make(
    std::string_view _name,
    Val&             _ref,
    Args&&... _args)
{
    using ValT = std::decay_t<Val>;
    static constexpr auto type_id{detail::to_type_id<ValT>()};

    if constexpr (type_id == TypeIdE::Boolean) {
        return Boolean<Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Integral) {
        return Integral<Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Enum) {
        if constexpr (sizeof...(Args) != 0) {
            // Caller supplied its own name<->value converters: forward them
            // verbatim. Such an enum needs no to_string/from_string overloads.
            static_assert(sizeof...(Args) == 2,
                "make: an enum takes an optional get_sv/set_sv pair");
            return Enum<Val, std::decay_t<Args>...>{_name, _ref, std::forward<Args>(_args)...};
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
            auto set_sv = [&](std::string_view _sv) -> bool {
                if constexpr (!std::is_const_v<std::remove_reference_t<decltype(_ref)>>) {
                    if (auto val = from_string(std::type_identity<ValT>{}, _sv)) {
                        _ref = *val;
                        return true;
                    }
                }
                return false;
            };
            return Enum<Val, decltype(get_sv), decltype(set_sv)>{_name, _ref, get_sv, set_sv};
        }
    } else if constexpr (type_id == TypeIdE::String) {
        return String<Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::IOStream) {
        return IOStream<>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::OStream) {
        return OStream<>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::IStream) {
        return IStream<>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Array) {
        return Container<Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Bitset) {
        return Container<Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::BitVector) {
        return Container<Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Container || type_id == TypeIdE::KeyContainer || type_id == TypeIdE::KeyValueContainer) {
        return Container<Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Reflectable) {
        return Reflectable<Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Pointer) {
        return Pointer<Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Optional) {
        return Optional<Val>{_name, _ref};
    } else if constexpr (type_id == TypeIdE::Variant) {
        return Variant<Val>{_name, _ref};
    }
}

template <typename Val>
    requires detail::CallableC<std::decay_t<Val>>
auto make(
    std::string_view _name,
    Val&&            _ref)
{
    return Callable<Val>{_name, std::forward<Val>(_ref)};
}

// Function-based counterpart of make: the reflected value is only reachable
// through the accessor closures, no reference is stored. The descriptor kind
// is deduced from the getter's return type.
template <typename GetF,
    typename SetF,
    typename... ArgsF>
auto makef(
    std::string_view _name,
    GetF&&           _get,
    SetF&&           _set,
    ArgsF&&... _args)
{
    using RetT = std::invoke_result_t<GetF>;
    using ValT = std::decay_t<RetT>;
    static constexpr auto type_id{detail::to_type_id<ValT>()};

    if constexpr (type_id == TypeIdE::Boolean) {
        return BooleanF<ValT, GetF, SetF>{_name, std::forward<GetF>(_get), std::forward<SetF>(_set)};
    } else if constexpr (type_id == TypeIdE::Integral) {
        return IntegralF<ValT, GetF, SetF>{_name, std::forward<GetF>(_get), std::forward<SetF>(_set)};
    } else if constexpr (type_id == TypeIdE::Enum) {
        if constexpr (sizeof...(ArgsF) != 0) {
            // Caller supplied its own name<->value converters: forward them
            // verbatim. Such an enum needs no to_string/from_string overloads.
            static_assert(sizeof...(ArgsF) == 2,
                "makef: an enum takes an optional get_sv/set_sv pair");
            return EnumF<ValT, GetF, SetF, std::decay_t<ArgsF>...>{_name, std::forward<GetF>(_get), std::forward<SetF>(_set), std::forward<ArgsF>(_args)...};
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
            return EnumF<ValT, GetF, SetF, decltype(get_sv), decltype(set_sv)>{_name, std::forward<GetF>(_get), std::forward<SetF>(_set), get_sv, set_sv};
        }
    } else if constexpr (type_id == TypeIdE::String) {
        auto view = [_get]() {
            return std::string_view{_get()};
        };
        return StringF<ValT, GetF, SetF, decltype(view)>{_name, std::forward<GetF>(_get), std::forward<SetF>(_set), view};
    } else if constexpr (type_id == TypeIdE::Container) {
        // RetT (not ValT): a getter returning a reference to a real container
        // stores that reference; one returning a view (std::span) stores the
        // view by value.
        return ContainerF<RetT, GetF, SetF>{_name, std::forward<GetF>(_get), std::forward<SetF>(_set)};
    } else {
        static_assert(type_id == TypeIdE::Boolean, "makef: unsupported value type");
    }
}

template <detail::AnyContainerC ValT>
template <typename T,
    typename V,
    typename C,
    typename... Args>
void Container<ValT>::emplace_back_with(
    this T& rthis,
    V&&     _rv,
    C&      _rc,
    Args&&... args)
    requires detail::ContainerC<ValT>
{
    rthis.ref.emplace_back(std::forward<Args>(args)...);
    solid::reflection::v2::reflect(rthis.ref.back(), std::forward<V>(_rv), _rc);
}

// for_each hands the visitor one metadata descriptor per element (id<0> and a
// descriptor with an empty name), so element boundaries stay visible to the
// visitor and the walked elements are never exposed directly.
template <detail::AnyContainerC ValT>
template <typename T,
    typename V,
    typename C>
void Container<ValT>::for_each(
    this T& rthis,
    V&&     _rv,
    C&      _rc)
{
    for (auto& re : rthis.ref) {
        std::invoke(_rv, solid::reflection::v2::id<0>, solid::reflection::v2::make(""sv, re), _rc);
    }
}

template <detail::StdArrayC ValT>
template <typename T,
    typename V,
    typename C>
void Container<ValT>::for_each(
    this T& rthis,
    V&&     _rv,
    C&      _rc)
{
    for (auto& re : rthis.ref) {
        std::invoke(_rv, solid::reflection::v2::id<0>, solid::reflection::v2::make(""sv, re), _rc);
    }
}

template <detail::StdArrayC ValT>
template <typename T,
    typename V,
    typename C,
    typename... Args>
void Container<ValT>::emplace_back_with(
    this T& rthis,
    V&&     _rv,
    C&      _rc,
    Args&&... args)
{
    auto& item = rthis.ref.at(rthis.offset++);
    item       = {std::forward<Args>(args)...};
    solid::reflection::v2::reflect(item, std::forward<V>(_rv), _rc);
}

// Bits are visited through a Boolean descriptor over a local copy; a mutation
// made by the visitor is written back on the non-const path.
template <detail::BitsetC ValT>
template <typename T,
    typename V,
    typename C>
void Container<ValT>::for_each(
    this T& rthis,
    V&&     _rv,
    C&      _rc)
{
    for (size_t i = 0; i < rthis.ref.size(); ++i) {
        bool val = rthis.ref[i];
        std::invoke(_rv, solid::reflection::v2::id<0>, solid::reflection::v2::make(""sv, val), _rc);
        if constexpr (!std::is_const_v<ValT>) {
            rthis.ref[i] = val;
        }
    }
}

template <detail::BitVectorC ValT>
template <typename T,
    typename V,
    typename C>
void Container<ValT>::for_each(
    this T& rthis,
    V&&     _rv,
    C&      _rc)
{
    for (size_t i = 0; i < rthis.ref.size(); ++i) {
        bool val = rthis.ref[i];
        std::invoke(_rv, solid::reflection::v2::id<0>, solid::reflection::v2::make(""sv, val), _rc);
        if constexpr (!std::is_const_v<ValT>) {
            rthis.ref[i] = val;
        }
    }
}

template <typename ValT>
template <typename T,
    typename V,
    typename C>
bool Pointer<ValT>::with_if(
    this T& rthis,
    V&&     _rv,
    C&      _rc)
{
    if (rthis.has_value()) {
        solid::reflection::v2::reflect(*rthis.ref, std::forward<V>(_rv), _rc);
        return true;
    }
    return false;
}

template <typename ValT>
template <typename T,
    typename V,
    typename C>
bool Optional<ValT>::with_if(
    this T& rthis,
    V&&     _rv,
    C&      _rc)
{
    if (rthis.has_value()) {
        solid::reflection::v2::reflect(*rthis.ref, std::forward<V>(_rv), _rc);
        return true;
    }
    return false;
}

template <typename ValT>
template <typename T,
    typename V,
    typename C>
void Reflectable<ValT>::reflect(
    this T& rthis,
    V&&     _rv,
    C&      _rc)
{
    solid::reflection::v2::reflect(rthis.ref, std::forward<V>(_rv), _rc, rthis.name);
}

// Invokes _rr(descriptor, ctx) for the field whose id matches Id; ids of
// different kinds (integral vs enumerator) compare through their size_t value.
template <detail::FieldIdC auto Id,
    typename T,
    typename R,
    typename Ctx>
void reflect_at(
    T&   rt,
    R&&  _rr,
    Ctx& _rc)
{
    auto ref = [&_rr](auto _id, auto&& _field, Ctx& _rc) {
        if constexpr (static_cast<size_t>(_id.value) == static_cast<size_t>(Id)) {
            _rr(std::forward<decltype(_field)>(_field), _rc);
        }
    };
    reflect(rt, ref, _rc);
}

template <typename ValT>
template <typename This,
    typename T,
    typename V,
    typename C,
    typename... Args>
void Variant<ValT>::emplace_with(
    this This& rthis,
    V&&        _rv,
    C&         _rc,
    Args&&... args)
{
    auto& rval{rthis.ref.template emplace<T>(std::forward<Args>(args)...)};
    solid::reflection::v2::reflect(rval, std::forward<V>(_rv), _rc);
}

template <typename ValT>
template <typename This,
    size_t I,
    typename V,
    typename C,
    typename... Args>
void Variant<ValT>::emplace_with(
    this This& rthis,
    V&&        _rv,
    C&         _rc,
    Args&&... args)
{
    auto& rval{rthis.ref.template emplace<I>(std::forward<Args>(args)...)};
    solid::reflection::v2::reflect(rval, std::forward<V>(_rv), _rc);
}

template <typename ValT>
template <typename This,
    typename V,
    typename C>
bool Variant<ValT>::with_if(
    this This& rthis,
    V&&        _rv,
    C&         _rc)
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

#ifdef SOLID_USE_REFLECTION
namespace detail {
template <typename T>
constexpr bool is_annotate_min_v = false;
template <typename T>
constexpr bool is_annotate_min_v<annotate::min_t<T>> = true;

template <typename T>
constexpr bool is_annotate_max_v = false;
template <typename T>
constexpr bool is_annotate_max_v<annotate::max_t<T>> = true;

// Applies one annotation value to a member's descriptor, through the
// descriptor's own setters only.
template <auto Ann, typename Field>
void apply_annotation(Field& _rfield)
{
    using AnnT = decltype(Ann);
    if constexpr (std::is_same_v<AnnT, annotate::max_size_t>) {
        if constexpr (requires { _rfield.with_max_size(Ann.value); }) {
            _rfield.with_max_size(Ann.value);
        } else {
            static_assert(false, "annotate::max_size is not supported on this member kind");
        }
    } else if constexpr (is_annotate_min_v<AnnT> || is_annotate_max_v<AnnT>) {
        using ValueT = typename Field::value_type;
        static_assert(std::is_integral_v<ValueT>, "annotate::min/max need an integral member");
        static_assert(std::in_range<ValueT>(Ann.value), "annotate::min/max value is out of the member's range");
        constexpr auto value = static_cast<ValueT>(Ann.value);
        if constexpr (is_annotate_min_v<AnnT>) {
            _rfield.with_min(value);
        } else {
            _rfield.with_max(value);
        }
    }
}

consteval bool is_skipped(std::meta::info _member)
{
    for (auto annotation : std::meta::annotations_of(_member)) {
        if (std::meta::remove_cvref(std::meta::type_of(annotation)) == ^^annotate::skip_t) {
            return true;
        }
    }
    return false;
}

template <std::meta::info Member,
    size_t                Id,
    typename T,
    typename R,
    typename Ctx>
void native_reflect_member(T& _rv, R& _rr, Ctx& _rc)
{
    if constexpr (!is_skipped(Member)) { // a skipped member's descriptor is never instantiated
        auto field = make(std::meta::identifier_of(Member), _rv.[:Member:]);
        template for (constexpr auto annotation : std::define_static_array(std::meta::annotations_of(Member)))
        {
            using AnnT = std::remove_cvref_t<typename[:std::meta::type_of(annotation):]>;
            apply_annotation<std::meta::extract<AnnT>(annotation)>(field);
        }
        _rr(id<Id>, field, _rc);
    }
}
} // namespace detail

template <typename T,
    typename R,
    typename Ctx>
void detail::native_reflect(
    T&   _rv,
    R&   _rr,
    Ctx& _rc)
{
    using ValueT                  = std::remove_const_t<T>;
    static constexpr auto ctx     = std::meta::access_context::current();
    static constexpr auto bases   = std::define_static_array(std::meta::bases_of(^^ValueT, ctx));
    static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(^^ValueT, ctx));

    template for (constexpr auto base : bases)
    {
        using BaseT = typename[:std::meta::type_of(base):];
        solid::reflection::v2::reflect(static_cast<std::conditional_t<std::is_const_v<T>, BaseT const, BaseT>&>(_rv), _rr, _rc);
    }

    [&]<size_t... I>(std::index_sequence<I...>) {
        (detail::native_reflect_member<members[I], I + 1>(_rv, _rr, _rc), ...);
    }(std::make_index_sequence<members.size()>{});
}
#endif

} // namespace solid::reflection::v2

// Reflect a field exposed via accessors: getter `getf()` paired with setter
// `set_<getf>(value)`. The setter lambda is generic and only instantiated when
// actually called, so it is never materialized on the const (serialize) path.
// Used as _rr(id<N>, SFR_V2_MAKEF(rthis, getf), _rc).
#define SFR_V2_MAKEF(rthis, getf)                              \
    solid::reflection::v2::makef(                              \
        std::string_view{#getf},                               \
        [&rthis]() -> decltype(auto) { return rthis.getf(); }, \
        [&rthis](auto&&... _val) { rthis.set_##getf(std::forward<decltype(_val)>(_val)...); })
