/* Declarations file listener.hpp
	
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

#ifndef CS_TCP_LISTENER_HPP
#define CS_TCP_LISTENER_HPP

#include <vector>

#include "clientserver/core/object.hpp"
#include "clientserver/core/common.hpp"


namespace clientserver{
namespace tcp{

class Station;
class Channel;
//! The base class for all tcp::Listener(s)
/*!
	<b>Usage:</b><br>
	- inherit and implement Listener::execute to add channel
	accept filtering and Connection creation.
*/
class Listener: public Object{
public:
	~Listener();
	Station &station();
	virtual int execute(ulong _evs, TimeSpec &_rtout) = 0;
protected:
	typedef std::vector<Channel*>	ChannelVecTp;
	Listener(Station *_pst, uint _res = 16, ulong _fullid = 0);
	Station			&rst;
	ChannelVecTp	chvec;
};

inline Station &Listener::station(){
	return rst;
}

}//namespace tcp
}//namespace clientserver

#endif
