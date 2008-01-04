/* Declarations file station.h
	
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

#ifndef CS_TCP_STATION_H
#define CS_TCP_STATION_H

#include <vector>

class AddrInfoIterator;

namespace clientserver{
namespace tcp{

class ListenerSelector;
class Channel;

class Station{
public:
	static Station* create(const AddrInfoIterator &_rai, int _listensz = 100);
	~Station();
	typedef std::vector<Channel*>	ChannelVecTp;
	int accept(ChannelVecTp &cv/*, Constrainer &_rcons*/);
private:
	friend class ListenerSelector;
	int descriptor()const{return sd;}
private:
	Station(int sd);
	int sd;
};

}//namespace tcp
}//namespace clientserver

#endif
