/* Declarations file visitor.hpp
	
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

#ifndef CS_TCP_VISITOR_H
#define CS_TCP_VISITOR_H

#include "clientserver/core/visitor.hpp"

namespace clientserver{
class Object;
namespace tcp{

class Connection;
class Listener;
//!A base visitor for a connections and liteners
/*!
	TODO: move vist methods to clientserver::Visitior
*/
class Visitor: public clientserver::Visitor{
public:
	Visitor();
	virtual ~Visitor();
	virtual int visit(Connection&) = 0;
	virtual int visit(Listener&) = 0;
};

}//namespace tcp
}//namespace clientserver

#endif
