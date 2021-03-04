// solid/utility/anytuple.hpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <utility>

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/typetraits.hpp"

namespace solid {

namespace detail {

class AnyTupleBase {
public:
    virtual ~AnyTupleBase() {}

    virtual void*                         getIf(const std::type_info&) = 0;
    virtual std::unique_ptr<AnyTupleBase> clone() const                = 0;
};

template <typename... Args>
class AnyTuple : public AnyTupleBase {
    using TupleT = std::tuple<Args...>;

    TupleT tuple_;

    template <size_t Index = 0>
    void* getIf(const std::type_info& _type_info)
    {
        if constexpr (Index < std::tuple_size_v<TupleT>) {
            if (std::type_index(_type_info) == std::type_index(typeid(std::tuple_element_t<Index, TupleT>))) {
                return &std::get<Index>(tuple_);
            }
            return getIf<Index + 1>(_type_info);
        }
        return nullptr;
    }

    void* getIf(const std::type_info& _type_info) override
    {
        return getIf<>(_type_info);
    }

    std::unique_ptr<AnyTupleBase> clone() const override
    {
        return std::make_unique<AnyTuple<Args...>>(tuple_);
    }

public:
    AnyTuple(const std::tuple<Args...>& _tuple)
        : tuple_(_tuple)
    {
    }
    AnyTuple(std::tuple<Args...>&& _tuple)
        : tuple_(std::forward<std::tuple<Args...>>(_tuple))
    {
    }
};

} //namespace detail

class AnyTuple {
    std::unique_ptr<detail::AnyTupleBase> base_ptr_;

public:
    AnyTuple() {}

    template <typename... Args>
    AnyTuple(std::tuple<Args...>&& _tuple)
        : base_ptr_(std::make_unique<detail::AnyTuple<Args...>>(std::forward<std::tuple<Args...>>(_tuple)))
    {
    }

    AnyTuple(const AnyTuple& _other)
    {
        if (_other.base_ptr_) {
            base_ptr_ = _other.base_ptr_->clone();
        }
    }

    AnyTuple(AnyTuple&& _other)
        : base_ptr_(std::move(_other.base_ptr_))
    {
    }

    AnyTuple& operator=(const AnyTuple& _other)
    {
        if (_other.base_ptr_) {
            base_ptr_ = _other.base_ptr_->clone();
        }
        return *this;
    }

    AnyTuple& operator=(AnyTuple&& _other)
    {
        base_ptr_ = std::move(_other.base_ptr_);
        return *this;
    }

    explicit operator bool() const noexcept
    {
        return base_ptr_.get() != nullptr;
    }

    template <typename... Args>
    AnyTuple& operator=(std::tuple<Args...>&& _tuple)
    {
        base_ptr_ = std::make_unique<detail::AnyTuple<Args...>>(std::forward<std::tuple<Args...>>(_tuple));
        return *this;
    }

    template <typename T>
    const T* getIf() const
    {
        if (base_ptr_) {
            return reinterpret_cast<const T*>(base_ptr_->getIf(typeid(T)));
        }
        return nullptr;
    }

    template <typename T>
    T* getIf()
    {
        if (base_ptr_) {
            return reinterpret_cast<T*>(base_ptr_->getIf(typeid(T)));
        }
        return nullptr;
    }
};

} //namespace solid
