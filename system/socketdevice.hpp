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
	ErrorCodeT create(const ResolveIterator &_rri);
	//! Create a socket given its family, its type and its protocol type
	ErrorCodeT create(
		SocketInfo::Family = SocketInfo::Inet4,
		SocketInfo::Type _type = SocketInfo::Stream,
		int _proto = 0
	);
	//! Connect the socket
	ErrorCodeT connect(const SocketAddressStub &_rsas, bool &_can_wait);
	ErrorCodeT connect(const SocketAddressStub &_rsas);
	//! Bind the socket to a specific addr:port
	ErrorCodeT bind(const SocketAddressStub &_rsa);
	//! Prepares the socket for accepting
	ErrorCodeT prepareAccept(const SocketAddressStub &_rsas, size_t _listencnt = 10);
	//! Accept an incomming connection
	ErrorCodeT accept(SocketDevice &_dev, bool &_can_retry);
	ErrorCodeT accept(SocketDevice &_dev);
	//! Make a connection blocking
	/*!
		\param _msec if _msec > 0 will make socket blocking with the given amount of milliseconds
	*/
	ErrorCodeT makeBlocking(size_t _msec);
	ErrorCodeT makeBlocking();
	//! Make the socket nonblocking
	ErrorCodeT makeNonBlocking();
	//! Check if its blocking
	ErrorCodeT isBlocking(bool &_rrv)const;
	ErrorCodeT enableNoDelay();
	ErrorCodeT disableNoDelay();
	
	ErrorCodeT enableLinger();
	ErrorCodeT disableLinger();
	
	ErrorCodeT hasNoDelay(bool &_rrv)const;
	
	ErrorCodeT enableCork();//TCP_CORK - only on linux, TCP_NOPUSH on FreeBSD
	ErrorCodeT disableCork();
	ErrorCodeT hasCork(bool &_rrv)const;
	
	//ErrorCodeT sendBufferSize(size_t _sz);
	//ErrorCodeT recvBufferSize(size_t _sz);
	ErrorCodeT sendBufferSize(int &_rrv);
	ErrorCodeT recvBufferSize(int &_rrv);
	
	ErrorCodeT sendBufferSize(int &_rrv)const;
	ErrorCodeT recvBufferSize(int &_rrv)const;
	//! Write data on socket
	int send(const char* _pb, size_t _ul, bool &_rcan_retry, ErrorCodeT &_rerr, unsigned _flags = 0);
	//! Reads data from a socket
	int recv(char *_pb, size_t _ul, bool &_rcan_retry, ErrorCodeT &_rerr, unsigned _flags = 0);
	//! Send a datagram to a socket
	int send(const char* _pb, size_t _ul, const SocketAddressStub &_sap, bool &_rcan_retry, ErrorCodeT &_rerr);
	//! Recv data from a socket
	int recv(char *_pb, size_t _ul, SocketAddress &_rsa, bool &_rcan_retry, ErrorCodeT &_rerr);
	//! Gets the remote address for a connected socket
	ErrorCodeT remoteAddress(SocketAddress &_rsa)const;
	//! Gets the local address for a socket
	ErrorCodeT localAddress(SocketAddress &_rsa)const;
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
	ErrorCodeT type(int &_rerr)const;
	//! Return true if the socket is listening
	//bool isListening()const;
};

}//namespace solid

#endif
