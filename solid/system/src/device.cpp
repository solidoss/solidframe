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
#include <windows.h>

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
#include <string>
#include <tuple>

#include "solid/system/cassert.hpp"
#include "solid/system/directory.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/filedevice.hpp"
#include "solid/system/log.hpp"
#include "solid/system/socketdevice.hpp"
#include "solid/system/socketinfo.hpp"

using namespace std;

namespace solid {

namespace {

#ifdef SOLID_ON_WINDOWS
constexpr char path_separator = '\\';
#else
constexpr char path_separator = '/';
#endif

} // namespace

//-- Device ----------------------------------------

Device::Device(Device&& _dev) noexcept
    : desc_(_dev.descriptor())
{
    _dev.desc_ = invalidDescriptor();
}

Device&
Device::operator=(Device&& _dev) noexcept
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

std::expected<std::size_t, std::error_code>
Device::read(std::span<std::byte> _buf)
{
#ifdef SOLID_ON_WINDOWS
    DWORD cnt = 0;
    if (ReadFile(desc_, _buf.data(), static_cast<DWORD>(_buf.size()), &cnt, nullptr)) {
        return static_cast<std::size_t>(cnt);
    }
    return std::unexpected(last_system_error());
#else
    const auto rv = ::read(desc_, _buf.data(), _buf.size());
    if (rv < 0) {
        return std::unexpected(last_system_error());
    }
    return static_cast<std::size_t>(rv);
#endif
}

std::expected<std::size_t, std::error_code>
Device::write(std::span<const std::byte> _buf)
{
#ifdef SOLID_ON_WINDOWS
    DWORD cnt = 0;
    if (WriteFile(desc_, _buf.data(), static_cast<DWORD>(_buf.size()), &cnt, nullptr)) {
        return static_cast<std::size_t>(cnt);
    }
    return std::unexpected(last_system_error());
#else
    const auto rv = ::write(desc_, _buf.data(), _buf.size());
    if (rv < 0) {
        return std::unexpected(last_system_error());
    }
    return static_cast<std::size_t>(rv);
#endif
}

std::expected<void, std::error_code>
Device::cancel()
{
#ifdef SOLID_ON_WINDOWS
    if (CancelIoEx(Device::descriptor(), nullptr) != 0) {
        return {};
    }
    return std::unexpected(last_system_error());
#else
    return {};
#endif
}

void Device::close()
{
    if (ok()) {
#ifdef SOLID_ON_WINDOWS
        CloseHandle(desc_);
#else
        ::close(desc_);
#endif
        desc_ = invalidDescriptor();
    }
}

std::expected<void, std::error_code>
Device::flush()
{
#ifdef SOLID_ON_WINDOWS
    if (FlushFileBuffers(desc_)) {
        return {};
    }
    return std::unexpected(last_system_error());
#else
    if (::fsync(desc_) == 0) {
        return {};
    }
    return std::unexpected(last_system_error());
#endif
}

//-- SeekableDevice ----------------------------------------

std::expected<std::size_t, std::error_code>
SeekableDevice::read(std::span<std::byte> _buf, int64_t _off)
{
#ifdef SOLID_ON_WINDOWS
    const auto off = seek(0, SeekCur);
    if (!off) {
        return std::unexpected(off.error());
    }
    if (const auto rv = seek(_off); !rv) {
        return std::unexpected(rv.error());
    }
    const auto rv = Device::read(_buf);
    std::ignore   = seek(*off);
    return rv;
#else
    const auto rv = ::pread(descriptor(), _buf.data(), _buf.size(), _off);
    if (rv < 0) {
        return std::unexpected(last_system_error());
    }
    return static_cast<std::size_t>(rv);
#endif
}

std::expected<std::size_t, std::error_code>
SeekableDevice::write(std::span<const std::byte> _buf, int64_t _off)
{
#ifdef SOLID_ON_WINDOWS
    const auto off = seek(0, SeekCur);
    if (!off) {
        return std::unexpected(off.error());
    }
    if (const auto rv = seek(_off); !rv) {
        return std::unexpected(rv.error());
    }
    const auto rv = Device::write(_buf);
    std::ignore   = seek(*off);
    return rv;
#else
    const auto rv = ::pwrite(descriptor(), _buf.data(), _buf.size(), _off);
    if (rv < 0) {
        return std::unexpected(last_system_error());
    }
    return static_cast<std::size_t>(rv);
#endif
}

namespace {
#ifdef SOLID_ON_WINDOWS
constexpr DWORD seekmap[3] = {FILE_BEGIN, FILE_CURRENT, FILE_END};
#else
constexpr int seekmap[3] = {SEEK_SET, SEEK_CUR, SEEK_END};
#endif
} // namespace

std::expected<int64_t, std::error_code>
SeekableDevice::seek(int64_t _pos, SeekRef _ref)
{
#ifdef SOLID_ON_WINDOWS
    LARGE_INTEGER li;

    li.QuadPart = _pos;
    li.LowPart  = SetFilePointer(descriptor(), li.LowPart, &li.HighPart, seekmap[_ref]);

    if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
        return std::unexpected(last_system_error());
    }
    return static_cast<int64_t>(li.QuadPart);
#else
    const auto rv = ::lseek(descriptor(), _pos, seekmap[_ref]);
    if (rv < 0) {
        return std::unexpected(last_system_error());
    }
    return static_cast<int64_t>(rv);
#endif
}

std::expected<void, std::error_code>
SeekableDevice::truncate(int64_t _len)
{
#ifdef SOLID_ON_WINDOWS
    if (const auto rv = seek(_len); !rv) {
        return std::unexpected(rv.error());
    }
    if (SetEndOfFile(descriptor())) {
        return {};
    }
    return std::unexpected(last_system_error());
#else
    if (::ftruncate(descriptor(), _len) == 0) {
        return {};
    }
    return std::unexpected(last_system_error());
#endif
}

//-- File ----------------------------------------

FileDevice::FileDevice()
{
}

#ifdef SOLID_ON_WINDOWS
namespace {
HANDLE
do_open(const char* _fname, int _how)
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
        if (_how & FileDevice::TruncateE) {
            creat = TRUNCATE_EXISTING;
        }
    }

    if (_how & FileDevice::AppendE) {
        acc |= FILE_APPEND_DATA;
    }

    return CreateFileA(_fname, acc, FILE_SHARE_READ, nullptr, creat, FILE_ATTRIBUTE_NORMAL, nullptr);
}
} // namespace
#endif

std::expected<void, std::error_code>
FileDevice::open(std::string_view _fname, int _how)
{
    const std::string fname{_fname};
#ifdef SOLID_ON_WINDOWS
    descriptor(do_open(fname.c_str(), _how));
    if (!ok()) {
        return std::unexpected(last_system_error());
    }
    if (_how & AppendE) {
        if (const auto rv = seek(0, SeekEnd); !rv) {
            close();
            return std::unexpected(rv.error());
        }
    }
    return {};
#else
    descriptor(::open(fname.c_str(), _how, 00666));
    if (!ok()) {
        return std::unexpected(last_system_error());
    }
    return {};
#endif
}

std::expected<void, std::error_code>
FileDevice::create(std::string_view _fname, int _how)
{
    return this->open(_fname, _how | CreateE | TruncateE);
}

std::expected<int64_t, std::error_code>
FileDevice::size() const
{
#ifdef SOLID_ON_WINDOWS
    LARGE_INTEGER li;

    li.QuadPart = 0;
    if (GetFileSizeEx(descriptor(), &li)) {
        return static_cast<int64_t>(li.QuadPart);
    }
    return std::unexpected(last_system_error());
#else
    struct stat st;
    if (::fstat(descriptor(), &st) != 0) {
        return std::unexpected(last_system_error());
    }
    return static_cast<int64_t>(st.st_size);
#endif
}

/*static*/ bool
FileDevice::can_retry_open(const std::error_code& _err) noexcept
{
#ifdef SOLID_ON_WINDOWS
    (void)_err;
    return false;
#else
    return _err == std::errc::too_many_files_open || _err == std::errc::not_enough_memory;
#endif
}

/*static*/ std::expected<int64_t, std::error_code>
FileDevice::size(std::string_view _fname)
{
#ifdef SOLID_ON_WINDOWS
    FileDevice fd;
    if (const auto rv = fd.open(_fname, ReadOnlyE); !rv) {
        return std::unexpected(rv.error());
    }
    return fd.size();
#else
    const std::string fname{_fname};
    struct stat       st;
    if (::stat(fname.c_str(), &st) != 0) {
        return std::unexpected(last_system_error());
    }
    return static_cast<int64_t>(st.st_size);
#endif
}

//-- Directory -------------------------------------

/*static*/ std::expected<void, std::error_code>
Directory::create(std::string_view _path)
{
    const std::string path{_path};
#ifdef SOLID_ON_WINDOWS
    if (CreateDirectoryA(path.c_str(), nullptr) == TRUE) {
        return {};
    }
    return std::unexpected(last_system_error());
#else
    if (::mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0) {
        return {};
    }
    return std::unexpected(last_system_error());
#endif
}

/*static*/ std::expected<void, std::error_code>
Directory::create_all(std::string_view _path)
{
    const auto create_if_missing = [](std::string_view _dir) -> std::expected<void, std::error_code> {
        if (const auto rv = create(_dir); !rv && rv.error() != std::errc::file_exists) {
            return rv;
        }
        return {};
    };

    size_t sep_pos = _path.find(path_separator);

    while (sep_pos != std::string_view::npos) {
        if (sep_pos != 0) {
            if (const auto rv = create_if_missing(_path.substr(0, sep_pos)); !rv) {
                return rv;
            }
        }
        sep_pos = _path.find(path_separator, sep_pos + 1);
    }
    return create_if_missing(_path);
}

/*static*/ std::expected<void, std::error_code>
Directory::erase_file(std::string_view _path)
{
    const std::string path{_path};
#ifdef SOLID_ON_WINDOWS
    if (DeleteFileA(path.c_str()) == TRUE) {
        return {};
    }
    return std::unexpected(last_system_error());
#else
    if (::unlink(path.c_str()) == 0) {
        return {};
    }
    return std::unexpected(last_system_error());
#endif
}

/*static*/ std::expected<void, std::error_code>
Directory::rename_file(std::string_view _from, std::string_view _to)
{
    const std::string from{_from};
    const std::string to{_to};
#ifdef SOLID_ON_WINDOWS
    if (MoveFileExA(from.c_str(), to.c_str(), MOVEFILE_WRITE_THROUGH | MOVEFILE_REPLACE_EXISTING)) {
        return {};
    }
    return std::unexpected(last_system_error());
#else
    if (::rename(from.c_str(), to.c_str()) == 0) {
        return {};
    }
    return std::unexpected(last_system_error());
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
} // namespace
#endif

ErrorCodeT
last_socket_error()
{
#ifdef SOLID_ON_WINDOWS
    const DWORD err = WSAGetLastError();
    return ErrorCodeT(err, std::system_category());
#else
    return solid::last_system_error();
#endif
}

namespace {

#ifdef SOLID_ON_WINDOWS
using SockBufLenT = int;
inline char*
sock_buf(std::span<std::byte> _buf)
{
    return reinterpret_cast<char*>(_buf.data());
}
inline const char*
sock_buf(std::span<const std::byte> _buf)
{
    return reinterpret_cast<const char*>(_buf.data());
}
#else
using SockBufLenT = size_t;
inline void*
sock_buf(std::span<std::byte> _buf)
{
    return _buf.data();
}
inline const void*
sock_buf(std::span<const std::byte> _buf)
{
    return _buf.data();
}
#endif

inline SockBufLenT
sock_len(const size_t _sz)
{
    return static_cast<SockBufLenT>(_sz);
}

inline bool
is_system_error(const std::error_code& _err) noexcept
{
    return _err.category() == std::system_category() || _err.category() == std::generic_category();
}

} // namespace

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

SocketDevice&
SocketDevice::operator=(SocketDevice&& _dev) noexcept
{
    *static_cast<Device*>(this) = static_cast<Device&&>(_dev);
    return *this;
}

SocketDevice::~SocketDevice()
{
    close();
}

std::expected<void, std::error_code>
SocketDevice::shutdownRead()
{
#ifdef SOLID_ON_WINDOWS
    if (ok()) {
        shutdown(descriptor(), SD_RECEIVE);
    }
#else
    if (ok()) {
        if (shutdown(descriptor(), SHUT_RD) != 0) {
            return std::unexpected(last_system_error());
        }
    }
#endif
    return {};
}

std::expected<void, std::error_code>
SocketDevice::shutdownWrite()
{
#ifdef SOLID_ON_WINDOWS
    if (ok()) {
        shutdown(descriptor(), SD_SEND);
    }
#else
    if (ok()) {
        if (shutdown(descriptor(), SHUT_WR) != 0) {
            return std::unexpected(last_system_error());
        }
    }
#endif
    return {};
}

std::expected<void, std::error_code>
SocketDevice::shutdownReadWrite()
{
#ifdef SOLID_ON_WINDOWS
    if (ok()) {
        shutdown(descriptor(), SD_BOTH);
    }
#else
    if (ok()) {
        if (shutdown(descriptor(), SHUT_RDWR) != 0) {
            return std::unexpected(last_system_error());
        }
    }
#endif
    return {};
}

void SocketDevice::close()
{
    if (ok()) {
        std::ignore = shutdownReadWrite();
#ifdef SOLID_ON_WINDOWS
        closesocket(descriptor());
        Device::descriptor((HANDLE)invalidDescriptor(), false);
#else
        Device::close();
#endif
    }
}

std::expected<void, std::error_code>
SocketDevice::create(const ResolveIterator& _rri)
{
    return create(static_cast<SocketInfo::Family>(_rri.family()), static_cast<SocketInfo::Type>(_rri.type()), _rri.protocol());
}

std::expected<void, std::error_code>
SocketDevice::create(
    SocketInfo::Family _family,
    SocketInfo::Type   _type,
    int                _proto)
{
#ifdef SOLID_ON_WINDOWS
    // NOTE: must use WSASocket instead of socket because
    // the latter seems to create the socket with OVERLAPPED support
    // which will not work with above WriteFile for synchronous IO
    SOCKET s = WSASocketW(_family, _type, _proto, nullptr, 0, WSA_FLAG_OVERLAPPED);
    Device::descriptor((HANDLE)s);
    if (!ok()) {
        return std::unexpected(last_socket_error());
    }
    std::ignore = enableLoopbackFastPath();
    return {};
#else
    Device::descriptor(::socket(_family, _type, _proto));
    if (!ok()) {
        return std::unexpected(last_socket_error());
    }
    return {};
#endif
}

std::expected<void, std::error_code>
SocketDevice::connect(const SocketAddressStub& _rsas)
{
    if (::connect(descriptor(), _rsas.sockAddr(), _rsas.size()) == 0) {
        return {};
    }
    return std::unexpected(last_socket_error());
}

std::expected<void, std::error_code>
SocketDevice::prepareAccept(const SocketAddressStub& _rsas, size_t _listencnt)
{
    int yes = 1;
    if (::setsockopt(descriptor(), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes)) < 0) {
        return std::unexpected(last_socket_error());
    }

    if (::bind(descriptor(), _rsas.sockAddr(), _rsas.size()) < 0) {
        return std::unexpected(last_socket_error());
    }

    if (::listen(descriptor(), static_cast<int>(_listencnt)) < 0) {
        return std::unexpected(last_socket_error());
    }
    return {};
}

std::expected<SocketDevice, std::error_code>
SocketDevice::accept()
{
    SocketDevice dev;
#if defined(SOLID_ON_WINDOWS)
    const SOCKET rv = ::accept(descriptor(), nullptr, nullptr);
    if (rv == INVALID_SOCKET) {
        return std::unexpected(last_socket_error());
    }
    dev.Device::descriptor((HANDLE)rv);
    std::ignore = dev.enableLoopbackFastPath();
#else
    const int rv = ::accept(descriptor(), nullptr, nullptr);
    if (rv < 0) {
        return std::unexpected(last_socket_error());
    }
    dev.Device::descriptor(rv);
#endif
    return dev;
}

std::expected<void, std::error_code>
SocketDevice::bind(const SocketAddressStub& _rsa)
{
    if (::bind(descriptor(), _rsa.sockAddr(), _rsa.size()) == 0) {
        return {};
    }
    return std::unexpected(last_socket_error());
}

std::expected<void, std::error_code>
SocketDevice::makeBlocking()
{
#ifdef SOLID_ON_WINDOWS
    u_long mode = 0;
    if (ioctlsocket(descriptor(), FIONBIO, &mode) != NO_ERROR) {
        return std::unexpected(last_socket_error());
    }
    return {};
#else
    const int flg = fcntl(descriptor(), F_GETFL);
    if (flg < 0) {
        return std::unexpected(last_socket_error());
    }
    if (fcntl(descriptor(), F_SETFL, flg & ~O_NONBLOCK) < 0) {
        return std::unexpected(last_socket_error());
    }
    return {};
#endif
}

std::expected<void, std::error_code>
SocketDevice::makeBlocking(size_t _msec)
{
    if (const auto rv = makeBlocking(); !rv) {
        return rv;
    }
#ifdef SOLID_ON_WINDOWS
    DWORD tout(static_cast<DWORD>(_msec));
    if (setsockopt(descriptor(), SOL_SOCKET, SO_RCVTIMEO, (char*)&tout, sizeof(tout)) == SOCKET_ERROR) {
        return std::unexpected(last_socket_error());
    }
    tout = static_cast<DWORD>(_msec);
    if (setsockopt(descriptor(), SOL_SOCKET, SO_SNDTIMEO, (char*)&tout, sizeof(tout)) == SOCKET_ERROR) {
        return std::unexpected(last_socket_error());
    }
    return {};
#else
    struct timeval timeout;
    timeout.tv_sec  = static_cast<decltype(timeout.tv_sec)>(_msec / 1000);
    timeout.tv_usec = static_cast<decltype(timeout.tv_usec)>((_msec % 1000) * 1000);
    if (setsockopt(descriptor(), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) != 0) {
        return std::unexpected(last_socket_error());
    }
    if (setsockopt(descriptor(), SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) != 0) {
        return std::unexpected(last_socket_error());
    }
    return {};
#endif
}

std::expected<void, std::error_code>
SocketDevice::makeNonBlocking()
{
#ifdef SOLID_ON_WINDOWS
    u_long mode = 1;
    if (ioctlsocket(descriptor(), FIONBIO, &mode) == NO_ERROR) {
        return {};
    }
    return std::unexpected(last_socket_error());
#else
    const int flg = fcntl(descriptor(), F_GETFL);
    if (flg < 0) {
        return std::unexpected(last_socket_error());
    }
    if (fcntl(descriptor(), F_SETFL, flg | O_NONBLOCK) >= 0) {
        return {};
    }
    return std::unexpected(last_socket_error());
#endif
}

std::expected<bool, std::error_code>
SocketDevice::isBlocking() const
{
#ifdef SOLID_ON_WINDOWS
    return std::unexpected(solid::error_not_implemented);
#else
    const int flg = fcntl(descriptor(), F_GETFL);

    if (flg < 0) {
        return std::unexpected(last_socket_error());
    }
    return (flg & O_NONBLOCK) == 0;
#endif
}

std::expected<std::size_t, std::error_code>
SocketDevice::send(std::span<const std::byte> _buf, unsigned _flags)
{
    const auto rv = ::send(descriptor(), sock_buf(_buf), sock_len(_buf.size()), static_cast<int>(_flags));
    if (rv < 0) {
        return std::unexpected(last_socket_error());
    }
    return static_cast<std::size_t>(rv);
}

std::expected<std::size_t, std::error_code>
SocketDevice::recv(std::span<std::byte> _buf, unsigned _flags)
{
    const auto rv = ::recv(descriptor(), sock_buf(_buf), sock_len(_buf.size()), static_cast<int>(_flags));
    if (rv < 0) {
        return std::unexpected(last_socket_error());
    }
    return static_cast<std::size_t>(rv);
}

std::expected<std::size_t, std::error_code>
SocketDevice::send(std::span<const std::byte> _buf, const SocketAddressStub& _sap)
{
    const auto rv = ::sendto(descriptor(), sock_buf(_buf), sock_len(_buf.size()), 0, _sap.sockAddr(), _sap.size());
    if (rv < 0) {
        return std::unexpected(last_socket_error());
    }
    return static_cast<std::size_t>(rv);
}

std::expected<std::size_t, std::error_code>
SocketDevice::recv(std::span<std::byte> _buf, SocketAddress& _rsa)
{
    _rsa.clear();
    _rsa.sz       = SocketAddress::capacity;
    const auto rv = ::recvfrom(descriptor(), sock_buf(_buf), sock_len(_buf.size()), 0, _rsa.sockAddr(), &_rsa.sz);
    if (rv < 0) {
        return std::unexpected(last_socket_error());
    }
    return static_cast<std::size_t>(rv);
}

std::expected<SocketAddress, std::error_code>
SocketDevice::remoteAddress() const
{
    SocketAddress sa;
    sa.sz = SocketAddress::capacity;
    if (getpeername(descriptor(), sa.sockAddr(), &sa.sz) != 0) {
        return std::unexpected(last_socket_error());
    }
    return sa;
}

std::expected<SocketAddress, std::error_code>
SocketDevice::localAddress() const
{
    SocketAddress sa;
    sa.sz = SocketAddress::capacity;
    if (getsockname(descriptor(), sa.sockAddr(), &sa.sz) != 0) {
        return std::unexpected(last_socket_error());
    }
    return sa;
}

std::expected<int, std::error_code>
SocketDevice::type() const
{
#ifdef SOLID_ON_WINDOWS
    return std::unexpected(solid::error_not_implemented);
#else
    int       val   = 0;
    socklen_t valsz = sizeof(int);
    if (getsockopt(descriptor(), SOL_SOCKET, SO_TYPE, &val, &valsz) != 0) {
        return std::unexpected(last_socket_error());
    }
    return val;
#endif
}

std::expected<void, std::error_code>
SocketDevice::enableLoopbackFastPath()
{
#if defined(SOLID_ON_WINDOWS)
    uint32_t param = 1;
    DWORD    ret;
    int      rv = WSAIoctl(descriptor(), SIO_LOOPBACK_FAST_PATH /*_WSAIOW(IOC_VENDOR, 16)*/,
        &param, sizeof(param), NULL, 0, &ret, 0, 0);
    if (rv == 0) {
        return {};
    }
    return std::unexpected(last_socket_error());
#else
    return {};
#endif
}

namespace {

template <class T>
std::expected<void, std::error_code>
set_socket_option(const SocketDevice::DescriptorT _desc, const int _level, const int _option, const T _value)
{
    if (setsockopt(_desc, _level, _option, reinterpret_cast<const char*>(&_value), sizeof(_value)) == 0) {
        return {};
    }
    return std::unexpected(last_socket_error());
}

template <class T>
std::expected<T, std::error_code>
get_socket_option(const SocketDevice::DescriptorT _desc, const int _level, const int _option)
{
    T         value{};
    socklen_t sz = sizeof(value);
    if (getsockopt(_desc, _level, _option, reinterpret_cast<char*>(&value), &sz) == 0) {
        return value;
    }
    return std::unexpected(last_socket_error());
}

} // namespace

std::expected<void, std::error_code>
SocketDevice::enableNoDelay()
{
#if defined(SOLID_ON_WINDOWS)
    return set_socket_option<BOOL>(descriptor(), IPPROTO_TCP, TCP_NODELAY, TRUE);
#else
    return set_socket_option<int>(descriptor(), IPPROTO_TCP, TCP_NODELAY, 1);
#endif
}

std::expected<void, std::error_code>
SocketDevice::disableNoDelay()
{
#if defined(SOLID_ON_WINDOWS)
    return set_socket_option<BOOL>(descriptor(), IPPROTO_TCP, TCP_NODELAY, FALSE);
#else
    return set_socket_option<int>(descriptor(), IPPROTO_TCP, TCP_NODELAY, 0);
#endif
}

std::expected<bool, std::error_code>
SocketDevice::hasNoDelay() const
{
    return get_socket_option<int>(descriptor(), IPPROTO_TCP, TCP_NODELAY).transform([](const int _flag) { return _flag != 0; });
}

std::expected<void, std::error_code>
SocketDevice::enableNoSignal()
{
#if defined(SOLID_ON_WINDOWS) || defined(SOLID_ON_DARWIN)
    return std::unexpected(solid::error_not_implemented);
#else
    return set_socket_option<int>(descriptor(), SOL_SOCKET, MSG_NOSIGNAL, 1);
#endif
}

std::expected<void, std::error_code>
SocketDevice::disableNoSignal()
{
#if defined(SOLID_ON_WINDOWS) || defined(SOLID_ON_DARWIN)
    return std::unexpected(solid::error_not_implemented);
#else
    return set_socket_option<int>(descriptor(), SOL_SOCKET, MSG_NOSIGNAL, 0);
#endif
}

std::expected<void, std::error_code>
SocketDevice::enableLinger()
{
    return std::unexpected(solid::error_not_implemented);
}

std::expected<void, std::error_code>
SocketDevice::disableLinger()
{
    return std::unexpected(solid::error_not_implemented);
}

std::expected<void, std::error_code>
SocketDevice::enableCork()
{
#if defined(SOLID_ON_LINUX)
    return set_socket_option<int>(descriptor(), IPPROTO_TCP, TCP_CORK, 1);
#else
    return std::unexpected(solid::error_not_implemented);
#endif
}

std::expected<void, std::error_code>
SocketDevice::disableCork()
{
#if defined(SOLID_ON_LINUX)
    return set_socket_option<int>(descriptor(), IPPROTO_TCP, TCP_CORK, 0);
#else
    return std::unexpected(solid::error_not_implemented);
#endif
}

std::expected<bool, std::error_code>
SocketDevice::hasCork() const
{
#if defined(SOLID_ON_LINUX)
    return get_socket_option<int>(descriptor(), IPPROTO_TCP, TCP_CORK).transform([](const int _flag) { return _flag != 0; });
#else
    return std::unexpected(solid::error_not_implemented);
#endif
}

std::expected<void, std::error_code>
SocketDevice::sendBufferSize(int _sz)
{
    return set_socket_option<int>(descriptor(), SOL_SOCKET, SO_SNDBUF, _sz);
}

std::expected<void, std::error_code>
SocketDevice::recvBufferSize(int _sz)
{
    return set_socket_option<int>(descriptor(), SOL_SOCKET, SO_RCVBUF, _sz);
}

std::expected<int, std::error_code>
SocketDevice::sendBufferSize() const
{
    return get_socket_option<int>(descriptor(), SOL_SOCKET, SO_SNDBUF);
}

std::expected<int, std::error_code>
SocketDevice::recvBufferSize() const
{
    return get_socket_option<int>(descriptor(), SOL_SOCKET, SO_RCVBUF);
}

std::error_code
SocketDevice::error() const
{
    return error(descriptor());
}

/*static*/ std::error_code
SocketDevice::error(const DescriptorT _des)
{
    int       val   = 0;
    socklen_t valsz = sizeof(int);
    if (getsockopt(_des, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&val), &valsz) == 0) {
        return std::error_code(val, std::system_category());
    }
    return last_socket_error();
}

/*static*/ bool
SocketDevice::can_wait_connect(const std::error_code& _err) noexcept
{
#ifdef SOLID_ON_WINDOWS
    return is_system_error(_err) && _err.value() == WSAEWOULDBLOCK;
#else
    return _err == std::errc::operation_in_progress;
#endif
}

/*static*/ bool
SocketDevice::can_retry_io(const std::error_code& _err) noexcept
{
#ifdef SOLID_ON_WINDOWS
    return is_system_error(_err) && _err.value() == WSAEWOULDBLOCK;
#else
    return _err == std::errc::resource_unavailable_try_again || _err == std::errc::operation_would_block;
#endif
}

/*static*/ bool
SocketDevice::can_retry_accept(const std::error_code& _err) noexcept
{
#ifdef SOLID_ON_WINDOWS
    return is_system_error(_err) && _err.value() == WSAEWOULDBLOCK;
#else
    if (!is_system_error(_err)) {
        return false;
    }
    switch (_err.value()) {
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
    case ENETDOWN:
    case EPROTO:
    case ENOPROTOOPT:
    case EHOSTDOWN:
#ifdef ENONET
    case ENONET:
#endif
    case EHOSTUNREACH:
    case EOPNOTSUPP:
    case ENETUNREACH:
        return true;
    default:
        return false;
    }
#endif
}

std::ostream&
operator<<(std::ostream& _ros, const LocalAddressPlot& _ra)
{
    if (const auto sa = _ra.rsd.localAddress()) {
        _ros << *sa;
    }
    return _ros;
}

std::ostream&
operator<<(std::ostream& _ros, const RemoteAddressPlot& _ra)
{
    if (const auto sa = _ra.rsd.remoteAddress()) {
        _ros << *sa;
    }
    return _ros;
}

std::ostream&
operator<<(std::ostream& _ros, const LocalEndpointPlot& _ra)
{
    if (const auto sa = _ra.rsd.localAddress()) {
        _ros << *sa << ':' << sa->port();
    }
    return _ros;
}

std::ostream&
operator<<(std::ostream& _ros, const RemoteEndpointPlot& _ra)
{
    if (const auto sa = _ra.rsd.remoteAddress()) {
        _ros << *sa << ':' << sa->port();
    }
    return _ros;
}

/*static*/ size_t
SocketInfo::max_listen_backlog_size()
{
    return 128; // TODO: try take the value from the system
}

} // namespace solid
