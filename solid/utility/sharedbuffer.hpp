#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <thread>

#include "solid/system/common.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/pimpl.hpp"

namespace solid {

class BufferManager;

namespace impl {

struct SharedBufferData {
    friend class BufferManager;

    std::atomic<std::size_t> use_count_;
    std::thread::id          make_thread_id_;
    SharedBufferData*        pnext_    = nullptr;
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

    char* release(size_t& _previous_use_count);
    char* collapse(size_t& _previous_use_count);

    char* data()
    {
        return data_;
    }
};

class SharedBufferBase {
protected:
    friend class BufferManager;

    static inline SharedBufferData sentinel{};
    static SharedBufferData*       allocate_data(std::size_t _cap);

    SharedBufferData* pdata_;

    SharedBufferBase()
        : pdata_(&sentinel)
    {
    }

    SharedBufferBase(const std::size_t _cap)
        : pdata_(allocate_data(_cap))
    {
    }

    SharedBufferBase(std::size_t _cap, const std::thread::id& _thr_id);

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

    [[nodiscard]] std::size_t actualCapacity() const;

    [[nodiscard]] auto makerThreadId() const
    {
        return pdata_->make_thread_id_;
    }

    [[nodiscard]] bool empty() const
    {
        return pdata_->size_ == 0;
    }

    [[nodiscard]] std::size_t useCount() const
    {
        return pdata_->use_count_.load();
    }

    size_t reset()
    {
        size_t previous_use_count = 0;
        if (*this) {
            auto* buf = pdata_->release(previous_use_count);
            delete[] buf;
            pdata_ = &sentinel;
        }
        return previous_use_count;
    }
};

} // namespace impl

class MutableSharedBuffer;

//-----------------------------------------------------------------------------
// SharedBuffer
//-----------------------------------------------------------------------------

class SharedBuffer : public impl::SharedBufferBase {
    friend class BufferManager;
    friend SharedBuffer make_shared_buffer(std::size_t);

    SharedBuffer(const std::size_t _cap)
        : SharedBufferBase(_cap)
    {
    }

    SharedBuffer(const std::size_t _cap, const std::thread::id& _thr_id)
        : SharedBufferBase(_cap, _thr_id)
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

    ~SharedBuffer()
    {
        reset();
    }

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

    SharedBufferView(const std::size_t _cap, const std::thread::id& _thr_id)
        : SharedBufferBase(_cap, _thr_id)
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
    friend class BufferManager;
    friend class SharedBufferView;

    friend MutableSharedBuffer make_mutable_buffer(std::size_t);

    MutableSharedBuffer(const std::size_t _cap)
        : impl::SharedBufferBase(_cap)
    {
    }

    MutableSharedBuffer(const std::size_t _cap, const std::thread::id& _thr_id)
        : impl::SharedBufferBase(_cap, _thr_id)
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

    ~MutableSharedBuffer()
    {
        reset();
    }

    explicit operator bool() const noexcept
    {
        return impl::SharedBufferBase::operator bool();
    }

    [[nodiscard]] size_t useCount() const
    {
        return impl::SharedBufferBase::useCount();
    }

    char* mdata()
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

    ~ConstSharedBuffer()
    {
        reset();
    }

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
// BufferManager
//-----------------------------------------------------------------------------

class BufferManager : NonCopyable {
    friend class impl::SharedBufferData;
    friend class impl::SharedBufferBase;
    struct Data;
    PimplT<Data> pimpl_;

    using DataT = impl::SharedBufferData;

    static char*  release(DataT* _pdata);
    static DataT* allocate(size_t _cap);

public:
    struct LocalData;

    struct Configuration {
        size_t default_local_max_count_ = 0;
    };

    static BufferManager& instance(const Configuration* _pconfig = nullptr);

    static SharedBuffer make(const size_t _cap)
    {
        return SharedBuffer{_cap, std::this_thread::get_id()};
    }

    static MutableSharedBuffer makeMutable(const size_t _cap)
    {
        return MutableSharedBuffer{make(_cap)};
    }

    static void   localMaxCount(size_t _cap, size_t _count);
    static size_t localMaxCount(size_t _cap);
    static size_t localCount(size_t _cap);

    static const Configuration& configuration();

private:
    BufferManager(const Configuration& _config);
    ~BufferManager();
};

} // namespace solid