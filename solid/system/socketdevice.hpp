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
    typedef SOCKET DescriptorT;
#else
    typedef int DescriptorT;
#endif

    //!Copy constructor
    SocketDevice(SocketDevice&& _sd) noexcept;
    //!Basic constructor
    SocketDevice();
    //!Destructor
    ~SocketDevice();
    //!Assign operator
    SocketDevice& operator=(SocketDevice&& _dev) noexcept;
    //! Shutdown reading
    void shutdownRead();
    //! Shutdown writing
    void shutdownWrite();
    //! Shutdown reading and writing
    void shutdownReadWrite();
    //! Create a socket based ResolveIterator
    ErrorCodeT create(const ResolveIterator& _rri);
    //! Create a socket given its family, its type and its protocol type
    ErrorCodeT create(
        SocketInfo::Family      = SocketInfo::Inet4,
        SocketInfo::Type _type  = SocketInfo::Stream,
        int              _proto = 0);
    //! Connect the socket
    ErrorCodeT connect(const SocketAddressStub& _rsas, bool& _can_wait);
    ErrorCodeT connect(const SocketAddressStub& _rsas);
    //! Bind the socket to a specific addr:port
    ErrorCodeT bind(const SocketAddressStub& _rsa);
    //! Prepares the socket for accepting
    ErrorCodeT prepareAccept(const SocketAddressStub& _rsas, size_t _listencnt = 10);
    //! Accept an incoming connection
    ErrorCodeT accept(SocketDevice& _dev, bool& _can_retry);
    ErrorCodeT accept(SocketDevice& _dev);
    //! Make a connection blocking
    /*!
        \param _msec if _msec > 0 will make socket blocking with the given amount of milliseconds
    */
    ErrorCodeT makeBlocking(size_t _msec);
    ErrorCodeT makeBlocking();
    //! Make the socket nonblocking
    ErrorCodeT makeNonBlocking();
    //! Check if its blocking
    ErrorCodeT isBlocking(bool& _rrv) const;
    ErrorCodeT enableNoDelay();
    ErrorCodeT disableNoDelay();

    ErrorCodeT enableNoSignal();
    ErrorCodeT disableNoSignal();

    ErrorCodeT enableLinger();
    ErrorCodeT disableLinger();

    ErrorCodeT hasNoDelay(bool& _rrv) const;

    ErrorCodeT enableCork(); //TCP_CORK - only on linux, TCP_NOPUSH on FreeBSD
    ErrorCodeT disableCork();
    ErrorCodeT hasCork(bool& _rrv) const;

    ErrorCodeT enableLoopbackFastPath();

    //ErrorCodeT sendBufferSize(size_t _sz);
    //ErrorCodeT recvBufferSize(size_t _sz);
    ErrorCodeT sendBufferSize(int& _rrv);
    ErrorCodeT recvBufferSize(int& _rrv);

    ErrorCodeT sendBufferSize(int& _rrv) const;
    ErrorCodeT recvBufferSize(int& _rrv) const;
    //! Write data on socket
    ssize_t send(const char* _pb, size_t _ul, bool& _rcan_retry, ErrorCodeT& _rerr, unsigned _flags = 0);
    //! Reads data from a socket
    ssize_t recv(char* _pb, size_t _ul, bool& _rcan_retry, ErrorCodeT& _rerr, unsigned _flags = 0);
    //! Send a datagram to a socket
    ssize_t send(const char* _pb, size_t _ul, const SocketAddressStub& _sap, bool& _rcan_retry, ErrorCodeT& _rerr);
    //! Recv data from a socket
    ssize_t recv(char* _pb, size_t _ul, SocketAddress& _rsa, bool& _rcan_retry, ErrorCodeT& _rerr);
    //! Gets the remote address for a connected socket
    ErrorCodeT remoteAddress(SocketAddress& _rsa) const;
    //! Gets the local address for a socket
    ErrorCodeT localAddress(SocketAddress& _rsa) const;
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
    void close();

    static ErrorCodeT error(const DescriptorT _des);

    ErrorCodeT error() const;

    //! Get the socket type
    ErrorCodeT type(int& _rrv) const;
    //! Return true if the socket is listening
    //bool isListening()const;

private:
    SocketDevice(const SocketDevice& _dev);
    SocketDevice& operator=(const SocketDevice& _dev);
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

} //namespace solid
