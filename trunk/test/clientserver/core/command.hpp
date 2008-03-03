/* Declarations file command.hpp
	
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

#ifndef TEST_COMMAND_H
#define TEST_COMMAND_H

#include "clientserver/core/command.hpp"

namespace test{
class Connection;
class Listener;
class Object;
//extends the interface of command to support test Objects
//see implementation in server
//! The base class for all commands in the test server
struct Command: clientserver::Command{
	virtual int execute(Connection &);
	virtual int execute(Listener &);
	virtual int execute(Object &);
};



}//namespace test
#endif
