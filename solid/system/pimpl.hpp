// solid/system/pimpl.hpp
//
// Copyright (c) 2017 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <memory>
#include <type_traits>

namespace solid {

template <typename T, size_t Cp = 0, size_t Align = alignof(max_align_t)>
class Pimpl;

template <typename T, size_t Cp, size_t Align>
class Pimpl {
    T* ptr_;
    alignas(Align) uint8_t data_[Cp];

public:
    template <typename... Args>
    Pimpl(Args&&... args)
    {
        static_assert(sizeof(T) <= Cp && alignof(T) <= Align);
#if __cpp_static_assert >= 202306L
        // Not real C++ yet (std::format should be constexpr to work):
        static_assert(sizeof(T) <= Cp, std::format("Increase the capacity! Expected {}, got {}", sizeof(T), Cp));
#else
        static_assert(sizeof(T) <= Cp, "Increase the capacity");
        static_assert(alignof(T) <= Align, "Increase the alignament");
#endif
        ptr_ = new (data_) T(std::forward<Args>(args)...);
    }

    Pimpl(const Pimpl& _other)
    {
        static_assert(sizeof(T) <= Cp, "Increase the capacity");
        static_assert(alignof(T) <= Align, "Increase the alignament");
        ptr_ = new (data_) T(*_other.ptr_);
    }

    Pimpl(Pimpl&& _other)
    {
        static_assert(sizeof(T) <= Cp, "Increase the capacity");
        static_assert(alignof(T) <= Align, "Increase the alignament");
        ptr_ = new (data_) T(std::move(*_other.ptr_));
    }

    ~Pimpl()
    {
        static_assert(sizeof(T) <= Cp, "Increase the capacity");
        static_assert(alignof(T) <= Align, "Increase the alignament");
        std::destroy_at(std::launder(ptr_));
    }

    T* operator->() noexcept
    {
        return ptr_;
    }

    T& operator*() noexcept
    {
        return *ptr_;
    }

    const T* operator->() const noexcept
    {
        return ptr_;
    }

    const T& operator*() const noexcept
    {
        return *ptr_;
    }
};

template <typename T>
using PimplT = std::unique_ptr<T>;

template <typename T, typename... Args>
std::unique_ptr<T> make_pimpl(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

} // namespace solid
