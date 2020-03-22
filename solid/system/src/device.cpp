// solid/system/src/device.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/system/common.hpp"

#ifdef SOLID_ON_WINDOWS
#define NOMINMAX
#include <WinSock2.h>
#include <Windows.h>

#include <Mstcpip.h>

// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

#else
#define _FILE_OFFSET_BITS 64
#include <netinet/tcp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include "solid/system/cassert.hpp"
#include "solid/system/directory.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/filedevice.hpp"
#include "solid/system/log.hpp"
#include "solid/system/socketdevice.hpp"
#include "solid/system/socketinfo.hpp"

using namespace std;

namespace solid {

Device::Device(Device&& _dev) noexcept
    : desc_(_dev.descriptor())
{
    _dev.desc_ = invalidDescriptor();
}

Device& Device::operator=(Device&& _dev) noexcept
{
    close();
    desc_      = _dev.descriptor();
    _dev.desc_ = invalidDescriptor();
    return *this;
}
Device::Device(DescriptorT _desc)
    : desc_(_desc)
{
}

Device::~Device()
{
    close();
}

ssize_t Device::read(char* _pb, size_t _bl)
{
    solid_assert(ok());
#ifdef SOLID_ON_WINDOWS
    DWORD cnt;
    if (ReadFile(desc_, _pb, static_cast<DWORD>(_bl), &cnt, nullptr)) {
        return cnt;
    } else {
        return -1;
    }
#else
    return ::read(desc_, _pb, _bl);
#endif
}

ssize_t Device::write(const char* _pb, size_t _bl)
{
    solid_assert(ok());
#ifdef SOLID_ON_WINDOWS
    DWORD cnt;
    /*OVERLAPPED ovp;
    ovp.Offset = 0;
    ovp.OffsetHigh = 0;
    ovp.hEvent = nullptr;*/
    if (WriteFile(desc_, const_cast<char*>(_pb), static_cast<DWORD>(_bl), &cnt, nullptr)) {
        return cnt;
    } else {
        return -1;
    }
#else
    return ::write(desc_, _pb, _bl);
#endif
}

bool Device::cancel()
{
#ifdef SOLID_ON_WINDOWS
    return CancelIoEx(Device::descriptor(), nullptr) != 0;
#else
    return true;
#endif
}

void Device::close()
{
    if (ok()) {
#ifdef SOLID_ON_WINDOWS
        CloseHandle(desc_);
#else
        if (::close(desc_) != 0) {
            solid_assert(errno != EBADF);
        }
#endif
        desc_ = invalidDescriptor();
    }
}

void Device::flush()
{
    solid_assert(ok());
#ifdef SOLID_ON_WINDOWS
    FlushFileBuffers(desc_);
#else
    fsync(desc_);
#endif
}

//-- SeekableDevice ----------------------------------------
ssize_t SeekableDevice::read(char* _pb, size_t _bl, int64_t _off)
{
#ifdef SOLID_ON_WINDOWS
    int64_t off(seek(0, SeekCur));
    seek(_off);
    ssize_t rv = Device::read(_pb, _bl);
    seek(off);
    return rv;
#else
    return pread(descriptor(), _pb, _bl, _off);
#endif
}

ssize_t SeekableDevice::write(const char* _pb, size_t _bl, int64_t _off)
{
#ifdef SOLID_ON_WINDOWS
    int64_t off(seek(0, SeekCur));
    seek(_off);
    ssize_t rv = Device::write(_pb, _bl);
    seek(off);
    return rv;
#else
    return pwrite(descriptor(), _pb, _bl, _off);
#endif
}

#ifdef SOLID_ON_WINDOWS
const DWORD seekmap[3] = {FILE_BEGIN, FILE_CURRENT, FILE_END};
#else
const int seekmap[3] = {SEEK_SET, SEEK_CUR, SEEK_END};
#endif

int64_t SeekableDevice::seek(int64_t _pos, SeekRef _ref)
{
#ifdef SOLID_ON_WINDOWS
    LARGE_INTEGER li;

    li.QuadPart = _pos;
    li.LowPart  = SetFilePointer(descriptor(), li.LowPart, &li.HighPart, seekmap[_ref]);

    if (
        li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
        li.QuadPart = -1;
    }
    return li.QuadPart;
#else
    return ::lseek(descriptor(), _pos, seekmap[_ref]);
#endif
}

bool SeekableDevice::truncate(int64_t _len)
{
#ifdef SOLID_ON_WINDOWS
    seek(_len);
    return SetEndOfFile(descriptor());
#else
    return ::ftruncate(descriptor(), _len) == 0;
#endif
}

//-- File ----------------------------------------

FileDevice::FileDevice()
{
}

#ifdef SOLID_ON_WINDOWS
HANDLE do_open(WCHAR* _pwc, const char* _fname, const size_t _sz, const size_t _wcp, int _how)
{
    DWORD acc(0);
    if (_how & FileDevice::ReadOnlyE) {
        acc |= GENERIC_READ;
    } else if (_how & FileDevice::WriteOnlyE) {
        acc |= GENERIC_WRITE;
    } else if (_how & FileDevice::ReadWriteE) {
        acc |= (GENERIC_READ | GENERIC_WRITE);
    }

    DWORD creat(0);

    if (_how & FileDevice::CreateE) {
        if (_how & FileDevice::TruncateE) {
            creat = CREATE_ALWAYS;
        } else {
            creat = OPEN_ALWAYS;
        }
    } else {
        creat = OPEN_EXISTING;
    }

    if (_how & FileDevice::AppendE) {
        acc |= FILE_APPEND_DATA;
    }
    if (_how & FileDevice::TruncateE) {
        creat = TRUNCATE_EXISTING;
    }

    const HANDLE h = CreateFileA(_fname, acc, FILE_SHARE_READ, nullptr, creat, FILE_ATTRIBUTE_NORMAL, nullptr);
    return h;
}
#endif

bool FileDevice::open(const char* _fname, int _how)
{
#ifdef SOLID_ON_WINDOWS
    //_fname is utf-8, so we need to convert it to WCHAR
    const size_t sz(strlen(_fname));
    const size_t szex = sz + 1;
    if (szex < 256) {
        WCHAR pwc[512];
        descriptor(do_open(pwc, _fname, sz, 512, _how));
    } else if (szex < 512) {
        WCHAR pwc[1024];
        descriptor(do_open(pwc, _fname, sz, 1024, _how));
    } else if (szex < 1024) {
        WCHAR pwc[2048];
        descriptor(do_open(pwc, _fname, sz, 2048, _how));
    } else {
        WCHAR pwc[4096];
        descriptor(do_open(pwc, _fname, sz, 4096, _how));
    }
    if (ok()) {
        if (_how & AppendE) {
            seek(0, SeekEnd);
        }
    }
    return ok();
#else
    descriptor(::open(_fname, _how, 00666));
    return ok();
#endif
}

bool FileDevice::create(const char* _fname, int _how)
{
    return this->open(_fname, _how | CreateE | TruncateE);
}

int64_t FileDevice::size() const
{
#ifdef SOLID_ON_WINDOWS
    LARGE_INTEGER li;

    li.QuadPart = 0;
    if (GetFileSizeEx(descriptor(), &li)) {
        return li.QuadPart;
    } else {
        return -1;
    }
#else
    struct stat st;
    if (fstat(descriptor(), &st) != 0) {
        return -1;
    }
    return st.st_size;
#endif
}
bool FileDevice::canRetryOpen() const
{
#ifdef SOLID_ON_WINDOWS
    return false;
#else
    return (errno == EMFILE) || (errno == ENOMEM);
#endif
}
/*static*/ int64_t FileDevice::size(const char* _fname)
{
#ifdef SOLID_ON_WINDOWS
    FileDevice fd;
    if (fd.open(_fname, ReadOnlyE)) {
        return -1;
    }
    return fd.size();
#else
    struct stat st;
    if (stat(_fname, &st) != 0) {
        return -1;
    }
    return st.st_size;
#endif
}
//-- Directory -------------------------------------
/*static*/ bool Directory::create(const char* _fname)
{
#ifdef SOLID_ON_WINDOWS
    return CreateDirectoryA(_fname, nullptr) == TRUE;
#else
    return mkdir(_fname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
#endif
}

/*static*/ bool Directory::create_all(const char* _fname)
{
#ifdef SOLID_ON_WINDOWS
    static constexpr const char sep = '\\';
#else
    static constexpr const char sep = '/';
#endif
    string path(_fname);

    size_t sep_pos = path.find(sep, 0);

    while (sep_pos != string::npos) {
        path[sep_pos] = 0;
        create(path.c_str());
        path[sep_pos] = sep;
        sep_pos       = path.find(sep, sep_pos + 1);
    }
    create(_fname);
    return true;
}

/*static*/ bool Directory::eraseFile(const char* _fname)
{
#ifdef SOLID_ON_WINDOWS
    return DeleteFileA(_fname) == TRUE;
#else
    return unlink(_fname) == 0;
#endif
}
/*static*/ bool Directory::renameFile(const char* _from, const char* _to)
{
#ifdef SOLID_ON_WINDOWS
    return MoveFileA(_from, _to);
#else
    return ::rename(_from, _to) == 0;
#endif
}

#ifndef SOLID_HAS_DEBUG
#ifdef SOLID_ON_WINDOWS
struct wsa_cleaner {
    ~wsa_cleaner()
    {
        WSACleanup();
    }
};
#endif
#endif

//---- SocketDevice ---------------------------------
#ifdef SOLID_ON_WINDOWS
namespace {
struct Starter {
    WSADATA wsaData;
    Starter()
    {
        WORD wVersionRequested;
        wVersionRequested = MAKEWORD(2, 2);
        int err           = WSAStartup(wVersionRequested, &wsaData);
        solid_check_log(err == 0, generic_logger, "Error WSAStartup: " << err);
    }
    ~Starter()
    {
        WSACleanup();
    }
} __starter;
} //namespace
#endif

ErrorCodeT last_socket_error()
{
#ifdef SOLID_ON_WINDOWS
    const DWORD err = WSAGetLastError();
    return ErrorCodeT(err, std::system_category());
#else
    return solid::last_system_error();
#endif
}

SocketDevice::SocketDevice(SocketDevice&& _sd) noexcept
    : Device(std::move(_sd))
{
#ifndef SOLID_HAS_DEBUG
#ifdef SOLID_ON_WINDOWS
    static const wsa_cleaner wsaclean;
#endif
#endif
}

SocketDevice::SocketDevice()
{
}

SocketDevice& SocketDevice::operator=(SocketDevice&& _dev) noexcept
{
    *static_cast<Device*>(this) = static_cast<Device&&>(_dev);
    return *this;
}

SocketDevice::~SocketDevice()
{
    close();
}

void SocketDevice::shutdownRead()
{
#ifdef SOLID_ON_WINDOWS
    if (ok()) {
        shutdown(descriptor(), SD_RECEIVE);
    }
#else
    if (ok()) {
        shutdown(descriptor(), SHUT_RD);
    }
#endif
}
void SocketDevice::shutdownWrite()
{
#ifdef SOLID_ON_WINDOWS
    if (ok()) {
        shutdown(descriptor(), SD_SEND);
    }
#else
    if (ok()) {
        shutdown(descriptor(), SHUT_WR);
    }
#endif
}
void SocketDevice::shutdownReadWrite()
{
#ifdef SOLID_ON_WINDOWS
    if (ok()) {
        shutdown(descriptor(), SD_BOTH);
    }
#else
    if (ok()) {
        shutdown(descriptor(), SHUT_RDWR);
    }
#endif
}
void SocketDevice::close()
{
#ifdef SOLID_ON_WINDOWS
    shutdownReadWrite();
    if (ok()) {
        closesocket(descriptor());
        Device::descriptor((HANDLE)invalidDescriptor(), false);
    }
#else
    shutdownReadWrite();
    Device::close();
#endif
}

ErrorCodeT SocketDevice::create(const ResolveIterator& _rri)
{
#ifdef SOLID_ON_WINDOWS
    //NOTE: must use WSASocket instead of socket because
    //the latter seems to create the socket with OVERLAPPED support
    //which will not work with above WriteFile for synchronous IO
    SOCKET s = WSASocketW(_rri.family(), _rri.type(), _rri.protocol(), nullptr, 0, WSA_FLAG_OVERLAPPED);
    Device::descriptor((HANDLE)s);
    enableLoopbackFastPath();
#else
    Device::descriptor(socket(_rri.family(), _rri.type(), _rri.protocol()));
#endif
    return ok() ? ErrorCodeT() : last_socket_error();
}

ErrorCodeT SocketDevice::create(
    SocketInfo::Family _family,
    SocketInfo::Type   _type,
    int                _proto)
{
#ifdef SOLID_ON_WINDOWS
    SOCKET s = WSASocketW(_family, _type, _proto, nullptr, 0, WSA_FLAG_OVERLAPPED);
    Device::descriptor((HANDLE)s);
    enableLoopbackFastPath();
#else
    Device::descriptor(socket(_family, _type, _proto));
#endif
    return ok() ? ErrorCodeT() : last_socket_error();
}

ErrorCodeT SocketDevice::connect(const SocketAddressStub& _rsas, bool& _rcan_wait)
{
#ifdef SOLID_ON_WINDOWS
    const int rv = ::connect(descriptor(), _rsas.sockAddr(), _rsas.size());
    _rcan_wait   = (WSAGetLastError() == WSAEWOULDBLOCK);
#else
    const int rv = ::connect(descriptor(), _rsas.sockAddr(), _rsas.size());
    _rcan_wait = (errno == EINPROGRESS);
#endif
    return rv == 0 ? ErrorCodeT() : last_socket_error();
}

ErrorCodeT SocketDevice::connect(const SocketAddressStub& _rsas)
{
#ifdef SOLID_ON_WINDOWS
    int rv = ::connect(descriptor(), _rsas.sockAddr(), _rsas.size());
#else
    int rv = ::connect(descriptor(), _rsas.sockAddr(), _rsas.size());
#endif
    return rv == 0 ? ErrorCodeT() : last_socket_error();
}

// int SocketDevice::connect(const ResolveIterator &_rai){
// #ifdef SOLID_ON_WINDOWS
//  int rv = ::connect(descriptor(), _rai.addr(), _rai.size());
//  if (rv < 0) { // sau rv == -1 ...
//      if(WSAGetLastError() == WSAEWOULDBLOCK) return NOK;
//      edbgx(Debug::system, "socket connect");
//      close();
//      return BAD;
//  }
//  return OK;
// #else
//  int rv = ::connect(descriptor(), _rai.addr(), _rai.size());
//  if (rv < 0) { // sau rv == -1 ...
//      if(errno == EINPROGRESS) return NOK;
//      edbgx(Debug::system, "socket connect: "<<strerror(errno));
//      close();
//      return BAD;
//  }
//  return OK;
// #endif
// }
ErrorCodeT SocketDevice::prepareAccept(const SocketAddressStub& _rsas, size_t _listencnt)
{
#ifdef SOLID_ON_WINDOWS
    int yes = 1;
    int rv  = setsockopt(descriptor(), SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));
    if (rv < 0) {
        return last_socket_error();
    }

    rv = ::bind(descriptor(), _rsas.sockAddr(), _rsas.size());
    if (rv < 0) {
        return last_socket_error();
    }

    rv = listen(descriptor(), static_cast<int>(_listencnt));
    if (rv < 0) {
        return last_socket_error();
    }
    return last_socket_error();
#else
    int yes = 1;
    int rv = setsockopt(descriptor(), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&yes), sizeof(yes));
    if (rv < 0) {
        return last_socket_error();
    }

    rv = ::bind(descriptor(), _rsas.sockAddr(), _rsas.size());
    if (rv < 0) {
        return last_socket_error();
    }
    rv = listen(descriptor(), static_cast<int>(_listencnt));
    if (rv < 0) {
        return last_socket_error();
    }
    return ErrorCodeT();
#endif
}

ErrorCodeT SocketDevice::accept(SocketDevice& _dev, bool& _rcan_retry)
{
#if defined(SOLID_ON_WINDOWS)
    const SOCKET rv = ::accept(descriptor(), nullptr, nullptr);
    _rcan_retry     = (WSAGetLastError() == WSAEWOULDBLOCK);
    _dev.Device::descriptor((HANDLE)rv);
    _dev.enableLoopbackFastPath();
    return _dev ? ErrorCodeT() : last_socket_error();
#elif defined(SOLID_ON_DARWIN) || defined(SOLID_ON_FREEBSD)
    const int rv = ::accept(descriptor(), nullptr, nullptr);
    _rcan_retry = (errno == EAGAIN || errno == ENETDOWN || errno == EPROTO || errno == ENOPROTOOPT || errno == EHOSTDOWN || errno == EHOSTUNREACH || errno == EOPNOTSUPP || errno == ENETUNREACH);
    _dev.Device::descriptor(rv);

    return rv > 0 ? ErrorCodeT() : last_socket_error();
#else
    const int rv = ::accept(descriptor(), nullptr, nullptr);
    _rcan_retry  = (errno == EAGAIN || errno == ENETDOWN || errno == EPROTO || errno == ENOPROTOOPT || errno == EHOSTDOWN || errno == ENONET || errno == EHOSTUNREACH || errno == EOPNOTSUPP || errno == ENETUNREACH);
    _dev.Device::descriptor(rv);

    return rv > 0 ? ErrorCodeT() : last_socket_error();
#endif
}

ErrorCodeT SocketDevice::accept(SocketDevice& _dev)
{
#ifdef SOLID_ON_WINDOWS
    SocketAddress sa;
    SOCKET        rv = ::accept(descriptor(), sa, &sa.sz);
    _dev.Device::descriptor((HANDLE)rv);
    _dev.enableLoopbackFastPath();
    return last_socket_error();
#else
    int rv = ::accept(descriptor(), nullptr, nullptr);
    _dev.Device::descriptor(rv);
    return rv > 0 ? ErrorCodeT() : last_socket_error();
#endif
}

ErrorCodeT SocketDevice::bind(const SocketAddressStub& _rsa)
{
#ifdef SOLID_ON_WINDOWS
    /*int rv = */ ::bind(descriptor(), _rsa.sockAddr(), _rsa.size());
    return last_socket_error();
#else
    int rv = ::bind(descriptor(), _rsa.sockAddr(), _rsa.size());
    return rv == 0 ? ErrorCodeT() : last_socket_error();
#endif
}

ErrorCodeT SocketDevice::makeBlocking()
{
#ifdef SOLID_ON_WINDOWS
    u_long mode = 0;
    int    rv   = ioctlsocket(descriptor(), FIONBIO, &mode);
    if (rv != NO_ERROR) {
        return last_socket_error();
    }
    return ErrorCodeT();
#else
    int flg = fcntl(descriptor(), F_GETFL);
    if (flg == -1) {
        return last_socket_error();
    }
    flg &= ~O_NONBLOCK;
    /*int rv = */ fcntl(descriptor(), F_SETFL, flg);
    return last_socket_error();
#endif
}

ErrorCodeT SocketDevice::makeBlocking(size_t _msec)
{
#ifdef SOLID_ON_WINDOWS
    u_long mode = 0;
    int    rv   = ioctlsocket(descriptor(), FIONBIO, &mode);
    if (rv != NO_ERROR) {
        return last_socket_error();
    }
    DWORD tout(static_cast<DWORD>(_msec));
    rv = setsockopt(descriptor(), SOL_SOCKET, SO_RCVTIMEO, (char*)&tout, sizeof(tout));
    if (rv == SOCKET_ERROR) {
        return last_socket_error();
    }
    tout = static_cast<DWORD>(_msec);
    rv   = setsockopt(descriptor(), SOL_SOCKET, SO_SNDTIMEO, (char*)&tout, sizeof(tout));
    if (rv == SOCKET_ERROR) {
        return last_socket_error();
    }
    return ErrorCodeT();
#else
    int flg = fcntl(descriptor(), F_GETFL);
    if (flg == -1) {
        return last_socket_error();
    }
    flg &= ~O_NONBLOCK;
    int rv = fcntl(descriptor(), F_SETFL, flg);
    if (rv < 0) {
        return last_socket_error();
    }
    struct timeval timeout;
    timeout.tv_sec = _msec / 1000;
    timeout.tv_usec = _msec % 1000;
    rv = setsockopt(descriptor(), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
    if (rv != 0) {
        return last_socket_error();
    }
    rv = setsockopt(descriptor(), SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
    if (rv != 0) {
        return last_socket_error();
    }
    return ErrorCodeT();
#endif
}

ErrorCodeT SocketDevice::makeNonBlocking()
{
#ifdef SOLID_ON_WINDOWS
    u_long mode = 1;
    int    rv   = ioctlsocket(descriptor(), FIONBIO, &mode);

    if (rv == NO_ERROR) {
        return ErrorCodeT();
    }
    return last_socket_error();
#else
    int flg = fcntl(descriptor(), F_GETFL);
    if (flg == -1) {
        return last_socket_error();
    }
    int rv = fcntl(descriptor(), F_SETFL, flg | O_NONBLOCK);
    if (rv >= 0) {
        return ErrorCodeT();
    }
    return last_socket_error();
#endif
}

ErrorCodeT SocketDevice::isBlocking(bool& _rrv) const
{
#ifdef SOLID_ON_WINDOWS
    return solid::error_not_implemented;
#else

    const int flg = fcntl(descriptor(), F_GETFL);

    if (flg != -1) {
        _rrv = ((flg & O_NONBLOCK) == 0);
        return ErrorCodeT();
    }
    return last_socket_error();
#endif
}

ssize_t SocketDevice::send(const char* _pb, size_t _ul, bool& _rcan_retry, ErrorCodeT& _rerr, unsigned)
{
#ifdef SOLID_ON_WINDOWS
#if 1
    ssize_t rv  = ::send(descriptor(), _pb, static_cast<int>(_ul), 0);
    _rcan_retry = (WSAGetLastError() == WSAEWOULDBLOCK);
    _rerr       = last_socket_error();
    return rv;
#else
    DWORD  bytes_sent = 0;
    int    status;
    WSABUF buffer;
    buffer.len  = _ul;
    buffer.buf  = (char*)_pb;
    status      = WSASend(descriptor(), &buffer, 1, &bytes_sent, 0, NULL, NULL);
    _rcan_retry = (WSAGetLastError() == WSAEWOULDBLOCK);
    _rerr       = last_socket_error();
    if (bytes_sent)
        return bytes_sent;
    return -1;
#endif
#else
    ssize_t rv = ::send(descriptor(), _pb, _ul, 0);
    _rcan_retry = (errno == EAGAIN || errno == EWOULDBLOCK);
    _rerr = last_socket_error();
    return rv;
#endif
}
ssize_t SocketDevice::recv(char* _pb, size_t _ul, bool& _rcan_retry, ErrorCodeT& _rerr, unsigned)
{
#ifdef SOLID_ON_WINDOWS
#if 1
    ssize_t rv  = ::recv(descriptor(), _pb, static_cast<int>(_ul), 0);
    _rcan_retry = (WSAGetLastError() == WSAEWOULDBLOCK);
    _rerr       = last_socket_error();
    return rv;
#else
    WSABUF buffer;
    DWORD  flags     = 0;
    buffer.buf       = _pb;
    buffer.len       = _ul;
    DWORD bytes_read = 0;
    int   rv         = WSARecv(descriptor(), &buffer, 1, &bytes_read, &flags, NULL, NULL);
    _rcan_retry      = (WSAGetLastError() == WSAEWOULDBLOCK);
    _rerr            = last_socket_error();
    if (bytes_read)
        return bytes_read;
    return -1;
#endif
#else
    ssize_t rv = ::recv(descriptor(), _pb, _ul, 0);
    _rcan_retry = (errno == EAGAIN || errno == EWOULDBLOCK);
    _rerr = last_socket_error();
    return rv;
#endif
}
ssize_t SocketDevice::send(const char* _pb, size_t _ul, const SocketAddressStub& _sap, bool& _rcan_retry, ErrorCodeT& _rerr)
{
#ifdef SOLID_ON_WINDOWS
    ssize_t rv  = ::sendto(descriptor(), _pb, static_cast<int>(_ul), 0, _sap.sockAddr(), _sap.size());
    _rcan_retry = (WSAGetLastError() == WSAEWOULDBLOCK);
    _rerr       = last_socket_error();
    return rv;
#else
    ssize_t rv = ::sendto(descriptor(), _pb, _ul, 0, _sap.sockAddr(), _sap.size());
    _rcan_retry = (errno == EAGAIN || errno == EWOULDBLOCK);
    _rerr = last_socket_error();
    return rv;
#endif
}
ssize_t SocketDevice::recv(char* _pb, size_t _ul, SocketAddress& _rsa, bool& _rcan_retry, ErrorCodeT& _rerr)
{
#ifdef SOLID_ON_WINDOWS
    _rsa.clear();
    _rsa.sz     = SocketAddress::Capacity;
    ssize_t rv  = ::recvfrom(descriptor(), _pb, static_cast<int>(_ul), 0, _rsa.sockAddr(), &_rsa.sz);
    _rcan_retry = (WSAGetLastError() == WSAEWOULDBLOCK);
    _rerr       = last_socket_error();
    return rv;
#else
    _rsa.clear();
    _rsa.sz = SocketAddress::Capacity;
    ssize_t rv = ::recvfrom(descriptor(), _pb, _ul, 0, _rsa.sockAddr(), &_rsa.sz);
    _rcan_retry = (errno == EAGAIN || errno == EWOULDBLOCK);
    _rerr = last_socket_error();
    return rv;
#endif
}

ErrorCodeT SocketDevice::remoteAddress(SocketAddress& _rsa) const
{
#ifdef SOLID_ON_WINDOWS
    _rsa.clear();
    _rsa.sz = SocketAddress::Capacity;
    int rv  = getpeername(descriptor(), _rsa.sockAddr(), &_rsa.sz);
    if (rv == 0) {
        return ErrorCodeT();
    }
    return last_socket_error();
#else
    _rsa.clear();
    _rsa.sz = SocketAddress::Capacity;
    int rv = getpeername(descriptor(), _rsa.sockAddr(), &_rsa.sz);
    if (rv == 0) {
        return ErrorCodeT();
    }
    return last_socket_error();
#endif
}

ErrorCodeT SocketDevice::localAddress(SocketAddress& _rsa) const
{
#ifdef SOLID_ON_WINDOWS
    _rsa.clear();
    _rsa.sz = SocketAddress::Capacity;
    int rv  = getsockname(descriptor(), _rsa.sockAddr(), &_rsa.sz);
    if (rv == 0) {
        return ErrorCodeT();
    }
    return last_socket_error();
#else
    _rsa.clear();
    _rsa.sz = SocketAddress::Capacity;
    int rv = getsockname(descriptor(), _rsa.sockAddr(), &_rsa.sz);
    if (rv == 0) {
        return ErrorCodeT();
    }
    return last_socket_error();
#endif
}

ErrorCodeT SocketDevice::type(int& _rrv) const
{
#ifdef SOLID_ON_WINDOWS
    return solid::error_not_implemented;
#else
    int val = 0;
    socklen_t valsz = sizeof(int);
    int rv = getsockopt(descriptor(), SOL_SOCKET, SO_TYPE, &val, &valsz);
    if (rv == 0) {
        _rrv = val;
        return ErrorCodeT();
    }

    return last_socket_error();
#endif
}

// SocketDevice::RetValE SocketDevice::isListening(ErrorCodeT &_rerr)const{
// #ifdef SOLID_ON_WINDOWS
//  return Error;
// #else
//  int val = 0;
//  socklen_t valsz = sizeof(int);
//  int rv = getsockopt(descriptor(), SOL_SOCKET, SO_ACCEPTCONN, &val, &valsz);
//  if(rv == 0){
//      _rerr = last_error();
//      return val != 0 ? Success : Failure;
//  }
//
//  if(this->type() == SocketInfo::Datagram){
//      return Failure;
//  }
//  return Error;
// #endif
// }

ErrorCodeT SocketDevice::enableLoopbackFastPath()
{
#if defined(SOLID_ON_WINDOWS)
    uint32_t param = 1;
    DWORD    ret;
    int      rv = WSAIoctl(descriptor(), SIO_LOOPBACK_FAST_PATH /*_WSAIOW(IOC_VENDOR, 16)*/,
        &param, sizeof(param), NULL, 0, &ret, 0, 0);
    if (rv == 0) {
        return ErrorCodeT();
    }
    return last_socket_error();
#else
    return ErrorCodeT();
#endif
}

ErrorCodeT SocketDevice::enableNoDelay()
{
#if defined(SOLID_ON_WINDOWS)
    BOOL flag = true;
    int  rv   = setsockopt(descriptor(), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&flag), sizeof(flag));
#else
    int flag = 1;
    int rv = setsockopt(descriptor(), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&flag), sizeof(flag));
#endif
    if (rv == 0) {
        return ErrorCodeT();
    }
    return last_socket_error();
}

ErrorCodeT SocketDevice::disableNoDelay()
{
#if defined(SOLID_ON_WINDOWS)
    BOOL flag = false;
    int  rv   = setsockopt(descriptor(), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&flag), sizeof(flag));
#else
    int flag = 0;
    int rv = setsockopt(descriptor(), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&flag), sizeof(flag));
#endif
    if (rv == 0) {
        return ErrorCodeT();
    }
    return last_socket_error();
}

ErrorCodeT SocketDevice::enableNoSignal()
{
#if defined(SOLID_ON_WINDOWS) || defined(SOLID_ON_DARWIN)
    return solid::error_not_implemented;
#else
    int flag = 1;
    int rv = setsockopt(descriptor(), SOL_SOCKET, MSG_NOSIGNAL, reinterpret_cast<char*>(&flag), sizeof(flag));
    if (rv == 0) {
        return ErrorCodeT();
    }
    return last_socket_error();
#endif
}

ErrorCodeT SocketDevice::disableNoSignal()
{
#if defined(SOLID_ON_WINDOWS) || defined(SOLID_ON_DARWIN)
    return solid::error_not_implemented;
#else
    int flag = 0;
    int rv = setsockopt(descriptor(), SOL_SOCKET, MSG_NOSIGNAL, reinterpret_cast<char*>(&flag), sizeof(flag));
    if (rv == 0) {
        return ErrorCodeT();
    }
    return last_socket_error();
#endif
}

ErrorCodeT SocketDevice::enableLinger()
{
    return solid::error_not_implemented;
}

ErrorCodeT SocketDevice::disableLinger()
{
    return solid::error_not_implemented;
}

ErrorCodeT SocketDevice::hasNoDelay(bool& _rrv) const
{
    int       flag = 0;
    socklen_t sz(sizeof(flag));
    int       rv = getsockopt(descriptor(), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&flag), &sz);
    if (rv == 0) {
        _rrv = (flag != 0);
        return ErrorCodeT();
    }
    return last_socket_error();
}

ErrorCodeT SocketDevice::enableCork()
{
#ifdef SOLID_ON_WINDOWS
    return solid::error_not_implemented;
#elif defined(SOLID_ON_LINUX)
    int flag = 1;
    int rv = setsockopt(descriptor(), IPPROTO_TCP, TCP_CORK, reinterpret_cast<char*>(&flag), sizeof(flag));
    if (rv == 0) {
        return ErrorCodeT();
    }
    return last_socket_error();
#else
    return solid::error_not_implemented;
#endif
}

ErrorCodeT SocketDevice::disableCork()
{
#ifdef SOLID_ON_WINDOWS
    return solid::error_not_implemented;
#elif defined(SOLID_ON_LINUX)
    int flag = 0;
    int rv = setsockopt(descriptor(), IPPROTO_TCP, TCP_CORK, reinterpret_cast<char*>(&flag), sizeof(flag));
    if (rv == 0) {
        return ErrorCodeT();
    }
    return last_socket_error();
#else
    return solid::error_not_implemented;
#endif
}

ErrorCodeT SocketDevice::hasCork(bool& _rrv) const
{
#ifdef SOLID_ON_WINDOWS
    return solid::error_not_implemented;
#elif defined(SOLID_ON_LINUX)
    int flag = 0;
    socklen_t sz(sizeof(flag));
    int rv = getsockopt(descriptor(), IPPROTO_TCP, TCP_CORK, reinterpret_cast<char*>(&flag), &sz);
    if (rv == 0) {
        _rrv = (flag != 0);
        return ErrorCodeT();
    }
    return last_socket_error();
#else
    //TODO:
    (void)_rrv;
    return solid::error_not_implemented;
#endif
}

ErrorCodeT SocketDevice::sendBufferSize(int& _rsz)
{
    if (_rsz >= 0) {
        int sockbufsz(_rsz);
        int rv = setsockopt(descriptor(), SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&sockbufsz), sizeof(sockbufsz));
        if (rv == 0) {
            return ErrorCodeT();
        }
        return last_socket_error();
    }
    int       sockbufsz(0);
    socklen_t sz(sizeof(sockbufsz));
    int       rv = getsockopt(descriptor(), SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&sockbufsz), &sz);

    if (rv == 0) {
        _rsz = sockbufsz;
        return ErrorCodeT();
    }
    return last_socket_error();
}

ErrorCodeT SocketDevice::recvBufferSize(int& _rsz)
{
    if (_rsz >= 0) {
        int sockbufsz(_rsz);
        int rv = setsockopt(descriptor(), SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&sockbufsz), sizeof(sockbufsz));
        if (rv == 0) {
            return ErrorCodeT();
        }
    } else {
        int       sockbufsz(0);
        socklen_t sz(sizeof(sockbufsz));
        int       rv = getsockopt(descriptor(), SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&sockbufsz), &sz);
        if (rv == 0) {
            _rsz = sockbufsz;
            return ErrorCodeT();
        }
    }
    return last_socket_error();
}

ErrorCodeT SocketDevice::sendBufferSize(int& _rsz) const
{
    int       sockbufsz(0);
    socklen_t sz(sizeof(sockbufsz));
    int       rv = getsockopt(descriptor(), SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&sockbufsz), &sz);

    if (rv == 0) {
        _rsz = sockbufsz;
        return ErrorCodeT();
    }
    return last_socket_error();
}

ErrorCodeT SocketDevice::recvBufferSize(int& _rsz) const
{
    int       sockbufsz(0);
    socklen_t sz(sizeof(sockbufsz));
    int       rv = getsockopt(descriptor(), SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&sockbufsz), &sz);
    if (rv == 0) {
        _rsz = sockbufsz;
        return ErrorCodeT();
    }
    return last_socket_error();
}

ErrorCodeT SocketDevice::error() const
{
    int       val   = 0;
    socklen_t valsz = sizeof(int);
    int       rv    = getsockopt(descriptor(), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&val), &valsz);
    if (rv == 0) {
        return ErrorCodeT(val, std::system_category());
    }
    return last_socket_error();
}

/*static*/ ErrorCodeT SocketDevice::error(const DescriptorT _des)
{
    int       val   = 0;
    socklen_t valsz = sizeof(int);
    int       rv    = getsockopt(_des, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&val), &valsz);
    if (rv == 0) {
        return ErrorCodeT(val, std::system_category());
    }
    return last_socket_error();
}

std::ostream& operator<<(std::ostream& _ros, const LocalAddressPlot& _ra)
{
    SocketAddress sa;
    _ra.rsd.localAddress(sa);
    _ros << sa;
    return _ros;
}

std::ostream& operator<<(std::ostream& _ros, const RemoteAddressPlot& _ra)
{
    SocketAddress sa;
    _ra.rsd.remoteAddress(sa);
    _ros << sa;
    return _ros;
}

std::ostream& operator<<(std::ostream& _ros, const LocalEndpointPlot& _ra)
{
    SocketAddress sa;
    _ra.rsd.localAddress(sa);
    _ros << sa << ':' << sa.port();
    return _ros;
}

std::ostream& operator<<(std::ostream& _ros, const RemoteEndpointPlot& _ra)
{
    SocketAddress sa;
    _ra.rsd.remoteAddress(sa);
    _ros << sa << ':' << sa.port();
    return _ros;
}

/*static*/ size_t SocketInfo::max_listen_backlog_size()
{
    return 128; //TODO: try take the value from the system
}

} //namespace solid
