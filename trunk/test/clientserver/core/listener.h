/* Declarations file listener.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TESTLISTENER_H
#define TESTLISTENER_H

#include "clientserver/tcp/listener.h"

namespace test{

class Service;
//! A simple listener
class Listener: public clientserver::tcp::Listener{
public:
	typedef Service		ServiceTp;
	Listener(clientserver::tcp::Station *_pst, uint _res, ulong _fullid):	
		clientserver::tcp::Listener(_pst, _res, _fullid){}
	virtual ~Listener();
	virtual int execute(ulong, TimeSpec&);
};

}//namespace test

#endif
