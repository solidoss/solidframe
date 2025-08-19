
#include "solid/utility/sharedbuffer.hpp"
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
    constexpr std::size_t allign = 256;
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
char* SharedBufferData::release(size_t& _previous_use_count)
{
    if ((_previous_use_count = use_count_.fetch_sub(1)) == 1) {
        if (make_thread_id_ == InvalidIndex{}) {
            return buffer_;
        } else {
            return BufferPool::release(*this);
        }
    }
    return nullptr;
}

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

SharedBufferBase::SharedBufferBase(const std::size_t _cap, bool)
{
    uint64_t own_id         = InvalidIndex{};
    size_t   alloc_capacity = 0;
    pdata_                  = BufferPool::allocate(_cap, own_id, alloc_capacity);

    if (pdata_ == nullptr) [[unlikely]] {
        const std::size_t new_cap = compute_capacity<sizeof(SharedBufferData)>((alloc_capacity != 0U) ? alloc_capacity : _cap);
        char*             pbuf    = new char[new_cap];
        pdata_                    = new (pbuf) SharedBufferData{pbuf};
        pdata_->capacity_         = _cap;
    }
    pdata_->make_thread_id_ = own_id;
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

//-----------------------------------------------------------------------------
#if 0
struct BufferManager::LocalData {
    struct Entry {
        impl::SharedBufferData* ptop_      = nullptr;
        size_t                  max_count_ = BufferManager::configuration().default_local_max_count_;
        size_t                  count_     = 0;

        inline bool empty() const noexcept
        {
            return ptop_ == nullptr;
        }

        inline bool full() const noexcept
        {
            return max_count_ != 0 && count_ >= max_count_;
        }

        inline impl::SharedBufferData* pop() noexcept
        {
            if (!empty()) {
                auto* ptmp = ptop_;
                ptop_      = ptop_->pnext_;
                --count_;
                return ptmp;
            }
            return nullptr;
        }

        inline void push(impl::SharedBufferData* _pnode) noexcept
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

    Data(const BufferManager::Configuration& _config)
        : config_(_config)
    {
    }
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

/* static */ char* BufferManager::release(DataT* _pdata)
{
    if (_pdata) {
        if (_pdata->make_thread_id_ == std::this_thread::get_id()) {
            const std::size_t new_cap = compute_capacity<sizeof(DataT)>(_pdata->capacity_);
            auto&             entry   = local_data.entry_map_[new_cap];

            if (!entry.full()) {
                entry.push(_pdata);
                return nullptr;
            }
        }
        return _pdata->buffer_;
    }
    return nullptr;
}

/* static */ BufferManager::DataT* BufferManager::allocate(const size_t _cap)
{
    const std::size_t new_cap = compute_capacity<sizeof(DataT)>(_cap);
    auto&             entry   = local_data.entry_map_[new_cap];
    auto*             pdata   = entry.pop();
    if (pdata) {
        pdata->use_count_.store(1);
        pdata->size_     = 0;
        pdata->pnext_    = nullptr;
        pdata->capacity_ = _cap;
    }
    return pdata;
}

/* static */ void BufferManager::localMaxCount(const size_t _cap, const size_t _count)
{
    const std::size_t new_cap                 = compute_capacity<sizeof(DataT)>(_cap);
    local_data.entry_map_[new_cap].max_count_ = _count;
}

/* static */ size_t BufferManager::localMaxCount(const size_t _cap)
{
    const std::size_t new_cap = compute_capacity<sizeof(DataT)>(_cap);
    return local_data.entry_map_[new_cap].max_count_;
}

/* static */ size_t BufferManager::localCount(const size_t _cap)
{
    const std::size_t new_cap = compute_capacity<sizeof(DataT)>(_cap);
    return local_data.entry_map_[new_cap].count_;
}

/* static */ const BufferManager::Configuration& BufferManager::configuration()
{
    return instance().pimpl_->config_;
}
#endif

namespace {
using CacheVectorT = std::vector<impl::SharedBufferData*>;

struct RemoteStub {
    CacheVectorT fill_vec_;

    [[nodiscard]] bool canPush() const
    {
        return fill_vec_.size() != fill_vec_.capacity();
    }
    void push(impl::SharedBufferData* _rpdata)
    {
        fill_vec_.emplace_back(_rpdata);
    }

    [[nodiscard]] bool empty() const
    {
        return fill_vec_.empty();
    }
};

struct Stub {
    using CacheStackT = Stack<CacheVectorT>;
    using RemoteMapT  = std::unordered_map<uint64_t, RemoteStub>;

    CacheStackT filled_stack_;
    CacheStackT emptied_stack_;
    RemoteMapT  remote_map_;

    [[nodiscard]] bool empty() const
    {
        return not filled_stack_.empty() and not filled_stack_.top().empty();
    }

    auto& remote(uint64_t _own_id)
    {
        return remote_map_[_own_id];
    }

    impl::SharedBufferData* pop()
    {
        assert(not empty());
        auto* pdata = filled_stack_.top().back();
        filled_stack_.top().pop_back();
        if (filled_stack_.top().empty()) {
            emptied_stack_.push(std::move(filled_stack_.top()));
            filled_stack_.pop();
        }
        return pdata;
    }

    [[nodiscard]] bool canPush() const
    {
        return not filled_stack_.empty() and filled_stack_.top().size() != filled_stack_.top().capacity();
    }

    void push(impl::SharedBufferData* _rpdata)
    {
        filled_stack_.top().emplace_back(_rpdata);
    }

    bool fetchEmpty()
    {
        if (not emptied_stack_.empty()) {
            assert(emptied_stack_.top().capacity() != 0u);
            filled_stack_.push(std::move(emptied_stack_.top()));
            assert(emptied_stack_.top().capacity() == 0u);
            emptied_stack_.pop();
            return true;
        }
        return false;
    }

    bool fetchEmpty(RemoteStub& _rremote_stub)
    {
        assert(_rremote_stub.fill_vec_.capacity() == 0u);
        if (not emptied_stack_.empty()) {
            assert(emptied_stack_.top().capacity() != 0u);
            _rremote_stub.fill_vec_ = std::move(emptied_stack_.top());
            assert(emptied_stack_.top().capacity() == 0u);
            emptied_stack_.pop();
            return true;
        }
        return false;
    }
};

uint64_t pack(uint64_t const _index, uint64_t const _unique)
{
    return _index << 32U + (_unique & 0xffffffffU);
}

pair<uint32_t, uint32_t> unpack(uint64_t const _id)
{
    return {_id >> 32U, _id & 0xffffffffU};
}

} // namespace

struct BufferPool::OwnData {
    uint32_t          unique_{};
    std::vector<Stub> stubs_;

    bool fetch(size_t const _index, Stub& _local_stub)
    {
        return false;
    }

    bool fetchEmpty(size_t const _index, Stub& _local_stub)
    {
        return false;
    }
};

struct BufferPool::LocalData {
    uint64_t const    own_id_{InvalidIndex{}};
    OwnData&          rown_;
    std::vector<Stub> stubs_;

    LocalData(BufferPool::Data&);

    ~LocalData();

    auto& own()
    {
        return rown_;
    }

    [[nodiscard]] uint64_t ownId() const
    {
        return own_id_;
    }
};

struct BufferPool::Data {
    const Configuration config_;
    vector<OwnData>     own_vec_;
    Stack<size_t>       free_own_entries_stack_;
    uint32_t            own_index_{0};
    std::mutex          mutex_;
    SpinLock*           pspin_lock_array_ = nullptr;
    SpinLock            spin_lock_;

    Data(const BufferPool::Configuration& _config)
        : config_(_config)
    {
        own_vec_.resize(config_.max_thread_count_);
        pspin_lock_array_ = new SpinLock[config_.thread_spin_lock_count_];
    }

    ~Data()
    {
        // TODO:
        delete[] pspin_lock_array_;
    }

    auto& spinLock(size_t const _index)
    {
        return pspin_lock_array_[_index % config_.thread_spin_lock_count_];
    }

    [[nodiscard]] auto const& config() const
    {
        return config_;
    }

    bool fetch(size_t const _index, Stub& _local_stub)
    {
        return false;
    }

    bool fetchEmpty(size_t const _index, Stub& _local_stub)
    {
        return false;
    }

    bool createEmpty(size_t const /*_index*/, Stub& _local_stub) const
    {
        CacheVectorT vec;
        vec.reserve(config().cache_vector_capacity_);
        _local_stub.emptied_stack_.push(std::move(vec));
        return true;
    }

    void push(uint64_t const _own_id, size_t const _index, RemoteStub& _rremote_stub, Stub& _rstub)
    {
    }

    uint64_t createOwnId()
    {
        uint64_t id;
        {
            lock_guard<mutex> lock(mutex_);
            if (not free_own_entries_stack_.empty()) {
                id = free_own_entries_stack_.top();
                free_own_entries_stack_.pop();
            } else {
                solid_check(own_index_ < own_vec_.size());
                id = own_index_;
                ++own_index_;
            }
        }
        return pack(id, own_vec_[id].unique_);
    }

    auto& unsafeOwn(size_t const _own_id)
    {
        return own_vec_[unpack(_own_id).first];
    }

    void clearOwn(size_t const _own_index)
    {
        auto& own = own_vec_[_own_index];
        {
            lock_guard<SpinLock> lock(spinLock(_own_index));
            ++own.unique_;
        }
        // after this point every push to own will fail thus own data will not change
        lock_guard<SpinLock> lock(spin_lock_);
    }

    void releaseOwnId(uint64_t const _own_id)
    {
        auto const [index, unique] = unpack(_own_id);
        (void)unique;
        clearOwn(index);
        {
            lock_guard<mutex> lock(mutex_);
            free_own_entries_stack_.push(index);
        }
    }
};

BufferPool::LocalData::LocalData(BufferPool::Data& _rdata)
    : own_id_(_rdata.createOwnId())
    , rown_(_rdata.unsafeOwn(own_id_))
{
}

BufferPool::LocalData::~LocalData()
{
    BufferPool::data().releaseOwnId(own_id_);
}

BufferPool::BufferPool(const BufferPool::Configuration& _config)
    : pimpl_{make_pimpl<Data>(_config)}
{
}

BufferPool::~BufferPool() {}

BufferPool::Data& BufferPool::data()
{
    return *instance().pimpl_;
}

BufferPool::LocalData& BufferPool::local_data()
{
    static thread_local LocalData local_data{data()};
    return local_data;
}

BufferPool& BufferPool::do_instance(Configuration const& _rconfig)
{
    static BufferPool bman{_rconfig};
    return bman;
}

char* BufferPool::release(DataT& _rbuf_data)
{
    assert(_rbuf_data.make_thread_id_ != InvalidIndex{});

    auto& rdata = data();

    auto [index, capacity] = rdata.config().capacity_dispatch_fnc_(_rbuf_data.capacity_);

    auto& rlocal_data = local_data();

    if (_rbuf_data.make_thread_id_ == rlocal_data.ownId()) {
        auto& rstub = rlocal_data.stubs_[index];
        if (rstub.canPush()) {
            rstub.push(&_rbuf_data);
            return nullptr;
        }
        if (rstub.fetchEmpty()) {
            rstub.push(&_rbuf_data);
            return nullptr;
        }

        if (rlocal_data.own().fetchEmpty(index, rstub)) {
            rstub.fetchEmpty();
            rstub.push(&_rbuf_data);
            return nullptr;
        }

        if (rdata.fetchEmpty(index, rstub)) {
            rstub.fetchEmpty();
            rstub.push(&_rbuf_data);
            return nullptr;
        }

        if (rdata.createEmpty(index, rstub)) {
            rstub.fetchEmpty();
            rstub.push(&_rbuf_data);
            return nullptr;
        }

    } else {
        auto& rstub        = rlocal_data.stubs_[index];
        auto& rremote_stub = rstub.remote(_rbuf_data.make_thread_id_);

        if (rremote_stub.canPush()) {
            rremote_stub.push(&_rbuf_data);
            return nullptr;
        }

        if (not rremote_stub.empty()) {
            rdata.push(_rbuf_data.make_thread_id_, index, rremote_stub, rstub);
        }

        if (rstub.fetchEmpty(rremote_stub)) {
            rstub.push(&_rbuf_data);
            return nullptr;
        }

        if (rlocal_data.own().fetchEmpty(index, rstub)) {
            rstub.fetchEmpty(rremote_stub);
            rremote_stub.push(&_rbuf_data);
            return nullptr;
        }

        if (rdata.fetchEmpty(index, rstub)) {
            rstub.fetchEmpty(rremote_stub);
            rremote_stub.push(&_rbuf_data);
            return nullptr;
        }

        if (rdata.createEmpty(index, rstub)) {
            rstub.fetchEmpty(rremote_stub);
            rremote_stub.push(&_rbuf_data);
            return nullptr;
        }
    }

    return _rbuf_data.buffer_;
}

BufferPool::DataT* BufferPool::allocate(size_t _cap, uint64_t& _rown_id, size_t& _ralloc_capacity)
{
    auto& rdata = data();

    auto [index, capacity] = rdata.config().capacity_dispatch_fnc_(_cap);

    if (capacity == 0U or index > rdata.config().capacity_count_) [[unlikely]] {
        return nullptr;
    }

    auto& rlocal_data = local_data();

    _rown_id         = rlocal_data.ownId();
    _ralloc_capacity = capacity;

    auto& rstub = rlocal_data.stubs_[index];

    if (not rstub.empty()) {
        return rstub.pop();
    }

    if (rlocal_data.own().fetch(index, rstub)) {
        return rstub.pop();
    }

    if (rdata.fetch(index, rstub)) {
        return rstub.pop();
    }
    return nullptr;
}

} // namespace solid