
#include "solid/utility/sharedbuffer.hpp"
#include <new>
#include <unordered_map>

namespace solid {

SharedBuffer::Data SharedBuffer::sentinel;

char* SharedBuffer::Data::release()
{
    if (use_count_.fetch_sub(1) == 1) {
        if (make_thread_id_ == std::thread::id{}) {
            return buffer_;
        } else {
            return BufferManager::release(this);
        }
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

SharedBuffer::SharedBuffer(const std::size_t _cap, const std::thread::id& _thr_id)
{
    const std::size_t new_cap = compute_capacity(_cap, sizeof(Data));

    pdata_ = BufferManager::allocate(new_cap);
    if (pdata_ == nullptr) [[unlikely]] {
        char* pbuf        = new char[new_cap];
        pdata_            = new (pbuf) Data{pbuf};
        pdata_->capacity_ = (pbuf + new_cap) - pdata_->data();
    }
    pdata_->make_thread_id_ = _thr_id;
}

//-----------------------------------------------------------------------------

struct BufferManager::LocalData {
    struct Entry {
        SharedBuffer::Data* ptop_      = nullptr;
        size_t              max_count_ = BufferManager::configuration().default_local_max_count_;
        size_t              count_     = 0;

        inline bool empty() const noexcept
        {
            return ptop_ == nullptr;
        }

        inline bool full() const noexcept
        {
            return max_count_ != 0 && count_ >= max_count_;
        }

        inline SharedBuffer::Data* pop() noexcept
        {
            if (!empty()) {
                auto* ptmp = ptop_;
                ptop_      = ptop_->pnext_;
                --count_;
                return ptmp;
            }
            return nullptr;
        }

        inline void push(SharedBuffer::Data* _pnode) noexcept
        {
            if (_pnode) {
                _pnode->pnext_ = ptop_;
                ptop_          = _pnode;
                ++count_;
            }
        }
    };

    using CapacityMapT = std::unordered_map<size_t, Entry>;
    CapacityMapT entry_map_;
};

namespace {

thread_local BufferManager::LocalData local_data;

} // namespace

struct BufferManager::Data {
    const Configuration config_;
};

BufferManager::BufferManager(const BufferManager::Configuration& _config)
    : pimpl_{make_pimpl<Data>(_config)}
{
}

BufferManager::~BufferManager() {}

/* static */ BufferManager& BufferManager::instance(const BufferManager::Configuration* _pconfig)
{
    static const Configuration cfg;
    static BufferManager       ins{_pconfig ? *_pconfig : cfg};
    return ins;
}

/* static */ char* BufferManager::release(SharedBuffer::Data* _pdata)
{
    if (_pdata) {
        if (_pdata->make_thread_id_ == std::this_thread::get_id()) {
            auto& entry = local_data.entry_map_[_pdata->capacity()];
            if (!entry.full()) {
                entry.push(_pdata);
                return nullptr;
            }
        }
        return _pdata->buffer_;
    }
    return nullptr;
}

/* static */ SharedBuffer::Data* BufferManager::allocate(const size_t _cap)
{
    auto& entry = local_data.entry_map_[_cap];
    auto* pdata = entry.pop();
    if (pdata) {
        pdata->use_count_.store(1);
        pdata->size_  = 0;
        pdata->pnext_ = nullptr;
    }
    return pdata;
}

/* static */ void BufferManager::localMaxCount(const size_t _cap, const size_t _count)
{
    local_data.entry_map_[_cap].max_count_ = _count;
}

/* static */ size_t BufferManager::localMaxCount(const size_t _cap)
{
    return local_data.entry_map_[_cap].max_count_;
}

/* static */ size_t BufferManager::localCount(const size_t _cap)
{
    return local_data.entry_map_[_cap].count_;
}

/* static */ const BufferManager::Configuration& BufferManager::configuration()
{
    return instance().pimpl_->config_;
}

} // namespace solid