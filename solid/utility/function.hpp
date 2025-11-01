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
#include <cstdint>
#include <memory>
#include <solid/utility/any.hpp>
#include <type_traits>
#define SOLID_THROW_ON_BIG_FUNCTION
#include <algorithm>
#include <cstddef>
#include <functional>
#include <typeindex>
#include <utility>

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/anyimpl.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/typetraits.hpp"

namespace solid {

constexpr size_t function_default_size  = 32 - sizeof(std::uintptr_t);
constexpr size_t function_default_align = alignof(std::uintptr_t);

template <class, size_t SmallSize = function_default_size, size_t SmallAlign = function_default_align>
class Function; // undefined

template <class T>
struct is_function;

template <class R, class... ArgTypes, size_t SmallSize, size_t SmallAlign>
struct is_function<Function<R(ArgTypes...), SmallSize, SmallAlign>> : std::true_type {
};

template <class T>
struct is_function : std::false_type {
};

namespace fnc_impl {

using any_impl::representation;
using any_impl::RepresentationE;
using any_impl::reversed_representation_and_flags_mask;

template <class R, class... ArgTypes>
struct BaseRTTI {
    using CopyFncT   = uintptr_t(const void*, void*, size_t, size_t, void*&);
    using MoveFncT   = uintptr_t(void*, void*, size_t, size_t, void*&);
    using InvokeFncT = R(void*, ArgTypes&&...);

    InvokeFncT& invoke_fnc_;
    CopyFncT&   copy_fnc_;
    MoveFncT&   move_fnc_;
    const bool  is_copyable_;
    const bool  is_movable_;

    static BaseRTTI const& get(uintptr_t const _rtti) noexcept
    {
        return *reinterpret_cast<const BaseRTTI*>(_rtti & reversed_representation_and_flags_mask);
    }
};

template <class R, class... ArgTypes>
struct BigRTTI : BaseRTTI<R, ArgTypes...> {
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

template <class R, class... ArgTypes>
struct SmallRTTI : BaseRTTI<R, ArgTypes...> {
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

template <class T, class R, class... ArgTypes>
R do_invoke(void* _pvalue, ArgTypes&&... _args)
{
    T* pfun = reinterpret_cast<T*>(_pvalue);
    return std::invoke(*pfun, static_cast<ArgTypes&&>(_args)... /* std::forward<ArgTypes>(_args)... */);
}

template <class T, class R, class... ArgTypes>
uintptr_t do_copy(
    const void*,
    void*, size_t, size_t,
    void*&);

template <class T, class R, class... ArgTypes>
uintptr_t do_move(
    void*,
    void*, size_t, size_t,
    void*&);

template <class T, class R, class... ArgTypes>
uintptr_t do_move_big(
    void*,
    void*, size_t, size_t,
    void*&);

template <class T, class R, class... ArgTypes>
inline constexpr BigRTTI<R, ArgTypes...> big_rtti = {
    {do_invoke<T, R, ArgTypes&&...>,
        do_copy<T, R, ArgTypes...>,
        do_move_big<T, R, ArgTypes...>,
        std::is_copy_constructible_v<T>,
        std::is_move_constructible_v<T>},
    BigRTTI<R, ArgTypes...>::template destroy<T>,
};

template <class T, class R, class... ArgTypes>
inline constexpr SmallRTTI<R, ArgTypes...> small_rtti = {
    {do_invoke<T, R, ArgTypes&&...>,
        do_copy<T, R, ArgTypes...>,
        do_move<T, R, ArgTypes...>,
        std::is_copy_constructible_v<T>,
        std::is_move_constructible_v<T>},
    std::is_trivially_copyable_v<T> ? nullptr : &SmallRTTI<R, ArgTypes...>::template destroy<T>,
};

template <class T, class R, class... ArgTypes>
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
            return representation(&small_rtti<T, R, ArgTypes...>, RepresentationE::Small);
        } else {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
            solid_throw("Big Function");
#endif
            _rpto_big = ::new T(*static_cast<const T*>(_pfrom));
            return representation(&big_rtti<T, R, ArgTypes...>, RepresentationE::Big);
        }
    } else {
        solid_throw("Function: contained value not copyable");
        return 0;
    }
}

template <class T, class R, class... ArgTypes>
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
            return representation(&small_rtti<T, R, ArgTypes...>, RepresentationE::Small);
        } else {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
            solid_throw("Big Function");
#endif
            _rpto_big = ::new T{std::move(*static_cast<T*>(_pfrom))};
            return representation(&big_rtti<T, R, ArgTypes...>, RepresentationE::Big);
        }
    } else {
        solid_throw("Function: contained value not movable");
        return 0u;
    }
}

template <class T, class R, class... ArgTypes>
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
            return representation(&small_rtti<T, R, ArgTypes...>, RepresentationE::Small);
        } else {
            _rpto_big = static_cast<T*>(_pfrom);
            return representation(&big_rtti<T, R, ArgTypes...>, RepresentationE::Big);
        }
    } else {
        _rpto_big = static_cast<T*>(_pfrom);
        return representation(&big_rtti<T, R, ArgTypes...>, RepresentationE::Big);
    }
}

} // namespace fnc_impl

template <class R, class... ArgTypes, size_t SmallSize, size_t SmallAlign>
class Function<R(ArgTypes...), SmallSize, SmallAlign> {
    using BaseRTTI_T = fnc_impl::BaseRTTI<R, ArgTypes...>;

    struct Small {
        using RTTI_T = fnc_impl::SmallRTTI<R, ArgTypes...>;
        alignas(SmallAlign) mutable unsigned char data_[SmallSize];
    };

    struct Big {
        using RTTI_T = fnc_impl::BigRTTI<R, ArgTypes...>;
        mutable void* ptr_;
    };

    struct Storage {
        uintptr_t rtti_ = 0;
        union {
            Small small_;
            Big   big_;
        };
    };

    Storage storage_{};

    template <class F, size_t S, size_t A>
    friend class Function;

public:
    using ThisT = Function<R(ArgTypes...), SmallSize, SmallAlign>;

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

    Function() noexcept = default;

    Function(std::nullptr_t) noexcept {}

    Function(const ThisT& _other)
    {
        doCopyFrom(_other);
    }

    template <size_t Sz>
    Function(const Function<R(ArgTypes...), Sz>& _other)
    {
        doCopyFrom(_other);
    }

    Function(ThisT&& _other) noexcept
    {
        doMoveFrom(_other);
    }

    template <size_t Sz>
    Function(Function<R(ArgTypes...), Sz>&& _other) noexcept
    {
        doMoveFrom(_other);
    }

    template <class T, std::enable_if_t<std::conjunction_v<std::negation<is_function<std::decay_t<T>>>, std::negation<is_specialization<std::decay_t<T>, std::in_place_type_t>> /*,
        std::is_copy_constructible<std::decay_t<T>>*/
                                            >,
                           int>
        = 0>
    Function(const T& _fun)
    {
        using FncT = std::remove_cvref_t<std::decay_t<T>>;
        if constexpr (is_small_type<FncT>()) {
            storage_.rtti_ = fnc_impl::representation(&fnc_impl::small_rtti<FncT, R, ArgTypes...>, fnc_impl::RepresentationE::Small);
            auto& rval     = reinterpret_cast<FncT&>(storage_.small_.data_);
            std::construct_at(std::addressof(rval), _fun);
        } else {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
            solid_throw("Big Function");
#endif
            FncT* const ptr    = ::new FncT(_fun);
            storage_.big_.ptr_ = ptr;
            storage_.rtti_     = fnc_impl::representation(&fnc_impl::big_rtti<FncT, R, ArgTypes...>, fnc_impl::RepresentationE::Big);
        }
    }
    template <class T, std::enable_if_t<std::conjunction_v<std::negation<is_function<std::decay_t<T>>>, std::negation<is_specialization<std::decay_t<T>, std::in_place_type_t>> /*,
        std::is_copy_constructible<std::decay_t<T>>*/
                                            >,
                           int>
        = 0>
    Function(T&& _fun)
    {
        using FncT = std::remove_cvref_t<T>;
        if constexpr (is_small_type<FncT>()) {
            storage_.rtti_ = fnc_impl::representation(&fnc_impl::small_rtti<FncT, R, ArgTypes...>, fnc_impl::RepresentationE::Small);
            auto& rval     = reinterpret_cast<FncT&>(storage_.small_.data_);
            std::construct_at(std::addressof(rval), std::move(_fun));
        } else {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
            solid_throw("Big Function");
#endif
            FncT* const ptr    = ::new FncT(std::move(_fun));
            storage_.big_.ptr_ = ptr;
            storage_.rtti_     = fnc_impl::representation(&fnc_impl::big_rtti<FncT, R, ArgTypes...>, fnc_impl::RepresentationE::Big);
        }
    }

    ~Function() noexcept
    {
        auto const rtti = storage_.rtti_;
        switch (fnc_impl::representation(rtti)) {
        [[likely]] case fnc_impl::RepresentationE::Small:
            if (auto* pf = Small::RTTI_T::get(rtti).pdestroy_fnc_) {
                (*pf)(&storage_.small_.data_);
            }
            break;
        case fnc_impl::RepresentationE::Big:
            Big::RTTI_T::get(rtti).destroy_fnc_(storage_.big_.ptr_);
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

    template <size_t Sz>
    ThisT& operator=(const Function<R(ArgTypes...), Sz>& _other)
    {
        *this = ThisT{_other};
        return *this;
    }

    template <size_t Sz>
    ThisT& operator=(Function<R(ArgTypes...), Sz>&& _other) noexcept
    {
        reset();
        doMoveFrom(_other);
        return *this;
    }

    template <class T, std::enable_if_t<std::conjunction_v<std::negation<is_function<std::decay_t<T>>>, std::is_copy_constructible<std::decay_t<T>>>, int> = 0>
    ThisT& operator=(T&& _rvalue)
    {
        *this = ThisT{std::forward<T>(_rvalue)};
        return *this;
    }

    void reset() noexcept
    {
        auto const rtti = storage_.rtti_;
        switch (fnc_impl::representation(rtti)) {
        [[likely]] case fnc_impl::RepresentationE::Small:
            if (auto* pfnc = Small::RTTI_T::get(rtti).pdestroy_fnc_) {
                (*pfnc)(&storage_.small_.data_);
            }
            break;
        case fnc_impl::RepresentationE::Big:
            Big::RTTI_T::get(rtti).destroy_fnc_(storage_.big_.ptr_);
            break;
        case fnc_impl::RepresentationE::None:
            [[fallthrough]];
        default:
            break;
        }
        storage_.rtti_ = 0;
    }

    R operator()(ArgTypes... _args) const
    {
        auto const rtti = storage_.rtti_;
        if (auto const repr = fnc_impl::representation(rtti); repr == fnc_impl::RepresentationE::Small) [[likely]] {
            return Small::RTTI_T::get(rtti).invoke_fnc_(&storage_.small_.data_, static_cast<ArgTypes&&>(_args)...);
        } else if (repr == fnc_impl::RepresentationE::Big) {
            return Big::RTTI_T::get(rtti).invoke_fnc_(&storage_.big_.ptr_, static_cast<ArgTypes&&>(_args)...);
        } else {
            throw std::bad_function_call();
        }
    }

    template <size_t Sz>
    void swap(Function<R(ArgTypes...), Sz>& _other) noexcept
    {
        _other = std::exchange(*this, std::move(_other));
    }

    bool has_value() const noexcept
    {
        return storage_.rtti_ != 0;
    }
    bool empty() const noexcept
    {
        return !has_value();
    }

    explicit operator bool() const noexcept
    {
        return has_value();
    }

    bool is_movable() const
    {
        auto const rtti = storage_.rtti_;
        if (rtti) [[likely]] {
            return BaseRTTI_T::get(rtti).is_movable_;
        }
        return true;
    }
    bool is_copyable() const
    {
        auto const rtti = storage_.rtti_;
        if (rtti) [[likely]] {
            return BaseRTTI_T::get(rtti).is_copyable_;
        }
        return true;
    }

    [[nodiscard]] bool is_small() const
    {
        return fnc_impl::representation(storage_.rtti_) == fnc_impl::RepresentationE::Small;
    }

    [[nodiscard]] bool is_big() const
    {
        return fnc_impl::representation(storage_.rtti_) == fnc_impl::RepresentationE::Big;
    }

private:
    template <size_t Sz>
    void doMoveFrom(Function<R(ArgTypes...), Sz>& _other)
    {
        storage_.rtti_ = 0u;
        switch (fnc_impl::representation(_other.storage_.rtti_)) {
        case fnc_impl::RepresentationE::Small: {
            storage_.rtti_ = Small::RTTI_T::get(_other.storage_.rtti_).move_fnc_(_other.storage_.small_.data_, storage_.small_.data_, smallCapacity(), smallAlign(), storage_.big_.ptr_);
            _other.reset();
        } break;
        case fnc_impl::RepresentationE::Big: {
            storage_.rtti_ = Big::RTTI_T::get(_other.storage_.rtti_).move_fnc_(_other.storage_.big_.ptr_, storage_.small_.data_, smallCapacity(), smallAlign(), storage_.big_.ptr_);
            if (is_big()) {
                _other.storage_.rtti_ = 0u;
            }
            _other.reset();
        } break;
        default:
            break;
        }
    }

    template <size_t Sz>
    void doCopyFrom(const Function<R(ArgTypes...), Sz>& _other)
    {
        storage_.rtti_ = 0u;
        switch (fnc_impl::representation(_other.storage_.rtti_)) {
        case fnc_impl::RepresentationE::Small: {
            storage_.rtti_ = Small::RTTI_T::get(_other.storage_.rtti_).copy_fnc_(_other.storage_.small_.data_, storage_.small_.data_, smallCapacity(), smallAlign(), storage_.big_.ptr_);
        } break;
        case fnc_impl::RepresentationE::Big: {
            storage_.rtti_ = Big::RTTI_T::get(_other.storage_.rtti_).copy_fnc_(_other.storage_.big_.ptr_, storage_.small_.data_, smallCapacity(), smallAlign(), storage_.big_.ptr_);
        } break;
        default:
            break;
        }
    }

    template <typename Fnc>
    void doEmplace(Fnc _fun)
    {
        using FncT = std::remove_cvref_t<Fnc>;
        if constexpr (is_small_type<FncT>()) {
            storage_.rtti_ = representation(&fnc_impl::small_rtti<FncT, R, ArgTypes...>, fnc_impl::RepresentationE::Small);
            auto& rval     = reinterpret_cast<FncT&>(storage_.small_.data_);
            std::construct_at(std::addressof(rval), std::move(_fun));
            // new (&rval) FncT(std::move(_fun));
        } else {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
            solid_throw("Big Function");
#endif
            FncT* const ptr    = ::new FncT(std::move(_fun));
            storage_.big_.ptr_ = ptr;
            storage_.rtti_     = representation(&fnc_impl::big_rtti<FncT, R, ArgTypes...>, fnc_impl::RepresentationE::Big);
        }
    }
};

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

} // namespace solid

#ifdef SOLID_USE_STD_FUNCTION

#define solid_function_t(...) std::function<__VA_ARGS__>

#else

#define solid_function_t(...) solid::Function<__VA_ARGS__>

#endif

#define solid_function_empty(f) (!f)
#define solid_function_clear(f) (f = nullptr)
