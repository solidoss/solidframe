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
	static ERROR_NS::error_code last_error();
	enum RetValE{
		Error = solid::Error,
		Success = solid::Success,
		Failure = solid::Failure,
		Pending
	};
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
	RetValE connect(ERROR_NS::error_code &_rerr, const SocketAddressStub &_rsas);
	//! Bind the socket to a specific addr:port
	bool bind(ERROR_NS::error_code &_rerr, const SocketAddressStub &_rsa);
	//! Prepares the socket for accepting
	bool prepareAccept(ERROR_NS::error_code &_rerr, const SocketAddressStub &_rsas, size_t _listencnt = 10);
	//! Accept an incomming connection
	RetValE accept(ERROR_NS::error_code &_rerr, SocketDevice &_dev);
	//! Make a connection blocking
	/*!
		\param _msec if _msec > 0 will make socket blocking with the given amount of milliseconds
	*/
	bool makeBlocking(ERROR_NS::error_code &_rerr, size_t _msec);
	bool makeBlocking(ERROR_NS::error_code &_rerr);
	//! Make the socket nonblocking
	bool makeNonBlocking(ERROR_NS::error_code &_rerr);
	//! Check if its blocking
	RetValE isBlocking(ERROR_NS::error_code &_rerr)const;
	bool enableNoDelay(ERROR_NS::error_code &_rerr);
	bool disableNoDelay(ERROR_NS::error_code &_rerr);
	RetValE hasNoDelay(ERROR_NS::error_code &_rerr)const;
	
	bool enableCork(ERROR_NS::error_code &_rerr);//TCP_CORK - only on linux, TCP_NOPUSH on FreeBSD
	bool disableCork(ERROR_NS::error_code &_rerr);
	RetValE hasCork(ERROR_NS::error_code &_rerr)const;
	
	bool sendBufferSize(ERROR_NS::error_code &_rerr, size_t _sz);
	bool recvBufferSize(ERROR_NS::error_code &_rerr, size_t _sz);
	int sendBufferSize(ERROR_NS::error_code &_rerr)const;
	int recvBufferSize(ERROR_NS::error_code &_rerr)const;
	//! Return true if nonblocking and the prevoious nonblocking opperation did not complete
	/*!
		In case of nonblocking sockets, use this method after:connect, accept, read, write,send
		recv to see if the opearation failed temporarly and that you should try later.
		Call this method imediatly after a connect,accept,send, recv when they return with error.
	*/
	bool shouldWait()const;
	//! Write data on socket
	int send(const char* _pb, unsigned _ul, unsigned _flags = 0);
	//! Reads data from a socket
	int recv(char *_pb, unsigned _ul, unsigned _flags = 0);
	//! Send a datagram to a socket
	int send(const char* _pb, unsigned _ul, const SocketAddressStub &_sap);
	//! Recv data from a socket
	int recv(char *_pb, unsigned _ul, SocketAddress &_rsa);
	//! Gets the remote address for a connected socket
	bool remoteAddress(ERROR_NS::error_code &_rerr, SocketAddress &_rsa)const;
	//! Gets the local address for a socket
	bool localAddress(ERROR_NS::error_code &_rerr, SocketAddress &_rsa)const;
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
	int type(ERROR_NS::error_code &_rerr)const;
	//! Return true if the socket is listening
	//bool isListening(ERROR_NS::error_code &_rerr)const;
};

}//namespace solid

#endif
