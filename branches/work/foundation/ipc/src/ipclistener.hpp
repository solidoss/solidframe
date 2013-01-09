/* Declarations file ipclistener.hpp
	
	Copyright 2013 Valentin Palade 
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

#ifndef FOUNDATION_IPC_SRC_IPC_LISTENER_HPP
#define FOUNDATION_IPC_SRC_IPC_LISTENER_HPP

#include "foundation/aio/aiosingleobject.hpp"
#include "foundation/ipc/ipcservice.hpp"
#include "system/socketdevice.hpp"

namespace foundation{

namespace aio{
namespace openssl{
class Context;
}
}

namespace ipc{

class Listener: public Dynamic<Listener, foundation::aio::SingleObject>{
public:
	typedef Service		ServiceT;
	Listener(
		const SocketDevice &_rsd,
		Service::Types	_type,
		foundation::aio::openssl::Context *_pctx = NULL
	);
	/*virtual*/ int execute(ulong, TimeSpec&);
private:
	typedef std::auto_ptr<foundation::aio::openssl::Context> SslContextPtrT;
	SocketDevice		sd;
	Service::Types		type;
	SslContextPtrT		pctx;
};


}//namespace ipc

}//namespace foundation

#endif
