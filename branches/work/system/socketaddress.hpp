/* Declarations file socketaddress.hpp
	
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

#ifndef SYSTEM_SOCKETADDRESS_HPP
#define SYSTEM_SOCKETADDRESS_HPP

#include "common.hpp"
#include "socketinfo.hpp"

struct SocketAddressInfo;
//struct sockaddr_in;
//struct sockaddr_in6;
//==================================================================
//! A wrapper for POSIX addrinfo (see man getaddrinfo)
/*!
	Usually it will hold all data needed for creating and connecting 
	a socket
*/
struct ResolveIterator{
	ResolveIterator():paddr(NULL){}
	ResolveIterator& next(){paddr = paddr->ai_next; return *this;}
	int family()const {return paddr->ai_family;}
	int type()const {return paddr->ai_socktype;}
	int protocol()const{return paddr->ai_protocol;}
	size_t size()const{return paddr->ai_addrlen;}
	sockaddr* address()const{return paddr->ai_addr;}
	operator bool()const{return paddr != NULL;}
	ResolveIterator &operator++(){return next();}
private:
	friend struct ResolveData;
	ResolveIterator(addrinfo *_pa):paddr(_pa){}
	const addrinfo	*paddr;
};
//==================================================================
//! A wrapper for POSIX getaddrinfo (see man getaddrinfo)
/*!
	This is an address resolver class.
	It resolves names to ip addresses.
	It is a blocking resolver, so use it with care expecially when combined with
	nonblocking/asynchronous IO.
*/
struct ResolveData{
	enum Flags{
		CannonName = AI_CANONNAME,
		NumericHost = AI_NUMERICHOST,
		All	= AI_ALL,
		AddrConfig = AI_ADDRCONFIG,
		V4Mapped  = AI_V4MAPPED,
		NumericService = AI_NUMERICSERV
	};
	typedef ResoveIterator const_iterator;
	
	ResolveData(){}
	ResolveData(const ResolveData &_rai);
	
	~ResolveData();
	//! Get an iterator to he first resolved ip address
	const_iterator begin()const;
	const_iterator end()const;
	//! Check if the returned list of ip addresses is empty
	bool empty()const;
	ResolveData& operator=(const ResolveData &_rrd);
private:
	SharedStub	*pss;
};

ResolveData synchronous_resolve(
	const char *_node, 
	const char *_service, 
	int _flags,
	int _family = -1,
	int _type = -1,
	int _proto = -1
);	

ResolveData synchronous_resolve(const char *_node, int _port);
ResolveData synchronous_resolve(
	const char *_node, 
	int _port,
	int _flags,
	int _family = -1,
	int _type = -1,
	int _proto = -1
);
//==================================================================
struct SocketAddress;
struct SocketAddressInet;
struct SocketAddressInet4;
struct SocketAddressInet6;
struct SocketAddressLocal;
#ifdef ON_WINDOWS
typedef int socklen_t;
#endif
//! A pair of a sockaddr pointer and a size
/*!
	It is a commodity structure, it will not allocate data
	for sockaddr pointer nor it will delete it. Use this 
	structure with SocketAddress and SocketAddressInfoIterator
*/
struct SocketAddressStub{
	SocketAddressStub(sockaddr *_pa = NULL, size_t _sz = 0):addr(_pa),sz(_sz){}
	
	SocketAddressStub(const SocketAddressInfoIterator &_it);
	SocketAddressStub(const SocketAddress &_rsa);
	SocketAddressStub(const SocketAddressInet &_rsa);
	SocketAddressStub(const SocketAddressInet4 &_rsa);
	SocketAddressStub(const SocketAddressInet6 &_rsa);
	SocketAddressStub(const SocketAddressLocal &_rsa);
	
	SocketInfo::Family family()const{return (SocketInfo::Family)addr->sa_family;}
	
	SocketAddressStub& operator=(const SocketAddressInfoIterator &_it);
	SocketAddressStub& operator=(const SocketAddress &_rsa);
	SocketAddressStub& operator=(const SocketAddressInet &_rsa);
	SocketAddressStub& operator=(const SocketAddressInet4 &_rsa);
	SocketAddressStub& operator=(const SocketAddressInet6 &_rsa);
	SocketAddressStub& operator=(const SocketAddressLocal &_rsa);
	
	void clear(){
		addr = NULL;
		sz = 0;
	}
	bool isInet4()const{
		return addr->sa_family == AF_INET;
	}
	bool isInet6()const{
		return addr->sa_family == AF_INET6;
	}
	bool isLocal()const{
		return addr->sa_family == AF_UNIX;
	}
	
	socklen_t size()const{
		return sz;
	}
	uint16 port()const;
	
	const sockaddr	*sockAddr()const{
		return addr;
	}
private:
	const sockaddr	*addr;
	socklen_t		sz;
};
//==================================================================
//! Holds a generic socket address
/*!
	On unix it can be either: inet_v4, inet_v6 or unix/local address 
*/
struct SocketAddress{
private:
	union AddrUnion{
		sockaddr		addr;
		sockaddr_in 	inaddr4;
		sockaddr_in6 	inaddr6;
		sockaddr_un		localaddr;
	};
public:
	enum {Capacity = sizeof(AddrUnion)};
	
	SocketAddress():sz(0){clear();}
	SocketAddress(const SocketAddressInfoIterator &);
	SocketAddress(const SocketAddressStub &);
	SocketAddress(const char* _addr, int _port = 0);
	
	SocketAddress& operator=(const SocketAddressInfoIterator &);
	SocketAddress& operator=(const SocketAddressStub &);
	
	SocketAddressInfo::Family family()const{
		return (SocketAddressInfo::Family)sockAddr()->sa_family;
	}
	
	bool isInet4()const{
		return sockAddr()->sa_family == AF_INET;
	}
	bool isInet6()const{
		return sockAddr()->sa_family == AF_INET6;
	}
	bool isLocal()const{
		return sockAddr()->sa_family == AF_UNIX;
	}
	
	bool isLoopback()const;
	bool isInvalid()const;
	bool isBroadcast()const;
	
	bool empty()const;
	
	const socklen_t&	size()const {return sz;}

	sockaddr* sockAddr(){
		return &d.addr;
	}
	const sockaddr* sockAddr()const{
		return &d.addr;
	}
	operator sockaddr*(){return sockAddr();}
	operator const sockaddr*()const{return sockAddr();}
	//! Get the name associated to the address
	/*!
		Generates the string name associated to a specific address
		filling the given buffers. It is a wrapper for POSIX,
		getnameinfo.
		Usage:<br>
		<CODE>
		char			host[SocketAddress::HostNameCapacity];<br>
		char			port[SocketAddress::ServerNameCapacity];<br>
		SocketAddress	addr;<br>
		channel().localAddress(addr);<br>
		addr.name(<br>
			host,<br>
			SocketAddress::HostNameCapacity,<br>
			port,<br>
			SocketAddress::ServiceNameCapacity,<br>
			SocketAddress::NumericService<br>
		);<br>
		</CODE>
		\retval BAD for error, OK for success.
		\param _host An output buffer to keep the host name.
		\param _hostcp The capacity of the output host buffer.
		\param _serv An output buffer to keep the service name/port.
		\param _servcp The capacity of the output service buffer.
		\param _flags Some request flags
	*/
	int toString(
		char* _host,
		unsigned _hostcp,
		char* _serv,
		unsigned _servcp,
		uint32	_flags = 0
	)const;
	
	bool operator<(const SocketAddress &_raddr)const;
	bool operator==(const SocketAddress &_raddr)const;
	
	void address(const char*_str);
	
	in_addr& address(){
		return d.inaddr4.sin_addr;
	}
	
	const in_addr& address()const{
		return d.inaddr4.sin_addr;
	}
	
	int port()const;
	void port(int _port);
	void clear();
	size_t hash()const;
	size_t addressHash()const;
	
	void path(const char*_pth);
	const char* path()const;
private:
	AddrUnion	d;
	socklen_t	sz;
};
//==================================================================
struct SocketAddressInet{
};
//==================================================================
struct SocketAddressInet4{
};
//==================================================================
struct SocketAddressInet6{
};
//==================================================================
bool operator<(const in_addr &_inaddr1, const in_addr &_inaddr1);

bool operator==(const in_addr &_inaddr1, const in_addr &_inaddr1);

bool operator<(const in6_addr &_inaddr1, const in6_addr &_inaddr1);

bool operator==(const in6_addr &_inaddr1, const in6_addr &_inaddr1);

size_t hash(const in_addr &_inaddr);

size_t hash(const in6_addr &_inaddr);
//==================================================================
struct SocketAddressLocal{
};
//==================================================================
#ifndef NINLINES
#include "system/socketaddress.ipp"
#endif
//==================================================================

#endif
