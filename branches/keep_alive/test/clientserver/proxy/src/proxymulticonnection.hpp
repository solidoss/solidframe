/* Declarations file proxymulticonnection.hpp
	
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

#ifndef PROXYMULTICONNECTION_HPP
#define PROXYMULTICONNECTION_HPP

#include "utility/queue.hpp"
#include <string>
#include <deque>
#include "clientserver/tcp/multiconnection.hpp"
#include "clientserver/core/readwriteobject.hpp"
#include "system/socketaddress.hpp"

class SocketAddress;

namespace clientserver{
class Visitor;
namespace tcp{
class Channel;
}
}

class AddrInfo;

namespace test{

class Visitor;

namespace proxy{

class Service;

class MultiConnection: public clientserver::ReadWriteObject<clientserver::tcp::MultiConnection>{
public:
	typedef Service	ServiceTp;
	typedef clientserver::ReadWriteObject<clientserver::tcp::MultiConnection> BaseTp;
	
	MultiConnection(clientserver::tcp::Channel *_pch, const char *_node = NULL, const char *_srv = NULL);
	~MultiConnection();
	int execute(ulong _sig, TimeSpec &_tout);
	int execute();
	int accept(clientserver::Visitor &);
private:
	void run(unsigned _pos);
private:
	struct Buffer{
		enum{
			Capacity = 2*1024
		};
		Buffer():size(0), usecnt(0){}
		char		data[Capacity];
		unsigned	size;
		unsigned	usecnt;
	};
	typedef Queue<Buffer*>	BufferQueueTp;
	struct Stub{
		Stub():crtcmd(0), inexecq(false){}
		char			crtcmd;
		bool			inexecq;
		std::string		param1;
		std::string		param2;
		Buffer			recvbuf;
		BufferQueueTp	sendbufq;
	};
	typedef Queue<unsigned>		ExecQueueTp;
	typedef std::deque<Stub>	StubVectorTp;
	StubVectorTp		stubv;
	ExecQueueTp			execq;
	AddrInfo			*pai;
	AddrInfoIterator	it;
	bool				b;
};

}//namespace proxy
}//namespace test

#endif
