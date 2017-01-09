// utility/istream.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_ISTREAM_HPP
#define UTILITY_ISTREAM_HPP

#include "utility/stream.hpp"
#include "utility/common.hpp"

namespace solid{

//! A stream for reading 
class InputStream: virtual public Stream{
public:
    virtual ~InputStream();
    virtual int read(char *, uint32_t, uint32_t _flags = 0) = 0;
    virtual int read(uint64_t _offset, char*, uint32_t, uint32_t _flags = 0);
    bool readAll(char *, uint32_t, uint32_t _flags = 0);
    bool iok()const;
    bool ieof()const;
    bool ibad()const;
    bool ifail()const;
};

//! An InputStreamIterator - an offset within the stream: a pointer to an istream
struct InputStreamIterator{
    InputStreamIterator(InputStream *_ps = NULL, int64_t _off = 0);
    void reinit(InputStream *_ps = NULL, int64_t _off = 0);
    int64_t start();
    InputStream* operator->() const{return ps;}
    InputStream& operator*() {return *ps;}
    InputStream     *ps;
    int64_t     off;
};

#ifndef SOLID_HAS_NO_INLINES
#include "utility/istream.ipp"
#endif

}//namespace solid

#endif
