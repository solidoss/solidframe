// example_file_open_pool.cpp
//
// Copyright (c) 2007, 2008, 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/system/filedevice.hpp"
#include "solid/system/memory.hpp"
#include "solid/utility/workpool.hpp"
#include <cerrno>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>

using namespace std;
using namespace solid;

namespace {

template <class T>
static T align(T _v, unsigned long _by);

template <class T>
static T* align(T* _v, const unsigned long _by)
{
    if ((size_t)_v % _by) {
        return _v + (_by - ((size_t)_v % _by));
    } else {
        return _v;
    }
}

template <class T>
static T align(T _v, const unsigned long _by)
{
    if (_v % _by) {
        return _v + (_by - (_v % _by));
    } else {
        return _v;
    }
}

const uint32_t pagesize = memory_page_size();

using FileDeuqeT = std::deque<FileDevice>;

struct Context {
    Context()
        : readsz(4 * pagesize)
        , bf(new char[readsz + pagesize])
        , buf(align(bf, pagesize))
    {
    }
    ~Context()
    {
        delete[] bf;
    }

    const unsigned long readsz;
    char*               bf;
    char*               buf;
};

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 4) {
        cout << "./file_open_pool /path/to/folder file-count folder-count" << endl;
        return 0;
    }
    // char c;
    char name[1024];
    int  filecnt   = atoi(argv[2]);
    int  foldercnt = atoi(argv[3]);
    sprintf(name, "%s", argv[1]);
    char*      fldname = name + strlen(argv[1]);
    char*      fname   = name + strlen(argv[1]) + 1 + 8;
    FileDeuqeT fdq;
    int        cnt   = 0;
    uint64_t   totsz = 0;
    for (int i = foldercnt; i; --i) {
        sprintf(fldname, "/%08u", i);
        *fname = 0;
        for (int j = filecnt; j; --j) {
            sprintf(fname, "/%08u.txt", j);
            ++cnt;
            fdq.push_back(FileDevice());
            if (fdq.back().open(name, FileDevice::ReadWriteE)) {
                cout << "error " << strerror(errno) << " " << cnt << endl;
                cout << "failed to open file " << name << endl;
                return 0;
            } else {
                // cout<<"name = "<<name<<" size = "<<fdq.back().size()<<endl;
                totsz += fdq.back().size();
            }
        }
    }
    cout << "fdq size = " << fdq.size() << " total size " << totsz << endl;
    // return 0;

    using WorkPoolT = lockfree::WorkPool<FileDevice*, void>;

    WorkPoolT wp{
        solid::WorkPoolConfiguration(),
        [](FileDevice* _pfile, Context&& _rctx) {
            int64_t sz = _pfile->size();
            int     toread;
            int     cnt = 0;
            while (sz > 0) {
                toread = _rctx.readsz;
                if (toread > sz)
                    toread = sz;
                int rv = _pfile->read(_rctx.buf, toread);
                cnt += rv;
                sz -= rv;
            }
        },
        Context()};
    for (FileDeuqeT::iterator it(fdq.begin()); it != fdq.end(); ++it) {
        wp.push(&(*it));
    }
    return 0;
}
