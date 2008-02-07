/* Declarations file station.h
	
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

#ifndef CS_TCP_STATION_H
#define CS_TCP_STATION_H

#include <vector>

class AddrInfoIterator;

namespace clientserver{
namespace tcp{

class ListenerSelector;
class Channel;

//! An asynchrounous ready tcp connection listener - accepter - Stations
/*!
	The tcp::Station is a wrapper for a TCP listener/server socket. It listens
	for incomming connections and create communications clientserver::tcp::Channels.
	Every clientserver::tcp::Listener will use one station.
*/
class Station{
public:
	//! Create a new station for a localaddress
	static Station* create(const AddrInfoIterator &_rai, int _listensz = 100);
	//! Destructor
	~Station();
	typedef std::vector<Channel*>	ChannelVecTp;
	//! Accept connections, pushing them into cv
	int accept(ChannelVecTp &_cv);
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
