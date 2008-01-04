/* Implementation file station.cpp
	
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "system/debug.h"

#include "core/common.h"

#include "tcp/station.h"
#include "tcp/channel.h"

namespace clientserver{
namespace tcp{

Station* Station::create(const AddrInfoIterator &_rai, int _listensz){
	int sd;
	if(_rai.type() != AddrInfo::Stream) return NULL;
	if((sd = socket(_rai.family(), _rai.type(), _rai.protocol())) == -1){
		edbg("error creating listener socket");
		return NULL;
	}
	int yes = 1;
	if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,(char*)&yes,sizeof(yes)) < 0){
		edbg("error setting sockopt");
		return NULL;
	}
// 	struct sockaddr_in      saddr;
// 	memset(&saddr,0,sizeof(saddr));
// 	saddr.sin_family = AF_INET;
// 	//saddr.sin_addr.s_addr = htonl(INADDR_ANY);
// 	saddr.sin_port = htons(_port);
	if(bind(sd, _rai.addr(), _rai.size()) == -1){
		edbg("error while binding");
		close(sd);
		return NULL;
	}
	if(listen(sd, _listensz) == -1){//make
		edbg("error listening");
		close(sd);
		return NULL;
	}
	if(fcntl(sd, F_SETFL, O_NONBLOCK) < 0){
		//TODO: some error message.
		edbg("error making server socket nonblocking");
		close(sd);
		return NULL;

	}
	/*
	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 2*50;
	setsockopt(sd,SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(sd,SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));*/
	return new Station(sd);

}
Station::Station(int _sd):sd(_sd){
}
Station::~Station(){
	close(sd);
}
int Station::accept(ChannelVecTp &_cv/*, Constrainer &_rcons*/){
	sockaddr_in		saddr;
	_cv.clear();
	//struct sockaddr_in      saddr;
	do{
		int psd;
		socklen_t len = sizeof(saddr);
		if((psd = ::accept(sd,(struct sockaddr*) &saddr, &len)) < 0){
			if(errno == EAGAIN){
				idbg("eagain on accept");
				return NOK;
			}
			edbg("error accepting");
			return BAD;
		}
		//TODO: not so nice so try to change.
		Channel *pch = new Channel(psd);
		if(pch->isOk()) _cv.push_back(pch);
		else delete pch;
	}while(_cv.size() < _cv.capacity());
	return OK;
}

}//namespace tcp
}//namespace clientserver

