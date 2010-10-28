/* Implementation file addrtest.cpp
	
	Copyright 2007, 2008 Valentin Palade
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <poll.h>
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <iostream>
#include <cstring>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <ifaddrs.h>

#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

void listLocalInterfaces();

void testLiterals();

int main(int argc, char *argv[]){
	testLiterals();
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
	
	//list all the local interfaces
	listLocalInterfaces();
	
	
	//AddrInfo ai(node, srv);
	AddrInfo ai(node, srv, flags, family, type, proto);
	AddrInfoIterator it(ai.begin());
	while(it){
		int sd = socket(it.family(), it.type(), it.protocol());
		if(sd > 0){
			cout<<"Connecting..."<<endl;
			fcntl(sd, F_SETFL, O_NONBLOCK);
			if(!connect(sd, it.addr(), it.size())){
				cout<<"Connected!"<<endl;
			}else{
				if(errno != EINPROGRESS){
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

#if 0
void listLocalInterfaces(){
	enum{
		MAX_IFS = 64
	};
	SocketDevice s;
	s.create(AddrInfo::Inet4, AddrInfo::Stream, 0);
	
	ifreq *ifrp, *ifend;
	struct ifconf ifc;
	struct ifreq ifs[MAX_IFS];
	
	ifc.ifc_len = sizeof( ifs );
	ifc.ifc_buf = (char*)ifs;
	if (ioctl(s.descriptor(), SIOCGIFCONF, (caddr_t)&ifc) < 0){
		cout<<"error ioctl SIOCGIFCONF "<<strerror(errno)<<endl;
		return;
	}
	
	//iterate through interface info:
	int total,current;
	int remaining = total = ifc.ifc_len;
	ifrp = ifc.ifc_req;
	ifend = ifs + (ifc.ifc_len / sizeof(struct ifreq));
	
	char host[512];
	char srvc[128];
	
	for(;ifrp != ifend; ++ifrp){
		sockaddr_in *addr;
		if( ifrp->ifr_addr.sa_family == AF_INET ){
			addr = (struct sockaddr_in *)&(ifrp->ifr_addr);
			getnameinfo(&(ifrp->ifr_addr), sizeof(sockaddr_in), host, 512, srvc, 128, NI_NUMERICHOST | NI_NUMERICSERV);
			cout<<"name = "<<ifrp->ifr_name<<" addr = "<<host<<":"<<srvc<<endl;
		}
	}

}

#endif

void listLocalInterfaces(){
	struct ifaddrs* ifap;
	if(::getifaddrs(&ifap)){
		cout<<"getifaddrs did not work"<<endl;
		return;
	}
	
	struct ifaddrs *it(ifap);
	char host[512];
	char srvc[128];
	
	while(it){
		sockaddr_in *addr;
		if(it->ifa_addr && it->ifa_addr->sa_family == AF_INET)
        {
            struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(it->ifa_addr);
            if(addr->sin_addr.s_addr != 0){
            	getnameinfo(it->ifa_addr, sizeof(sockaddr_in), host, 512, srvc, 128, NI_NUMERICHOST | NI_NUMERICSERV);
				cout<<"name = "<<it->ifa_name<<" addr = "<<host<<":"<<srvc<<endl;
            }
		}
		it = it->ifa_next;
	}
	freeifaddrs(ifap);
}


void testLiterals(){
	uint64 uv64(-1);
	uint32 uv32(-1);
	int64  v64(-1);
	cout<<"uv64 = "<<uv64<<endl;
	cout<<"uv32 = "<<uv32<<endl;
	cout<<"v64 = "<<v64<<endl;
	cout<<"uv64 == -1 "<<(uv64 == -1)<<endl;
	cout<<"uv32 == -1 "<<(uv32 == -1)<<endl;
	cout<<"uv64 == -1 "<<(-1 == uv64)<<endl;
	cout<<"uv32 == -1 "<<(-1 == uv32)<<endl;
}
