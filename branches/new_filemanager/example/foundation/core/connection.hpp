/* Declarations file connection.hpp
	
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

#ifndef TESTCONNECTION_HPP
#define TESTCONNECTION_HPP

#include "utility/streampointer.hpp"
#include "foundation/aio/aiosingleobject.hpp"
#include "common.hpp"
#include "tstring.hpp"

namespace concept{
//! The base class for all concept connections
class Connection: public foundation::aio::SingleObject{
public:
	typedef std::pair<uint32, uint32>	FileUidTp;
	typedef std::pair<uint32, uint32>	RequestUidTp;
protected:
	Connection(const SocketDevice &_rsd):
			foundation::aio::SingleObject(_rsd){}
	Connection(){}
};

}//namespace concept

#endif
