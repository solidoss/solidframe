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
#include <type_traits>
#define SOLID_THROW_ON_BIG_FUNCTION
#include <algorithm>
#include <cstddef>
#include <functional>
#include <typeindex>
#include <utility>

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/typetraits.hpp"

namespace solid {

#if 0 // TODO:remove
inline constexpr size_t function_default_data_size = 3 * sizeof(void*);

template <class T>
inline constexpr const T& function_max(const T& a, const T& b)
{
    return (a < b) ? b : a;
}
#endif

template <class, size_t DataSize = 1>
class Function; // undefined

template <class T>
struct is_function;

template <class R, class... ArgTypes, size_t DataSize>
struct is_function<Function<R(ArgTypes...), DataSize>> : std::true_type {
};

template <class T>
struct is_function : std::false_type {
};

namespace fnc_impl {
enum struct RepresentationE : uintptr_t {
    None = 0,
    Small,
    Big,
};
constexpr uintptr_t representation_mask                    = 3;
constexpr uintptr_t representation_and_flags_mask          = representation_mask;
constexpr uintptr_t reversed_representation_and_flags_mask = ~representation_mask;
constexpr uintptr_t representation_small = to_underlying(RepresentationE::Small);
constexpr uintptr_t representation_big = to_underlying(RepresentationE::Big);

template <class R, class... ArgTypes>
struct SmallRTTI;

template <class R, class... ArgTypes>
struct BigRTTI {
    using BigRTTIT   = BigRTTI<R, ArgTypes...>;
    using SmallRTTIT = SmallRTTI<R, ArgTypes...>;

    using DestroyFncT = void(void*);
    using CopyFncT    = RepresentationE(const void*, void*, const size_t, uintptr_t&, void*&, uintptr_t&);
    using MoveFncT    = RepresentationE(void*, void*, const size_t, uintptr_t&, void*&, uintptr_t&);
    using InvokeFncT  = R(const void*, ArgTypes&&...);

    template <class T>
    static void destroy(void* const _what) noexcept
    {
        ::delete static_cast<T*>(_what);
    }

    InvokeFncT&  invoke_fnc_;
    DestroyFncT& destroy_fnc_;
    CopyFncT&    copy_fnc_;
    MoveFncT&    move_fnc_;
    const bool   is_copyable_;
    const bool   is_movable_;
};

template <class R, class... ArgTypes>
struct SmallRTTI {
    using BigRTTIT    = BigRTTI<R, ArgTypes...>;
    using SmallRTTIT  = SmallRTTI<R, ArgTypes...>;
    using DestroyFncT = void(void*);
    using CopyFncT    = RepresentationE(const void*, void*, const size_t, uintptr_t&, void*&, uintptr_t&);
    using MoveFncT    = RepresentationE(void*, void*, const size_t, uintptr_t&, void*&, uintptr_t&);
    using InvokeFncT  = R(const void*, ArgTypes&&...);

    template <class T>
    static void destroy(void* const _what)
    {
        std::destroy_at(static_cast<T*>(_what));
    }

    InvokeFncT&  invoke_fnc_;
    DestroyFncT* pdestroy_fnc_;
    CopyFncT&    copy_fnc_;
    MoveFncT&    move_fnc_;
    const bool   is_copyable_;
    const bool   is_movable_;
};

template <class T, class R, class... ArgTypes>
R do_invoke(const void* _pvalue, ArgTypes&&... _args)
{
    return std::invoke(*const_cast<T*>(static_cast<const T*>(_pvalue)), static_cast<ArgTypes&&>(_args)... /* std::forward<ArgTypes>(_args)... */);
}

template <class T, class R, class... ArgTypes>
RepresentationE do_copy(
    const void* _pfrom,
    void* _pto_small, const size_t _small_cap, uintptr_t& _rpsmall_rtti,
    void*& _rpto_big, uintptr_t& _rpbig_rtti);

template <class T, class R, class... ArgTypes>
RepresentationE do_move(
    void* _pfrom,
    void* _pto_small, const size_t _small_cap, uintptr_t& _rpsmall_rtti,
    void*& _rpto_big, uintptr_t& _rpbig_rtti);

template <class T, class R, class... ArgTypes>
RepresentationE do_move_big(
    void* _pfrom,
    void* _pto_small, const size_t _small_cap, uintptr_t& _rpsmall_rtti,
    void*& _rpto_big, uintptr_t& _rpbig_rtti);

template <class T, class R, class... ArgTypes>
inline constexpr BigRTTI<R, ArgTypes...> big_rtti = {
    .invoke_fnc_  = do_invoke<T, R, ArgTypes&&...>,
    .destroy_fnc_ = &BigRTTI<R, ArgTypes...>::template destroy<T>,
    .copy_fnc_    = &do_copy<T, R, ArgTypes...>,
    .move_fnc_    = &do_move_big<T, R, ArgTypes...>,
    .is_copyable_  = std::is_copy_constructible_v<T>,
    .is_movable_   = std::is_move_constructible_v<T>};

template <class T, class R, class... ArgTypes>
inline constexpr SmallRTTI<R, ArgTypes...> small_rtti = {
    .invoke_fnc_  =  do_invoke<T, R, ArgTypes&&...>,
    .pdestroy_fnc_ = std::is_trivially_copyable_v<T> ? nullptr : &SmallRTTI<R, ArgTypes...>::template destroy<T>,
    .copy_fnc_    = do_copy<T, R, ArgTypes...>,
    .move_fnc_    = do_move<T, R, ArgTypes...>,
    .is_copyable_  = std::is_copy_constructible_v<T>,
    .is_movable_   = std::is_move_constructible_v<T>};

template <class T, class R, class... ArgTypes>
RepresentationE do_copy(
    const void* _pfrom,
    void* _pto_small, const size_t _small_cap, uintptr_t& _rpsmall_rtti,
    void*& _rpto_big, uintptr_t& _rpbig_rtti)
{
    if constexpr (alignof(T) <= alignof(max_align_t) && std::is_copy_constructible_v<T>) {
        if (sizeof(T) <= _small_cap) {
            T&       rdst = *static_cast<T*>(_pto_small);
            const T& rsrc = *static_cast<const T*>(_pfrom);
            ::new (const_cast<void*>(static_cast<const volatile void*>(std::addressof(rdst)))) T(rsrc);
            _rpsmall_rtti = reinterpret_cast<uintptr_t>(&small_rtti<T, R, ArgTypes...>);
            return RepresentationE::Small;
        } else {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
            solid_throw("Big Function");
#endif
            _rpto_big   = ::new T(*static_cast<const T*>(_pfrom));
            _rpbig_rtti = reinterpret_cast<uintptr_t>(&big_rtti<T, R, ArgTypes...>);
            return RepresentationE::Big;
        }
    } else if constexpr (std::is_trivially_constructible_v<T> || std::is_copy_constructible_v<T>) {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
        solid_throw("Big Function");
#endif
        _rpto_big   = ::new T(*static_cast<const T*>(_pfrom));
        _rpbig_rtti = reinterpret_cast<uintptr_t>(&big_rtti<T, R, ArgTypes...>);
        return RepresentationE::Big;
    } else {
        solid_throw("Function: contained value not copyable");
        return RepresentationE::None;
    }
}

template <class T, class R, class... ArgTypes>
RepresentationE do_move(
    void* _pfrom,
    void* _pto_small, const size_t _small_cap, uintptr_t& _rpsmall_rtti,
    void*& _rpto_big, uintptr_t& _rpbig_rtti)
{
    if constexpr (alignof(T) <= alignof(max_align_t) && std::is_move_constructible_v<T>) {
        if (sizeof(T) <= _small_cap) {
            T& rdst = *static_cast<T*>(_pto_small);
            T& rsrc = *static_cast<T*>(_pfrom);
            ::new (const_cast<void*>(static_cast<const volatile void*>(std::addressof(rdst)))) T{std::move(rsrc)};
            _rpsmall_rtti = reinterpret_cast<uintptr_t>(&small_rtti<T, R, ArgTypes...>);
            return RepresentationE::Small;
        } else {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
            solid_throw("Big Function");
#endif
            _rpto_big   = ::new T{std::move(*static_cast<T*>(_pfrom))};
            _rpbig_rtti = reinterpret_cast<uintptr_t>(&big_rtti<T, R, ArgTypes...>);
            return RepresentationE::Big;
        }
    } else if constexpr (std::is_move_constructible_v<T>) {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
        solid_throw("Big Function");
#endif
        _rpto_big   = ::new T{std::move(*static_cast<T*>(_pfrom))};
        _rpbig_rtti = reinterpret_cast<uintptr_t>(&big_rtti<T, R, ArgTypes...>);
        return RepresentationE::Big;
    } else {
        solid_throw("Function: contained value not movable");
        return RepresentationE::None;
    }
}

template <class T, class R, class... ArgTypes>
RepresentationE do_move_big(
    void* _pfrom,
    void* _pto_small, const size_t _small_cap, uintptr_t& _rpsmall_rtti,
    void*& _rpto_big, uintptr_t& _rpbig_rtti)
{
    if constexpr (alignof(T) <= alignof(max_align_t) && std::is_move_constructible_v<T>) {
        if (sizeof(T) <= _small_cap) {
            T& rdst = *static_cast<T*>(_pto_small);
            T& rsrc = *static_cast<T*>(_pfrom);
            ::new (const_cast<void*>(static_cast<const volatile void*>(std::addressof(rdst)))) T{std::move(rsrc)};
            _rpsmall_rtti = reinterpret_cast<uintptr_t>(&small_rtti<T, R, ArgTypes...>);
            return RepresentationE::Small;
        } else {
            _rpto_big   = static_cast<T*>(_pfrom); //::new T{ std::move(*static_cast<T*>(_pfrom)) };
            _rpbig_rtti = reinterpret_cast<uintptr_t>(&big_rtti<T, R, ArgTypes...>);
            return RepresentationE::Big;
        }
    } else {
        _rpto_big   = static_cast<T*>(_pfrom); //::new T{ std::move(*static_cast<T*>(_pfrom)) };
        _rpbig_rtti = reinterpret_cast<uintptr_t>(&big_rtti<T, R, ArgTypes...>);
        return RepresentationE::Big;
    }
}

constexpr size_t compute_small_capacity(const size_t _req_capacity)
{
    constexpr size_t default_total_size = 32u;

    const size_t end_capacity = sizeof(void*);
    const size_t req_capacity = std::max(_req_capacity, std::max(end_capacity, sizeof(max_align_t)) - end_capacity);
    const size_t tot_capacity = std::max(default_total_size, padded_size(req_capacity + sizeof(void*), alignof(max_align_t)));

    return tot_capacity - end_capacity;
}
} // namespace fnc_impl

template <class R, class... ArgTypes, size_t DataSize>
class Function<R(ArgTypes...), DataSize> {
    static constexpr size_t small_capacity = fnc_impl::compute_small_capacity(DataSize);
    static constexpr size_t big_padding    = small_capacity - sizeof(void*);

    struct Small {
        using RTTI_T = fnc_impl::SmallRTTI<R, ArgTypes...>;
        uintptr_t     prtti_ = 0;
        unsigned char data_[small_capacity];

        [[nodiscard]] auto* rtti() const noexcept
        {
            return reinterpret_cast<const RTTI_T*>(prtti_ & fnc_impl::reversed_representation_and_flags_mask);
        }

        void rtti(uintptr_t const _ptr)
        {
            prtti_ = _ptr | fnc_impl::representation_small;
        }
    };

    struct Big {
        using RTTI_T = fnc_impl::BigRTTI<R, ArgTypes...>;
        uintptr_t     prtti_;
        void*         ptr_;
        unsigned char padding_[big_padding];
        

        [[nodiscard]] auto* rtti() const noexcept
        {
            return reinterpret_cast<const RTTI_T*>(prtti_ & fnc_impl::reversed_representation_and_flags_mask);
        }

        void rtti(uintptr_t const _ptr)
        {
            prtti_ = _ptr | fnc_impl::representation_big;
        }
    };

    struct Storage {
        union {
            Small small_;
            Big   big_;
        };
    };

    union {
        Storage          storage_{};
        std::max_align_t dummy_;
    };
    template <class F, size_t S>
    friend class Function;

private:
    fnc_impl::RepresentationE representation() const noexcept
    {
        auto const retval = static_cast<fnc_impl::RepresentationE>(storage_.small_.prtti_ & fnc_impl::representation_and_flags_mask);
        assert(retval == static_cast<fnc_impl::RepresentationE>(storage_.big_.prtti_ & fnc_impl::representation_and_flags_mask));
        return retval;
    }

    void representation(const fnc_impl::RepresentationE _repr) noexcept
    {
        // storage_.small_.prtti_ &= (~fnc_impl::representation_and_flags_mask);
        // storage_.small_.prtti_ |= static_cast<uintptr_t>(_repr);
        storage_.small_.prtti_ = (storage_.small_.prtti_ & (~fnc_impl::representation_and_flags_mask)) | to_underlying(_repr);
        assert(_repr == static_cast<fnc_impl::RepresentationE>(storage_.big_.prtti_ & fnc_impl::representation_and_flags_mask));
    }

public:
    using ThisT = Function<R(ArgTypes...), DataSize>;

    template <class T>
    static constexpr bool is_small_type()
    {
        return alignof(T) <= alignof(max_align_t) && sizeof(T) <= small_capacity;
    }

    static constexpr size_t smallCapacity()
    {
        return small_capacity;
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
#if 1
    template <class T, std::enable_if_t<std::conjunction_v<std::negation<is_function<std::decay_t<T>>>, std::negation<is_specialization<std::decay_t<T>, std::in_place_type_t>> /*,
        std::is_copy_constructible<std::decay_t<T>>*/
                                            >,
                           int>
        = 0>
    Function(const T& _fun)
    {
        using FncT = std::remove_cvref_t<std::decay_t<T>>;
        if constexpr (is_small_type<FncT>()) {
            storage_.small_.rtti(reinterpret_cast<uintptr_t>(&fnc_impl::small_rtti<FncT, R, ArgTypes...>));
            auto& rval = reinterpret_cast<FncT&>(storage_.small_.data_);
            std::construct_at(std::addressof(rval), _fun);
        } else {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
            solid_throw("Big Function");
#endif
            FncT* const ptr    = ::new FncT(_fun);
            storage_.big_.ptr_ = ptr;
            storage_.big_.rtti(reinterpret_cast<uintptr_t>(&fnc_impl::big_rtti<FncT, R, ArgTypes...>));
        }
    }
#endif
    template <class T, std::enable_if_t<std::conjunction_v<std::negation<is_function<std::decay_t<T>>>, std::negation<is_specialization<std::decay_t<T>, std::in_place_type_t>> /*,
        std::is_copy_constructible<std::decay_t<T>>*/
                                            >,
                           int>
        = 0>
    Function(T&& _fun)
    {
        using FncT = std::remove_cvref_t<T>;
        if constexpr (is_small_type<FncT>()) {
            storage_.small_.rtti(reinterpret_cast<uintptr_t>(&fnc_impl::small_rtti<FncT, R, ArgTypes...>));
            auto& rval = reinterpret_cast<FncT&>(storage_.small_.data_);
            std::construct_at(std::addressof(rval), std::move(_fun));
            //new (&rval) FncT(std::move(_fun));
        } else {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
            solid_throw("Big Function");
#endif
            FncT* const ptr    = ::new FncT(std::move(_fun));
            storage_.big_.ptr_ = ptr;
            storage_.big_.rtti(reinterpret_cast<uintptr_t>(&fnc_impl::big_rtti<FncT, R, ArgTypes...>));
        }
    }

    ~Function() noexcept
    {
        switch (representation()) {
        [[likely]] case fnc_impl::RepresentationE::Small:
            if (auto const* prtti = storage_.small_.rtti(); prtti->pdestroy_fnc_) {
                prtti->pdestroy_fnc_(&storage_.small_.data_);
            }
            break;
        case fnc_impl::RepresentationE::Big:
            storage_.big_.rtti()->destroy_fnc_(storage_.big_.ptr_);
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
        switch (representation()) {
        [[likely]] case fnc_impl::RepresentationE::Small:
            if (auto const* prtti = storage_.small_.rtti(); prtti->pdestroy_fnc_) {
                prtti->pdestroy_fnc_(&storage_.small_.data_);
            }
            break;
        case fnc_impl::RepresentationE::Big:
            storage_.big_.rtti()->destroy_fnc_(storage_.big_.ptr_);
            break;
        case fnc_impl::RepresentationE::None:
            [[fallthrough]];
        default:
            break;
        }
        storage_.small_.prtti_ = 0;
    }

    R operator()(ArgTypes... _args) const
    {
        if (has_value()) {
            if (is_small()) [[likely]] {
                return storage_.small_.rtti()->invoke_fnc_(&storage_.small_.data_, static_cast<ArgTypes&&>(_args)... /* std::forward<ArgTypes>(_args)... */);
            } else {
                return storage_.big_.rtti()->invoke_fnc_(storage_.big_.ptr_, static_cast<ArgTypes&&>(_args)... /* std::forward<ArgTypes>(_args)... */);
            }
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
        return storage_.small_.prtti_ != 0;
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
        if (is_small()) {
            return storage_.small_.rtti()->is_movable_;
        } else if (is_big()) {
            return storage_.big_.rtti()->is_copyable_;
        }
        return true;
    }
    bool is_copyable() const
    {
        if (is_small()) {
            return storage_.small_.rtti()->is_copyable_;
        } else if (is_big()) {
            return storage_.big_.rtti()->is_copyable_;
        }
        return true;
    }

    [[nodiscard]] bool is_small() const
    {
        return representation() == fnc_impl::RepresentationE::Small;
    }

    [[nodiscard]] bool is_big() const
    {
        return representation() == fnc_impl::RepresentationE::Big;
    }

private:
    template <size_t Sz>
    void doMoveFrom(Function<R(ArgTypes...), Sz>& _other)
    {
        representation(fnc_impl::RepresentationE::None);
        switch (_other.representation()) {
        case fnc_impl::RepresentationE::Small: {
            const auto repr = _other.storage_.small_.rtti()->move_fnc_(
                _other.storage_.small_.data_,
                storage_.small_.data_, small_capacity, storage_.small_.prtti_,
                storage_.big_.ptr_, storage_.big_.prtti_);
            representation(repr);
        } break;
        case fnc_impl::RepresentationE::Big: {
            const auto repr = _other.storage_.big_.rtti()->move_fnc_(
                _other.storage_.big_.ptr_,
                storage_.small_.data_, small_capacity, storage_.small_.prtti_,
                storage_.big_.ptr_, storage_.big_.prtti_);
            representation(repr);
        } break;
        default:
            break;
        }
    }

    template <size_t Sz>
    void doCopyFrom(const Function<R(ArgTypes...), Sz>& _other)
    {
        representation(fnc_impl::RepresentationE::None);
        switch (_other.representation()) {
        case fnc_impl::RepresentationE::Small: {
            const auto repr = _other.storage_.small_.rtti()->copy_fnc_(
                _other.storage_.small_.data_,
                storage_.small_.data_, small_capacity, storage_.small_.prtti_,
                storage_.big_.ptr_, storage_.big_.prtti_);
            representation(repr);
        } break;
        case fnc_impl::RepresentationE::Big: {
            const auto repr = _other.storage_.big_.rtti()->copy_fnc_(
                _other.storage_.big_.ptr_,
                storage_.small_.data_, small_capacity, storage_.small_.prtti_,
                storage_.big_.ptr_, storage_.big_.prtti_);
            representation(repr);
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
            storage_.small_.rtti(reinterpret_cast<uintptr_t>(&fnc_impl::small_rtti<FncT, R, ArgTypes...>));
            auto& rval = reinterpret_cast<FncT&>(storage_.small_.data_);
            //std::construct_at(std::addressof(rval), std::move(_fun));
            new (&rval) FncT(std::move(_fun));
        } else {
#if defined(SOLID_THROW_ON_BIG_FUNCTION)
            solid_throw("Big Function");
#endif
            FncT* const ptr    = ::new FncT(std::move(_fun));
            storage_.big_.ptr_ = ptr;
            storage_.big_.rtti(reinterpret_cast<uintptr_t>(&fnc_impl::big_rtti<FncT, R, ArgTypes...>));
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
