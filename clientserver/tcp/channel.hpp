/* Declarations file channel.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CS_TCP_CHANNEL_HPP
#define CS_TCP_CHANNEL_HPP

#include <sys/epoll.h>

#include "system/socketaddress.hpp"
#include "utility/queue.hpp"
#include "clientserver/core/common.hpp"

class AddrInfoIterator;

class IStreamIterator;
class OStreamIterator;

namespace clientserver{
namespace tcp{

class ConnectionSelector;
class Station;
class SecureChannel;
struct ChannelData;
//! Wrapper for asynchronous tcp socket communication
/*!
	Use this in every clientserver::tcp::Connection to do the
	actual asynchrounous io.
	
	<b>Notes:</b><br>
	- The Channel can only be used within a <code>clientserver::tcp::ConnectionSelector</code>,
		<code>clientserver::SelectPool</code>
	- For now there is no secure communication (there is no definition
	for SecureChannel.
	- The secure channel may change a litle the initialization interface
	of the <code>clientserver::tcp::Channel</code>, but the existing communication
	interface should not change.
	- The return values for io methods follows the standard of the solidground:
		# OK for opperation completion
		# BAD unrecoverable error
		# NOK the connection must wait for the opperation to
		complete asynchronously.
*/
class Channel{
public:
	enum {
		RAISE_ON_END = 2, 
		TRY_ONLY = 4, 
		PREPARE_ONLY = 8};
	static Channel* create(const AddrInfoIterator &_rai);
	//Channel();
	~Channel();
	//! Returns true if the socket was successfully created.
	int ok()const;
	//! Tries to connect/ initiate connect to a specified address
	int connect(const AddrInfoIterator&);
	//! Returns true if the channel is secure - 
	int isSecure();
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
	int arePendingSends();
protected:
private:
	Channel(int _sd);
	enum {
		INTOUT = EPOLLIN,
		OUTTOUT = EPOLLOUT,
		IO_REQUEST_FLAGS = INTOUT | OUTTOUT,
		IO_TOUT_FLAGS = INTOUT | OUTTOUT,
		MUST_START = 1 << 31,
	};
	ulong ioRequest()const;
	int doSendPlain(const char*, uint32, uint32);
	int doSendSecure(const char*, uint32, uint32);
	int doRecvPlain(char*, uint32, uint32);
	int doRecvSecure(char*, uint32, uint32);
	//the private interface is visible to ConnectionSelector
	friend class ConnectionSelector;
	friend class Station;
	void prepare();
	void unprepare();
	int doRecv();
	int doSend();
	int doRecvPlain();
	int doRecvSecure();
	int doSendPlain();
	int doSendSecure();
	int descriptor()const{return sd;}
private:
	int 				sd;
	uint64				rcvcnt;
	uint64				sndcnt;
	//NOTE: rcvcnt and sndcnt not in pcd because a connection can switch pools
	ChannelData			*pcd;
	SecureChannel		*psch;
};
#ifdef UINLINES
#include "src/channel.ipp"
#endif
}//namespace tcp
}//namespace clientserver

#endif
