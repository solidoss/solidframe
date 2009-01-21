/* Implementation file listener.hpp
	
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
#ifndef AIO_LISTENER_HPP
#define AIO_LISTENER_HPP

#include "foundation/aio/aioobject.hpp"

class SocketDevice;
class SocketAddress;
class AddrInfoIterator;

namespace foundation{

namespace aio{

namespace tcp{
//! A class for asynchronous listening and accepting incomming TCP connections
/*!
	Inherit this class in your server to have support for accepting incomming
	TCP connections.<br>
	All methods marked as asynchronous have three return values:<br>
	- BAD (-1) for unrecoverable error like socket connection closed;<br>
	- NOK (1) opperation is pending for completion within aio::Selector;<br>
	- OK (0) opperation completed successfully.<br>
*/
class Listener: public Object{
public:
	//! Constructor with an aio::Socket
	Listener(Socket *_psock = NULL);
	//! Constructor with a SocketDevice
	Listener(const SocketDevice &_rsd);
	//! Returns true if the socket descriptor/handle is valid
	bool socketOk()const;
	//! Asynchronous accept an incomming connection
	int socketAccept(SocketDevice &_rsd);
	
	//! Erase the associated aio::Socket
	void socketErase();
	//! Set the associated aio::Socket given the aio::Socket
	uint socketSet(Socket *_psock);
	//! Set the associated aio::Socket given the SocketDevice
	uint socketSet(const SocketDevice &_rsd);
	//! Request for registering the socket onto the aio::Selector
	void socketRequestRegister();
	//! Request for registering the socket onto the aio::Selector
	/*!
		The sockets are automatically unregistered and erased
		on object destruction.
	*/
	void socketRequestUnregister();
	//! Get the socket state
	int socketState()const;
	//! Set the socket state
	void socketState(int _st);
private:
	SocketStub	stub;
	int32		req;
	int32		res;
	int32		tout;
};

}//namespace tcp

}//namespace aio

}//namespace foundation


#endif
