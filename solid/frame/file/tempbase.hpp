// solid/frame/file/tempbase.hpp
//
// Copyright (c) 2010 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/common.hpp"
#include "solid/system/error.hpp"

namespace solid {
namespace frame {
namespace file {

struct TempBase {
    const size_t   tempstorageid;
    const uint32_t tempid;
    const uint64_t tempsize;

    TempBase(
        size_t   _storageid,
        uint32_t _id,
        uint64_t _size)
        : tempstorageid(_storageid)
        , tempid(_id)
        , tempsize(_size)
    {
    }

    virtual ~TempBase();

    virtual bool open(
        const char*  _path,
        const size_t _openflags,
        bool         _remove,
        ErrorCodeT&  _rerr)
        = 0;

    virtual void close(const char* _path, bool _remove) = 0;

    virtual ssize_t read(char* _pb, size_t _bl, int64_t _off)        = 0;
    virtual ssize_t write(const char* _pb, size_t _bl, int64_t _off) = 0;
    virtual int64_t size() const                                     = 0;
    virtual bool    truncate(int64_t _len = 0)                       = 0;
    virtual void    flush();
};

} //namespace file
} //namespace frame
} //namespace solid
