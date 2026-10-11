// solid/system/socketdevice.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/device.hpp"
#include "solid/system/error.hpp"
#include "solid/system/socketaddress.hpp"

namespace solid {

//! A wrapper for berkeley sockets
class SocketDevice : public Device {
public:
#ifdef SOLID_ON_WINDOWS
    using DescriptorT = SOCKET;
#else
    using DescriptorT = int;
#endif

    //! Move constructor
    SocketDevice(SocketDevice&& _sd) noexcept;
    //! Basic constructor
    SocketDevice();
    //! Destructor
    ~SocketDevice();
    //! Move assign operator
    SocketDevice& operator=(SocketDevice&& _dev) noexcept;

    SocketDevice(const SocketDevice&)            = delete;
    SocketDevice& operator=(const SocketDevice&) = delete;

    //! Shutdown reading
    [[nodiscard]] std::expected<void, std::error_code> shutdownRead();
    //! Shutdown writing
    [[nodiscard]] std::expected<void, std::error_code> shutdownWrite();
    //! Shutdown reading and writing
    [[nodiscard]] std::expected<void, std::error_code> shutdownReadWrite();
    //! Create a socket based ResolveIterator
    [[nodiscard]] std::expected<void, std::error_code> create(const ResolveIterator& _rri);
    //! Create a socket given its family, its type and its protocol type
    [[nodiscard]] std::expected<void, std::error_code> create(
        SocketInfo::Family      = SocketInfo::Inet4,
        SocketInfo::Type _type  = SocketInfo::Stream,
        int              _proto = 0);
    //! Connect the socket
    /*!
        On failure, use can_wait_connect(error) to check whether the connection
        is in progress (non-blocking socket) and can be waited for.
    */
    [[nodiscard]] std::expected<void, std::error_code> connect(const SocketAddressStub& _rsas);
    //! Bind the socket to a specific addr:port
    [[nodiscard]] std::expected<void, std::error_code> bind(const SocketAddressStub& _rsa);
    //! Prepares the socket for accepting
    [[nodiscard]] std::expected<void, std::error_code> prepareAccept(const SocketAddressStub& _rsas, size_t _listencnt = 10);
    //! Accept an incoming connection
    /*!
        On failure, use can_retry_accept(error) to check whether the accept can be retried.
    */
    [[nodiscard]] std::expected<SocketDevice, std::error_code> accept();
    //! Make a connection blocking
    /*!
        \param _msec will make socket blocking with the given amount of milliseconds as timeout
    */
    [[nodiscard]] std::expected<void, std::error_code> makeBlocking(size_t _msec);
    [[nodiscard]] std::expected<void, std::error_code> makeBlocking();
    //! Make the socket nonblocking
    [[nodiscard]] std::expected<void, std::error_code> makeNonBlocking();
    //! Check if its blocking
    [[nodiscard]] std::expected<bool, std::error_code> isBlocking() const;

    [[nodiscard]] std::expected<void, std::error_code> enableNoDelay();
    [[nodiscard]] std::expected<void, std::error_code> disableNoDelay();
    [[nodiscard]] std::expected<bool, std::error_code> hasNoDelay() const;

    [[nodiscard]] std::expected<void, std::error_code> enableNoSignal();
    [[nodiscard]] std::expected<void, std::error_code> disableNoSignal();

    [[nodiscard]] std::expected<void, std::error_code> enableLinger();
    [[nodiscard]] std::expected<void, std::error_code> disableLinger();

    [[nodiscard]] std::expected<void, std::error_code> enableCork(); // TCP_CORK - only on linux, TCP_NOPUSH on FreeBSD
    [[nodiscard]] std::expected<void, std::error_code> disableCork();
    [[nodiscard]] std::expected<bool, std::error_code> hasCork() const;

    [[nodiscard]] std::expected<void, std::error_code> enableLoopbackFastPath();

    //! Set the send buffer size
    [[nodiscard]] std::expected<void, std::error_code> sendBufferSize(int _sz);
    //! Set the receive buffer size
    [[nodiscard]] std::expected<void, std::error_code> recvBufferSize(int _sz);
    //! Get the send buffer size
    [[nodiscard]] std::expected<int, std::error_code> sendBufferSize() const;
    //! Get the receive buffer size
    [[nodiscard]] std::expected<int, std::error_code> recvBufferSize() const;

    //! Write data on socket - returns the number of bytes sent
    /*!
        On failure, use can_retry_io(error) to check whether the operation can be retried.
    */
    [[nodiscard]] std::expected<std::size_t, std::error_code> send(std::span<const std::byte> _buf, unsigned _flags = 0);
    //! Reads data from a socket - returns the number of bytes received (0 on orderly shutdown)
    /*!
        On failure, use can_retry_io(error) to check whether the operation can be retried.
    */
    [[nodiscard]] std::expected<std::size_t, std::error_code> recv(std::span<std::byte> _buf, unsigned _flags = 0);
    //! Send a datagram to a socket - returns the number of bytes sent
    [[nodiscard]] std::expected<std::size_t, std::error_code> send(std::span<const std::byte> _buf, const SocketAddressStub& _sap);
    //! Receive a datagram from a socket - returns the number of bytes received and fills _rsa with the sender address
    [[nodiscard]] std::expected<std::size_t, std::error_code> recv(std::span<std::byte> _buf, SocketAddress& _rsa);
    //! Gets the remote address for a connected socket
    [[nodiscard]] std::expected<SocketAddress, std::error_code> remoteAddress() const;
    //! Gets the local address for a socket
    [[nodiscard]] std::expected<SocketAddress, std::error_code> localAddress() const;

    void close();

    //! The pending socket error (SO_ERROR) for the given descriptor
    static std::error_code error(DescriptorT _des);
    //! The pending socket error (SO_ERROR)
    [[nodiscard]] std::error_code error() const;

    //! Get the socket type
    [[nodiscard]] std::expected<int, std::error_code> type() const;

    //! Check if a failed connect is actually in progress and can be waited for
    static bool can_wait_connect(const std::error_code& _err) noexcept;
    //! Check if a failed accept may succeed on retry
    static bool can_retry_accept(const std::error_code& _err) noexcept;
    //! Check if a failed send/recv may succeed on retry (i.e. would block)
    static bool can_retry_io(const std::error_code& _err) noexcept;
#ifdef SOLID_ON_WINDOWS
    static const DescriptorT invalidDescriptor()
    {
        return INVALID_SOCKET;
    }
    DescriptorT descriptor() const { return reinterpret_cast<DescriptorT>(Device::descriptor()); }
    bool        ok() const
    {
        return descriptor() != invalidDescriptor();
    }
#endif
};

struct LocalAddressPlot {
    const SocketDevice& rsd;
    LocalAddressPlot(const SocketDevice& _rsd)
        : rsd(_rsd)
    {
    }
};

struct LocalEndpointPlot {
    const SocketDevice& rsd;
    LocalEndpointPlot(const SocketDevice& _rsd)
        : rsd(_rsd)
    {
    }
};

inline LocalAddressPlot local_address(const SocketDevice& _rsd)
{
    return LocalAddressPlot(_rsd);
}

inline LocalEndpointPlot local_endpoint(const SocketDevice& _rsd)
{
    return LocalEndpointPlot(_rsd);
}

struct RemoteAddressPlot {
    const SocketDevice& rsd;
    RemoteAddressPlot(const SocketDevice& _rsd)
        : rsd(_rsd)
    {
    }
};

struct RemoteEndpointPlot {
    const SocketDevice& rsd;
    RemoteEndpointPlot(const SocketDevice& _rsd)
        : rsd(_rsd)
    {
    }
};

inline RemoteAddressPlot remote_address(const SocketDevice& _rsd)
{
    return RemoteAddressPlot(_rsd);
}

inline RemoteEndpointPlot remote_endpoint(const SocketDevice& _rsd)
{
    return RemoteEndpointPlot(_rsd);
}

std::ostream& operator<<(std::ostream& _ros, const LocalAddressPlot& _ra);
std::ostream& operator<<(std::ostream& _ros, const RemoteAddressPlot& _ra);
std::ostream& operator<<(std::ostream& _ros, const LocalEndpointPlot& _ra);
std::ostream& operator<<(std::ostream& _ros, const RemoteEndpointPlot& _ra);

} // namespace solid
