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

#ifndef SOLID_FRAME_IPC_SRC_IPC_LISTENER_HPP
#define SOLID_FRAME_IPC_SRC_IPC_LISTENER_HPP

#include "frame/aio/aiosingleobject.hpp"
#include "frame/ipc/ipcservice.hpp"
#include "system/socketdevice.hpp"

namespace solid{
namespace frame{
namespace aio{
namespace openssl{
class Context;
}
}

namespace ipc{

class Listener: public Dynamic<Listener, frame::aio::SingleObject>{
public:
	Listener(
		Service &_rsvc,
		const SocketDevice &_rsd,
		Service::Types	_type,
		frame::aio::openssl::Context *_pctx = NULL
	);
	/*virtual*/ int execute(ulong, TimeSpec&);
private:
	typedef std::auto_ptr<frame::aio::openssl::Context> SslContextPtrT;
	Service				&rsvc;
	SocketDevice		sd;
	Service::Types		type;
	SslContextPtrT		pctx;
	int					state;
};


}//namespace ipc
}//namespace frame
}//namespace solid

#endif
