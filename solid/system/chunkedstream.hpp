// solid/system/chunkedstream.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <cstring>
#include <istream>
#include <memory>
#include <ostream>

namespace solid {

template <size_t NodeSize, size_t BundledNodeSize>
class ChunkedBuffer;

template <size_t NodeSize>
class ChunkedBufferBase : public std::streambuf {
protected:
    template <size_t Size>
    struct BufNode {
        static constexpr const size_t buffer_capacity = Size - sizeof(std::unique_ptr<BufNode>);

        char                     buf_[buffer_capacity];
        std::unique_ptr<BufNode> pnext_;

        char* begin()
        {
            return &buf_[0];
        }

        const char* begin() const
        {
            return &buf_[0];
        }
        const char* end() const
        {
            return &buf_[buffer_capacity];
        }

        char* end()
        {
            return &buf_[buffer_capacity];
        }

        BufNode()
            : pnext_(nullptr)
        {
        }
    };

    using BufNodeT = BufNode<NodeSize>;

    BufNodeT*       plast_;
    char*           pcrt_;
    const char*     pend_;
    std::streamsize size_;
    BufNodeT*       pinode_;
    std::streamsize isize_;

private:
    void reset()
    {
        plast_  = nullptr;
        pcrt_   = nullptr;
        pend_   = nullptr;
        size_   = 0;
        pinode_ = nullptr;
        isize_  = 0;
    }

protected:
    ChunkedBufferBase()
        : plast_(nullptr)
        , pcrt_(nullptr)
        , pend_(nullptr)
        , size_(0)
        , pinode_(nullptr)
        , isize_(0)
    {
    }

    ChunkedBufferBase(ChunkedBufferBase&& _cbb)
        : plast_(_cbb.plast_)
        , pcrt_(_cbb.pcrt_)
        , pend_(_cbb.pend_)
        , size_(_cbb.size_)
        , pinode_(_cbb.pinode_)
        , isize_(0)
    {
        _cbb.reset();
    }

    bool firstINode(BufNodeT* _pbn)
    {
        pinode_ = _pbn;
        isize_  = 0;
        if (pinode_ != nullptr) {
            std::streamsize sz = pinode_->buffer_capacity;
            if (sz > size_) {
                sz = size_;
            }

            setg(pinode_->begin(), pinode_->begin(), pinode_->begin() + sz);
            return true;
        } else {
            setg(nullptr, nullptr, nullptr);
            return false;
        }
    }

    // do not call this on base destructor
    void clear()
    {
        BufNodeT* pbn = first();
        if (pbn != nullptr) {
            std::unique_ptr<BufNodeT> pup = std::move(pbn->pnext_);

            while (pup) {
                std::unique_ptr<BufNodeT> pupn = std::move(pup->pnext_);
                pup                            = std::move(pupn);
            }
        }
    }

private:
    virtual BufNodeT*       allocateFirst() = 0;
    virtual BufNodeT*       first()         = 0;
    virtual const BufNodeT* first() const   = 0;

    void allocate()
    {
        if (plast_ != nullptr) {
            plast_->pnext_ = std::make_unique<BufNodeT>();
            plast_         = plast_->pnext_.get();
            pcrt_          = plast_->buf_;
            pend_          = plast_->end();
        } else {
            plast_ = allocateFirst();
            pcrt_  = plast_->buf_;
            pend_  = plast_->end();
        }
    }

    // write one character
    int_type overflow(int_type c) override
    {
        if (c != EOF) {
            if (pcrt_ == pend_) {
                allocate();
            }
            *pcrt_ = c;
            ++pcrt_;
            ++size_;
        }
        return c;
    }

    // write multiple characters
    std::streamsize xsputn(const char* s, std::streamsize num) override
    {
        std::streamsize sz = num;
        while (sz) {
            if (pcrt_ == pend_) {
                allocate();
            }
            std::streamsize towrite = pend_ - pcrt_;
            if (towrite > sz)
                towrite = sz;
            memcpy(pcrt_, s, towrite);
            s += towrite;
            sz -= towrite;
            pcrt_ += towrite;
        }
        size_ += num;
        return num;
    }

    bool nextINode()
    {
        isize_ += pinode_->buffer_capacity;
        if (isize_ > size_) {
            isize_ = size_;
        }
        pinode_ = pinode_->pnext_.get();
        if (pinode_) {
            std::streamsize sz = pinode_->buffer_capacity;

            if (sz > (size_ - isize_)) {
                sz = (size_ - isize_);
            }

            setg(pinode_->begin(), pinode_->begin(), pinode_->begin() + sz);
            return true;
        } else {
            setg(nullptr, nullptr, nullptr);
            return false;
        }
    }

    int_type underflow() override
    {
        if (gptr() < egptr()) { // buffer not exhausted
            return traits_type::to_int_type(*gptr());
        }

        if (nextINode()) {
            return traits_type::to_int_type(*gptr());
        }
        return traits_type::eof();
    }

    std::streamsize xsgetn(char_type* _s, std::streamsize _n) override
    {
        std::streamsize isz = 0;

        if (gptr() == egptr()) {
            nextINode();
        }

        while (_n != 0 && gptr() < egptr()) {

            std::streamsize sz = egptr() - gptr();
            if (sz > _n) {
                sz = _n;
            }
            memcpy(_s, gptr(), static_cast<size_t>(sz));
            gbump(static_cast<int>(sz));

            isz += sz;
            _n -= sz;
            _s += sz;

            if (gptr() == egptr()) {
                nextINode();
            }
        }

        return isz;
    }

public:
    std::ostream& writeTo(std::ostream& _ros) const
    {
        const BufNodeT* pbn = first();
        do {
            if (pbn == plast_) {
                _ros.write(pbn->buf_, pcrt_ - pbn->begin());
            } else {
                _ros.write(pbn->buf_, BufNodeT::buffer_capacity);
            }
            pbn = pbn->pnext_.get();
        } while (pbn);
        return _ros;
    }

    std::streamsize size() const
    {
        return size_;
    }

    template <class F>
    void visit(F _f) const
    {
        const BufNodeT* pbn = first();
        do {
            size_t sz;
            if (pbn == plast_) {
                sz = pcrt_ - pbn->begin();
            } else {
                sz = BufNodeT::buffer_capacity;
            }

            _f(pbn->buf_, sz);

            pbn = pbn->pnext_.get();
        } while (pbn != nullptr);
    }
};

template <size_t NodeSize>
class ChunkedBuffer<NodeSize, 0> : public ChunkedBufferBase<NodeSize> {
    using BaseT    = ChunkedBufferBase<NodeSize>;
    using BufNodeT = typename BaseT::BufNodeT;

    std::unique_ptr<BufNodeT> first_;

    BufNodeT* allocateFirst() override
    {
        first_ = std::make_unique<BufNodeT>();
        return first_.get();
    }
    BufNodeT* first() override
    {
        return first_.get();
    }

    const BufNodeT* first() const override
    {
        return first_.get();
    }

public:
    ChunkedBuffer() {}

    ChunkedBuffer(ChunkedBuffer&& _cb)
        : BaseT(std::move(_cb))
        , first_(std::move(_cb.first_))
    {
        BaseT::firstINode(first_.get());
    }

    ~ChunkedBuffer()
    {
        BaseT::clear();
    }
};

template <size_t NodeSize>
class ChunkedBuffer<NodeSize, NodeSize> : public ChunkedBufferBase<NodeSize> {
    using BaseT    = ChunkedBufferBase<NodeSize>;
    using BufNodeT = typename BaseT::BufNodeT;

    BufNodeT first_;

    BufNodeT* allocateFirst() override
    {
        return &first_;
    }
    BufNodeT* first() override
    {
        return &first_;
    }

    const BufNodeT* first() const override
    {
        return &first_;
    }

public:
    ~ChunkedBuffer()
    {
        BaseT::clear();
    }
};

template <size_t NodeSize, size_t BundledNodeSize = 0>
class IChunkedStream;

template <size_t NodeSize, size_t BundledNodeSize = 0>
class OChunkedStream : public std::ostream {
    friend class IChunkedStream<NodeSize, BundledNodeSize>;
    ChunkedBuffer<NodeSize, BundledNodeSize> buf_;

public:
    OChunkedStream()
        : std::ostream(nullptr)
    {
        rdbuf(&buf_);
    }

    std::ostream& writeTo(std::ostream& _ros) const
    {
        return buf_.writeTo(_ros);
    }
    size_t size() const
    {
        return buf_.size();
    }

    template <class F>
    void visit(F _f) const
    {
        buf_.visit(_f);
    }
};

template <size_t NodeSize, size_t BundledNodeSize>
class IChunkedStream : public std::istream {
    ChunkedBuffer<NodeSize, BundledNodeSize> buf_;

public:
    IChunkedStream()
        : std::istream(nullptr)
    {
        rdbuf(&buf_);
    }
    explicit IChunkedStream(OChunkedStream<NodeSize, BundledNodeSize>&& _ocs)
        : std::istream(nullptr)
        , buf_(std::move(_ocs.buf_))
    {
        rdbuf(&buf_);
    }

    IChunkedStream& operator=(OChunkedStream<NodeSize, BundledNodeSize>&& _ocs)
    {
        buf_ = std::move(_ocs.buf_);
        return *this;
    }
};

} // namespace solid
