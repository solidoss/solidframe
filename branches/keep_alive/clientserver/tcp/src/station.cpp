/* Implementation file station.cpp
	
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "system/debug.hpp"

#include "core/common.hpp"

#include "tcp/station.hpp"
#include "tcp/channel.hpp"

namespace clientserver{
namespace tcp{

Station* Station::create(const AddrInfoIterator &_rai, int _listensz){
	SocketDevice sd;
	if(_rai.type() != AddrInfo::Stream) return NULL;
	
	sd.create(_rai);
	sd.makeNonBlocking();
	sd.prepareAccept(_rai, _listensz);
	if(sd.ok())	return new Station(sd);
	return NULL;

}
Station::Station(SocketDevice &_sd):sd(_sd){
}
Station::~Station(){
}
int Station::accept(ChannelVecTp &_cv/*, Constrainer &_rcons*/){
	sockaddr_in		saddr;
	_cv.clear();
	//struct sockaddr_in      saddr;
	do{
		SocketDevice csd;
		switch(sd.accept(csd)){
			case BAD: return BAD;
			case OK: break;
			case NOK: return NOK;
		}
		//TODO: not so nice so try to change.
		Channel *pch = new Channel(csd);
		if(pch->ok()) _cv.push_back(pch);
		else delete pch;
	}while(_cv.size() < _cv.capacity());
	return OK;
}

}//namespace tcp
}//namespace clientserver

