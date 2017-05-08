// solid/utility/memoryfile.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/common.hpp"
#include "solid/utility/algorithm.hpp"
#include "solid/utility/common.hpp"
#include <deque>
#include <limits>

namespace solid {

//! Store in memory files
/*!
    It has the same interface like FileDevice.
    It has support for writing only at different offsets.
    It uses the given allocator for allocating/releasing data buffers.

*/
class MemoryFile {
public:
    struct Allocator {
        virtual ~Allocator() {}
        virtual uint     bufferSize() const            = 0;
        virtual char*    allocate()                    = 0;
        virtual void     release(char*)                = 0;
        virtual uint64_t computeCapacity(uint64_t _cp) = 0;
    };
    template <uint Sz = 4096>
    struct BasicAllocator : Allocator {
        static BasicAllocator<Sz>& instance()
        {
            static BasicAllocator<Sz> ba;
            return ba;
        }
        /*virtual*/ uint  bufferSize() const { return Sz; }
        /*virtual*/ char* allocate()
        {
            return new char[Sz];
        }
        /*virtual*/ void release(char* _p)
        {
            delete[] _p;
        }
        /*virtual*/ uint64_t computeCapacity(uint64_t _cp)
        {
            return ((_cp / Sz) + 1) * Sz;
        }
    };

public:
    //! Constructor with the file capacity and the allocator
    MemoryFile(const int64_t _cp = 0, Allocator& _ral = BasicAllocator<>::instance());
    //! Destructor
    ~MemoryFile();
    //! Read data from file from offset
    int read(char* _pb, uint32_t _bl, int64_t _off);
    //! Write data to file at offset
    int write(const char* _pb, uint32_t _bl, int64_t _off);
    //! Read data from file
    int read(char* _pb, uint32_t _bl);
    //! Write data to file
    int write(const char* _pb, uint32_t _bl);
    //! Move the file cursor at position
    int64_t seek(int64_t _pos, SeekRef _ref = SeekBeg);
    //! Truncate the file
    int truncate(int64_t _len = 0);
    //! Return the size of the file
    int64_t size() const;
    int64_t capacity() const;

private:
    binary_search_result_t doFindBuffer(uint32_t _idx) const;
    binary_search_result_t doLocateBuffer(uint32_t _idx) const;
    char* doGetBuffer(uint32_t _idx) const;
    char* doCreateBuffer(uint32_t _idx, bool& _created);

private:
    struct Buffer {
        Buffer(uint32_t _idx = 0, char* _data = nullptr)
            : idx(_idx)
            , data(_data)
        {
        }
        uint32_t idx;
        char*    data;
    };

    static int64_t compute_capacity(const int64_t _cp, Allocator& _ral);

    struct BuffCmp;
    friend struct BuffCmp;
    typedef std::deque<Buffer> BufferVectorT;

    const int64_t    cp;
    int64_t          sz;
    int64_t          off;
    mutable uint32_t crtbuffidx;
    const uint32_t   bufsz;
    Allocator&       ra;
    BufferVectorT    bv;
};

} //namespace solid
