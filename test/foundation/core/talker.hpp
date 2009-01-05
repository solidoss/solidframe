/* Declarations file talker.hpp
	
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

#ifndef TESTTALKER_HPP
#define TESTTALKER_HPP

#include "foundation/aio/udp/talker.hpp"
#include "common.hpp"

class SocketDevice;

namespace test{
//! The base class for all talkers in the test server
class Talker: public foundation::aio::udp::Talker{
public:
	virtual ~Talker(){}
protected:
	Talker(){}
	Talker(const SocketDevice &_rsd):foundation::aio::udp::Talker(_rsd){}
};

}

#endif
