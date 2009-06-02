/* Implementation file addrtest.cpp
	
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

#include <poll.h>
#include "system/socketaddress.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <iostream>
#include <cstring>

using namespace std;

int main(int argc, char *argv[]){
	if(argc < 3){
		cout<<"error too few arguments"<<endl;
		return 0;
	}
	const char *node = NULL;
	if(strlen(argv[1])) node = argv[1];
	const char *srv = NULL;
	if(strlen(argv[2])) srv = argv[2];
	int flags = 0;
	int family = AddrInfo::Inet4;
	int type = AddrInfo::Stream;
	int proto = 0;
	//AddrInfo ai(node, srv);
	AddrInfo ai(node, srv, flags, family, type, proto);
	AddrInfoIterator it(ai.begin());
	while(it){
		int sd = socket(it.family(), it.type(), it.protocol());
		if(sd > 0){
			cout<<"Connecting..."<<endl;
			fcntl(sd, F_SETFL, O_NONBLOCK);
			if(false/* !connect(sd, it.addr(), it.size())*/){
				cout<<"Connected!"<<endl;
			}else{
				if(false/*errno != EINPROGRESS*/){
					cout<<"Failed connect"<<endl;
				}else{
					cout<<"Polling ..."<<endl;
					pollfd pfd;
					pfd.fd = sd;
					pfd.events = 0;//POLLOUT;
					int rv = poll(&pfd, 1, -1);
					cout<<"pollrv = "<<rv<<endl;
					if(pfd.revents & (POLLERR | POLLHUP | POLLNVAL)){
						
						cout<<"poll err "<<(pfd.revents & POLLERR)<<' '<<(pfd.revents & POLLHUP)<<' '<<(pfd.revents & POLLNVAL)<<' '<<endl;
					}
				}
			}
			socklen_t len = 4;
			int v = 0;
			int rv = getsockopt(sd, SOL_SOCKET, SO_ERROR, &v, &len);
			cout<<"getsockopt rv = "<<rv<<" v = "<<v<<" err = "<<strerror(v)<<" len = "<<len<<endl;
			
		}else{
			cout<<"failed socket"<<endl;
		}
		++it;
	}
	return 0;
}

