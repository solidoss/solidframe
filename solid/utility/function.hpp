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

#include <algorithm>
#include <cstddef>
#include <functional>
#include <typeindex>
#include <typeinfo>
#include <utility>

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/typetraits.hpp"

namespace solid {
inline constexpr size_t FunctionDefaultSize = 3 * sizeof(void*);

inline constexpr size_t function_size_from_sizeof(const size_t _sizeof)
{
    return _sizeof - sizeof(void*);
}
template <class T>
inline constexpr const T& function_max(const T& a, const T& b)
{
    return (a < b) ? b : a;
}
template <class, size_t DataSize = FunctionDefaultSize>
class Function; //undefined

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
constexpr uintptr_t representation_mask           = 3;
constexpr uintptr_t representation_and_flags_mask = representation_mask /* + (3 << 2)*/;

template <class R, class... ArgTypes>
struct SmallRTTI;

template <class R, class... ArgTypes>
struct BigRTTI {
    using BigRTTIT   = BigRTTI<R, ArgTypes...>;
    using SmallRTTIT = SmallRTTI<R, ArgTypes...>;

    using DestroyFncT = void(void*);
    using CopyFncT    = RepresentationE(const void*, void*, const size_t, const SmallRTTIT*&, void*&, const BigRTTIT*&);
    using MoveFncT    = RepresentationE(void*, void*, const size_t, const SmallRTTIT*&, void*&, const BigRTTIT*&);
    using InvokeFncT  = R(const void*, ArgTypes&&...);

    template <class T>
    static void destroy(void* const _what) noexcept
    {
        ::delete static_cast<T*>(_what);
    }

    InvokeFncT*  pinvoke_fnc_;
    DestroyFncT* pdestroy_fnc_;
    CopyFncT*    pcopy_fnc_;
    MoveFncT*    pmove_fnc_;
    const bool   is_copyable_;
    const bool   is_movable_;
};

template <class R, class... ArgTypes>
struct SmallRTTI {
    using BigRTTIT    = BigRTTI<R, ArgTypes...>;
    using SmallRTTIT  = SmallRTTI<R, ArgTypes...>;
    using DestroyFncT = void(void*) noexcept;
    using CopyFncT    = RepresentationE(const void*, void*, const size_t, const SmallRTTIT*&, void*&, const BigRTTIT*&);
    using MoveFncT    = RepresentationE(void*, void*, const size_t, const SmallRTTIT*&, void*&, const BigRTTIT*&);
    using InvokeFncT  = R(const void*, ArgTypes&&...);

    template <class T>
    static void destroy(void* const _what) noexcept
    {
        static_cast<T*>(_what)->~T();
    }

    InvokeFncT*  pinvoke_fnc_;
    DestroyFncT* pdestroy_fnc_;
    CopyFncT*    pcopy_fnc_;
    MoveFncT*    pmove_fnc_;
    const bool   is_copyable_;
    const bool   is_movable_;
};

template <class T, class R, class... ArgTypes>
R do_invoke(const void* _pvalue, ArgTypes&&... _args)
{
    return std::invoke(*const_cast<T*>(static_cast<const T*>(_pvalue)), std::forward<ArgTypes>(_args)...);
}

template <class T, class R, class... ArgTypes>
RepresentationE do_copy(
    const void* _pfrom,
    void* _pto_small, const size_t _small_cap, const SmallRTTI<R, ArgTypes...>*& _rpsmall_rtti,
    void*& _rpto_big, const BigRTTI<R, ArgTypes...>*& _rpbig_rtti);

template <class T, class R, class... ArgTypes>
RepresentationE do_move(
    void* _pfrom,
    void* _pto_small, const size_t _small_cap, const SmallRTTI<R, ArgTypes...>*& _rpsmall_rtti,
    void*& _rpto_big, const BigRTTI<R, ArgTypes...>*& _rpbig_rtti);

template <class T, class R, class... ArgTypes>
RepresentationE do_move_big(
    void* _pfrom,
    void* _pto_small, const size_t _small_cap, const SmallRTTI<R, ArgTypes...>*& _rpsmall_rtti,
    void*& _rpto_big, const BigRTTI<R, ArgTypes...>*& _rpbig_rtti);

template <class T, class R, class... ArgTypes>
inline constexpr BigRTTI<R, ArgTypes...> big_rtti = {
    &do_invoke<T, R, ArgTypes&&...>,
    &BigRTTI<R, ArgTypes...>::template destroy<T>, &do_copy<T, R, ArgTypes...>, &do_move_big<T, R, ArgTypes...>,
    std::is_copy_constructible_v<T>, std::is_move_constructible_v<T>};

template <class T, class R, class... ArgTypes>
inline constexpr SmallRTTI<R, ArgTypes...> small_rtti = {
    &do_invoke<T, R, ArgTypes&&...>,
    &SmallRTTI<R, ArgTypes...>::template destroy<T>, &do_copy<T, R, ArgTypes...>, &do_move<T, R, ArgTypes...>,
    std::is_copy_constructible_v<T>, std::is_move_constructible_v<T>};

template <class T, class R, class... ArgTypes>
RepresentationE do_copy(
    const void* _pfrom,
    void* _pto_small, const size_t _small_cap, const SmallRTTI<R, ArgTypes...>*& _rpsmall_rtti,
    void*& _rpto_big, const BigRTTI<R, ArgTypes...>*& _rpbig_rtti)
{
    if constexpr (alignof(T) <= alignof(max_align_t) && std::is_copy_constructible_v<T>) {
        if (sizeof(T) <= _small_cap) {
            T&       rdst = *static_cast<T*>(_pto_small);
            const T& rsrc = *static_cast<const T*>(_pfrom);
            ::new (const_cast<void*>(static_cast<const volatile void*>(std::addressof(rdst)))) T(rsrc);
            _rpsmall_rtti = &small_rtti<T, R, ArgTypes...>;
            return RepresentationE::Small;
        } else {
            _rpto_big   = ::new T(*static_cast<const T*>(_pfrom));
            _rpbig_rtti = &big_rtti<T, R, ArgTypes...>;
            return RepresentationE::Big;
        }
    } else if constexpr (std::is_trivially_constructible_v<T> || std::is_copy_constructible_v<T>) {
        _rpto_big   = ::new T(*static_cast<const T*>(_pfrom));
        _rpbig_rtti = &big_rtti<T, R, ArgTypes...>;
        return RepresentationE::Big;
    } else {
        solid_throw("Any: contained value not copyable");
        return RepresentationE::None;
    }
}

template <class T, class R, class... ArgTypes>
RepresentationE do_move(
    void* _pfrom,
    void* _pto_small, const size_t _small_cap, const SmallRTTI<R, ArgTypes...>*& _rpsmall_rtti,
    void*& _rpto_big, const BigRTTI<R, ArgTypes...>*& _rpbig_rtti)
{
    if constexpr (alignof(T) <= alignof(max_align_t) && std::is_move_constructible_v<T>) {
        if (sizeof(T) <= _small_cap) {
            T& rdst = *static_cast<T*>(_pto_small);
            T& rsrc = *static_cast<T*>(_pfrom);
            ::new (const_cast<void*>(static_cast<const volatile void*>(std::addressof(rdst)))) T{std::move(rsrc)};
            _rpsmall_rtti = &small_rtti<T, R, ArgTypes...>;
            return RepresentationE::Small;
        } else {
            _rpto_big   = ::new T{std::move(*static_cast<T*>(_pfrom))};
            _rpbig_rtti = &big_rtti<T, R, ArgTypes...>;
            return RepresentationE::Big;
        }
    } else if constexpr (std::is_move_constructible_v<T>) {
        _rpto_big   = ::new T{std::move(*static_cast<T*>(_pfrom))};
        _rpbig_rtti = &big_rtti<T, R, ArgTypes...>;
        return RepresentationE::Big;
    } else {
        solid_throw("Function: contained value not movable");
        return RepresentationE::None;
    }
}

template <class T, class R, class... ArgTypes>
RepresentationE do_move_big(
    void* _pfrom,
    void* _pto_small, const size_t _small_cap, const SmallRTTI<R, ArgTypes...>*& _rpsmall_rtti,
    void*& _rpto_big, const BigRTTI<R, ArgTypes...>*& _rpbig_rtti)
{
    if constexpr (alignof(T) <= alignof(max_align_t) && std::is_move_constructible_v<T>) {
        if (sizeof(T) <= _small_cap) {
            T& rdst = *static_cast<T*>(_pto_small);
            T& rsrc = *static_cast<T*>(_pfrom);
            ::new (const_cast<void*>(static_cast<const volatile void*>(std::addressof(rdst)))) T{std::move(rsrc)};
            _rpsmall_rtti = &small_rtti<T, R, ArgTypes...>;
            return RepresentationE::Small;
        } else {
            _rpto_big   = static_cast<T*>(_pfrom); //::new T{ std::move(*static_cast<T*>(_pfrom)) };
            _rpbig_rtti = &big_rtti<T, R, ArgTypes...>;
            return RepresentationE::Big;
        }
    } else {
        _rpto_big   = static_cast<T*>(_pfrom); //::new T{ std::move(*static_cast<T*>(_pfrom)) };
        _rpbig_rtti = &big_rtti<T, R, ArgTypes...>;
        return RepresentationE::Big;
    }
}
} // namespace fnc_impl

template <class R, class... ArgTypes, size_t DataSize>
class Function<R(ArgTypes...), DataSize> {
    static constexpr size_t min_capacity   = sizeof(void*) * 3;
    static constexpr size_t small_capacity = function_max(min_capacity, padded_size(DataSize, sizeof(void*))) - sizeof(void*);
    static constexpr size_t big_padding    = small_capacity - sizeof(void*);

    struct Small {
        unsigned char                              data_[small_capacity];
        const fnc_impl::SmallRTTI<R, ArgTypes...>* prtti_;
    };

    struct Big {
        unsigned char                            padding_[big_padding];
        void*                                    ptr_;
        const fnc_impl::BigRTTI<R, ArgTypes...>* prtti_;
    };

    struct Storage {
        union {
            Small small_;
            Big   big_;
        };
        uintptr_t type_data_;
    };
    //static_assert(sizeof(_Storage_t) == _Any_trivial_space_size + sizeof(void*));
    //static_assert(is_standard_layout_v<_Storage_t>);

    union {
        Storage     storage_{};
        max_align_t dummy_;
    };
    template <class F, size_t S>
    friend class Function;

private:
    fnc_impl::RepresentationE representation() const noexcept
    {
        return static_cast<fnc_impl::RepresentationE>(storage_.type_data_ & fnc_impl::representation_and_flags_mask);
    }

    void representation(const fnc_impl::RepresentationE _repr) noexcept
    {
        storage_.type_data_ &= (~fnc_impl::representation_and_flags_mask);
        storage_.type_data_ |= static_cast<uintptr_t>(_repr);
    }

    const std::type_info* typeInfo() const noexcept
    {
        return reinterpret_cast<const std::type_info*>(storage_.type_data_ & ~fnc_impl::representation_and_flags_mask);
    }

    template <class T>
    static constexpr bool is_small_type()
    {
        return alignof(T) <= alignof(max_align_t) && sizeof(T) <= small_capacity;
    }

public:
    using ThisT = Function<R(ArgTypes...), DataSize>;

    Function() noexcept {}

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
                           int> = 0>
    Function(const T& _value)
    {
        doEmplace<std::decay_t<T>>(std::move(_value));
    }

    template <class T, std::enable_if_t<std::conjunction_v<std::negation<is_function<std::decay_t<T>>>, std::negation<is_specialization<std::decay_t<T>, std::in_place_type_t>> /*,
        std::is_copy_constructible<std::decay_t<T>>*/
                                            >,
                           int> = 0>
    Function(T&& _rvalue)
    {
        doEmplace<std::decay_t<T>>(std::forward<T>(_rvalue));
    }

    ~Function() noexcept
    {
        reset();
    }

    ThisT& operator=(const ThisT& _other)
    {
        *this = ThisT{_other};
        return *this;
    }

    ThisT& operator=(ThisT&& _other)
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
        case fnc_impl::RepresentationE::Small:
            storage_.small_.prtti_->pdestroy_fnc_(&storage_.small_.data_);
            break;
        case fnc_impl::RepresentationE::Big:
            storage_.big_.prtti_->pdestroy_fnc_(storage_.big_.ptr_);
            break;
        case fnc_impl::RepresentationE::None:
        default:
            break;
        }
        storage_.type_data_ = 0;
    }

    R operator()(ArgTypes... _args) const
    {
        if (has_value()) {
            if (is_small()) {
                return storage_.small_.prtti_->pinvoke_fnc_(&storage_.small_.data_, std::forward<ArgTypes>(_args)...);
            } else {
                return storage_.big_.prtti_->pinvoke_fnc_(storage_.big_.ptr_, std::forward<ArgTypes>(_args)...);
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
        return storage_.type_data_ != 0;
    }
    bool empty() const noexcept
    {
        return !has_value();
    }

    explicit operator bool() const noexcept
    {
        return has_value();
    }

    const std::type_info& type() const noexcept
    {
        const std::type_info* const pinfo = typeInfo();
        return pinfo ? *pinfo : typeid(void);
    }

    bool is_movable() const
    {
        if (is_small()) {
            return storage_.small_.prtti_->is_movable_;
        } else if (is_big()) {
            return storage_.big_.prtti_->is_copyable_;
        }
        return true;
    }
    bool is_copyable() const
    {
        if (is_small()) {
            return storage_.small_.prtti_->is_copyable_;
        } else if (is_big()) {
            return storage_.big_.prtti_->is_copyable_;
        }
        return true;
    }

    bool is_small() const
    {
        return representation() == fnc_impl::RepresentationE::Small;
    }
    bool is_big() const
    {
        return representation() == fnc_impl::RepresentationE::Big;
    }

private:
    template <size_t Sz>
    void doMoveFrom(Function<R(ArgTypes...), Sz>& _other)
    {
        storage_.type_data_ = _other.storage_.type_data_;
        representation(fnc_impl::RepresentationE::None);
        switch (_other.representation()) {
        case fnc_impl::RepresentationE::Small: {
            const auto repr = _other.storage_.small_.prtti_->pmove_fnc_(
                _other.storage_.small_.data_,
                storage_.small_.data_, small_capacity, storage_.small_.prtti_,
                storage_.big_.ptr_, storage_.big_.prtti_);
            representation(repr);
        } break;
        case fnc_impl::RepresentationE::Big: {
            const auto repr = _other.storage_.big_.prtti_->pmove_fnc_(
                _other.storage_.big_.ptr_,
                storage_.small_.data_, small_capacity, storage_.small_.prtti_,
                storage_.big_.ptr_, storage_.big_.prtti_);
            representation(repr);
            if (repr == fnc_impl::RepresentationE::Big) {
                _other.storage_.type_data_ = 0;
            }
        } break;
        default:
            break;
        }
    }

    template <size_t Sz>
    void doCopyFrom(const Function<R(ArgTypes...), Sz>& _other)
    {
        storage_.type_data_ = _other.storage_.type_data_;
        representation(fnc_impl::RepresentationE::None);
        switch (_other.representation()) {
        case fnc_impl::RepresentationE::Small: {
            const auto repr = _other.storage_.small_.prtti_->pcopy_fnc_(
                _other.storage_.small_.data_,
                storage_.small_.data_, small_capacity, storage_.small_.prtti_,
                storage_.big_.ptr_, storage_.big_.prtti_);
            representation(repr);
        } break;
        case fnc_impl::RepresentationE::Big: {
            const auto repr = _other.storage_.big_.prtti_->pcopy_fnc_(
                _other.storage_.big_.ptr_,
                storage_.small_.data_, small_capacity, storage_.small_.prtti_,
                storage_.big_.ptr_, storage_.big_.prtti_);
            representation(repr);
        } break;
        default:
            break;
        }
    }

    template <class T, class... Args>
    T& doEmplace(Args&&... _args)
    {
        if constexpr (is_small_type<T>()) {
            auto& rval = reinterpret_cast<T&>(storage_.small_.data_);

            ::new (const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval)))) T{std::forward<Args>(_args)...};
            storage_.small_.prtti_ = &fnc_impl::small_rtti<T, R, ArgTypes...>;
            storage_.type_data_    = reinterpret_cast<uintptr_t>(&typeid(T));
            representation(fnc_impl::RepresentationE::Small);

            return rval;
        } else {
            T* const ptr         = ::new T(std::forward<Args>(_args)...);
            storage_.big_.ptr_   = ptr;
            storage_.big_.prtti_ = &fnc_impl::big_rtti<T, R, ArgTypes...>;
            storage_.type_data_  = reinterpret_cast<uintptr_t>(&typeid(T));
            representation(fnc_impl::RepresentationE::Big);
            return *ptr;
        }
    }
};

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

} //namespace solid

#ifdef SOLID_USE_STD_FUNCTION

#define solid_function_t(...) std::function<__VA_ARGS__>

#else

#define solid_function_t(...) solid::Function<__VA_ARGS__>

#endif

#define solid_function_empty(f) (!f)
#define solid_function_clear(f) (f = nullptr)
