#ifndef SYSTEM_SOCKETDEVICE_HPP
#define SYSTEM_SOCKETDEVICE_HPP

#include "system/socketaddress.hpp"
#include "system/device.hpp"

//! A wrapper for berkeley sockets
class SocketDevice: public Device{
public:
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
	//! Create a socket based on SocketAddressInfo iterator (use SocketAddressInfo to create one)
	int create(const SocketAddressInfoIterator &_rai);
	//! Create a socket given its family, its type and its protocol type
	int create(SocketAddressInfo::Family = SocketAddressInfo::Inet4, SocketAddressInfo::Type _type = SocketAddressInfo::Stream, int _proto = 0);
	//! Connect the socket
	int connect(const SocketAddressInfoIterator &_rai);
	//! Bind the socket to a specific addr:port
	int bind(const SocketAddressInfoIterator &_rai);
	//! Bind the socket to a specific addr:port
	int bind(const SocketAddressPair &_rsa);
	//! Prepares the socket for accepting
	int prepareAccept(const SocketAddressInfoIterator &_rai, unsigned _listencnt = 10);
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
	bool isBlocking();
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
	int send(const char* _pb, unsigned _ul, const SocketAddressPair &_sap);
	//! Recv data from a socket
	int recv(char *_pb, unsigned _ul, SocketAddress &_rsa);
	//! Gets the remote address for a connected socket
	int remoteAddress(SocketAddress &_rsa)const;
	//! Gets the local address for a socket
	int localAddress(SocketAddress &_rsa)const;
	int descriptor()const{return Device::descriptor();}
	//! Get the socket type
	int type()const;
	//! Return true if the socket is listening
	bool isListening()const;
};


#endif
