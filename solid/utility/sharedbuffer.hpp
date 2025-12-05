#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <limits>
#include <thread>
#include <type_traits>

#include "solid/system/common.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/pimpl.hpp"
#include "solid/system/spinlock.hpp"
#include "solid/utility/common.hpp"

namespace solid {

namespace impl {

class BufferPoolBase;

struct /* alignas(hardware_destructive_interference_size) */ SharedBufferData {
    friend class BufferManager;

    std::atomic<std::size_t> use_count_;
    BufferPoolBase*          ppool_    = nullptr;
    std::size_t              size_     = 0;
    std::size_t              capacity_ = 0;
    char*                    buffer_   = nullptr;
    char                     data_[8];

    SharedBufferData()
        : use_count_(1)
    {
    }

    SharedBufferData(char* _buffer)
        : use_count_(1)
        , buffer_(_buffer)
    {
    }

    SharedBufferData& acquire()
    {
        use_count_.fetch_add(1);
        return *this;
    }

    bool release()
    {
        return use_count_.fetch_sub(1) == 1;
    }

    char* collapse(size_t& _previous_use_count);

    char* data()
    {
        return data_;
    }

    void reset()
    {
        use_count_.store(1);
        size_ = 0;
    }
};

class SharedBufferBase {
protected:
    friend class BufferManager;

    static inline SharedBufferData sentinel{};

    static SharedBufferData* allocate_data(std::size_t _cap);

    SharedBufferData* pdata_;

    SharedBufferBase(SharedBufferData* _pdata = &sentinel)
        : pdata_(_pdata)
    {
    }

    SharedBufferBase(const std::size_t _cap)
        : pdata_(allocate_data(_cap))
    {
    }

    SharedBufferBase(const SharedBufferBase& _other)
        : pdata_(_other ? &_other.pdata_->acquire() : _other.pdata_)
    {
    }

    SharedBufferBase(SharedBufferBase&& _other) noexcept
        : pdata_(_other.pdata_)
    {
        _other.pdata_ = &sentinel;
    }

    [[nodiscard]] char* data() const
    {
        return pdata_->data();
    }

    void doCopy(const SharedBufferBase& _other)
    {
        if (pdata_ != _other.pdata_) {
            reset();

            if (_other) {
                pdata_ = &_other.pdata_->acquire();
            }
        }
    }

    void doMove(SharedBufferBase&& _other)
    {
        if (pdata_ != _other.pdata_) {
            reset();
            pdata_        = _other.pdata_;
            _other.pdata_ = &sentinel;
        }
    }

public:
    static constexpr size_t npos = std::numeric_limits<size_t>::max();

    explicit operator bool() const noexcept
    {
        return pdata_ != &sentinel;
    }
    ~SharedBufferBase()
    {
        reset();
    }

    [[nodiscard]] std::size_t size() const
    {
        return pdata_->size_;
    }

    [[nodiscard]] std::size_t capacity() const
    {
        return pdata_->capacity_;
    }

    [[nodiscard]] bool empty() const
    {
        return pdata_->size_ == 0;
    }

    [[nodiscard]] std::size_t useCount() const
    {
        return pdata_->use_count_.load();
    }

    void reset();
};

} // namespace impl

class MutableSharedBuffer;

//-----------------------------------------------------------------------------
// SharedBuffer
//-----------------------------------------------------------------------------

class SharedBuffer : public impl::SharedBufferBase {
    friend class impl::BufferPoolBase;
    friend SharedBuffer make_shared_buffer(std::size_t);

    SharedBuffer(const std::size_t _cap)
        : SharedBufferBase(_cap)
    {
    }

public:
    SharedBuffer() = default;

    SharedBuffer(const SharedBuffer& _other)
        : SharedBufferBase(_other)
    {
    }

    SharedBuffer(SharedBuffer&& _other) noexcept
        : SharedBufferBase(std::move(_other))
    {
    }

    SharedBuffer(MutableSharedBuffer&& _other);

    ~SharedBuffer() = default;

    [[nodiscard]] char* data() const
    {
        return pdata_->data();
    }

    void append(const std::size_t _size)
    {
        pdata_->size_ += _size;
    }

    void resize(const std::size_t _size = 0)
    {
        pdata_->size_ = _size;
    }

    SharedBuffer& operator=(const SharedBuffer& _other)
    {
        doCopy(_other);
        return *this;
    }

    SharedBuffer& operator=(SharedBuffer&& _other) noexcept
    {
        doMove(std::move(_other));
        return *this;
    }

    SharedBuffer& operator=(MutableSharedBuffer&& _other);

    void reset()
    {
        impl::SharedBufferBase::reset();
    }
};

inline SharedBuffer make_shared_buffer(const std::size_t _cap)
{
    return SharedBuffer(_cap);
}

//-----------------------------------------------------------------------------
// SharedBufferView
//-----------------------------------------------------------------------------

class SharedBufferView : protected impl::SharedBufferBase {
    friend class MutableSharedBuffer;
    const char* data_{nullptr};
    size_t      size_{0};

    SharedBufferView(
        MutableSharedBuffer const& _other, size_t _offset, size_t _size);

protected:
    SharedBufferView(size_t const _cap)
        : SharedBufferBase(_cap)
        , data_(impl::SharedBufferBase::data())
    {
    }

    SharedBufferView(impl::SharedBufferBase&& _other)
        : impl::SharedBufferBase(std::move(_other))
        , data_(impl::SharedBufferBase::data())
    {
    }

public:
    SharedBufferView() = default;

    SharedBufferView(SharedBufferView&& _other) noexcept
        : impl::SharedBufferBase(std::move(_other))
        , data_(_other.data_)
        , size_(_other.size_)
    {
        _other.data_ = nullptr;
        _other.size_ = 0U;
    }

    SharedBufferView(SharedBufferView const& _other)
        : impl::SharedBufferBase(_other)
        , data_(_other.data_)
        , size_(_other.size_)
    {
    }

    explicit operator bool() const noexcept
    {
        return impl::SharedBufferBase::operator bool();
    }

    [[nodiscard]] size_t useCount() const
    {
        return impl::SharedBufferBase::useCount();
    }

    [[nodiscard]] char const* cdata() const
    {
        return data_;
    }

    [[nodiscard]] size_t csize() const
    {
        return size_;
    }

    [[nodiscard]] bool cempty() const
    {
        return size_ == 0U;
    }

    MutableSharedBuffer collapse();

    void reset()
    {
        impl::SharedBufferBase::reset();
        data_ = nullptr;
        size_ = 0U;
    }

    SharedBufferView& operator=(SharedBufferView&& _other) noexcept
    {
        if (this != &_other) {
            doMove(std::move(_other));
            data_        = _other.data_;
            size_        = _other.size_;
            _other.data_ = nullptr;
            _other.size_ = 0;
        }
        return *this;
    }
};

//-----------------------------------------------------------------------------
// MutableSharedBuffer
//-----------------------------------------------------------------------------

class ConstSharedBuffer;

class MutableSharedBuffer : protected impl::SharedBufferBase {
    friend class ConstSharedBuffer;
    friend class SharedBuffer;
    friend class impl::BufferPoolBase;
    friend class impl::SharedBufferBase;
    friend class SharedBufferView;

    friend MutableSharedBuffer make_mutable_buffer(std::size_t);

    MutableSharedBuffer(const std::size_t _cap)
        : impl::SharedBufferBase(_cap)
    {
    }

    MutableSharedBuffer(impl::SharedBufferBase&& _other)
        : impl::SharedBufferBase(std::move(_other))
    {
    }

    MutableSharedBuffer(ConstSharedBuffer&& _other);

    MutableSharedBuffer(SharedBuffer&& _other)
        : impl::SharedBufferBase(std::move(_other))
    {
    }

    MutableSharedBuffer(SharedBufferView&& _other)
        : impl::SharedBufferBase(std::move(_other))
    {
    }

public:
    MutableSharedBuffer() = default;

    MutableSharedBuffer(const MutableSharedBuffer& _other) = delete;

    MutableSharedBuffer(MutableSharedBuffer&& _other) noexcept
        : impl::SharedBufferBase(std::move(_other))
    {
    }

    ~MutableSharedBuffer() = default;

    explicit operator bool() const noexcept
    {
        return impl::SharedBufferBase::operator bool();
    }

    [[nodiscard]] size_t useCount() const
    {
        return impl::SharedBufferBase::useCount();
    }

    [[nodiscard]] char* mdata() const
    {
        return impl::SharedBufferBase::data() + size();
    }

    [[nodiscard]] size_t msize() const
    {
        return capacity() - size();
    }

    void append(const std::size_t _size)
    {
        solid_check(_size <= msize());
        pdata_->size_ += _size;
    }

    void clear()
    {
        pdata_->size_ = 0;
    }

    [[nodiscard]] size_t capacity() const
    {
        return impl::SharedBufferBase::capacity();
    }

    MutableSharedBuffer& operator=(const MutableSharedBuffer& _other) = delete;

    MutableSharedBuffer& operator=(MutableSharedBuffer&& _other) noexcept
    {
        if (this != &_other) {
            doMove(std::move(_other));
        }
        return *this;
    }

    MutableSharedBuffer& operator=(SharedBuffer&& _other)
    {
        doMove(std::move(_other));
        return *this;
    }

    [[nodiscard]] SharedBufferView view(size_t const _offset = 0, size_t _size = impl::SharedBufferBase::npos) const
    {
        if (_size == impl::SharedBufferBase::npos) {
            _size = size();
        }
        solid_check((_offset + _size) <= size());
        return {*this, _offset, _size};
    }

    void reset()
    {
        impl::SharedBufferBase::reset();
    }
};

inline MutableSharedBuffer make_mutable_buffer(const std::size_t _cap)
{
    return MutableSharedBuffer(make_shared_buffer(_cap));
}

//-----------------------------------------------------------------------------
// RingSharedBuffer
//-----------------------------------------------------------------------------

class RingSharedBuffer : public MutableSharedBuffer {
    size_t consume_offset_ = 0;

public:
    [[nodiscard]] char const* data() const
    {
        return impl::SharedBufferBase::data();
    }

    [[nodiscard]] char const* cdata() const
    {
        return data() + consume_offset_;
    }
    [[nodiscard]] size_t csize() const
    {
        return size() - consume_offset_;
    }

    void consume(size_t const _size)
    {
        solid_check(_size <= csize());
        consume_offset_ += _size;
    }

    [[nodiscard]] bool cempty() const
    {
        return csize() == 0U;
    }

    [[nodiscard]] bool canOptimize() const
    {
        return useCount() == 1U;
    }

    RingSharedBuffer& operator=(MutableSharedBuffer&& _other) noexcept
    {
        if (this != &_other) {
            doMove(std::move(_other));
            consume_offset_ = 0U;
        }
        return *this;
    }

    void optimize();

    void reset()
    {
        impl::SharedBufferBase::reset();
        consume_offset_ = 0U;
    }
};

//-----------------------------------------------------------------------------
// ConstSharedBuffer
//-----------------------------------------------------------------------------

class ConstSharedBuffer : public impl::SharedBufferBase {
public:
    ConstSharedBuffer() = default;

    ConstSharedBuffer(const ConstSharedBuffer& _other)
        : SharedBufferBase(_other)
    {
    }

    ConstSharedBuffer(ConstSharedBuffer&& _other) noexcept
        : SharedBufferBase(std::move(_other))
    {
    }

    ConstSharedBuffer(MutableSharedBuffer&& _other)
        : SharedBufferBase(std::move(_other))
    {
    }

    ~ConstSharedBuffer() = default;

    [[nodiscard]] const char* data() const
    {
        return impl::SharedBufferBase::data();
    }

    MutableSharedBuffer collapse()
    {
        if (*this) {
            size_t previous_use_count = 0;
            auto*  buf                = pdata_->collapse(previous_use_count);
            if (buf) {
                pdata_->acquire();
                pdata_->size_ = 0;
                return MutableSharedBuffer(std::move(*this));
            } else {
                pdata_ = &sentinel;
            }
        }
        return {};
    }

    MutableSharedBuffer mutate()
    {
        if (useCount() == 1) {
            return MutableSharedBuffer(std::move(*this));
        } else {
            return {};
        }
    }

    ConstSharedBuffer& operator=(const ConstSharedBuffer& _other)
    {
        doCopy(_other);
        return *this;
    }

    ConstSharedBuffer& operator=(ConstSharedBuffer&& _other)
    {
        doMove(std::move(_other));
        return *this;
    }

    ConstSharedBuffer& operator=(SharedBuffer&& _other)
    {
        doMove(std::move(_other));
        return *this;
    }

    ConstSharedBuffer& operator=(MutableSharedBuffer&& _other)
    {
        doMove(std::move(_other));
        return *this;
    }
};

inline SharedBufferView::SharedBufferView(
    MutableSharedBuffer const& _other, size_t const _offset, size_t const _size)
    : impl::SharedBufferBase(_other)
    , data_(impl::SharedBufferBase::data() + _offset)
    , size_(_size)
{
}

inline MutableSharedBuffer SharedBufferView::collapse()
{
    if (*this) {
        size_t previous_use_count = 0;
        auto*  buf                = pdata_->collapse(previous_use_count);
        if (buf) {
            pdata_->acquire();
            pdata_->size_ = 0;
            this->size_   = 0;
            return MutableSharedBuffer(std::move(*this));
        } else {
            pdata_ = &sentinel;
        }
    }
    return {};
}

inline MutableSharedBuffer::MutableSharedBuffer(ConstSharedBuffer&& _other)
    : impl::SharedBufferBase(std::move(_other))
{
}

inline SharedBuffer& SharedBuffer::operator=(MutableSharedBuffer&& _other)
{
    doMove(std::move(_other));
    return *this;
}

inline SharedBuffer::SharedBuffer(MutableSharedBuffer&& _other)
    : SharedBufferBase(std::move(_other))
{
}

//-----------------------------------------------------------------------------
// BufferPool
//-----------------------------------------------------------------------------

struct BufferPoolDefaultConfiguration {
    using IndexT = uint32_t;

    static constexpr IndexT capacity_count = 11;

    static constexpr std::array<size_t, capacity_count> capacities{
        1024, 2048, 4096, 8 * 1024,
        16 * 1024, 32 * 1024, 64 * 1024, 128 * 1024, 256 * 1024, 512 * 1024, 1024 * 1024};

    static constexpr IndexT dispatch(size_t const _requested_capacity)
    {
        for (uint32_t i = 0; i < capacity_count; ++i) {
            if (_requested_capacity <= capacities[i]) {
                return i;
            }
        }
        return std::numeric_limits<IndexT>::max();
    }

    static constexpr size_t capacity(IndexT const _index)
    {
        if (_index < capacity_count) {
            return capacities[_index];
        }
        return 0;
    }

    static constexpr size_t count(IndexT const)
    {
        return 16 * 1024; // same for any buffer
    }
};

namespace impl {
class BufferPoolBase : NonCopyable {
    friend class SharedBufferBase;

protected:
    struct LockFreeEntry {
        std::atomic_flag    flag_;
        MutableSharedBuffer buf_;
    };
    struct LockFreeData {
        size_t                           count_        = 0;
        size_t                           create_count_ = 0u;
        std::unique_ptr<LockFreeEntry[]> entries_;
        size_t                           pop_index_                                           = 0;
        alignas(solid::hardware_destructive_interference_size) std::atomic_size_t push_index_ = 0;
    };

    std::unique_ptr<LockFreeData[]> data_entries_;
    std::atomic_size_t              await_return_count_ = 0u;
    std::atomic_flag                is_stopping_{};
    std::atomic_flag                can_destroy_{};

    virtual ~BufferPoolBase() = default;

    bool isFull()
    {
        return await_return_count_ == 0u;
    }

    [[nodiscard]] virtual size_t getIndex(size_t) const = 0;

    void setPool(MutableSharedBuffer& _rbuf)
    {
        assert(_rbuf);
        _rbuf.pdata_->ppool_ = this;
    }
    void resetPool(MutableSharedBuffer& _rbuf)
    {
        assert(_rbuf);
        _rbuf.pdata_->ppool_ = nullptr;
    }

    [[nodiscard]] size_t createCount(size_t const _data_index) const
    {
        auto& data_entry = data_entries_[_data_index];
        return data_entry.create_count_;
    }

    void incrementCreateCount(size_t const _data_index)
    {
        auto& data_entry = data_entries_[_data_index];
        ++data_entry.create_count_;
    }

    [[nodiscard]] MutableSharedBuffer pop(size_t const _data_index)
    {
        MutableSharedBuffer buf;
        auto&               data_entry = data_entries_[_data_index];

        auto const index  = data_entry.pop_index_;
        auto&      rentry = data_entry.entries_[index % data_entry.count_];

        if (rentry.flag_.test(std::memory_order_acquire)) {
            assert(rentry.buf_);
            buf = std::move(rentry.buf_);
            assert(not rentry.buf_);
            rentry.flag_.clear(std::memory_order_release);
            ++data_entry.pop_index_;
        }
        return buf;
    }

    void push(MutableSharedBuffer&& _buf)
    {
        if (not _buf) {
            return;
        }

        auto const data_index = getIndex(_buf.capacity());
        auto&      data_entry = data_entries_[data_index];
        {
            auto const index  = data_entry.push_index_.fetch_add(1, std::memory_order_relaxed);
            auto&      rentry = data_entry.entries_[index % data_entry.count_];

            while (rentry.flag_.test(std::memory_order_acquire)) {
                cpu_pause();
            }
            assert(not rentry.buf_);
            rentry.buf_ = std::move(_buf);
            rentry.flag_.test_and_set(std::memory_order_release);
        }
        if (await_return_count_.fetch_sub(1, std::memory_order_relaxed) == 1u and is_stopping_.test()) {
            can_destroy_.notify_one();
        }
    }
};
} // namespace impl

template <typename Config = BufferPoolDefaultConfiguration>
class BufferPool final : protected impl::BufferPoolBase {
    using ThisT = BufferPool<Config>;
    Config const config_;
    BufferPool(Config const& _config)
        : config_(_config)
    {
        data_entries_.reset(new LockFreeData[config_.capacity_count]);
        using IndexT = std::decay_t<decltype(config_.capacity_count)>;
        for (IndexT i = 0; i < config_.capacity_count; ++i) {
            auto& data_entry  = data_entries_[i];
            data_entry.count_ = config_.count(i);
            data_entry.entries_.reset(new LockFreeEntry[data_entry.count_]);
        }
    }

    auto capacity(size_t const _capacity) const
    {
        auto const idx = config_.dispatch(_capacity);
        if (idx != std::numeric_limits<decltype(idx)>::max()) {
            return config_.capacity(idx);
        }
        return _capacity;
    }

    [[nodiscard]] size_t getIndex(size_t _capacity) const override
    {
        auto const idx = config_.dispatch(_capacity);
        assert(idx != std::numeric_limits<decltype(idx)>::max());
        return idx;
    }

    ~BufferPool()
    {
        is_stopping_.test_and_set();
        while (not isFull()) {
            can_destroy_.wait(false);
        }

        for (size_t i = 0; i < config_.capacity_count; ++i) {
            auto& data_entry = data_entries_[i];
            for (size_t j = 0; j < data_entry.count_; ++j) {
                auto& entry = data_entry.entries_[j];
                if (entry.buf_) {
                    resetPool(entry.buf_);
                    entry.buf_.reset();
                }
            }
        }
    }

public:
    static auto& get(Config const& _config = {})
    {
        static thread_local ThisT ins{_config};
        return ins;
    }
    static MutableSharedBuffer create(const size_t _capacity)
    {
        auto&      rthis = get();
        auto const idx   = rthis.config_.dispatch(_capacity);

        if (idx != std::numeric_limits<decltype(idx)>::max()) {
            auto buf = rthis.pop(idx);
            if (buf) {
                buf.clear();
                ++rthis.await_return_count_;
                return buf;
            } else {
                if (rthis.createCount(idx) < rthis.config_.count(idx)) {
                    rthis.incrementCreateCount(idx);
                    buf = make_mutable_buffer(rthis.config_.capacity(idx));
                    rthis.setPool(buf);
                    ++rthis.await_return_count_;
                    return buf;
                }
            }
        }

        return make_mutable_buffer(_capacity);
    }
};

} // namespace solid