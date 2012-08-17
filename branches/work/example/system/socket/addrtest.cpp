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
#ifndef ON_WINDOWS
#include <poll.h>
#include <unistd.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <ifaddrs.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#endif
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "system/timespec.hpp"
#include "system/cassert.hpp"
#include <boost/concept_check.hpp>
#include <fcntl.h>
#include <cerrno>
#include <iostream>
#include <cstring>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

using namespace std;

void listLocalInterfaces();

void testLiterals();

int main(int argc, char *argv[]){
#ifdef ON_WINDOWS
	WSADATA	wsaData;
    int		err;
	WORD	wVersionRequested;
/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
    wVersionRequested = MAKEWORD(2, 2);

    err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
        /* Tell the user that we could not find a usable */
        /* Winsock DLL.                                  */
        printf("WSAStartup failed with error: %d\n", err);
        return 1;
    }
#endif
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
	int family = SocketInfo::Inet4;
	int type = SocketInfo::Stream;
	int proto = 0;
	
	//list all the local interfaces
	listLocalInterfaces();
	
	
	//SocketAddressInfo ai(node, srv);
	ResolveData rd =  synchronous_resolve(node, srv, flags, family, type, proto);
	ResolveIterator it(rd.begin());
	while(it != rd.end()){
		
		SocketAddress	sa(it);
		
		char			host[SocketInfo::HostStringCapacity];
		char			port[SocketInfo::ServiceStringCapacity];
		
		sa.toString(
			host, SocketInfo::HostStringCapacity,
			port, SocketInfo::ServiceStringCapacity,
			SocketInfo::NumericService | SocketInfo::NumericHost
		);
		
		cout<<"host = "<<host<<":"<<port<<endl;
		
		SocketDevice sd;
		sd.create(it);
		
		int rv = sd.connect(it);
		
		cout<<"rv = "<<rv<<endl;
		
		if(rv == OK){
			sd.write("hello word\r\n", strlen("hello word\r\n"));
		}
		
		SocketAddressInet4 sa4_0(it);
		
		sa4_0.toString(
			host, SocketInfo::HostStringCapacity,
			port, SocketInfo::ServiceStringCapacity,
			SocketInfo::NumericService | SocketInfo::NumericHost
		);
		
		cout<<"sa4_0 host = "<<host<<":"<<port<<endl;
		
		uint16 portx;
		uint32 addr;
		
		sa4_0.toUInt(addr, portx);
		
		++addr;
		
		SocketAddressInet4 sa4_1(addr, portx);
		
		sa4_1.toString(
			host, SocketInfo::HostStringCapacity,
			port, SocketInfo::ServiceStringCapacity,
			SocketInfo::NumericService | SocketInfo::NumericHost
		);
		
		cout<<"sa4_1 host = "<<host<<":"<<port<<endl;
		
		cassert(sa4_0 < sa4_1);
		
		cassert(sa4_0.address() < sa4_1.address());
		
		SocketAddressInet4::BinaryT	binaddr4;
		sa4_0.toBinary(binaddr4, portx);
		
		SocketAddressInet4 sa4_2(binaddr4, portx);
		
		sa4_2.toString(
			host, SocketInfo::HostStringCapacity,
			port, SocketInfo::ServiceStringCapacity,
			SocketInfo::NumericService | SocketInfo::NumericHost
		);
		
		cout<<"sa4_2 host = "<<host<<":"<<port<<endl;
		
		cassert(!(sa4_1 == sa4_2));
		cassert(sa4_0 == sa4_2);
		cassert(!(sa4_0 < sa4_2));
		cassert(!(sa4_2 < sa4_0));
		
// 		int sd = socket(it.family(), it.type(), it.protocol());
// 		struct timeval tosnd;
// 		struct timeval torcv;
// 		
// 		tosnd.tv_sec = -1;
// 		tosnd.tv_usec = -1;
// 		
// 		torcv.tv_sec = -1;
// 		torcv.tv_usec = -1;
// 		socklen_t socklen(sizeof(torcv));
// 		if(sd > 0){
// 			cout<<"Connecting..."<<endl;
// 			getsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char*)&torcv, &socklen);
// 			socklen = sizeof(tosnd);
// 			getsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, (char*)&tosnd, &socklen);
// 			cout<<"rcvtimeo "<<torcv.tv_sec<<"."<<torcv.tv_usec<<endl;
// 			cout<<"sndtimeo "<<tosnd.tv_sec<<"."<<tosnd.tv_usec<<endl;
// 			fcntl(sd, F_SETFL, O_NONBLOCK);
// 			if(!connect(sd, it.sockAddr(), it.size())){
// 				cout<<"Connected!"<<endl;
// 			}else{
// 				if(errno != EINPROGRESS){
// 					cout<<"Failed connect"<<endl;
// 				}else{
// 					cout<<"Polling ..."<<endl;
// 					pollfd pfd;
// 					pfd.fd = sd;
// 					pfd.events = 0;//POLLOUT;
// 					int rv = poll(&pfd, 1, -1);
// 					cout<<"pollrv = "<<rv<<endl;
// 					if(pfd.revents & (POLLERR | POLLHUP | POLLNVAL)){
// 						
// 						cout<<"poll err "<<(pfd.revents & POLLERR)<<' '<<(pfd.revents & POLLHUP)<<' '<<(pfd.revents & POLLNVAL)<<' '<<endl;
// 					}
// 				}
// 			}
// 			socklen_t len = 4;
// 			int v = 0;
// 			int rv = getsockopt(sd, SOL_SOCKET, SO_ERROR, &v, &len);
// 			cout<<"getsockopt rv = "<<rv<<" v = "<<v<<" err = "<<strerror(v)<<" len = "<<len<<endl;
// 			
// 		}else{
// 			cout<<"failed socket"<<endl;
// 		}
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
	s.create(SocketAddressInfo::Inet4, SocketAddressInfo::Stream, 0);
	
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
#ifndef ON_WINDOWS
	struct ifaddrs* ifap;
	if(::getifaddrs(&ifap)){
		cout<<"getifaddrs did not work"<<endl;
		return;
	}
	
	struct ifaddrs *it(ifap);
	char host[512];
	char srvc[128];
	
	while(it){
		//sockaddr_in *addr;
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
#endif
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
