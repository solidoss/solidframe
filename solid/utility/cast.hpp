#pragma once

#include <memory>

namespace solid {

template <class T, class U>
std::shared_ptr<T> static_pointer_cast(const std::shared_ptr<U>& r) noexcept
{
    return std::static_pointer_cast<T>(r);
}

template <class T, class U>
std::shared_ptr<T> static_pointer_cast(std::shared_ptr<U>&& r) noexcept
{
    return std::static_pointer_cast<T>(std::move(r));
}

template <class T, class U>
std::shared_ptr<T> dynamic_pointer_cast(const std::shared_ptr<U>& r) noexcept
{
    return std::dynamic_pointer_cast<T>(r);
}

template <class T, class U>
std::shared_ptr<T> dynamic_pointer_cast(std::shared_ptr<U>&& r) noexcept
{
    return std::dynamic_pointer_cast<T>(std::move(r));
}

} // namespace solid