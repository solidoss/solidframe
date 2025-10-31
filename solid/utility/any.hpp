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
#include "solid/utility/typetraits.hpp"

namespace solid {

constexpr size_t any_default_size  = 32 - sizeof(std::uintptr_t);
constexpr size_t any_default_align = sizeof(std::uintptr_t);

template <size_t SmallSize = any_default_size, size_t SmallAlign = any_default_align>
    requires(SmallSize > 0 and SmallSize >= SmallAlign
        and (SmallSize % SmallAlign == 0) and std::popcount(SmallAlign) == 1)
class Any;

template <class T>
struct is_any;

template <size_t V, size_t A>
struct is_any<Any<V, A>> : std::true_type {
};

template <class T>
struct is_any : std::false_type {
};

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
            ::new (const_cast<void*>(static_cast<const volatile void*>(std::addressof(rdst)))) T(rsrc);
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
            ::new (const_cast<void*>(static_cast<const volatile void*>(std::addressof(rdst)))) T{std::move(rsrc)};
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
            ::new (const_cast<void*>(static_cast<const volatile void*>(std::addressof(rdst)))) T{std::move(rsrc)};
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

template <size_t SmallSize, size_t SmallAlign>
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

    template <size_t S, size_t A>
        requires(S > 0 and S >= A
            and (S % A == 0) and std::popcount(A) == 1)
    friend class Any;

private:
    const std::type_info* typeInfo() const noexcept
    {
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return any_impl::BaseRTTI::get(rtti).get_type_info_fnc_();
        }
        return nullptr;
    }

public:
    using ThisT = Any<SmallSize, SmallAlign>;

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

    template <size_t Sz>
    Any(const Any<Sz>& _other)
    {
        doCopyFrom(_other);
    }

    Any(ThisT&& _other) noexcept
    {
        doMoveFrom(_other);
    }

    template <size_t Sz>
    Any(Any<Sz>&& _other) noexcept
    {
        doMoveFrom(_other);
    }

    template <
        class T, std::enable_if_t<std::conjunction_v<std::negation<is_any<std::decay_t<T>>>, std::negation<is_specialization<std::decay_t<T>, std::in_place_type_t>>>, int> = 0>
    Any(T&& _rvalue)
    {
        doEmplace<std::decay_t<T>>(std::forward<T>(_rvalue));
    }

    template <class T, class... Args,
        std::enable_if_t<
            std::conjunction_v<std::is_constructible<std::decay_t<T>, Args...> /*, std::is_copy_constructible<std::decay_t<T>>*/>,
            int>
        = 0>
    explicit Any(std::in_place_type_t<T>, Args&&... _args)
    {
        doEmplace<std::decay_t<T>>(std::forward<Args>(_args)...);
    }

    template <class T, class E, class... Args,
        std::enable_if_t<std::conjunction_v<std::is_constructible<std::decay_t<T>, std::initializer_list<E>&, Args...>,
                             std::is_copy_constructible<std::decay_t<T>>>,
            int>
        = 0>
    explicit Any(std::in_place_type_t<T>, std::initializer_list<E> _ilist, Args&&... _args)
    {
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

    template <size_t Sz>
    ThisT& operator=(const Any<Sz>& _other)
    {
        *this = ThisT{_other};
        return *this;
    }

    template <size_t Sz>
    ThisT& operator=(Any<Sz>&& _other) noexcept
    {
        reset();
        doMoveFrom(_other);
        return *this;
    }

    template <class T, std::enable_if_t<std::conjunction_v<std::negation<is_any<std::decay_t<T>>>, std::is_copy_constructible<std::decay_t<T>>>, int> = 0>
    ThisT& operator=(T&& _rvalue)
    {
        *this = ThisT{std::forward<T>(_rvalue)};
        return *this;
    }

    template <class T, class... Args,
        std::enable_if_t<
            std::conjunction_v<std::is_constructible<std::decay_t<T>, Args...>>,
            int>
        = 0>
    std::decay_t<T>& emplace(Args&&... _args)
    {
        reset();
        return doEmplace<std::decay_t<T>>(std::forward<Args>(_args)...);
    }
    template <class T, class E, class... Args,
        std::enable_if_t<std::conjunction_v<std::is_constructible<std::decay_t<T>, std::initializer_list<E>&, Args...> /*,
        is_copy_constructible<decay_t<T>>*/
                             >,
            int>
        = 0>
    std::decay_t<T>& emplace(std::initializer_list<E> _ilist, Args&&... _args)
    {
        reset();
        return doEmplace<std::decay_t<T>>(_ilist, std::forward<Args>(_args)...);
    }

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

    template <size_t Sz>
    void swap(Any<Sz>& _other) noexcept
    {
        _other = std::exchange(*this, std::move(_other));
    }

    bool has_value() const noexcept
    {
        return rtti_ != 0u;
    }

    explicit operator bool() const noexcept
    {
        return has_value();
    }

    bool empty() const noexcept
    {
        return !has_value();
    }

    const std::type_info& type() const noexcept
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

    bool is_movable() const
    {
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return any_impl::BaseRTTI::get(rtti).is_movable_;
        }
        return true;
    }
    bool is_copyable() const
    {
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return any_impl::BaseRTTI::get(rtti).is_copyable_;
        }
        return true;
    }

    bool is_tuple() const
    {
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return any_impl::BaseRTTI::get(rtti).is_tuple_;
        }
        return false;
    }

    bool is_small() const
    {
        return any_impl::representation(rtti_) == any_impl::RepresentationE::Small;
    }
    bool is_big() const
    {
        return any_impl::representation(rtti_) == any_impl::RepresentationE::Big;
    }

private:
    template <size_t Sz, size_t Al>
    void doMoveFrom(Any<Sz, Al>& _other)
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

    template <size_t Sz, size_t Al>
    void doCopyFrom(const Any<Sz, Al>& _other)
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
            ::new (const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval)))) T{std::forward<Args>(_args)...};
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

template <size_t S1, size_t A1, size_t S2, size_t A2>
inline void swap(Any<S1, A1>& _a1, Any<S2, A2>& _a2) noexcept
{
    _a1.swap(_a2);
}

template <class T, size_t Size = any_default_size, size_t Align = any_default_align, class... Args>
auto make_any(Args&&... _args)
{
    return Any<Size, Align>{std::in_place_type<T>, std::forward<Args>(_args)...};
}
template <class T, size_t Size = any_default_size, size_t Align = any_default_align, class E, class... Args>
auto make_any(std::initializer_list<E> _ilist, Args&&... _args)
{
    return Any<Size, Align>{std::in_place_type<T>, _ilist, std::forward<Args>(_args)...};
}

//-----------------------------------------------------------------------------

} // namespace solid
