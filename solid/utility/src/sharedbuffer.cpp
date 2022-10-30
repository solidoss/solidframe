
#include "solid/utility/sharedbuffer.hpp"
#include <new>

namespace solid {

SharedBuffer::Data SharedBuffer::sentinel;

char* SharedBuffer::Data::release()
{
    if (use_count_.fetch_sub(1) == 1) {
        return buffer_;
    }
    return nullptr;
}

inline constexpr std::size_t compute_capacity(const std::size_t _cap, const std::size_t _size_of_data)
{
    constexpr std::size_t allign = 256;
    const std::size_t     sum    = (_cap + _size_of_data);
    if ((sum % allign) == 0) {
        return sum;
    } else {
        return (sum - (sum % allign)) + allign;
    }
}

/* static */ SharedBuffer::Data* SharedBuffer::allocate_data(const std::size_t _cap)
{
    const std::size_t new_cap = compute_capacity(_cap, sizeof(Data));
    auto              pbuf    = new char[new_cap];

    auto pdata       = new (pbuf) Data{pbuf};
    pdata->capacity_ = (pbuf + new_cap) - pdata->data();
    return pdata;
}

} // namespace solid