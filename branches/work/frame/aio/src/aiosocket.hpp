// frame/aio/src/aiosocket.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_SRC_SOCKET_HPP
#define SOLID_FRAME_AIO_SRC_SOCKET_HPP

#include "system/socketdevice.hpp"
#include "system/common.hpp"

struct SocketAddress;
struct SocketAddressStub;
class SocketDevice;

namespace solid{
namespace frame{
namespace aio{

class Selector;
class SecureSocket;
//! Asynchronous socket
/*!
	Implements an asynchronous socket to be used by aio::Object and aio::Selector.
	The operations are initiated on the aio::Object, and eventually completed
	within the aio::Selector. A socket can be: an accepting TCP socket, a channel for
	TCP streaming, and a station for UDP communication.
*/
class Socket{
public:
	enum Type{
		ACCEPTOR,
		CHANNEL,
		STATION,
	};
	//!Constructor
	Socket(Type _tp, SecureSocket *_pss = NULL);
	Socket(Type _tp, const SocketDevice &_rsd, SecureSocket *_pss = NULL);
	~Socket();
	//!Returns true if the socket has a securesocket
	bool isSecure()const;
	//!Returns true if the filedescriptor is valid
	bool ok()const;
	//! Create the socket
	int create(const ResolveIterator& _rai);
	//! Asynchronous connect request
	int connect(const SocketAddressStub& _rsas);
	int accept(SocketDevice &_rsd);
	int accept(Socket &_rs);
	//! Send a buffer
	int send(const char* _pb, uint32 _bl, uint32 _flags = 0);
	//! Receives data into a buffer
	int recv(char *_pb, uint32 _bl, uint32 _flags = 0);
	//! The size of the buffer received
	uint32 recvSize()const;
	//! The amount of data sent
	const uint64& sendCount()const;
	//! The amount of data received.
	const uint64& recvCount()const;
	//! Return true if there are pending send opperations
	bool isSendPending()const;
	//! Return true if there are pending recv opperations
	bool isRecvPending()const;
	//! Return the local address of the socket
	int localAddress(SocketAddress &_rsa)const;
	//! Return the remote address of the socket
	int remoteAddress(SocketAddress &_rsa)const;
	
	//! Asynchrounous recv_from  call
	int recvFrom(char *_pb, uint32 _bl, uint32 _flags = 0);
	//! Asynchrounous send_to call
	int sendTo(const char *_pb, uint32 _bl, const SocketAddressStub &_sap, uint32 _flags = 0);
	//! The sender address for last received data.
	const SocketAddress &recvAddr() const;
	//! Setter for the secure socket
	void secureSocket(SecureSocket *_pss);
	//! Getter for the secure socket
	SecureSocket* secureSocket()const;
	//! Do a SSL accept - secure handshake
	int secureAccept();
	//! Do a SSL connect - secure handshake
	int secureConnect();
private:
	friend class Selector;
	void doPrepare();
	void doUnprepare();
	void doClear();
	
	void doWantAccept(int _w);
	void doWantConnect(int _w);
	void doWantRead(int _w);
	void doWantWrite(int _w);
	
	int doSend();
	int doRecv();
	
	int doSendPlain();
	int doRecvPlain();
	
	int doSendPlain(const char* _pb, uint32 _bl, uint32 _flags);
	int doRecvPlain(char *_pb, uint32 _bl, uint32 _flags);
	
	int doSendSecure();
	int doRecvSecure();
	
	int doSendSecure(const char* _pb, uint32 _bl, uint32 _flags);
	int doRecvSecure(char *_pb, uint32 _bl, uint32 _flags);
	
	int doSecureReadWrite(int _w);
	int doSecureAccept();
	int doSecureConnect();
	
	uint32 ioRequest()const;
	int descriptor()const{return sd.descriptor();}
private:
	struct StationData;
	struct AcceptorData;
	SocketDevice	sd;
	SecureSocket	*pss;
	uint16			type;
	uint16			want;
	uint64			rcvcnt;
	uint64			sndcnt;
	char*			rcvbuf;
	const char*		sndbuf;
	uint32			rcvlen;
	uint32			sndlen;
	uint32			ioreq;
	union{
		StationData		*psd;
		AcceptorData	*pad;
	}d;
};

#ifndef NINLINES
#include "aiosocket.ipp"
#endif


}//namespace aio
}//namespace frame
}//namespace solid

#endif

