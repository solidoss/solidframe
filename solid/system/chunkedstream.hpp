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
#include <memory>
#include <ostream>

namespace solid {

template <size_t Size>
class OChunkedBuffer : public std::streambuf {
    struct BufNode {
        static constexpr const size_t buffer_capacity = Size - sizeof(std::unique_ptr<BufNode>);

        char buf_[buffer_capacity];

        const char* begin() const
        {
            return &buf_[0];
        }
        const char* end() const
        {
            return &buf_[buffer_capacity];
        }

        std::unique_ptr<BufNode> pnext_;

        BufNode()
            : pnext_(nullptr)
        {
        }

    } first_;
    BufNode*        plast_;
    char*           pcrt_;
    const char*     pend_;
    std::streamsize size_;

private:
    void allocate()
    {
        plast_->pnext_.reset(new BufNode);
        plast_ = plast_->pnext_.get();
        pcrt_  = plast_->buf_;
        pend_  = plast_->end();
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

public:
    OChunkedBuffer()
        : plast_(&first_)
        , pcrt_(first_.buf_)
        , pend_(first_.end())
        , size_(0)
    {
        static_assert(sizeof(BufNode) == Size, "sizeof(BufNode) not equal to Size");
    }

    std::ostream& writeTo(std::ostream& _ros) const
    {
        const BufNode* pbn = &first_;
        do {
            if (pbn == plast_) {
                _ros.write(pbn->buf_, pcrt_ - pbn->begin());
            } else {
                _ros.write(pbn->buf_, BufNode::buffer_capacity);
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
        const BufNode* pbn = &first_;
        do {
            size_t sz;
            if (pbn == plast_) {
                sz = pcrt_ - pbn->begin();
            } else {
                sz = BufNode::buffer_capacity;
            }

            _f(pbn->buf_, sz);

            pbn = pbn->pnext_.get();
        } while (pbn != nullptr);
    }
};

template <size_t Size>
class OChunkedStream : public std::ostream {
protected:
    OChunkedBuffer<Size> buf_;

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

} //namespace solid
