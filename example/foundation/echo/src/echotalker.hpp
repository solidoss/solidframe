/* Declarations file echotalker.hpp
	
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

#ifndef ECHOTALKER_HPP
#define ECHOTALKER_HPP

#include "core/talker.hpp"

namespace foundation{
class Visitor;
}

class AddrInfo;

namespace concept{

class Visitor;

namespace echo{

class Service;

class Talker: public concept::Talker{
public:
	typedef Service	ServiceTp;
	typedef concept::Talker BaseTp;
	
	Talker(const char *_node, const char *_srv);
	Talker(const SocketDevice &_rsd);
	~Talker();
	int execute(ulong _sig, TimeSpec &_tout);
	int execute();
	int accept(foundation::Visitor &);
private:
	enum {BUFSZ = 1024};
	enum {INIT,READ, READ_TOUT, WRITE, WRITE_TOUT, WRITE2,WRITE_TOUT2};
	char			bbeg[BUFSZ];
	uint			sz;
	AddrInfo		*pai;
};

}//namespace echo
}//namespace concept

#endif
