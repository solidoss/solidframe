/* Implementation file talker.cpp
	
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

#include <cstdlib>

#include "udp/talker.hpp"
#include "udp/visitor.hpp"
#include "udp/station.hpp"

namespace clientserver{
namespace udp{
//--------------------------------------------------------------
Visitor::Visitor(){}
Visitor::~Visitor(){}
//-----------------------------------------------------------------------------------------
Talker::Talker(Station *pst):rst(*pst){}
//-----------------------------------------------------------------------------------------
Talker::~Talker(){
	delete &rst;
}
//-----------------------------------------------------------------------------------------
int Talker::accept(clientserver::Visitor &_rv){
	return static_cast<clientserver::udp::Visitor&>(_rv).visit(*this);
}
//-----------------------------------------------------------------------------------------
}//namespace udp
}//namespace clientserver

