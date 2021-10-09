// solid/utility/src/memoryfile.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <cerrno>
#include <cstring>

#include "solid/system/cassert.hpp"
#include "solid/utility/memoryfile.hpp"

namespace solid {

struct MemoryFile::BuffCmp {
    int operator()(const size_t& _k1, const MemoryFile::Buffer& _k2) const
    {
        if (_k1 < _k2.idx) {
            return -1;
        }
        if (_k2.idx < _k1) {
            return 1;
        }
        return 0;
    }
    int operator()(const MemoryFile::Buffer& _k1, const size_t& _k2) const
    {
        if (_k1.idx < _k2) {
            return -1;
        }
        if (_k2 < _k1.idx) {
            return 1;
        }
        return 0;
    }
};

/*static*/ int64_t MemoryFile::compute_capacity(int64_t _cp, Allocator& _ra)
{
    if (_cp <= 0 || _cp == std::numeric_limits<int64_t>::max()) {
        return std::numeric_limits<int64_t>::max();
    }
    _cp = _ra.computeCapacity(_cp);
    if (_cp <= 0 || _cp == std::numeric_limits<int64_t>::max()) {
        return std::numeric_limits<int64_t>::max();
    }
    return _cp;
}

MemoryFile::MemoryFile(
    const int64_t          _cp,
    MemoryFile::Allocator& _ra)
    : cp(compute_capacity(_cp, _ra))
    , sz(0)
    , off(0)
    , crtbuffidx(-1)
    , bufsz(_ra.bufferSize())
    , ra(_ra)
{
}

MemoryFile::~MemoryFile()
{
    for (BufferVectorT::const_iterator it(bv.begin()); it != bv.end(); ++it) {
        ra.release(it->data);
    }
}
int64_t MemoryFile::size() const
{
    return sz;
}
int64_t MemoryFile::capacity() const
{
    return cp;
}
ssize_t MemoryFile::read(char* _pb, size_t _bl)
{
    ssize_t rv(read(_pb, _bl, off));
    if (rv > 0) {
        off += rv;
    }
    return rv;
}

ssize_t MemoryFile::write(const char* _pb, uint32_t _bl)
{
    ssize_t rv(write(_pb, _bl, off));
    if (rv > 0) {
        off += rv;
    }
    return rv;
}

ssize_t MemoryFile::read(char* _pb, size_t _bl, int64_t _off)
{
    size_t  buffidx(static_cast<size_t>(_off / bufsz));
    size_t  buffoff(_off % bufsz);
    ssize_t rd(0);
    if (_off >= sz) {
        return -1;
    }
    if (static_cast<int64_t>(_off + _bl) > sz) {
        _bl = static_cast<uint32_t>(sz - _off);
    }
    while (_bl != 0u) {
        char*  bf(doGetBuffer(buffidx));
        size_t tocopy(bufsz - buffoff);
        if (tocopy > _bl) {
            tocopy = _bl;
        }

        if (bf == nullptr) {
            memset(_pb, '\0', tocopy);
        } else {
            memcpy(_pb, bf + buffoff, tocopy);
        }
        buffoff = 0;
        _pb += tocopy;
        rd += tocopy;
        _bl -= tocopy;
        buffoff = 0;
        if (_bl != 0u) {
            ++buffidx;
        }
    }
    if (_bl != 0 && rd == 0) {
        return -1;
    }
    return rd;
}

ssize_t MemoryFile::write(const char* _pb, size_t _bl, int64_t _off)
{
    size_t  buffidx(static_cast<size_t>(_off / bufsz));
    size_t  buffoff(_off % bufsz);
    ssize_t wd(0);
    while (_bl != 0u) {
        bool  created(false);
        char* bf(doCreateBuffer(buffidx, created));
        if (bf == nullptr) {
            break;
        }
        size_t tocopy(bufsz - buffoff);
        if (tocopy > _bl) {
            tocopy = _bl;
        }
        if (created) {
            memset(bf, '\0', buffoff);
            memset(bf + buffoff + tocopy, '\0', bufsz - buffoff - tocopy);
        }
        memcpy(bf + buffoff, _pb, tocopy);
        _pb += tocopy;
        wd += tocopy;
        _bl -= tocopy;
        buffoff = 0;
        if (_bl != 0u) {
            ++buffidx;
        }
    }
    if (_bl != 0u && wd == 0) {
        errno = ENOSPC;
        return -1;
    }

    if (sz < (_off + wd)) {
        sz = _off + wd;
    }
    return wd;
}

int64_t MemoryFile::seek(int64_t _pos, SeekRef _ref)
{
    switch (_ref) {
    case SeekBeg:
        if (_pos >= cp) {
            return -1;
        }
        return off = _pos;
    case SeekCur:
        if (off + _pos > cp) {
            return -1;
        }
        off += _pos;
        return off;
    case SeekEnd:
        if (sz + _pos > cp) {
            return -1;
        }
        off = sz + _pos;
        return off;
    }
    return -1;
}

int MemoryFile::truncate(int64_t _len)
{
    //TODO:
    solid_assert_log(_len == 0, generic_logger);
    sz         = 0;
    off        = 0;
    crtbuffidx = -1;
    for (BufferVectorT::const_iterator it(bv.begin()); it != bv.end(); ++it) {
        ra.release(it->data);
    }
    bv.clear();
    return -1;
}
inline binary_search_result_t MemoryFile::doFindBuffer(const size_t _idx) const
{
    return solid::binary_search(bv.begin(), bv.end(), _idx, BuffCmp());
}
inline char* MemoryFile::doGetBuffer(const size_t _idx) const
{
    binary_search_result_t pos(doLocateBuffer(_idx));
    if (pos.first) {
        return bv[pos.second].data;
    }
    return nullptr;
}

char* MemoryFile::doCreateBuffer(const size_t _idx, bool& _created)
{
    binary_search_result_t pos(doLocateBuffer(_idx));
    if (pos.first) { //found buffer, return the data
        return bv[pos.second].data;
    }
    //buffer not found
    //see if we did not reach the capacity
    if (static_cast<int64_t>(static_cast<int64_t>(bv.size()) * bufsz + bufsz) > cp) {
        return nullptr;
    }
    _created = true;
    char* b  = ra.allocate();
    bv.insert(bv.begin() + pos.second, Buffer(_idx, b));
    return b;
}

binary_search_result_t MemoryFile::doLocateBuffer(const size_t _idx) const
{
    if (bv.empty() || _idx > bv.back().idx) { //append
        crtbuffidx = bv.size();
        return binary_search_result_t(false, bv.size());
    }
    //see if it's arround the current buffer:
    if (crtbuffidx < bv.size()) {
        if (bv[crtbuffidx].idx == _idx) {
            return binary_search_result_t(true, crtbuffidx);
        }
        //see if its the next buffer:
        size_t nextidx(crtbuffidx + 1);
        if (nextidx < bv.size() && bv[nextidx].idx == _idx) {
            crtbuffidx = nextidx;
            return binary_search_result_t(true, crtbuffidx);
        }
    }
    binary_search_result_t pos = doFindBuffer(_idx);
    crtbuffidx                 = pos.second;
    return pos;
}

} //namespace solid
