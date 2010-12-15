/* Declarations file listener.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef EXAMPLE_CONCEPT_AIO_LISTENER_HPP
#define EXAMPLE_CONCEPT_AIO_LISTENER_HPP

#include "foundation/aio/aiosingleobject.hpp"
#include "system/socketdevice.hpp"
#include <memory>

namespace foundation{
namespace aio{
namespace openssl{
class Context;
}
}
}

namespace concept{

class Service;
//! A simple listener
class Listener: public Dynamic<Listener, foundation::aio::SingleObject>{
public:
	typedef Service		ServiceT;
	Listener(const SocketDevice &_rsd, foundation::aio::openssl::Context *_pctx = NULL);
	virtual int execute(ulong, TimeSpec&);
private:
	typedef std::auto_ptr<foundation::aio::openssl::Context> SslContextPtrT;
	SocketDevice		sd;
	SslContextPtrT		pctx;
};

}//namespace concept

#endif
