/* Declarations file station.hpp
	
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

#ifndef UDPSTATION_H
#define UDPSTATION_H

#include <sys/poll.h>

#include "system/socketaddress.hpp"

#include "utility/queue.hpp"

namespace clientserver{
namespace udp{

class TalkerSelector;
//! Wrapper for asynchronous tcp socket communication
/*!
	A class which allows a Talker to asynchronosly talk.
	
	<b>Notes:</b><br>
	- The udp::Station can only be used within a clientserver::udp::TalkerSelector,
		clientserver::SelectPool
	- The return values for io methods follows the standard of the solidground:
		# OK for opperation completion
		# BAD unrecoverable error
		# NOK the talker must wait for the opperation to
		complete asynchronously.
*/
class Station{
public:
	enum {RAISE_ON_DONE = 2};
	//! Create a simple non localy bound udp socket
	static Station *create();
	//! Create a simple localy bound udp socket
	static Station *create(const AddrInfoIterator &_rai);
	//! Another version of the above method
	static Station *create(const SockAddrPair &_rsa, AddrInfo::Family = AddrInfo::Inet4, int _proto = 0);
	//! Destructor
	~Station();
	//! Asynchrounous recv from call
	/*!
		Use recvAddr and recvSize to find the sender address and the size of 
		the last received data.
	*/
	int recvFrom(char *_pb, uint _bl, uint _flags = 0);
	//! Asynchrounous send to call
	int sendTo(const char *_pb, uint _bl, const SockAddrPair &_sap, uint _flags = 0);
	//! The size of the received data
	uint recvSize()const;
	//! The sender address for last received data.
	const SockAddrPair &recvAddr() const;
	//! The number of pending sends.
	uint sendPendingCount() const;
	//! Returns true if there is a pending receive.
	bool recvPending()const;
private:
	friend class TalkerSelector;
	struct Data{
		Data(char *_pb = NULL, uint _bl = 0, uint _flags = 0);
		Data(const SockAddrPair &_sap, 
			 char *_pb = NULL, 
			 uint _bl = 0, 
			 uint _flags = 0
		);
		Data(const SockAddrPair &_sap, 
			 const char *_pb, 
			 uint _bl = 0, 
			 uint _flags = 0
		);
		
		union{
			char		*pb;
			const char	*pcb;
		} b;
		uint        	bl;
		uint        	flags;
		SockAddrPair	sap;
	};
	//typedef Queue<Data,8>			DataQueueTp;
	//TODO: use the same ideea as in tcp::Channel
	typedef Queue<Data,4>			DataQueueTp;
	enum {INTOUT = POLLIN, OUTTOUT = POLLOUT};
	Station(int sd);
	ulong ioRequest()const;
	int doSend();
	int doRecv();
	int descriptor() const;
private:
	int 			sd;
	uint			rcvsz;
	Data			rcvd;
	SocketAddress	rcvsa;
	DataQueueTp		sndq;
};
#ifdef UINLINES
#include "src/station.ipp"
#endif


}//namespace udp
}//namespace clientserver
#endif
