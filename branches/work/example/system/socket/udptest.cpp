#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include <iostream>
using namespace std;

int main(){
	AddrInfo ai("0.0.0.0", "0", 0, AddrInfo::Inet4, AddrInfo::Datagram, 0);
	AddrInfoIterator it(ai.begin());
	
	SocketDevice sd;
	sd.create(it);
	sd.bind(it);
	
	if(sd.ok()){
		char				host[SocketAddress::HostNameCapacity];
		char				port[SocketAddress::ServiceNameCapacity];
		SocketAddress		addr;
		
		sd.localAddress(addr);
		addr.name(
			host,
			SocketAddress::HostNameCapacity,
			port,
			SocketAddress::ServiceNameCapacity,
			SocketAddress::NumericService | SocketAddress::NumericHost
		);
		cout<<host<<':'<<port<<endl;
	}
	cout<<sd.ok()<<endl;
	return 0;
}
