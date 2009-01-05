/* Declarations file listener.hpp
	
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

#ifndef TESTAIOLISTENER_HPP
#define TESTAIOLISTENER_HPP

#include "clientserver/aio/tcp/listener.hpp"
#include "system/socketdevice.hpp"

namespace clientserver{
namespace aio{
namespace openssl{
class Context;
}
}
}

namespace test{

class Service;
//! A simple listener
class Listener: public clientserver::aio::tcp::Listener{
public:
	typedef Service		ServiceTp;
	Listener(const SocketDevice &_rsd, clientserver::aio::openssl::Context *_pctx = NULL);
	~Listener();
	virtual int execute(ulong, TimeSpec&);
private:
	SocketDevice		sd;
	clientserver::aio::openssl::Context *pctx;
};

}//namespace test

#endif
