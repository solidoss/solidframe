
#include "solid/utility/sharedbuffer.hpp"
#include "solid/system/common.hpp"
#include "solid/system/spinlock.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/stack.hpp"
#include <cassert>
#include <cstddef>
#include <mutex>
#include <new>
#include <unordered_map>

using namespace std;

namespace solid {

// SharedBuffer::Data SharedBuffer::sentinel;

namespace {
template <size_t DataSize>
inline constexpr std::size_t compute_capacity(const std::size_t _cap)
{
    constexpr std::size_t allign = hardware_destructive_interference_size;
    const std::size_t     sum    = (_cap + DataSize);

    static_assert(allign >= DataSize);

    if ((sum % allign) == 0) {
        return sum;
    } else {
        return (sum - (sum % allign)) + allign;
    }
}
} // namespace

namespace impl {

char* SharedBufferData::collapse(size_t& _previous_use_count)
{
    _previous_use_count = use_count_.fetch_sub(1);
    if (_previous_use_count == 1) {
        return buffer_;
    }
    return nullptr;
}

/* static */ SharedBufferData* SharedBufferBase::allocate_data(const std::size_t _cap)
{
    const std::size_t new_cap = compute_capacity<sizeof(SharedBufferData)>(_cap);
    auto              pbuf    = new char[new_cap];

    auto pdata       = new (pbuf) SharedBufferData{pbuf};
    pdata->capacity_ = _cap;
    return pdata;
}

void SharedBufferBase::reset()
{
    if (*this and pdata_->release()) {

        if (pdata_->ppool_) {
            pdata_->reset();
            pdata_->ppool_->push(SharedBufferBase{pdata_});
        } else {
            delete[] pdata_->buffer_;
        }
        pdata_ = &sentinel;
    }
}

} // namespace impl

void RingSharedBuffer::optimize()
{
    solid_check(canOptimize());
    if (consume_offset_ == size()) [[likely]] {
        consume_offset_ = 0U;
        pdata_->size_   = 0U;
    } else {
        memmove(impl::SharedBufferBase::data(), cdata(), csize());
        pdata_->size_   = csize();
        consume_offset_ = 0U;
    }
}

} // namespace solid