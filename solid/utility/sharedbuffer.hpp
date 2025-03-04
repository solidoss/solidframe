#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

#include "solid/system/common.hpp"
#include "solid/system/pimpl.hpp"

namespace solid {

class BufferManager;

namespace impl {

class SharedBufferBase {
    friend class BufferManager;

protected:
    struct Data {
        std::atomic<std::size_t> use_count_;
        std::thread::id          make_thread_id_;
        Data*                    pnext_    = nullptr;
        std::size_t              size_     = 0;
        std::size_t              capacity_ = 0;
        char*                    buffer_   = nullptr;
        char                     data_[8];

        Data()
            : use_count_(1)
        {
        }

        Data(char* _buffer)
            : use_count_(1)
            , buffer_(_buffer)
        {
        }

        Data& acquire()
        {
            use_count_.fetch_add(1);
            return *this;
        }

        char* release(size_t& _previous_use_count);

        char* data()
        {
            return data_;
        }
    };

    static inline Data sentinel{};
    static Data*       allocate_data(const std::size_t _cap);

    Data* pdata_;

protected:
    SharedBufferBase()
        : pdata_(&sentinel)
    {
    }

    SharedBufferBase(const std::size_t _cap)
        : pdata_(allocate_data(_cap))
    {
    }

    SharedBufferBase(const std::size_t _cap, const std::thread::id& _thr_id);

    SharedBufferBase(const SharedBufferBase& _other)
        : pdata_(_other ? &_other.pdata_->acquire() : _other.pdata_)
    {
    }

    SharedBufferBase(SharedBufferBase&& _other)
        : pdata_(_other.pdata_)
    {
        _other.pdata_ = &sentinel;
    }

    char* data() const
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
    explicit operator bool() const noexcept
    {
        return pdata_ != &sentinel;
    }
    ~SharedBufferBase()
    {
        reset();
    }

    std::size_t size() const
    {
        return pdata_->size_;
    }

    std::size_t capacity() const
    {
        return pdata_->capacity_;
    }

    std::size_t actualCapacity() const;

    auto makerThreadId() const
    {
        return pdata_->make_thread_id_;
    }

    bool empty() const
    {
        return pdata_->size_ == 0;
    }

    std::size_t useCount() const
    {
        return pdata_->use_count_.load();
    }

    size_t reset()
    {
        size_t previous_use_count = 0;
        if (*this) {
            auto buf = pdata_->release(previous_use_count);
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
    friend SharedBuffer make_shared_buffer(const std::size_t);

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

    SharedBuffer(SharedBuffer&& _other)
        : SharedBufferBase(std::move(_other))
    {
    }

    SharedBuffer(MutableSharedBuffer&& _other);

    ~SharedBuffer()
    {
        reset();
    }

    char* data() const
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

    SharedBuffer& operator=(SharedBuffer&& _other)
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

class ConstSharedBuffer;

//-----------------------------------------------------------------------------
// MutableSharedBuffer
//-----------------------------------------------------------------------------

class MutableSharedBuffer : public impl::SharedBufferBase {
    friend class ConstSharedBuffer;
    friend class BufferManager;
    friend MutableSharedBuffer make_mutable_buffer(const std::size_t);

    MutableSharedBuffer(const std::size_t _cap)
        : SharedBufferBase(_cap)
    {
    }

    MutableSharedBuffer(const std::size_t _cap, const std::thread::id& _thr_id)
        : SharedBufferBase(_cap, _thr_id)
    {
    }

    MutableSharedBuffer(ConstSharedBuffer&& _other);

    MutableSharedBuffer(SharedBuffer&& _other)
        : SharedBufferBase(std::move(_other))
    {
    }

public:
    MutableSharedBuffer() = default;

    MutableSharedBuffer(const MutableSharedBuffer& _other) = delete;

    MutableSharedBuffer(MutableSharedBuffer&& _other)
        : SharedBufferBase(std::move(_other))
    {
    }

    ~MutableSharedBuffer()
    {
        reset();
    }

    char* data()
    {
        return impl::SharedBufferBase::data();
    }

    void append(const std::size_t _size)
    {
        pdata_->size_ += _size;
    }

    void resize(const std::size_t _size = 0)
    {
        pdata_->size_ = _size;
    }

    MutableSharedBuffer& operator=(const MutableSharedBuffer& _other) = delete;

    MutableSharedBuffer& operator=(MutableSharedBuffer&& _other)
    {
        doMove(std::move(_other));
        return *this;
    }
    MutableSharedBuffer& operator=(SharedBuffer&& _other)
    {
        doMove(std::move(_other));
        return *this;
    }
};

inline MutableSharedBuffer make_mutable_buffer(const std::size_t _cap)
{
    return MutableSharedBuffer(make_shared_buffer(_cap));
}

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

    ConstSharedBuffer(ConstSharedBuffer&& _other)
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

    const char* data() const
    {
        return impl::SharedBufferBase::data();
    }

    MutableSharedBuffer collapse()
    {
        if (*this) {
            size_t previous_use_count = 0;
            auto   buf                = pdata_->release(previous_use_count);
            if (buf) {
                pdata_->acquire();
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

inline MutableSharedBuffer::MutableSharedBuffer(ConstSharedBuffer&& _other)
    : SharedBufferBase(std::move(_other))
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
    friend class impl::SharedBufferBase;
    struct Data;
    PimplT<Data>                         pimpl_;
    static char*                         release(impl::SharedBufferBase::Data* _pdata);
    static impl::SharedBufferBase::Data* allocate(const size_t _cap);

public:
    struct LocalData;

    struct Configuration {
        size_t default_local_max_count_ = 0;
    };

    static BufferManager& instance(const Configuration* _pconfig = nullptr);

    inline static SharedBuffer make(const size_t _cap)
    {
        return SharedBuffer{_cap, std::this_thread::get_id()};
    }

    inline static MutableSharedBuffer makeMutable(const size_t _cap)
    {
        return MutableSharedBuffer{make(_cap)};
    }

    static void   localMaxCount(const size_t _cap, const size_t _count);
    static size_t localMaxCount(const size_t _cap);
    static size_t localCount(const size_t _cap);

    static const Configuration& configuration();

private:
    BufferManager(const Configuration& _config);
    ~BufferManager();
};

} // namespace solid