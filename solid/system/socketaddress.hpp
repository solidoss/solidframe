// solid/system/socketaddress.hpp
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

#ifndef SOLID_ON_WINDOWS
#include <arpa/inet.h>
#include <sys/un.h>
#endif

#include <array>
#include <cstring>
#include <expected>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>

#include "solid/system/socketinfo.hpp"

#include <memory>

#ifndef SOLID_HAS_NO_INLINES
#include "solid/system/cassert.hpp"
#endif

namespace solid {

class SocketDevice;
// struct sockaddr_in;
// struct sockaddr_in6;
//==================================================================
//! An interator for POSIX addrinfo (see man getaddrinfo)
/*!
    Usually it will hold all data needed for creating and connecting
    a socket. Use ResolverData::begin() to get started.
*/
struct ResolveIterator {
    ResolveIterator();
    ResolveIterator& next();
    int              family() const;
    int              type() const;
    int              protocol() const;
    size_t           size() const;
    sockaddr*        sockAddr() const;
                     operator bool() const;
    ResolveIterator& operator++();
    bool             operator==(const ResolveIterator& _rrit) const;

private:
    friend struct ResolveData;
    ResolveIterator(addrinfo* _pa);
    const addrinfo* paddr;
};
//==================================================================
struct DirectResoveInfo {
    enum FlagsE {
#ifndef SOLID_ON_WINDOWS
        CannonName     = AI_CANONNAME,
        All            = AI_ALL,
        AddrConfig     = AI_ADDRCONFIG,
        V4Mapped       = AI_V4MAPPED,
        NumericHost    = AI_NUMERICHOST,
        NumericService = AI_NUMERICSERV,
#else
        CannonName,
        All,
        AddrConfig,
        V4Mapped,
        NumericHost,
        NumericService,
#endif
        Numeric = NumericHost | NumericService,
    };
};
//! A shared pointer for POSIX addrinfo (see man getaddrinfo)
/*!
    Use synchronous_resolve to create ResolveData objects.
*/
struct ResolveData {
    typedef ResolveIterator const_iterator;

    ResolveData() = default;
    ResolveData(addrinfo* _pai);
    ResolveData(const ResolveData& _rai);
    ResolveData(ResolveData&& _rai) noexcept;

    ~ResolveData();
    //! Get an iterator to he first resolved ip address
    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] const_iterator end() const;
    //! Check if the returned list of ip addresses is empty
    [[nodiscard]] bool empty() const;
    void               clear();
    ResolveData&       operator=(const ResolveData& _rrd);
    ResolveData&       operator=(ResolveData&& _rrd) noexcept;

private:
    static void                       delete_addrinfo(void* _pv);
    typedef std::shared_ptr<addrinfo> AddrInfoSharedPtrT;
    AddrInfoSharedPtrT                aiptr;
};

//! The error category for name resolution errors (getaddrinfo/getnameinfo)
const std::error_category& resolve_category() noexcept;

//! Resolve a node (host) and service (port) - on failure returns an error from resolve_category() or the system category
[[nodiscard]] std::expected<ResolveData, std::error_code> synchronous_resolve(std::string_view _node, std::string_view _service);
[[nodiscard]] std::expected<ResolveData, std::error_code> synchronous_resolve(
    std::string_view _node,
    std::string_view _service,
    int              _flags,
    int              _family = -1,
    int              _type   = -1,
    int              _proto  = -1);

[[nodiscard]] std::expected<ResolveData, std::error_code> synchronous_resolve(std::string_view _node, int _port);
[[nodiscard]] std::expected<ResolveData, std::error_code> synchronous_resolve(
    std::string_view _node,
    int              _port,
    int              _flags,
    int              _family = -1,
    int              _type   = -1,
    int              _proto  = -1);
struct SocketAddressStub;
struct ReverseResolveInfo {
    enum FlagsE {
#ifndef SOLID_ON_WINDOWS
        NameRequest    = NI_NAMEREQD,
        Datagram       = NI_DGRAM,
        NoFQDN         = NI_NOFQDN,
        NumericHost    = NI_NUMERICHOST,
        NumericService = NI_NUMERICSERV,
#else
        NameRequest,
        Datagram,
        NoFQDN,
        NumericHost,
        NumericService,
#endif
        Numeric = NumericHost | NumericService,
    };
};
//! The result of a reverse resolve: the host and the service names
struct ReverseResolveData {
    std::string host;
    std::string service;
};
//! Reverse resolve a socket address into host and service names
[[nodiscard]] std::expected<ReverseResolveData, std::error_code> synchronous_resolve(const SocketAddressStub& _rsa, int _flags = 0);
//==================================================================
struct SocketAddress;
struct SocketAddressInet;
struct SocketAddressInet4;
struct SocketAddressInet6;
struct SocketAddressLocal;
#ifdef SOLID_ON_WINDOWS
typedef int socklen_t;
#endif
//! A pair of a sockaddr pointer and a size
/*!
    It is a commodity structure, it will not allocate data
    for sockaddr pointer nor it will delete it. Use this
    structure with SocketAddress and ResolveIterator
*/
struct SocketAddressStub {
    SocketAddressStub(sockaddr* _pa = nullptr, size_t _sz = 0);

    SocketAddressStub(const ResolveIterator& _it);
    SocketAddressStub(const SocketAddress& _rsa);
    SocketAddressStub(const SocketAddressInet& _rsa);
    SocketAddressStub(const SocketAddressInet4& _rsa);
    SocketAddressStub(const SocketAddressInet6& _rsa);
    SocketAddressStub(const SocketAddressLocal& _rsa);

    SocketAddressStub& operator=(const ResolveIterator& _it);
    SocketAddressStub& operator=(const SocketAddress& _rsa);
    SocketAddressStub& operator=(const SocketAddressInet& _rsa);
    SocketAddressStub& operator=(const SocketAddressInet4& _rsa);
    SocketAddressStub& operator=(const SocketAddressInet6& _rsa);
    SocketAddressStub& operator=(const SocketAddressLocal& _rsa);

    operator const sockaddr*() const;

    void clear();

    [[nodiscard]] SocketInfo::Family family() const;

    [[nodiscard]] bool isInet4() const;
    [[nodiscard]] bool isInet6() const;
    [[nodiscard]] bool isLocal() const;

    [[nodiscard]] socklen_t size() const;
    [[nodiscard]] int       port() const;

    [[nodiscard]] const sockaddr* sockAddr() const;

private:
    const sockaddr* addr = nullptr;
    socklen_t       sz   = 0;
};
//==================================================================
//! Holds a generic socket address
/*!
    On unix it can be either: inet_v4, inet_v6 or unix/local address
*/
struct SocketAddress {
private:
    union AddrUnion {
        sockaddr     addr;
        sockaddr_in  inaddr4;
        sockaddr_in6 inaddr6;
#ifndef SOLID_ON_WINDOWS
        sockaddr_un localaddr;
#endif
    };

public:
    static constexpr std::size_t capacity{sizeof(AddrUnion)};

    SocketAddress();
    SocketAddress(const SocketAddressStub&);
    SocketAddress(std::string_view _addr, int _port);
    SocketAddress(std::string_view _path);

    SocketAddress& operator=(const SocketAddressStub&);

    [[nodiscard]] SocketInfo::Family family() const;

    [[nodiscard]] bool isInet4() const;
    [[nodiscard]] bool isInet6() const;
    [[nodiscard]] bool isLocal() const;

    [[nodiscard]] bool isLoopback() const;
    [[nodiscard]] bool isInvalid() const;

    [[nodiscard]] bool empty() const;

    [[nodiscard]] const socklen_t& size() const;

    [[nodiscard]] const sockaddr* sockAddr() const;
                                  operator const sockaddr*() const;

    bool operator<(const SocketAddress& _raddr) const;
    bool operator==(const SocketAddress& _raddr) const;

    void address(std::string_view _str);

    [[nodiscard]] const in_addr&  address4() const;
    [[nodiscard]] const in6_addr& address6() const;
    [[nodiscard]] int             port() const;
    bool                          port(int _port);
    void                          clear();
    [[nodiscard]] size_t          hash() const;
    [[nodiscard]] size_t          addressHash() const;

    void             path(std::string_view _pth);
    std::string_view path() const;

private:
    friend class SocketDevice;
              operator sockaddr*();
    sockaddr* sockAddr();
    AddrUnion d;
    socklen_t sz;
};
//==================================================================
struct SocketAddressInet {
private:
    union AddrUnion {
        sockaddr     addr;
        sockaddr_in  inaddr4;
        sockaddr_in6 inaddr6;
    };

public:
    static constexpr std::size_t capacity{sizeof(AddrUnion)};

    typedef std::array<uint8_t, 4>  DataArray4T;
    typedef std::array<uint8_t, 16> DataArray6T;

    SocketAddressInet();
    SocketAddressInet(const SocketAddressStub&);
    SocketAddressInet(std::string_view _addr, int _port = 0);

    SocketAddressInet& operator=(const SocketAddressStub&);

    [[nodiscard]] SocketInfo::Family family() const;

    [[nodiscard]] bool isInet4() const;
    [[nodiscard]] bool isInet6() const;

    [[nodiscard]] bool isLoopback() const;
    [[nodiscard]] bool isInvalid() const;

    [[nodiscard]] bool empty() const;

    [[nodiscard]] const socklen_t& size() const;

    [[nodiscard]] const sockaddr* sockAddr() const;
                                  operator const sockaddr*() const;

    bool toBinary(DataArray4T& _bin, uint16_t& _port) const;
    bool toBinary(DataArray6T& _bin, uint16_t& _port) const;
    void fromBinary(const DataArray4T& _bin, uint16_t _port = 0);
    void fromBinary(const DataArray6T& _bin, uint16_t _port = 0);

    bool operator<(const SocketAddressInet& _raddr) const;
    bool operator==(const SocketAddressInet& _raddr) const;

    void address(std::string_view _str);

    [[nodiscard]] const in_addr&  address4() const;
    [[nodiscard]] const in6_addr& address6() const;

    [[nodiscard]] int    port() const;
    bool                 port(int _port);
    void                 clear();
    [[nodiscard]] size_t hash() const;
    [[nodiscard]] size_t addressHash() const;

private:
    friend class SocketDevice;
              operator sockaddr*();
    sockaddr* sockAddr();
    AddrUnion d;
    socklen_t sz;
};
//==================================================================
struct SocketAddressInet4 {
private:
    union AddrUnion {
        sockaddr    addr;
        sockaddr_in inaddr4;
    };

public:
    static constexpr std::size_t   capacity{sizeof(AddrUnion)};
    typedef std::array<uint8_t, 4> DataArrayT;

    SocketAddressInet4();
    SocketAddressInet4(const SocketAddressStub&);
    SocketAddressInet4(std::string_view _addr, int _port = 0);
    SocketAddressInet4(uint32_t _addr, int _port);
    SocketAddressInet4(const DataArrayT& _addr, int _port = 0);

    // SocketAddressInet4& operator=(const ResolveIterator &);
    SocketAddressInet4& operator=(const SocketAddressStub&);

    [[nodiscard]] SocketInfo::Family family() const;
    [[nodiscard]] bool               isInet4() const;

    [[nodiscard]] bool isLoopback() const;
    [[nodiscard]] bool isInvalid() const;

    [[nodiscard]] socklen_t size() const;

    [[nodiscard]] const sockaddr* sockAddr() const;
    // operator sockaddr*(){return sockAddr();}
    operator const sockaddr*() const;

    void toBinary(DataArrayT& _bin, uint16_t& _port) const;

    void toUInt(uint32_t& _addr, uint16_t& _port) const;

    void fromBinary(const DataArrayT& _bin, uint16_t _port = 0);
    void fromUInt(uint32_t _addr, uint16_t _port = 0);

    bool operator<(const SocketAddressInet4& _raddr) const;
    bool operator==(const SocketAddressInet4& _raddr) const;

    void address(std::string_view _str);

    [[nodiscard]] const in_addr& address() const;

    [[nodiscard]] int    port() const;
    void                 port(int _port);
    void                 clear();
    [[nodiscard]] size_t hash() const;
    [[nodiscard]] size_t addressHash() const;

private:
    friend class SocketDevice;
              operator sockaddr*();
    sockaddr* sockAddr();
    AddrUnion d;
};
//==================================================================
struct SocketAddressInet6 {
private:
    union AddrUnion {
        sockaddr     addr;
        sockaddr_in6 inaddr6;
    };

public:
    static constexpr std::size_t    capacity{sizeof(AddrUnion)};
    typedef std::array<uint8_t, 16> DataArrayT;

    SocketAddressInet6();
    SocketAddressInet6(const SocketAddressStub&);
    SocketAddressInet6(std::string_view _addr, int _port = 0);
    SocketAddressInet6(const DataArrayT& _addr, int _port = 0);

    SocketAddressInet6& operator=(const SocketAddressStub&);

    [[nodiscard]] SocketInfo::Family family() const;
    [[nodiscard]] bool               isInet6() const;

    [[nodiscard]] bool isLoopback() const;
    [[nodiscard]] bool isInvalid() const;

    [[nodiscard]] bool empty() const;

    [[nodiscard]] socklen_t size() const;

    [[nodiscard]] const sockaddr* sockAddr() const;
                                  operator const sockaddr*() const;

    void toBinary(DataArrayT& _bin, uint16_t& _port) const;

    void fromBinary(const DataArrayT& _bin, uint16_t _port = 0);

    bool operator<(const SocketAddressInet6& _raddr) const;
    bool operator==(const SocketAddressInet6& _raddr) const;

    void address(std::string_view _str);

    [[nodiscard]] const in6_addr& address() const;

    [[nodiscard]] int    port() const;
    void                 port(int _port);
    void                 clear();
    [[nodiscard]] size_t hash() const;
    [[nodiscard]] size_t addressHash() const;

private:
    friend class SocketDevice;
              operator sockaddr*();
    sockaddr* sockAddr();
    AddrUnion d;
};
//==================================================================
bool operator<(const in_addr& _inaddr1, const in_addr& _inaddr2);

bool operator==(const in_addr& _inaddr1, const in_addr& _inaddr2);

bool operator<(const in6_addr& _inaddr1, const in6_addr& _inaddr2);

bool operator==(const in6_addr& _inaddr1, const in6_addr& _inaddr2);

size_t in_addr_hash(const in_addr& _inaddr);

size_t in_addr_hash(const in6_addr& _inaddr);

std::ostream& operator<<(std::ostream& _ros, const SocketAddressInet4& _rsa);

std::ostream& operator<<(std::ostream& _ros, const SocketAddressInet& _rsa);

std::ostream& operator<<(std::ostream& _ros, const SocketAddress& _rsa);

//==================================================================
#ifndef SOLID_ON_WINDOWS
struct SocketAddressLocal {
private:
    union AddrUnion {
        sockaddr    addr;
        sockaddr_un localaddr;
    };

public:
    static constexpr std::size_t capacity{sizeof(AddrUnion)};

    SocketAddressLocal();
    SocketAddressLocal(std::string_view _path);

    SocketAddressLocal& operator=(const SocketAddressStub&);

    [[nodiscard]] SocketInfo::Family family() const;

    [[nodiscard]] bool empty() const;

    [[nodiscard]] const socklen_t& size() const;

    [[nodiscard]] const sockaddr* sockAddr() const;
                                  operator const sockaddr*() const;

    bool operator<(const SocketAddressLocal& _raddr) const;
    bool operator==(const SocketAddressLocal& _raddr) const;

    void                 clear();
    [[nodiscard]] size_t hash() const;
    [[nodiscard]] size_t addressHash() const;

    void             path(std::string_view _pth);
    std::string_view path() const;

private:
    friend class SocketDevice;
              operator sockaddr*();
    sockaddr* sockAddr();
    AddrUnion d;
    socklen_t sz = 0;
};
#endif
//==================================================================
#ifndef SOLID_HAS_NO_INLINES
#include "solid/system/socketaddress.ipp"
#endif
//==================================================================

} // namespace solid
