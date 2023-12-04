#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

#include "solid/system/common.hpp"
#include "solid/system/pimpl.hpp"

namespace solid {

class SharedBuffer {
    struct Data {
        std::atomic<std::size_t> use_count_;
        std::thread::id          make_thread_id_;
        Data*                    pnext_    = nullptr;
        std::size_t              size_     = 0;
        std::size_t              capacity_ = 0;
        char*                    buffer_;
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

        char* release();

        char* data()
        {
            return data_;
        }

        std::size_t capacity() const
        {
            return capacity_ + (data_ - buffer_);
        }
    };

    static Data sentinel;

    Data* pdata_;

    friend SharedBuffer make_shared_buffer(const std::size_t);
    friend class BufferManager;

    static Data* allocate_data(const std::size_t _cap);

    SharedBuffer(const std::size_t _cap)
        : pdata_(allocate_data(_cap))
    {
    }

    SharedBuffer(const std::size_t _cap, const std::thread::id& _thr_id);

public:
    explicit operator bool() const noexcept
    {
        return pdata_ != &sentinel;
    }

    SharedBuffer()
        : pdata_(&sentinel)
    {
    }
    SharedBuffer(const SharedBuffer& _other)
        : pdata_(_other ? &_other.pdata_->acquire() : _other.pdata_)
    {
    }
    SharedBuffer(SharedBuffer&& _other)
        : pdata_(_other.pdata_)
    {
        _other.pdata_ = &sentinel;
    }

    ~SharedBuffer()
    {
        reset();
    }

    char* data() const
    {
        return pdata_->data();
    }

    std::size_t size() const
    {
        return pdata_->size_;
    }

    std::size_t capacity() const
    {
        return pdata_->capacity_;
    }

    auto makerThreadId() const
    {
        return pdata_->make_thread_id_;
    }

    void append(const std::size_t _size)
    {
        pdata_->size_ += _size;
    }

    void resize(const std::size_t _size = 0)
    {
        pdata_->size_ = _size;
    }

    bool empty() const
    {
        return pdata_->size_ == 0;
    }

    std::size_t useCount() const
    {
        return pdata_->use_count_.load();
    }

    void reset()
    {
        if (*this) {
            auto buf = pdata_->release();
            delete[] buf;
            pdata_ = &sentinel;
        }
    }

    bool resurrect()
    {
        if (*this) {
            auto buf = pdata_->release();
            if (buf) {
                pdata_->acquire();
                return true;
            } else {
                pdata_ = &sentinel;
            }
        }
        return false;
    }

    SharedBuffer& operator=(const SharedBuffer& _other)
    {
        if (pdata_ != _other.pdata_) {
            reset();

            if (_other) {
                pdata_ = &_other.pdata_->acquire();
            }
        }
        return *this;
    }

    SharedBuffer& operator=(SharedBuffer&& _other)
    {
        if (pdata_ != _other.pdata_) {
            reset();
            pdata_        = _other.pdata_;
            _other.pdata_ = &sentinel;
        }
        return *this;
    }
};

inline SharedBuffer make_shared_buffer(const std::size_t _cap)
{
    return SharedBuffer(_cap);
}

class BufferManager : NonCopyable {
    friend class SharedBuffer;
    struct Data;
    PimplT<Data>               pimpl_;
    static char*               release(SharedBuffer::Data* _pdata);
    static SharedBuffer::Data* allocate(const size_t _cap);

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

    static void   localMaxCount(const size_t _cap, const size_t _count);
    static size_t localMaxCount(const size_t _cap);
    static size_t localCount(const size_t _cap);

    static const Configuration& configuration();

private:
    BufferManager(const Configuration& _config);
    ~BufferManager();
};

} // namespace solid