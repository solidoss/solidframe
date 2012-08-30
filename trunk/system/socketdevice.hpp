#ifndef SYSTEM_SOCKETDEVICE_HPP
#define SYSTEM_SOCKETDEVICE_HPP

#include "system/socketaddress.hpp"
#include "system/device.hpp"

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
	int create(const ResolveIterator &_rri);
	//! Create a socket given its family, its type and its protocol type
	int create(
		SocketInfo::Family = SocketInfo::Inet4,
		SocketInfo::Type _type = SocketInfo::Stream,
		int _proto = 0
	);
	//! Connect the socket
	int connect(const SocketAddressStub &_rsas);
	//! Bind the socket to a specific addr:port
	int bind(const SocketAddressStub &_rsa);
	//! Prepares the socket for accepting
	int prepareAccept(const SocketAddressStub &_rsas, unsigned _listencnt = 10);
	//! Accept an incomming connection
	int accept(SocketDevice &_dev);
	//! Make a connection blocking
	/*!
		\param _msec if _msec > 0 will make socket blocking with the given amount of milliseconds
	*/
	int makeBlocking(int _msec = -1);
	//! Make the socket nonblocking
	int makeNonBlocking();
	//! Check if its blocking
#ifndef ON_WINDOWS
	int isBlocking()const;
#endif
	
	int enableNoDelay();
	int disableNoDelay();
	int hasNoDelay()const;
	
	int enableCork();//TCP_CORK - only on linux, TCP_NOPUSH on FreeBSD
	int disableCork();
	int hasCork()const;
	
	int sendBufferSize(size_t _sz);
	int recvBufferSize(size_t _sz);
	int sendBufferSize()const;
	int recvBufferSize()const;
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
	int remoteAddress(SocketAddress &_rsa)const;
	//! Gets the local address for a socket
	int localAddress(SocketAddress &_rsa)const;
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
	int type()const;
	//! Return true if the socket is listening
	bool isListening()const;
};


#endif
