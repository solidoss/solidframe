// system/socketdevice.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_SOCKETDEVICE_HPP
#define SYSTEM_SOCKETDEVICE_HPP

#include "system/socketaddress.hpp"
#include "system/device.hpp"
#include "system/error.hpp"

namespace solid{

//! A wrapper for berkeley sockets
class SocketDevice: public Device{
public:
#ifdef ON_WINDOWS
	typedef SOCKET DescriptorT;
#else
	typedef int DescriptorT;
#endif
	//!Copy constructor
	SocketDevice(const SocketDevice &_sd);
	//!Basic constructor
	SocketDevice();
	//!Destructor
	~SocketDevice();
	//!Assign operator
	SocketDevice& operator=(const SocketDevice &_dev);
	//! Shutdown reading
	void shutdownRead();
	//! Shutdown writing
	void shutdownWrite();
	//! Shutdown reading and writing 
	void shutdownReadWrite();
	//! Create a socket based ResolveIterator
	bool create(const ResolveIterator &_rri);
	//! Create a socket given its family, its type and its protocol type
	bool create(
		SocketInfo::Family = SocketInfo::Inet4,
		SocketInfo::Type _type = SocketInfo::Stream,
		int _proto = 0
	);
	//! Connect the socket
	AsyncE connectNonBlocking(const SocketAddressStub &_rsas);
	bool connect(const SocketAddressStub &_rsas);
	//! Bind the socket to a specific addr:port
	bool bind(const SocketAddressStub &_rsa);
	//! Prepares the socket for accepting
	bool prepareAccept(const SocketAddressStub &_rsas, size_t _listencnt = 10);
	//! Accept an incomming connection
	AsyncE acceptNonBlocking(SocketDevice &_dev);
	bool accept(SocketDevice &_dev);
	//! Make a connection blocking
	/*!
		\param _msec if _msec > 0 will make socket blocking with the given amount of milliseconds
	*/
	bool makeBlocking(size_t _msec);
	bool makeBlocking();
	//! Make the socket nonblocking
	bool makeNonBlocking();
	//! Check if its blocking
	std::pair<bool, bool> isBlocking()const;
	bool enableNoDelay();
	bool disableNoDelay();
	std::pair<bool, bool> hasNoDelay()const;
	
	bool enableCork();//TCP_CORK - only on linux, TCP_NOPUSH on FreeBSD
	bool disableCork();
	std::pair<bool, bool> hasCork()const;
	
	bool sendBufferSize(size_t _sz);
	bool recvBufferSize(size_t _sz);
	std::pair<bool, size_t> sendBufferSize()const;
	std::pair<bool, size_t> recvBufferSize()const;
	//! Write data on socket
	int send(const char* _pb, size_t _ul, unsigned _flags = 0);
	//! Reads data from a socket
	int recv(char *_pb, size_t _ul, unsigned _flags = 0);
	//! Send a datagram to a socket
	int send(const char* _pb, size_t _ul, const SocketAddressStub &_sap);
	//! Recv data from a socket
	int recv(char *_pb, size_t _ul, SocketAddress &_rsa);
	//! Gets the remote address for a connected socket
	bool remoteAddress(SocketAddress &_rsa)const;
	//! Gets the local address for a socket
	bool localAddress(SocketAddress &_rsa)const;
#ifdef ON_WINDOWS
	static const DescriptorT invalidDescriptor(){
		return INVALID_SOCKET;
	}
	DescriptorT descriptor()const{return reinterpret_cast<DescriptorT>(Device::descriptor());}
	bool ok()const{
		return descriptor() != invalidDescriptor();
	}
#endif
	void close();
	//! Get the socket type
	std::pair<bool, int> type()const;
	//! Return true if the socket is listening
	//bool isListening()const;
};

}//namespace solid

#endif
