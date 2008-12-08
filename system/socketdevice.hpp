#ifndef SYSTEM_SOCKETDEVICE_HPP
#define SYSTEM_SOCKETDEVICE_HPP

#include "system/socketaddress.hpp"
#include "system/device.hpp"

class SocketDevice: public Device{
public:
	SocketDevice();
	~SocketDevice();
	void shutdownRead();
	void shutdownWrite();
	void shutdownReadWrite();
	int create(const AddrInfoIterator &_rai);
	int create(AddrInfo::Family = AddrInfo::Inet4, AddrInfo::Type _type = AddrInfo::Stream, int _proto = 0);
	int connect(const AddrInfoIterator &_rai);
	int bind(const AddrInfoIterator &_rai);
	int bind(const SockAddrPair &_rsa);
	int prepareAccept(const AddrInfoIterator &_rai, unsigned _listencnt = 10);
	int accept(SocketDevice &_dev);
	int makeBlocking(int _msec = -1);
	int makeNonBlocking();
	bool isBlocking();
	bool shouldWait()const;
	int send(const char* _pb, unsigned _ul, unsigned _flags = 0);
	int recv(char *_pb, unsigned _ul, unsigned _flags = 0);
	int send(const char* _pb, unsigned _ul, const SockAddrPair &_sap);
	int recv(char *_pb, unsigned _ul, SocketAddress &_rsa);
	int remoteAddress(SocketAddress &_rsa)const;
	int localAddress(SocketAddress &_rsa)const;
	int descriptor()const{return Device::descriptor();}
	
};


#endif
