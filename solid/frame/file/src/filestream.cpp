// solid/frame/file/src/filestore.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/frame/file/filestream.hpp"
#include "solid/system/log.hpp"

using namespace std;

namespace solid {
namespace frame {
namespace file {

extern const LoggerT logger;

FileBuf::FileBuf(
    FilePointerT& _rptr,
    char* _buf, size_t _bufcp)
    : dev(_rptr)
    , buf(_buf)
    , bufcp(_bufcp)
    , off(0)
{
    if (_bufcp != 0u) {
        setp(nullptr, nullptr);
        resetGet();
    }
}
FileBuf::FileBuf(
    char* _buf, size_t _bufcp)
    : buf(_buf)
    , bufcp(_bufcp)
    , off(0)
{
    if (_bufcp != 0u) {
        setp(nullptr, nullptr);
        resetGet();
    }
}
FileBuf::~FileBuf()
{
    solid_dbg(logger, Info, "");
    if (dev.get() != nullptr) {
        sync();
        dev.clear();
    }
}

FilePointerT& FileBuf::device()
{
    solid_dbg(logger, Info, "");
    return dev;
}
void FileBuf::device(FilePointerT& _rptr)
{
    solid_dbg(logger, Info, "");
    dev = _rptr;
}

/*virtual*/ streamsize FileBuf::showmanyc()
{
    solid_dbg(logger, Info, "");
    return 0;
}

/*virtual*/ FileBuf::int_type FileBuf::underflow()
{
    //solid_dbg(logger, Info, "");
    if (hasBuf()) {
        if (hasPut()) {
            solid_assert(false);
            if (!flushPut()) {
                return 0;
            }
        }
        if (gptr() < egptr()) { // buffer not exhausted
            return traits_type::to_int_type(*gptr());
        }
        //refill rbuf
        solid_dbg(logger, Info, "read " << bufcp << " from " << off);
        ssize_t rv = dev->read(buf, bufcp, off);
        if (rv > 0) {
            char* end = buf + rv;
            setg(buf, buf, end);
            //rbufsz = rv;
            return traits_type::to_int_type(*gptr());
        }
    } else {
        //very inneficient
        char    c;
        ssize_t rv = dev->read(&c, 1, off);
        if (rv == 1) {
            return traits_type::to_int_type(c);
        }
    }
    //rbufsz = 0;
    return traits_type::eof();
}

/*virtual*/ FileBuf::int_type FileBuf::uflow()
{
    //solid_dbg(logger, Info, "");
    if (hasBuf()) {
        return streambuf::uflow();
    }
    //very inneficient
    char          c;
    const ssize_t rv = dev->read(&c, 1, off);
    if (rv == 1) {
        ++off;
        return traits_type::to_int_type(c);
    }
    return traits_type::eof();
}

/*virtual*/ FileBuf::int_type FileBuf::pbackfail(int_type _c)
{
    solid_dbg(logger, Info, "" << _c);
    return traits_type::eof();
}

ssize_t FileBuf::writeAll(const char* _s, size_t _n)
{
    int     wcnt = 0;
    ssize_t rv;
    do {
        rv = dev->write(_s, _n, off);
        if (rv > 0) {
            _s += rv;
            _n -= rv;
            wcnt += rv;
        } else {
            wcnt = 0;
        }
    } while (rv > 0 && _n != 0);
    return wcnt;
}

/*virtual*/ FileBuf::int_type FileBuf::overflow(int_type _c)
{
    solid_dbg(logger, Info, "" << _c << " off = " << off);
    if (hasBuf()) {
        if (pptr() == nullptr) {
            if (hasGet()) {
                off += (gptr() - buf);
                resetGet();
            }
            resetPut();
            if (_c != traits_type::eof()) {
                *pptr() = _c;
                pbump(1);
            }
            return traits_type::to_int_type(_c);
        }

        char* endp = pptr();
        if (_c != traits_type::eof()) {
            *endp = _c;
            ++endp;
        }

        size_t  towrite = endp - pbase();
        ssize_t rv      = writeAll(pbase(), towrite);
        if (static_cast<size_t>(rv) == towrite) {
            off += towrite;
            resetPut();
        } else {
            setp(nullptr, nullptr);
        }
    } else {
        const char    c  = _c;
        const ssize_t rv = dev->write(&c, 1, off);
        if (rv == 1) {
            return traits_type::to_int_type(c);
        }
    }
    return traits_type::eof();
}

/*virtual*/ FileBuf::pos_type FileBuf::seekoff(
    off_type _off, ios_base::seekdir _way,
    ios_base::openmode _mode)
{
    solid_dbg(logger, Info, "seekoff = " << _off << " way = " << _way << " mode = " << _mode << " off = " << off);
    if (hasBuf()) {
        if (hasPut()) {
            solid_assert(!hasGet());
            if (!flushPut()) {
                return pos_type(-1);
            }
            setp(nullptr, nullptr);
            resetGet();
        }
        int64_t newoff = 0;
        if (_way == ios_base::beg) {
        } else if (_way == ios_base::end) {
            newoff        = off + (pptr() - pbase());
            int64_t devsz = dev->size();
            if (newoff <= devsz) {
                newoff = devsz;
            }
        } else if (_off == 0) { //cur
            return off + (gptr() - eback());
        } else {
            newoff = off;
            newoff += (gptr() - eback());
        }
        newoff += _off;
        ssize_t bufoff = egptr() - eback();
        if (newoff >= off && newoff < (off + bufoff)) {
            ssize_t newbufoff = static_cast<ssize_t>(newoff - off);
            setg(buf, buf + newbufoff, egptr());
            newoff = off + newbufoff;
        } else {
            off = newoff;
            setp(nullptr, nullptr);
            resetGet();
        }
        return newoff;
    }
    if (_way == ios_base::beg) {
        off = _off;
    } else if (_way == ios_base::end) {
        off = dev->size() + _off;
    } else { //cur
        off += _off;
    }
    return off;
}

/*virtual*/ FileBuf::pos_type FileBuf::seekpos(
    pos_type           _pos,
    ios_base::openmode _mode)
{
    solid_dbg(logger, Info, "" << _pos);
    return seekoff(_pos, std::ios_base::beg, _mode);
}

/*virtual*/ int FileBuf::sync()
{
    solid_dbg(logger, Info, "");
    if (hasPut()) {
        flushPut();
    }
    dev->flush();
    return 0;
}

// /*virtual*/ void FileBuf::imbue(const locale& _loc){
//
// }

/*virtual*/ streamsize FileBuf::xsgetn(char_type* _s, streamsize _n)
{
    solid_dbg(logger, Info, "" << _n << " off = " << off);
    if (hasBuf()) {
        if (hasPut()) {
            if (!flushPut()) {
                return 0;
            }
            setp(nullptr, nullptr);
            resetGet();
        }

        if (gptr() < egptr()) { // buffer not exhausted
            streamsize sz = egptr() - gptr();
            if (sz > _n) {
                sz = _n;
            }
            memcpy(_s, gptr(), static_cast<size_t>(sz));
            gbump(static_cast<int>(sz));
            return sz;
        }
        //read directly in the given buffer
        ssize_t rv = dev->read(_s, static_cast<size_t>(_n), off);
        if (rv > 0) {
            off += rv;
            resetGet();
            return rv;
        }
    } else {
        const ssize_t rv = dev->read(_s, static_cast<size_t>(_n), off);
        if (rv > 0) {
            off += rv;
            return rv;
        }
    }
    //rbufsz = 0;
    return 0;
}

bool FileBuf::flushPut()
{
    if (pptr() != epptr()) {
        size_t towrite = pptr() - pbase();
        solid_dbg(logger, Info, "" << towrite << " off = " << off);
        ssize_t rv = writeAll(buf, towrite);
        if (static_cast<size_t>(rv) == towrite) {
            off += towrite;
            resetBoth();
        } else {
            setp(nullptr, nullptr);
            return false;
        }
    }
    return true;
}

/*virtual*/ streamsize FileBuf::xsputn(const char_type* _s, streamsize _n)
{
    solid_dbg(logger, Info, "" << _n << " off = " << off);
    if (hasBuf()) {
        //NOTE: it should work with the following line too
        //return streambuf::xsputn(_s, _n);

        if (pptr() == nullptr) {
            if (hasGet()) {
                off += (gptr() - buf);
                resetGet();
            }
            resetPut();
        }

        const streamsize sz      = _n;
        const size_t     wleftsz = bufcp - (pptr() - pbase());
        size_t           towrite = static_cast<size_t>(_n);

        if (wleftsz < towrite) {
            towrite = wleftsz;
        }
        memcpy(pptr(), _s, towrite);
        _n -= towrite;
        _s += towrite;
        pbump(static_cast<int>(towrite));

        if (_n != 0 || towrite == wleftsz) {
            if (!flushPut()) {
                return 0;
            }
            if (_n != 0 && static_cast<size_t>(_n) <= bufcp / 2) {
                memcpy(pptr(), _s, static_cast<size_t>(_n));
                pbump(static_cast<int>(_n));
            } else if (_n != 0) {
                size_t  towrite = static_cast<size_t>(_n);
                ssize_t rv      = writeAll(_s, towrite);
                if (static_cast<size_t>(rv) == towrite) {
                    off += rv;
                    resetPut();
                } else {
                    return 0;
                }
            } else {
                resetPut();
            }
        }
        return sz;
    }

    const ssize_t rv = dev->write(_s, static_cast<size_t>(_n), off);
    if (rv > 0) {
        off += rv;
        return rv;
    }
    return 0;
}

} //namespace file
} //namespace frame
} //namespace solid
