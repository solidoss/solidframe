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
#include "solid/utility/common.hpp"
#include "solid/utility/typetraits.hpp"

namespace solid {
inline constexpr size_t any_default_data_size = 3 * sizeof(void*);

inline constexpr size_t any_size_from_sizeof(const size_t _sizeof)
{
    return _sizeof - sizeof(void*);
}
template <class T>
inline constexpr const T& any_max(const T& a, const T& b)
{
    return (a < b) ? b : a;
}
template <size_t DataSize = any_default_data_size>
class Any;

template <class T>
struct is_any;

template <size_t V>
struct is_any<Any<V>> : std::true_type {
};

template <class T>
struct is_any : std::false_type {
};

namespace any_impl {
enum struct RepresentationE : uintptr_t {
    None = 0,
    Small,
    Big,
};
constexpr uintptr_t representation_mask           = 3;
constexpr uintptr_t representation_and_flags_mask = representation_mask;

struct SmallRTTI;

struct BigRTTI {
    using DestroyFncT = void(void*);
    using CopyFncT    = RepresentationE(const void*, void*, const size_t, const SmallRTTI*&, void*&, const BigRTTI*&);
    using MoveFncT    = RepresentationE(void*, void*, const size_t, const SmallRTTI*&, void*&, const BigRTTI*&);
    using GetIfFncT   = const void*(const std::type_index&, const void*);

    template <class T>
    static void destroy(void* const _what) noexcept
    {
        ::delete static_cast<T*>(_what);
    }

    template <class T>
    static void* move(void* const _from)
    {
        return ::new T(std::move(*static_cast<T*>(_from)));
    }

    DestroyFncT* pdestroy_fnc_;
    CopyFncT*    pcopy_fnc_;
    MoveFncT*    pmove_fnc_;
    GetIfFncT*   pget_if_fnc_;
    const bool   is_copyable_;
    const bool   is_movable_;
    const bool   is_tuple_;
};

struct SmallRTTI {
    using DestroyFncT = void(void*) noexcept;
    using CopyFncT    = RepresentationE(const void*, void*, const size_t, const SmallRTTI*&, void*&, const BigRTTI*&);
    using MoveFncT    = RepresentationE(void*, void*, const size_t, const SmallRTTI*&, void*&, const BigRTTI*&);
    using GetIfFncT   = const void*(const std::type_index&, const void*);

    template <class T>
    static void destroy(void* const _what) noexcept
    {
        static_cast<T*>(_what)->~T();
    }

    DestroyFncT* pdestroy_fnc_;
    CopyFncT*    pcopy_fnc_;
    MoveFncT*    pmove_fnc_;
    GetIfFncT*   pget_if_fnc_;
    const bool   is_copyable_;
    const bool   is_movable_;
    const bool   is_tuple_;
};

template <class T>
RepresentationE do_copy(
    const void* _pfrom,
    void* _pto_small, const size_t _small_cap, const SmallRTTI*& _rpsmall_rtti,
    void*& _rpto_big, const BigRTTI*& _rpbig_rtti);

template <class T>
RepresentationE do_move(
    void* _pfrom,
    void* _pto_small, const size_t _small_cap, const SmallRTTI*& _rpsmall_rtti,
    void*& _rpto_big, const BigRTTI*& _rpbig_rtti);

template <class T>
RepresentationE do_move_big(
    void* _pfrom,
    void* _pto_small, const size_t _small_cap, const SmallRTTI*& _rpsmall_rtti,
    void*& _rpto_big, const BigRTTI*& _rpbig_rtti);

template <class T>
const void* do_get_if(const std::type_index& _type_index, const void* _pdata);

template <class T>
inline constexpr BigRTTI big_rtti = {
    &BigRTTI::destroy<T>, &do_copy<T>, &do_move_big<T>, &do_get_if<T>, std::is_copy_constructible_v<T>, std::is_move_constructible_v<T>, is_specialization_v<T, std::tuple>};

template <class T>
inline constexpr SmallRTTI small_rtti = {
    &SmallRTTI::destroy<T>, &do_copy<T>, &do_move<T>, &do_get_if<T>, std::is_copy_constructible_v<T>, std::is_move_constructible_v<T>, is_specialization_v<T, std::tuple>};

template <class T>
RepresentationE do_copy(
    const void* _pfrom,
    void* _pto_small, const size_t _small_cap, const SmallRTTI*& _rpsmall_rtti,
    void*& _rpto_big, const BigRTTI*& _rpbig_rtti)
{
    if constexpr (alignof(T) <= alignof(max_align_t) && std::is_copy_constructible_v<T>) {
        if (sizeof(T) <= _small_cap) {
            T&       rdst = *static_cast<T*>(_pto_small);
            const T& rsrc = *static_cast<const T*>(_pfrom);
            ::new (const_cast<void*>(static_cast<const volatile void*>(std::addressof(rdst)))) T(rsrc);
            _rpsmall_rtti = &small_rtti<T>;
            return RepresentationE::Small;
        } else {
            _rpto_big   = ::new T(*static_cast<const T*>(_pfrom));
            _rpbig_rtti = &big_rtti<T>;
            return RepresentationE::Big;
        }
    } else if constexpr (std::is_trivially_constructible_v<T> || std::is_copy_constructible_v<T>) {
        _rpto_big   = ::new T(*static_cast<const T*>(_pfrom));
        _rpbig_rtti = &big_rtti<T>;
        return RepresentationE::Big;
    } else {
        solid_throw("Any: contained value not copyable");
        return RepresentationE::None;
    }
}

template <class T>
RepresentationE do_move(
    void* _pfrom,
    void* _pto_small, const size_t _small_cap, const SmallRTTI*& _rpsmall_rtti,
    void*& _rpto_big, const BigRTTI*& _rpbig_rtti)
{
    if constexpr (alignof(T) <= alignof(max_align_t) && std::is_move_constructible_v<T>) {
        if (sizeof(T) <= _small_cap) {
            T& rdst = *static_cast<T*>(_pto_small);
            T& rsrc = *static_cast<T*>(_pfrom);
            ::new (const_cast<void*>(static_cast<const volatile void*>(std::addressof(rdst)))) T{std::move(rsrc)};
            _rpsmall_rtti = &small_rtti<T>;
            return RepresentationE::Small;
        } else {
            _rpto_big   = ::new T{std::move(*static_cast<T*>(_pfrom))};
            _rpbig_rtti = &big_rtti<T>;
            return RepresentationE::Big;
        }
    } else if constexpr (std::is_move_constructible_v<T>) {
        _rpto_big   = ::new T{std::move(*static_cast<T*>(_pfrom))};
        _rpbig_rtti = &big_rtti<T>;
        return RepresentationE::Big;
    } else {
        solid_throw("Any: contained value not movable");
        return RepresentationE::None;
    }
}

template <class T>
RepresentationE do_move_big(
    void* _pfrom,
    void* _pto_small, const size_t _small_cap, const SmallRTTI*& _rpsmall_rtti,
    void*& _rpto_big, const BigRTTI*& _rpbig_rtti)
{
    if constexpr (alignof(T) <= alignof(max_align_t) && std::is_move_constructible_v<T>) {
        if (sizeof(T) <= _small_cap) {
            T& rdst = *static_cast<T*>(_pto_small);
            T& rsrc = *static_cast<T*>(_pfrom);
            ::new (const_cast<void*>(static_cast<const volatile void*>(std::addressof(rdst)))) T{std::move(rsrc)};
            _rpsmall_rtti = &small_rtti<T>;
            return RepresentationE::Small;
        } else {
            _rpto_big   = static_cast<T*>(_pfrom); //::new T{ std::move(*static_cast<T*>(_pfrom)) };
            _rpbig_rtti = &big_rtti<T>;
            return RepresentationE::Big;
        }
    } else {
        _rpto_big   = static_cast<T*>(_pfrom); //::new T{ std::move(*static_cast<T*>(_pfrom)) };
        _rpbig_rtti = &big_rtti<T>;
        return RepresentationE::Big;
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

constexpr size_t compute_small_capacity(const size_t _req_capacity)
{
    const size_t end_capacity = sizeof(uintptr_t) + sizeof(void*);
    const size_t req_capacity = any_max(_req_capacity, any_max(end_capacity, sizeof(max_align_t)) - end_capacity);
    const size_t tot_capacity = padded_size(req_capacity + sizeof(uintptr_t) + sizeof(void*), alignof(max_align_t));

    return tot_capacity - end_capacity;
}

} // namespace any_impl

template <size_t DataSize>
class Any {
    static constexpr size_t small_capacity = any_impl::compute_small_capacity(any_max(sizeof(void*) * 3, DataSize));
    static constexpr size_t big_padding    = small_capacity - sizeof(void*);

    struct Small {
        unsigned char              data_[small_capacity];
        const any_impl::SmallRTTI* prtti_;
    };

    struct Big {
        unsigned char            padding_[big_padding];
        void*                    ptr_;
        const any_impl::BigRTTI* prtti_;
    };

    struct Storage {
        union {
            Small small_;
            Big   big_;
        };
        uintptr_t type_data_;
    };

    union {
        Storage     storage_{};
        max_align_t dummy_;
    };
    template <size_t S>
    friend class Any;

private:
    any_impl::RepresentationE representation() const noexcept
    {
        return static_cast<any_impl::RepresentationE>(storage_.type_data_ & any_impl::representation_and_flags_mask);
    }

    void representation(const any_impl::RepresentationE _repr) noexcept
    {
        storage_.type_data_ &= (~any_impl::representation_and_flags_mask);
        storage_.type_data_ |= static_cast<uintptr_t>(_repr);
    }

    const std::type_info* typeInfo() const noexcept
    {
        return reinterpret_cast<const std::type_info*>(storage_.type_data_ & ~any_impl::representation_and_flags_mask);
    }

public:
    using ThisT = Any<DataSize>;

    static constexpr size_t smallCapacity()
    {
        return small_capacity;
    }

    template <class T>
    static constexpr bool is_small_type()
    {
        return alignof(T) <= alignof(max_align_t) && sizeof(T) <= small_capacity;
    }

    constexpr Any() noexcept {
        storage_.type_data_ = 0;
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

    template <class T, std::enable_if_t<std::conjunction_v<std::negation<is_any<std::decay_t<T>>>, std::negation<is_specialization<std::decay_t<T>, std::in_place_type_t>> /*,
        std::is_copy_constructible<std::decay_t<T>>*/
                                            >,
                           int>
        = 0>
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

    ThisT& operator=(ThisT&& _other)
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
        switch (representation()) {
        case any_impl::RepresentationE::Small:
            storage_.small_.prtti_->pdestroy_fnc_(&storage_.small_.data_);
            break;
        case any_impl::RepresentationE::Big:
            storage_.big_.prtti_->pdestroy_fnc_(storage_.big_.ptr_);
            break;
        case any_impl::RepresentationE::None:
        default:
            break;
        }
        storage_.type_data_ = 0;
    }

    template <size_t Sz>
    void swap(Any<Sz>& _other) noexcept
    {
        _other = std::exchange(*this, std::move(_other));
    }

    bool has_value() const noexcept
    {
        return storage_.type_data_ != 0;
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
            return reinterpret_cast<const T*>(&storage_.small_.data_);
        } else {
            return static_cast<const T*>(storage_.big_.ptr_);
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
        switch (representation()) {
        case any_impl::RepresentationE::Small:
            return static_cast<const T*>(storage_.small_.prtti_->pget_if_fnc_(std::type_index(typeid(T)), &storage_.small_.data_));
        case any_impl::RepresentationE::Big:
            return static_cast<const T*>(storage_.big_.prtti_->pget_if_fnc_(std::type_index(typeid(T)), storage_.big_.ptr_));
        default:
            return nullptr;
        }
    }

    template <class T>
    T* get_if() noexcept
    {
        return const_cast<T*>(static_cast<const ThisT*>(this)->get_if<T>());
    }

    bool is_movable() const
    {
        if (is_small()) {
            return storage_.small_.prtti_->is_movable_;
        } else if (is_big()) {
            return storage_.big_.prtti_->is_movable_;
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

    bool is_tuple() const
    {
        if (is_small()) {
            return storage_.small_.prtti_->is_tuple_;
        } else if (is_big()) {
            return storage_.big_.prtti_->is_tuple_;
        }
        return false;
    }

    bool is_small() const
    {
        return representation() == any_impl::RepresentationE::Small;
    }
    bool is_big() const
    {
        return representation() == any_impl::RepresentationE::Big;
    }

private:
    template <size_t Sz>
    void doMoveFrom(Any<Sz>& _other)
    {
        storage_.type_data_ = _other.storage_.type_data_;
        representation(any_impl::RepresentationE::None);
        switch (_other.representation()) {
        case any_impl::RepresentationE::Small: {
            const auto repr = _other.storage_.small_.prtti_->pmove_fnc_(
                _other.storage_.small_.data_,
                storage_.small_.data_, small_capacity, storage_.small_.prtti_,
                storage_.big_.ptr_, storage_.big_.prtti_);
            representation(repr);
            _other.reset();
        } break;
        case any_impl::RepresentationE::Big: {
            const auto repr = _other.storage_.big_.prtti_->pmove_fnc_(
                _other.storage_.big_.ptr_,
                storage_.small_.data_, small_capacity, storage_.small_.prtti_,
                storage_.big_.ptr_, storage_.big_.prtti_);
            representation(repr);
            if (repr == any_impl::RepresentationE::Big) {
                _other.storage_.type_data_ = 0;
            } else {
                _other.reset();
            }
        } break;
        default:
            break;
        }
    }

    template <size_t Sz>
    void doCopyFrom(const Any<Sz>& _other)
    {
        storage_.type_data_ = _other.storage_.type_data_;
        representation(any_impl::RepresentationE::None);
        switch (_other.representation()) {
        case any_impl::RepresentationE::Small: {
            const auto repr = _other.storage_.small_.prtti_->pcopy_fnc_(
                _other.storage_.small_.data_,
                storage_.small_.data_, small_capacity, storage_.small_.prtti_,
                storage_.big_.ptr_, storage_.big_.prtti_);
            representation(repr);
        } break;
        case any_impl::RepresentationE::Big: {
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
            storage_.small_.prtti_ = &any_impl::small_rtti<T>;
            storage_.type_data_    = reinterpret_cast<uintptr_t>(&typeid(T));
            representation(any_impl::RepresentationE::Small);

            return rval;
        } else {
            T* const ptr         = ::new T(std::forward<Args>(_args)...);
            storage_.big_.ptr_   = ptr;
            storage_.big_.prtti_ = &any_impl::big_rtti<T>;
            storage_.type_data_  = reinterpret_cast<uintptr_t>(&typeid(T));
            representation(any_impl::RepresentationE::Big);
            return *ptr;
        }
    }
};

//-----------------------------------------------------------------------------

template <size_t S1, size_t S2>
inline void swap(Any<S1>& _a1, Any<S2>& _a2) noexcept
{
    _a1.swap(_a2);
}

template <class T, size_t Size = any_default_data_size, class... Args>
Any<Size> make_any(Args&&... _args)
{
    return Any<Size>{std::in_place_type<T>, std::forward<Args>(_args)...};
}
template <class T, size_t Size = any_default_data_size, class E, class... Args>
Any<Size> make_any(std::initializer_list<E> _ilist, Args&&... _args)
{
    return Any<Size>{std::in_place_type<T>, _ilist, std::forward<Args>(_args)...};
}

//-----------------------------------------------------------------------------

} // namespace solid
