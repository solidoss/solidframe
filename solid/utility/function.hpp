// solid/utility/function.hpp
//
// Copyright (c) 2018,2020 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#pragma once

// #define SOLID_THROW_ON_BIG_FUNCTION

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <solid/utility/any.hpp>
#include <type_traits>
#include <typeindex>
#include <utility>

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/anyimpl.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/typetraits.hpp"

namespace solid {

template <size_t Size, size_t Align = alignof(std::uintptr_t)>
    requires(std::popcount(Align) == 1)
constexpr size_t function_size_to_small_size()
{
    constexpr size_t max = std::max(sizeof(std::uintptr_t), Align);
    static_assert(Size >= max, "size must be greater than sizeof(std::uintptr_t)");
    return Size - max;
}

constexpr size_t function_default_size  = function_size_to_small_size<32>();
constexpr size_t function_default_align = alignof(std::uintptr_t);

template <class,
    size_t      SmallSize  = function_default_size,
    size_t      SmallAlign = function_default_align,
    StoreOption Option     = StoreOption::AcceptBig>
    requires(SmallSize > 0 and SmallSize >= SmallAlign
        and (SmallSize % SmallAlign == 0) and std::popcount(SmallAlign) == 1)
class Function; // undefined

template <class T>
struct is_function;

template <class R, class... ArgTypes, size_t SmallSize, size_t SmallAlign, StoreOption Option>
struct is_function<Function<R(ArgTypes...), SmallSize, SmallAlign, Option>> : std::true_type {
};

template <class T>
struct is_function : std::false_type {
};

template <class T>
inline constexpr bool is_function_v = is_function<T>::value;

namespace fnc_impl {

using any_impl::representation;
using any_impl::RepresentationE;
using any_impl::reversed_representation_and_flags_mask;

template <size_t SmallSize, size_t SmallAlign>
struct Storage {
    union Big {
        alignas(SmallAlign) mutable void* ptr_;
        alignas(SmallAlign) mutable char data_[SmallSize];
    };
    union {
        mutable Big  big_;
        mutable char data_[sizeof(Big)];
    };

    template <class T>
    static constexpr bool is_small_type()
    {
        return alignof(T) <= SmallAlign && sizeof(T) <= SmallSize;
    }

    void ptr(void* _ptr)
    {
        big_.ptr_ = _ptr;
    }

    void*& ptr() const noexcept
    {
        return big_.ptr_;
    }
    char* data() const noexcept
    {
        return data_;
    }
};

template <size_t SmallSize, size_t SmallAlign, class R, class... ArgTypes>
struct BaseRTTI {
    using CopyFncT   = uintptr_t(const void*, void*, void*&);
    using MoveFncT   = uintptr_t(void*, void*, void*&);
    using InvokeFncT = R(Storage<SmallSize, SmallAlign> const&, ArgTypes&&...);

    InvokeFncT& invoke_fnc_;
    CopyFncT&   copy_fnc_;
    MoveFncT&   move_fnc_;
    const bool  is_copyable_;
    const bool  is_movable_;

    static BaseRTTI const& get(uintptr_t const _rtti) noexcept
    {
        return *reinterpret_cast<const BaseRTTI*>(_rtti & reversed_representation_and_flags_mask);
    }

    template <class T>
    static R do_invoke(Storage<SmallSize, SmallAlign> const& _rstorage, ArgTypes&&... _args)
    {
        T* pfun;
        if constexpr (Storage<SmallSize, SmallAlign>::template is_small_type<T>()) {
            pfun = reinterpret_cast<T*>(_rstorage.data());
        } else {
            pfun = reinterpret_cast<T*>(_rstorage.ptr());
        }
        return std::invoke(*pfun, static_cast<ArgTypes&&>(_args)...);
    }
};

template <size_t SmallSize, size_t SmallAlign, class R, class... ArgTypes>
struct BigRTTI : BaseRTTI<SmallSize, SmallAlign, R, ArgTypes...> {
    using DestroyFncT = void(void*) noexcept;
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

template <size_t SmallSize, size_t SmallAlign, class R, class... ArgTypes>
struct SmallRTTI : BaseRTTI<SmallSize, SmallAlign, R, ArgTypes...> {
    using DestroyFncT = void(void*) noexcept;

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

template <size_t SmallSize, size_t SmallAlign, class T, class R, class... ArgTypes>
uintptr_t do_copy(
    const void*,
    void*,
    void*&);

template <size_t SmallSize, size_t SmallAlign, class T, class R, class... ArgTypes>
uintptr_t do_move(
    void*,
    void*,
    void*&);

template <size_t SmallSize, size_t SmallAlign, class T, class R, class... ArgTypes>
uintptr_t do_move_big(
    void*,
    void*,
    void*&);

template <size_t SmallSize, size_t SmallAlign, class T, class R, class... ArgTypes>
inline constexpr BigRTTI<SmallSize, SmallAlign, R, ArgTypes...> big_rtti = {
    {BaseRTTI<SmallSize, SmallAlign, R, ArgTypes...>::template do_invoke<T>,
        do_copy<SmallSize, SmallAlign, T, R, ArgTypes...>,
        do_move_big<SmallSize, SmallAlign, T, R, ArgTypes...>,
        std::is_copy_constructible_v<T>,
        std::is_move_constructible_v<T>},
    BigRTTI<SmallSize, SmallAlign, R, ArgTypes...>::template destroy<T>,
};

template <size_t SmallSize, size_t SmallAlign, class T, class R, class... ArgTypes>
inline constexpr SmallRTTI<SmallSize, SmallAlign, R, ArgTypes...> small_rtti = {
    {BaseRTTI<SmallSize, SmallAlign, R, ArgTypes...>::template do_invoke<T>,
        do_copy<SmallSize, SmallAlign, T, R, ArgTypes...>,
        do_move<SmallSize, SmallAlign, T, R, ArgTypes...>,
        std::is_copy_constructible_v<T>,
        std::is_move_constructible_v<T>},
    std::is_trivially_copyable_v<T> ? nullptr : &SmallRTTI<SmallSize, SmallAlign, R, ArgTypes...>::template destroy<T>,
};

template <size_t SmallSize, size_t SmallAlign, class T, class R, class... ArgTypes>
uintptr_t do_copy(
    const void* _pfrom,
    void*       _pto_small,
    void*&      _rpto_big)
{
    if constexpr (std::is_copy_constructible_v<T>) {
        if constexpr (sizeof(T) <= SmallSize and alignof(T) <= SmallAlign) {
            T&       rdst = *static_cast<T*>(_pto_small);
            const T& rsrc = *static_cast<const T*>(_pfrom);
            ::new (std::addressof(rdst)) T(rsrc);
            return representation(&small_rtti<SmallSize, SmallAlign, T, R, ArgTypes...>, RepresentationE::Small);
        } else {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
            solid_throw("Big Function");
#endif
            _rpto_big = ::new T(*static_cast<const T*>(_pfrom));
            return representation(&big_rtti<SmallSize, SmallAlign, T, R, ArgTypes...>, RepresentationE::Big);
        }
    } else {
        solid_throw("Function: contained value not copyable");
        return 0;
    }
}

template <size_t SmallSize, size_t SmallAlign, class T, class R, class... ArgTypes>
uintptr_t do_move(
    void*  _pfrom,
    void*  _pto_small,
    void*& _rpto_big)
{
    if constexpr (std::is_move_constructible_v<T>) {
        if constexpr (sizeof(T) <= SmallSize and alignof(T) <= SmallAlign) {
            T& rdst = *static_cast<T*>(_pto_small);
            T& rsrc = *static_cast<T*>(_pfrom);
            ::new (std::addressof(rdst)) T{std::move(rsrc)};
            return representation(&small_rtti<SmallSize, SmallAlign, T, R, ArgTypes...>, RepresentationE::Small);
        } else {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
            solid_throw("Big Function");
#endif
            _rpto_big = ::new T{std::move(*static_cast<T*>(_pfrom))};
            return representation(&big_rtti<SmallSize, SmallAlign, T, R, ArgTypes...>, RepresentationE::Big);
        }
    } else {
        solid_throw("Function: contained value not movable");
        return 0u;
    }
}

template <size_t SmallSize, size_t SmallAlign, class T, class R, class... ArgTypes>
uintptr_t do_move_big(
    void*  _pfrom,
    void*  _pto_small,
    void*& _rpto_big)
{
    if constexpr (std::is_move_constructible_v<T>) {
        if constexpr (sizeof(T) <= SmallSize and alignof(T) <= SmallAlign) {
            T& rdst = *static_cast<T*>(_pto_small);
            T& rsrc = *static_cast<T*>(_pfrom);
            ::new (std::addressof(rdst)) T{std::move(rsrc)};
            return representation(&small_rtti<SmallSize, SmallAlign, T, R, ArgTypes...>, RepresentationE::Small);
        } else {
            _rpto_big = static_cast<T*>(_pfrom);
            return representation(&big_rtti<SmallSize, SmallAlign, T, R, ArgTypes...>, RepresentationE::Big);
        }
    } else {
        _rpto_big = static_cast<T*>(_pfrom);
        return representation(&big_rtti<SmallSize, SmallAlign, T, R, ArgTypes...>, RepresentationE::Big);
    }
}

} // namespace fnc_impl

template <class R, class... ArgTypes,
    size_t SmallSize, size_t SmallAlign,
    StoreOption Option>
    requires(SmallSize > 0 and SmallSize >= SmallAlign
        and (SmallSize % SmallAlign == 0) and std::popcount(SmallAlign) == 1)
class Function<R(ArgTypes...), SmallSize, SmallAlign, Option> {
    using BaseRTTI_T  = fnc_impl::BaseRTTI<SmallSize, SmallAlign, R, ArgTypes...>;
    using SmallRTTI_T = fnc_impl::SmallRTTI<SmallSize, SmallAlign, R, ArgTypes...>;
    using BigRTTI_T   = fnc_impl::BigRTTI<SmallSize, SmallAlign, R, ArgTypes...>;
    using StorageT    = fnc_impl::Storage<SmallSize, SmallAlign>;

    uintptr_t        rtti_ = 0;
    mutable StorageT storage_{};

    template <class F, size_t S, size_t A, StoreOption O>
        requires(S > 0 and S >= A
            and (S % A == 0) and std::popcount(A) == 1)
    friend class Function;

public:
    using ThisT = Function<R(ArgTypes...), SmallSize, SmallAlign, Option>;

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
        return StorageT::template is_small_type<T>();
    }

    Function() noexcept = default;

    Function(std::nullptr_t) noexcept {}

    Function(const ThisT& _other)
    {
        doCopyFrom(_other);
    }

    template <size_t Sz, size_t Al, StoreOption Op>
    Function(const Function<R(ArgTypes...), Sz, Al, Op>& _other)
    {
        doCopyFrom(_other);
    }

    Function(ThisT&& _other) noexcept
    {
        doMoveFrom(_other);
    }

    template <size_t Sz, size_t Al, StoreOption Op>
    Function(Function<R(ArgTypes...), Sz, Al, Op>&& _other) noexcept
    {
        doMoveFrom(_other);
    }

    template <class T, StoreOption Opt = Option>
    Function(T&& _fun, std::integral_constant<StoreOption, Opt> = store_option_dispatch<Opt>())
        requires(not is_function_v<std::decay_t<T>> and not is_specialization_v<std::decay_t<T>, std::in_place_type_t>)
    {
        using FncT = std::decay_t<T>;
        static_assert(Opt == StoreOption::AcceptBig or is_small_type<FncT>(), "Function not small. Construct by using AcceptBigT{} or assign using .emplace()");
        if constexpr (is_small_type<FncT>()) {
            rtti_      = fnc_impl::representation(&fnc_impl::small_rtti<SmallSize, SmallAlign, FncT, R, ArgTypes...>, fnc_impl::RepresentationE::Small);
            auto& rval = reinterpret_cast<FncT&>(storage_.data_);
            std::construct_at(std::addressof(rval), std::forward<T>(_fun));
        } else {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
            solid_throw("Big Function");
#endif
            FncT* const ptr = ::new FncT(std::forward<T>(_fun));
            storage_.ptr(ptr);
            rtti_ = fnc_impl::representation(&fnc_impl::big_rtti<SmallSize, SmallAlign, FncT, R, ArgTypes...>, fnc_impl::RepresentationE::Big);
        }
    }

    ~Function() noexcept
    {
        auto const rtti = rtti_;
        switch (fnc_impl::representation(rtti)) {
        [[likely]] case fnc_impl::RepresentationE::Small:
            if (auto* pf = SmallRTTI_T::get(rtti).pdestroy_fnc_) {
                (*pf)(storage_.data_);
            }
            break;
        case fnc_impl::RepresentationE::Big:
            BigRTTI_T::get(rtti).destroy_fnc_(storage_.ptr());
            break;
        case fnc_impl::RepresentationE::None:
            [[fallthrough]];
        default:
            break;
        }
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
    ThisT& operator=(const Function<R(ArgTypes...), Sz, Al, Op>& _other)
    {
        *this = ThisT{_other};
        return *this;
    }

    template <size_t Sz, size_t Al, StoreOption Op>
    ThisT& operator=(Function<R(ArgTypes...), Sz, Al, Op>&& _other) noexcept
    {
        reset();
        doMoveFrom(_other);
        return *this;
    }

    template <class T>
    ThisT& operator=(T&& _rvalue)
    {
        *this = ThisT{std::forward<T>(_rvalue)};
        return *this;
    }

    template <class T>
    ThisT& emplace(T&& _rvalue)
    {
        *this = ThisT{std::forward<T>(_rvalue), AcceptBigT{}};
        return *this;
    }

    void reset() noexcept
    {
        auto const rtti = rtti_;
        switch (fnc_impl::representation(rtti)) {
        [[likely]] case fnc_impl::RepresentationE::Small:
            if (auto* pfnc = SmallRTTI_T::get(rtti).pdestroy_fnc_) {
                (*pfnc)(storage_.data_);
            }
            break;
        case fnc_impl::RepresentationE::Big:
            BigRTTI_T::get(rtti).destroy_fnc_(storage_.ptr());
            break;
        case fnc_impl::RepresentationE::None:
            [[fallthrough]];
        default:
            break;
        }
        rtti_ = 0;
    }

    R operator()(ArgTypes... _args) const
    {
        if (empty()) {
            throw std::bad_function_call();
        }
        return BaseRTTI_T::get(rtti_).invoke_fnc_(storage_, static_cast<ArgTypes&&>(_args)...);
    }

    template <size_t Sz, size_t Al, StoreOption Op>
    void swap(Function<R(ArgTypes...), Sz, Al, Op>& _other) noexcept
    {
        _other = std::exchange(*this, std::move(_other));
    }

    [[nodiscard]] bool has_value() const noexcept
    {
        return rtti_ != 0;
    }
    [[nodiscard]] bool empty() const noexcept
    {
        return !has_value();
    }

    explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]] bool is_movable() const
    {
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return BaseRTTI_T::get(rtti).is_movable_;
        }
        return true;
    }
    [[nodiscard]] bool is_copyable() const
    {
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return BaseRTTI_T::get(rtti).is_copyable_;
        }
        return true;
    }

    [[nodiscard]] bool is_small() const
    {
        return fnc_impl::representation(rtti_) == fnc_impl::RepresentationE::Small;
    }

    [[nodiscard]] bool is_big() const
    {
        return fnc_impl::representation(rtti_) == fnc_impl::RepresentationE::Big;
    }

private:
    template <size_t Sz, size_t Al, StoreOption Op>
    void doMoveFrom(Function<R(ArgTypes...), Sz, Al, Op>& _other)
    {
        rtti_ = 0u;
        switch (fnc_impl::representation(_other.rtti_)) {
        case fnc_impl::RepresentationE::Small: {
            rtti_ = SmallRTTI_T::get(_other.rtti_).move_fnc_(_other.storage_.data(), storage_.data_, storage_.ptr());
            _other.reset();
        } break;
        case fnc_impl::RepresentationE::Big: {
            rtti_ = BigRTTI_T::get(_other.rtti_).move_fnc_(_other.storage_.ptr(), storage_.data_, storage_.ptr());
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
    void doCopyFrom(const Function<R(ArgTypes...), Sz, Al, Op>& _other)
    {
        rtti_ = 0u;
        switch (fnc_impl::representation(_other.rtti_)) {
        case fnc_impl::RepresentationE::Small: {
            rtti_ = SmallRTTI_T::get(_other.rtti_).copy_fnc_(_other.storage_.data(), storage_.data_, storage_.ptr());
        } break;
        case fnc_impl::RepresentationE::Big: {
            rtti_ = BigRTTI_T::get(_other.rtti_).copy_fnc_(_other.storage_.ptr(), storage_.data_, storage_.ptr());
        } break;
        default:
            break;
        }
    }

    template <typename Fnc>
    void doEmplace(Fnc _fun)
    {
        using FncT = std::decay_t<Fnc>;
        if constexpr (is_small_type<FncT>()) {
            rtti_      = representation(&fnc_impl::small_rtti<SmallSize, SmallAlign, FncT, R, ArgTypes...>, fnc_impl::RepresentationE::Small);
            auto& rval = reinterpret_cast<FncT&>(storage_.data_);
            std::construct_at(std::addressof(rval), std::move(_fun));
            // new (&rval) FncT(std::move(_fun));
        } else {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
            solid_throw("Big Function");
#endif
            FncT* const ptr = ::new FncT(std::move(_fun));
            storage_.ptr(ptr);
            rtti_ = representation(&fnc_impl::big_rtti<SmallSize, SmallAlign, FncT, R, ArgTypes...>, fnc_impl::RepresentationE::Big);
        }
    }
};

//-----------------------------------------------------------------------------
template <class T>
using Function64T = Function<T, function_size_to_small_size<64>()>;
template <class T>
using Function96T = Function<T, function_size_to_small_size<96>()>;
template <class T>
using Function128T = Function<T, function_size_to_small_size<128>()>;
template <class T>
using Function256T = Function<T, function_size_to_small_size<256>()>;

template <class T>
using SmallFunctionT = Function<T, function_default_size, function_default_align, StoreOption::RejectBig>;

template <class T>
using SmallFunction64T = Function<T, function_size_to_small_size<64>(), function_default_align, StoreOption::RejectBig>;
template <class T>
using SmallFunction96T = Function<T, function_size_to_small_size<96>(), function_default_align, StoreOption::RejectBig>;
template <class T>
using SmallFunction128T = Function<T, function_size_to_small_size<128>(), function_default_align, StoreOption::RejectBig>;
template <class T>
using SmallFunction256T = Function<T, function_size_to_small_size<256>(), function_default_align, StoreOption::RejectBig>;
//-----------------------------------------------------------------------------

} // namespace solid
