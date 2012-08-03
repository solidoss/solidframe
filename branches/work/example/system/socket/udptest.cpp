#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include <iostream>
using namespace std;

int main(){
	ResolveData rd =  synchronous_resolve("0.0.0.0", "0", 0, SocketInfo::Inet4, SocketInfo::Datagram, 0);
	ResolveIterator it(rd.begin());
	
	SocketDevice sd;
	sd.create(it);
	sd.bind(it);
	
	if(sd.ok()){
		char				host[SocketInfo::HostStringCapacity];
		char				port[SocketInfo::ServiceStringCapacity];
		SocketAddress		addr;
		
		sd.localAddress(addr);
		addr.toString(
			host,
			SocketInfo::HostStringCapacity,
			port,
			SocketInfo::ServiceStringCapacity,
			SocketInfo::NumericService | SocketInfo::NumericHost
		);
		cout<<host<<':'<<port<<endl;
	}
	cout<<sd.ok()<<endl;
	return 0;
}
