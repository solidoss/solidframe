/* Declarations file betatalker.hpp
	
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

#ifndef BETATALKER_HPP
#define BETATALKER_HPP

#include "core/talker.hpp"

namespace foundation{
class Visitor;
}

class AddrInfo;

namespace concept{

class Visitor;

namespace beta{

class Service;
struct AddrMap;
class Talker: public concept::Talker{
public:
	typedef Service	ServiceT;
	typedef concept::Talker BaseT;
	
	Talker(const char *_node, const char *_srv);
	Talker(const SocketDevice &_rsd);
	~Talker();
	int execute(ulong _sig, TimeSpec &_tout);
	int execute();
	int accept(foundation::Visitor &);
private:
	enum {BUFSZ = 4*1024};
	enum {INIT,READ, READ_DONE, WRITE, WRITE_DONE};
	char			bbeg[BUFSZ];
	uint32			sz;
	uint32			id;
	AddrInfo		*pai;
	AddrMap			&addrmap;
};

}//namespace echo
}//namespace concept

#endif
