// solid/utility/any.hpp
//
// Copyright (c) 2016,2020 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once
#include <algorithm>
#include <cstddef>
#include <typeindex>
#include <typeinfo>
#include <utility>

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/anyimpl.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/typetraits.hpp"

namespace solid {

template <size_t Size, size_t Align = alignof(std::uintptr_t)>
    requires(std::popcount(Align) == 1)
constexpr size_t any_size_to_small_size()
{
    constexpr size_t max = std::max(sizeof(std::uintptr_t), Align);
    static_assert(Size >= max, "size must be greater than sizeof(std::uintptr_t)");
    return Size - max;
}

constexpr size_t any_default_size  = any_size_to_small_size<32>();
constexpr size_t any_default_align = sizeof(std::uintptr_t);

template <
    size_t      SmallSize  = any_default_size,
    size_t      SmallAlign = any_default_align,
    StoreOption Option     = StoreOption::AcceptBig>
    requires(SmallSize > 0 and SmallSize >= SmallAlign
        and (SmallSize % SmallAlign == 0) and std::popcount(SmallAlign) == 1)
class Any;

template <class T>
struct is_any;

template <size_t V, size_t A, StoreOption O>
struct is_any<Any<V, A, O>> : std::true_type {
};

template <class T>
struct is_any : std::false_type {
};

template <class T>
inline constexpr bool is_any_v = is_any<T>::value;

namespace any_impl {

struct SmallRTTI;
struct BigRTTI;

struct BaseRTTI {
    using DestroyFncT  = void(void*) noexcept;
    using CopyFncT     = uintptr_t(const void*, void*, size_t, size_t, void*&);
    using MoveFncT     = uintptr_t(void*, void*, size_t, size_t, void*&);
    using GetIfFncT    = const void*(const std::type_index&, const void*);
    using GetTypeInfoT = std::type_info const*() noexcept;

    CopyFncT&     copy_fnc_;
    MoveFncT&     move_fnc_;
    GetIfFncT&    get_if_fnc_;
    GetTypeInfoT& get_type_info_fnc_;
    const bool    is_copyable_;
    const bool    is_movable_;
    const bool    is_tuple_;

    static BaseRTTI const& get(uintptr_t const _rtti) noexcept
    {
        return *reinterpret_cast<const BaseRTTI*>(_rtti & reversed_representation_and_flags_mask);
    }
};

struct BigRTTI : BaseRTTI {

    DestroyFncT& destroy_fnc_;

    template <class T>
    static void destroy(void* const _what) noexcept
    {
        ::delete static_cast<T*>(_what);
    }

    static BigRTTI const& get(uintptr_t const _rtti) noexcept
    {
        return *reinterpret_cast<const BigRTTI*>(_rtti & reversed_representation_and_flags_mask);
    }
};

struct SmallRTTI : BaseRTTI {

    DestroyFncT* pdestroy_fnc_;

    template <class T>
    static void destroy(void* const _what) noexcept
    {
        std::destroy_at(std::launder(static_cast<T*>(_what)));
    }

    static SmallRTTI const& get(uintptr_t const _rtti) noexcept
    {
        return *reinterpret_cast<const SmallRTTI*>(_rtti & reversed_representation_and_flags_mask);
    }
};

template <class T>
uintptr_t do_copy(
    const void*,
    void*, size_t, size_t,
    void*&);

template <class T>
uintptr_t do_move(
    void*,
    void*, size_t, size_t,
    void*&);

template <class T>
uintptr_t do_move_big(
    void*,
    void*, size_t, size_t,
    void*&);

template <class T>
const void* do_get_if(const std::type_index&, const void*);

template <class T>
std::type_info const* get_type_info() noexcept;

template <class T>
inline constexpr BigRTTI big_rtti = {
    {do_copy<T>,
        do_move_big<T>,
        do_get_if<T>,
        get_type_info<T>,
        std::is_copy_constructible_v<T>,
        std::is_move_constructible_v<T>,
        is_specialization_v<T, std::tuple>},
    BigRTTI::destroy<T>};

template <class T>
inline constexpr SmallRTTI small_rtti = {
    {do_copy<T>,
        do_move<T>,
        do_get_if<T>,
        get_type_info<T>,
        std::is_copy_constructible_v<T>,
        std::is_move_constructible_v<T>,
        is_specialization_v<T, std::tuple>},
    std::is_trivially_copyable_v<T> ? nullptr : &SmallRTTI::destroy<T>};

template <class T>
uintptr_t do_copy(
    const void* _pfrom,
    void* _pto_small, const size_t _small_cap, const size_t _small_align,
    void*& _rpto_big)
{
    if constexpr (std::is_copy_constructible_v<T>) {
        if (sizeof(T) <= _small_cap and alignof(T) <= _small_align) {
            T&       rdst = *static_cast<T*>(_pto_small);
            const T& rsrc = *static_cast<const T*>(_pfrom);
            ::new (std::addressof(rdst)) T(rsrc);
            return representation(&small_rtti<T>, RepresentationE::Small);
        } else {
            _rpto_big = ::new T(*static_cast<const T*>(_pfrom));
            return representation(&big_rtti<T>, RepresentationE::Big);
        }
    } else {
        solid_throw("Any: contained value not copyable");
        return 0;
    }
}

template <class T>
uintptr_t do_move(
    void* _pfrom,
    void* _pto_small, const size_t _small_cap, const size_t _small_align,
    void*& _rpto_big)
{
    if constexpr (std::is_move_constructible_v<T>) {
        if (sizeof(T) <= _small_cap and alignof(T) <= _small_align) {
            T& rdst = *static_cast<T*>(_pto_small);
            T& rsrc = *static_cast<T*>(_pfrom);
            ::new (std::addressof(rdst)) T{std::move(rsrc)};
            return representation(&small_rtti<T>, RepresentationE::Small);
        } else {
            _rpto_big = ::new T{std::move(*static_cast<T*>(_pfrom))};
            return representation(&big_rtti<T>, RepresentationE::Big);
        }
    } else {
        solid_throw("Any: contained value not movable");
        return 0;
    }
}

template <class T>
uintptr_t do_move_big(
    void* _pfrom,
    void* _pto_small, const size_t _small_cap, const size_t _small_align,
    void*& _rpto_big)
{
    if constexpr (std::is_move_constructible_v<T>) {
        if (sizeof(T) <= _small_cap and alignof(T) <= _small_align) {
            T& rdst = *static_cast<T*>(_pto_small);
            T& rsrc = *static_cast<T*>(_pfrom);
            ::new (std::addressof(rdst)) T{std::move(rsrc)};
            return representation(&small_rtti<T>, RepresentationE::Small);
        } else {
            _rpto_big = static_cast<T*>(_pfrom);
            return representation(&big_rtti<T>, RepresentationE::Big);
        }
    } else {
        _rpto_big = static_cast<T*>(_pfrom);
        return representation(&big_rtti<T>, RepresentationE::Big);
    }
}

template <typename Tuple, size_t Index = 0>
const void* tuple_get_if_helper(const Tuple& _rtuple, const std::type_index& _type_index)
{
    if constexpr (Index < std::tuple_size_v<Tuple>) {
        if (_type_index == std::type_index(typeid(std::tuple_element_t<Index, Tuple>))) {
            return &std::get<Index>(_rtuple);
        }
        return tuple_get_if_helper<Tuple, Index + 1>(_rtuple, _type_index);
    }
    return nullptr;
}

template <class T>
const void* do_get_if(const std::type_index& _type_index, const void* _pdata)
{
    if constexpr (is_specialization_v<T, std::tuple>) {
        return tuple_get_if_helper<>(*static_cast<const T*>(_pdata), _type_index);
    } else if (std::type_index(typeid(T)) == _type_index) {
        return _pdata;
    }
    return nullptr;
}

template <class T>
std::type_info const* get_type_info() noexcept
{
    return &typeid(T);
}

} // namespace any_impl

template <size_t SmallSize, size_t SmallAlign, StoreOption Option>
    requires(SmallSize > 0 and SmallSize >= SmallAlign
        and (SmallSize % SmallAlign == 0) and std::popcount(SmallAlign) == 1)
class Any {

    struct Small {
        alignas(SmallAlign) unsigned char data_[SmallSize];
    };

    struct Big {
        void* pdata_;
    };

    uintptr_t rtti_ = 0;
    union {
        Small small_;
        Big   big_;
    };

    template <size_t S, size_t A, StoreOption O>
        requires(S > 0 and S >= A
            and (S % A == 0) and std::popcount(A) == 1)
    friend class Any;

private:
    [[nodiscard]] const std::type_info* typeInfo() const noexcept
    {
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return any_impl::BaseRTTI::get(rtti).get_type_info_fnc_();
        }
        return nullptr;
    }

public:
    using ThisT = Any<SmallSize, SmallAlign, Option>;

    static constexpr size_t smallCapacity()
    {
        return SmallSize;
    }

    static constexpr size_t smallAlign()
    {
        return SmallAlign;
    }

    template <class T>
    static constexpr bool is_small_type()
    {
        return alignof(T) <= smallAlign() && sizeof(T) <= smallCapacity();
    }

    constexpr Any() noexcept
    {
        rtti_ = 0;
    }

    Any(const ThisT& _other)
    {
        doCopyFrom(_other);
    }

    template <size_t Sz, size_t Al, StoreOption Op>
    Any(const Any<Sz, Al, Op>& _other)
    {
        doCopyFrom(_other);
    }

    Any(ThisT&& _other) noexcept
    {
        doMoveFrom(_other);
    }

    template <size_t Sz, size_t Al, StoreOption Op>
    Any(Any<Sz, Al, Op>&& _other) noexcept
    {
        doMoveFrom(_other);
    }

    template <class T, StoreOption Opt = Option>
    Any(T&& _rvalue, std::integral_constant<StoreOption, Opt> = store_option_dispatch<Opt>())
        requires(not is_any_v<std::decay_t<T>> and not is_specialization_v<std::decay_t<T>, std::in_place_type_t>)
    {
        using ValueT = std::decay_t<T>;
        static_assert(Opt == StoreOption::AcceptBig or is_small_type<ValueT>(), "Value not small. Construct by using AcceptBigT{} or assign using .emplace()");
        doEmplace<std::decay_t<T>>(std::forward<T>(_rvalue));
    }

    template <class T, class... Args, StoreOption Opt = Option>
    explicit Any(std::in_place_type_t<T>, Args&&... _args, std::integral_constant<StoreOption, Opt> = store_option_dispatch<Opt>())
        requires(std::is_constructible_v<std::decay_t<T>, Args...>)
    {
        using ValueT = std::decay_t<T>;
        static_assert(Opt == StoreOption::AcceptBig or is_small_type<ValueT>(), "Value not small. Construct by using AcceptBigT{} or assign using .emplace()");
        doEmplace<std::decay_t<T>>(std::forward<Args>(_args)...);
    }

    template <class T, class E, class... Args, StoreOption Opt = Option>
    explicit Any(std::in_place_type_t<T>, std::initializer_list<E> _ilist, Args&&... _args, std::integral_constant<StoreOption, Opt> = store_option_dispatch<Opt>())
        requires(std::is_constructible_v<std::decay_t<T>, std::initializer_list<E>&, Args...> and std::is_copy_constructible_v<std::decay_t<T>>)
    {
        using ValueT = std::decay_t<T>;
        static_assert(Opt == StoreOption::AcceptBig or is_small_type<ValueT>(), "Value not small. Construct by using AcceptBigT{} or assign using .emplace()");
        doEmplace<std::decay_t<T>>(_ilist, std::forward<Args>(_args)...);
    }

    ~Any() noexcept
    {
        reset();
    }

    ThisT& operator=(const ThisT& _other)
    {
        *this = ThisT{_other};
        return *this;
    }

    ThisT& operator=(ThisT&& _other) noexcept
    {
        reset();
        doMoveFrom(_other);
        return *this;
    }

    template <size_t Sz, size_t Al, StoreOption Op>
    ThisT& operator=(const Any<Sz, Al, Op>& _other)
    {
        *this = ThisT{_other};
        return *this;
    }

    template <size_t Sz, size_t Al, StoreOption Op>
    ThisT& operator=(Any<Sz, Al, Op>&& _other) noexcept
    {
        reset();
        doMoveFrom(_other);
        return *this;
    }

    template <class T>
    ThisT& operator=(T&& _rvalue)
        requires(not is_any_v<std::decay_t<T>> and std::is_copy_constructible_v<std::decay_t<T>>)
    {
        *this = ThisT{std::forward<T>(_rvalue)};
        return *this;
    }
#if 0
    template <class T>
    ThisT& emplace(T&& _rvalue)
    {
        *this = ThisT{std::forward<T>(_rvalue), AcceptBigT{}};
        return *this;
    }

    template <class T, class... Args>
    ThisT& emplace(Args&&... _args)
        requires(std::is_constructible_v<std::decay_t<T>, Args...>)
    {
        *this = ThisT{std::in_place_type_t<T>{}, std::forward<Args>(_args)..., AcceptBigT{}};
        return *this;
    }
    template <class T, class E, class... Args>
    ThisT& emplace(std::initializer_list<E> _ilist, Args&&... _args)
        requires(std::is_constructible_v<std::decay_t<T>, std::initializer_list<E>&, Args...>)
    {
        *this = ThisT{std::in_place_type_t<T>{}, _ilist, std::forward<Args>(_args)..., AcceptBigT{}};
        return *this;
    }
#endif
    void reset() noexcept
    {
        auto const rtti = rtti_;
        switch (any_impl::representation(rtti)) {
        [[likely]] case any_impl::RepresentationE::Small: {
            auto& rrtti = any_impl::SmallRTTI::get(rtti);
            if (rrtti.pdestroy_fnc_) {
                rrtti.pdestroy_fnc_(small_.data_);
            }
        } break;
        case any_impl::RepresentationE::Big: {
            any_impl::BigRTTI::get(rtti).destroy_fnc_(big_.pdata_);
        } break;
        case any_impl::RepresentationE::None:
        default:
            break;
        }
        rtti_ = 0u;
    }

    template <size_t Sz, size_t Al, StoreOption Op>
    void swap(Any<Sz, Al, Op>& _other) noexcept
    {
        _other = std::exchange(*this, std::move(_other));
    }

    [[nodiscard]] bool has_value() const noexcept
    {
        return rtti_ != 0u;
    }

    explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return !has_value();
    }

    [[nodiscard]] const std::type_info& type() const noexcept
    {
        const std::type_info* const pinfo = typeInfo();
        return pinfo ? *pinfo : typeid(void);
    }

    template <class T>
    const T* cast() const noexcept
    {
        const std::type_info* const pinfo = typeInfo();
        if (!pinfo || std::type_index(*pinfo) != std::type_index(typeid(T))) {
            return nullptr;
        }

        if constexpr (is_small_type<T>()) {
            return reinterpret_cast<const T*>(&small_.data_);
        } else {
            return static_cast<const T*>(big_.pdata_);
        }
    }

    template <class T>
    T* cast() noexcept
    {
        return const_cast<T*>(static_cast<const ThisT*>(this)->cast<T>());
    }

    template <class T>
    const T* get_if() const noexcept
    {
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            void const* pdata = any_impl::representation(rtti) == any_impl::RepresentationE::Small ? small_.data_ : big_.pdata_;
            return static_cast<const T*>(any_impl::BaseRTTI::get(rtti).get_if_fnc_(std::type_index(typeid(T)), pdata));
        }
        return nullptr;
    }

    template <class T>
    T* get_if() noexcept
    {
        return const_cast<T*>(static_cast<const ThisT*>(this)->get_if<T>());
    }

    [[nodiscard]] bool is_movable() const
    {
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return any_impl::BaseRTTI::get(rtti).is_movable_;
        }
        return true;
    }
    [[nodiscard]] bool is_copyable() const
    {
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return any_impl::BaseRTTI::get(rtti).is_copyable_;
        }
        return true;
    }

    [[nodiscard]] bool is_tuple() const
    {
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return any_impl::BaseRTTI::get(rtti).is_tuple_;
        }
        return false;
    }

    [[nodiscard]] bool is_small() const
    {
        return any_impl::representation(rtti_) == any_impl::RepresentationE::Small;
    }

    [[nodiscard]] bool is_big() const
    {
        return any_impl::representation(rtti_) == any_impl::RepresentationE::Big;
    }

private:
    template <size_t Sz, size_t Al, StoreOption Op>
    void doMoveFrom(Any<Sz, Al, Op>& _other)
    {
        auto const rtti = _other.rtti_;

        switch (any_impl::representation(rtti)) {
        case any_impl::RepresentationE::Small: {
            rtti_ = any_impl::BaseRTTI::get(rtti).move_fnc_(
                _other.small_.data_,
                small_.data_, smallCapacity(), smallAlign(),
                big_.pdata_);
            _other.reset();
        } break;
        case any_impl::RepresentationE::Big: {
            rtti_ = any_impl::BaseRTTI::get(rtti).move_fnc_(
                _other.big_.pdata_,
                small_.data_, smallCapacity(), smallAlign(),
                big_.pdata_);
            if (is_big()) {
                _other.rtti_ = 0u;
            }
            _other.reset();
        } break;
        default:
            break;
        }
    }

    template <size_t Sz, size_t Al, StoreOption Op>
    void doCopyFrom(const Any<Sz, Al, Op>& _other)
    {
        auto const rtti = _other.rtti_;

        switch (any_impl::representation(rtti)) {
        case any_impl::RepresentationE::Small: {
            rtti_ = any_impl::BaseRTTI::get(rtti).copy_fnc_(
                _other.small_.data_,
                small_.data_, smallCapacity(), smallAlign(),
                big_.pdata_);
        } break;
        case any_impl::RepresentationE::Big: {
            rtti_ = any_impl::BaseRTTI::get(rtti).copy_fnc_(
                _other.big_.pdata_,
                small_.data_, smallCapacity(), smallAlign(),
                big_.pdata_);
        } break;
        default:
            break;
        }
    }

    template <class T, class... Args>
    T& doEmplace(Args&&... _args)
    {
        if constexpr (is_small_type<T>()) {
            auto& rval = reinterpret_cast<T&>(small_.data_);
            ::new (std::addressof(rval)) T{std::forward<Args>(_args)...};
            rtti_ = representation(&any_impl::small_rtti<T>, any_impl::RepresentationE::Small);
            return rval;
        } else {
            T* const ptr = ::new T(std::forward<Args>(_args)...);
            big_.pdata_  = ptr;
            rtti_        = representation(&any_impl::big_rtti<T>, any_impl::RepresentationE::Big);
            return *ptr;
        }
    }
};

//-----------------------------------------------------------------------------

template <size_t S1, size_t A1, StoreOption O1, size_t S2, size_t A2, StoreOption O2>
inline void swap(Any<S1, A1, O1>& _a1, Any<S2, A2, O2>& _a2) noexcept
{
    _a1.swap(_a2);
}

using Any64T  = Any<any_size_to_small_size<64>()>;
using Any96T  = Any<any_size_to_small_size<96>()>;
using Any128T = Any<any_size_to_small_size<128>()>;
using Any256T = Any<any_size_to_small_size<256>()>;

using SmallAnyT    = Any<any_default_size, any_default_align, StoreOption::RejectBig>;
using SmallAny64T  = Any<any_size_to_small_size<64>(), any_default_align, StoreOption::RejectBig>;
using SmallAny96T  = Any<any_size_to_small_size<96>(), any_default_align, StoreOption::RejectBig>;
using SmallAny128T = Any<any_size_to_small_size<128>(), any_default_align, StoreOption::RejectBig>;
using SmallAny256T = Any<any_size_to_small_size<256>(), any_default_align, StoreOption::RejectBig>;

static_assert(sizeof(Any<>) == 32);
static_assert(sizeof(Any64T) == 64);
static_assert(sizeof(Any96T) == 96);
static_assert(sizeof(Any128T) == 128);
static_assert(sizeof(Any256T) == 256);

//-----------------------------------------------------------------------------

} // namespace solid
