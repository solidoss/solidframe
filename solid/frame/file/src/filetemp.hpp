// solid/frame/file/filetemp.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_FILE_TEMP_HPP
#define SOLID_FRAME_FILE_TEMP_HPP

#include "solid/system/common.hpp"
#include "solid/system/filedevice.hpp"

#include "solid/frame/file/tempbase.hpp"
#include "solid/utility/memoryfile.hpp"

namespace solid {
namespace frame {
namespace file {

struct TempFile : TempBase {
    TempFile(
        size_t   _storageid,
        size_t   _id,
        uint64_t _size); //:TempBase(_storageid, _id, _size){
private:
    /*virtual*/ ~TempFile();

    /*virtual*/ bool open(const char* _path, const size_t _openflags, bool _remove, ErrorCodeT& _rerr);
    /*virtual*/ void close(const char* _path, bool _remove);
    /*virtual*/ int read(char* _pb, uint32_t _bl, int64_t _off);
    /*virtual*/ int write(const char* _pb, uint32_t _bl, int64_t _off);
    /*virtual*/ int64_t size() const;

    /*virtual*/ bool truncate(int64_t _len = 0);
    /*virtual*/ void flush();

private:
    FileDevice fd;
};

struct TempMemory : TempBase {
    TempMemory(
        size_t   _storageid,
        size_t   _id,
        uint64_t _size); //:TempBase(_storageid, _id, _size){
private:
    /*virtual*/ ~TempMemory();

    /*virtual*/ bool open(const char* _path, const size_t _openflags, bool _remove, ErrorCodeT& _rerr);
    /*virtual*/ void close(const char* _path, bool _remove);
    /*virtual*/ int read(char* _pb, uint32_t _bl, int64_t _off);
    /*virtual*/ int write(const char* _pb, uint32_t _bl, int64_t _off);
    /*virtual*/ int64_t size() const;

    /*virtual*/ bool truncate(int64_t _len = 0);

private:
    MemoryFile mf;
};

} //namespace file
} //namespace frame
} //namespace solid

#endif
