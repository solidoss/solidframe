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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "common.hpp"

struct SocketAddressInfo;
//struct sockaddr_in;
//struct sockaddr_in6;
//! A wrapper for POSIX addrinfo (see man getaddrinfo)
/*!
	Usually it will hold all data needed for creating and connecting 
	a socket
*/
struct SocketAddressInfoIterator{
	SocketAddressInfoIterator():paddr(NULL){}
	SocketAddressInfoIterator& next(){paddr = paddr->ai_next; return *this;}
	int family()const {return paddr->ai_family;}
	int type()const {return paddr->ai_socktype;}
	int protocol()const{return paddr->ai_protocol;}
	size_t size()const{return paddr->ai_addrlen;}
	sockaddr* addr()const{return paddr->ai_addr;}
	operator bool()const{return paddr != NULL;}
	SocketAddressInfoIterator &operator++(){return next();}
private:
	friend class SocketAddressInfo;
	SocketAddressInfoIterator(addrinfo *_pa):paddr(_pa){}
	addrinfo	*paddr;
};
//! A wrapper for POSIX getaddrinfo (see man getaddrinfo)
/*!
	This is an address resolver class.
	It resolves names to ip addresses.
	It is a blocking resolver, so use it with care expecially when combined with
	nonblocking/asynchronous IO.
*/
struct SocketAddressInfo{
	enum Flags{
		CannonName = AI_CANONNAME,
		NumericHost = AI_NUMERICHOST,
		All	= AI_ALL,
		AddrConfig = AI_ADDRCONFIG,
		V4Mapped  = AI_V4MAPPED,
		NumericServ = AI_NUMERICSERV
	};
	enum Family{
		Local = AF_UNIX,
		Inet4 = AF_INET,
		Inet6 = AF_INET6
	};
	enum Type{
		Stream = SOCK_STREAM,
		Datagram = SOCK_DGRAM
	};
	SocketAddressInfo(){}
	//! Create an addr info using a name and a service name
	SocketAddressInfo(const char *_node, const char *_service){
		reinit(_node, _service);
	}
	//! Create an addr info using a name and a service name and extra parameters
	/*!
		Using this constructor you can request certain connection family, type protocol.
	*/
	SocketAddressInfo(
		const char *_node, 
		const char *_service, 
		int _flags,
		int _family = -1,
		int _type = -1,
		int _proto = -1
	){
		reinit(_node, _service, _flags, _family, _type, _proto);
	}
	//! Create an addr info using a name and a service port
	SocketAddressInfo(const char *_node, int _port);
	//! Create an addr info using a name and a service port and extra parameters
	/*!
		Using this constructor you can request certain connection family, type protocol.
	*/
	SocketAddressInfo(
		const char *_node, 
		int _port,
		int _flags,
		int _family = -1,
		int _type = -1,
		int _proto = -1
	){
		reinit(_node, _port, _flags, _family, _type, _proto);
	}
	
	~SocketAddressInfo();
	//! Initiate an addr info using a name and a service name
	void reinit(const char *_node, const char *_service);
	//! Initiate an addr info using a name and a service name and extra parameters
	void reinit(
		const char *_node, 
		const char *_service,
		int _flags,
		int _family = -1,
		int _type = -1,
		int _proto = -1	
	);
	//! Initiate an addr info using a name and a service port
	void reinit(const char *_node, int _port);
	//! Initiate an addr info using a name and a service port and extra parameters
	void reinit(
		const char *_node, 
		int _port,
		int _flags,
		int _family = -1,
		int _type = -1,
		int _proto = -1
	);
	//! Get an iterator to he first resolved ip address
	SocketAddressInfoIterator begin(){ return ib;}
	//! Check if the returned list of ip addresses is empty
	bool empty()const{return ib.paddr == NULL;}
private:
	SocketAddressInfoIterator	ib;
};

struct SocketAddress;
struct SocketAddress4;
//! A pair of a sockaddr pointer and a size
/*!
	It is a commodity structure, it will not allocate data for sockaddr pointer
	nor it will delete it. Use this structure in with SocketAddress and SocketAddressInfoIterator
*/
struct SocketAddressPair{
	SocketAddressPair(sockaddr *_pa = NULL, size_t _sz = 0):addr(_pa),sz(_sz){}
	SocketAddressPair(const SocketAddressInfoIterator &_it);
	SocketAddressPair(const SocketAddress &_rsa);
	SocketAddressPair(const SocketAddress4 &_rsa);
	SocketAddressInfo::Family family()const{return (SocketAddressInfo::Family)addr->sa_family;}
	SocketAddressPair& operator=(const SocketAddressInfoIterator &_it);
	SocketAddressPair& operator=(const SocketAddress &_rsa);
	SocketAddressPair& operator=(const SocketAddress4 &_rsa);
	bool isInet4()const{
		return sz == sizeof(sockaddr_in);
	}
	bool isInet6()const{
		return sz == sizeof(sockaddr_in6);
	}
	socklen_t size()const{
		return sz;
	}
	void size(socklen_t _sz){
		sz = _sz;
	}
	//bool operator<(const SocketAddressPair &_addr)const;
	const sockaddr	*addr;
	socklen_t	sz;
};

//! A pair of a sockaddr_in pointer and a size
/*!
	It is a commodity structure, it will not allocate data for sockaddr pointer
	nor it will delete it. Use this structure in with SocketAddress and SocketAddressInfoIterator
*/

struct SocketAddressPair4{
	SocketAddressPair4(const SocketAddressPair &_rsap);
	SocketAddressPair4(const SocketAddress &_rsa);
	int port()const;
	void port(uint16 _port);
	bool operator<(const SocketAddressPair4 &_addr)const;
	SocketAddressInfo::Family family()const{return (SocketAddressInfo::Family)addr->sin_family;}
	sockaddr_in	*addr;
	socklen_t	size()const{return sizeof(sockaddr_in);}
};

//! A pair of a sockaddr_in6 pointer and a size
/*!
	It is a commodity structure, it will not allocate data for sockaddr pointer
	nor it will delete it. Use this structure in with SocketAddress and SocketAddressInfoIterator
*/
struct SocketAddressPair6{
	SocketAddressPair6(const SocketAddressPair &_rsa);
	SocketAddressPair6(const SocketAddress &_rsa);
	int port()const;
	void port(uint16 _port);
	bool operator<(const SocketAddressPair6 &_addr)const;
	SocketAddressInfo::Family family()const{return (SocketAddressInfo::Family)addr->sin6_family;}
	sockaddr_in6	*addr;
	socklen_t	size()const{return sizeof(sockaddr_in6);}
};

//! Holds a socket address
/*!
	The address will be hold within a buffer of size,
	sizeof(sockaddr_in6).
*/
struct SocketAddress{
	enum {Capacity = sizeof(sockaddr_in6)};
	enum {HostNameCapacity = NI_MAXHOST};
	enum {ServiceNameCapacity = NI_MAXSERV};
	//! Some request flags
	enum {
		NumericHost = NI_NUMERICHOST,	//!< Generate only numeric host
		NameRequest = NI_NAMEREQD,		//!< Force name lookup - fail if not found
		NumericService = NI_NUMERICSERV	//!< Generate only the port number
	};
	SocketAddress():sz(0){clear();}
	SocketAddress(const SocketAddressInfoIterator &);
	SocketAddress(const SocketAddressPair &);
	SocketAddress(const SocketAddressPair4 &);
	SocketAddress(const SocketAddressPair6 &);
	SocketAddress& operator=(const SocketAddressInfoIterator &);
	SocketAddress& operator=(const SocketAddressPair &);
	SocketAddressInfo::Family family()const{return (SocketAddressInfo::Family)addr()->sa_family;}
	const socklen_t&	size()const {return sz;}
	socklen_t&	size(){return sz;}
	const socklen_t&	size(const socklen_t &_rsz){
		sz = _rsz;
		return sz;
	}
	sockaddr* addr(){return reinterpret_cast<sockaddr*>(buf);}
	const sockaddr* addr()const{return reinterpret_cast<const sockaddr*>(buf);}
	operator sockaddr*(){return addr();}
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
	int name(
		char* _host,
		unsigned _hostcp,
		char* _serv,
		unsigned _servcp,
		uint32	_flags = 0
	)const;
	
	void addr(const sockaddr* _sa, size_t _sz);
	
	bool operator<(const SocketAddress &_raddr)const;
	bool operator==(const SocketAddress &_raddr)const;
	
	int port()const;
	void port(int _port);
	void clear();
	size_t hash()const;
private:
	sockaddr_in* addrin(){return reinterpret_cast<sockaddr_in*>(buf);}
	const sockaddr_in* addrin()const{return reinterpret_cast<const sockaddr_in*>(buf);}
	socklen_t	sz;
	char 		buf[Capacity];
};

struct SocketAddress4{
	enum {Capacity = sizeof(sockaddr_in)};
	enum {HostNameCapacity = NI_MAXHOST};
	enum {ServiceNameCapacity = NI_MAXSERV};
	//! Some request flags
	enum {
		NumericHost = NI_NUMERICHOST,	//!< Generate only numeric host
		NameRequest = NI_NAMEREQD,		//!< Force name lookup - fail if not found
		NumericService = NI_NUMERICSERV	//!< Generate only the port number
	};
	SocketAddress4(){clear();}
	SocketAddress4(const SocketAddressInfoIterator &);
	SocketAddress4(const SocketAddressPair &);
	SocketAddress4(const SocketAddressPair4 &);
	SocketAddress4& operator=(const SocketAddressInfoIterator &);
	SocketAddress4& operator=(const SocketAddressPair &);
	SocketAddressInfo::Family family()const{return (SocketAddressInfo::Family)addr()->sa_family;}
	socklen_t	size()const {return Capacity;}
	//socklen_t&	size(){return sz;}
	sockaddr* addr(){return reinterpret_cast<sockaddr*>(buf);}
	const sockaddr* addr()const{return reinterpret_cast<const sockaddr*>(buf);}
	operator sockaddr*(){return addr();}
	//! Get the name associated to the address
	/*!
		Generates the string name associated to a specific address
		filling the given buffers. It is a wrapper for POSIX,
		getnameinfo.
		Usage:<br>
		<CODE>
		char			host[SocketAddress::HostNameCapacity];<br>
		char			port[SocketAddress::ServiceNameCapacity];<br>
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
	int name(
		char* _host,
		unsigned _hostcp,
		char* _serv,
		unsigned _servcp,
		uint32	_flags = 0
	)const;
	
	void addr(const sockaddr* _sa, size_t _sz);
	
	bool operator<(const SocketAddress4 &_raddr)const;
	bool operator==(const SocketAddress4 &_raddr)const;
	
	int port()const;
	void port(int _port);
	void clear();
	size_t hash()const;
private:
	sockaddr_in* addrin(){return reinterpret_cast<sockaddr_in*>(buf);}
	const sockaddr_in* addrin()const{return reinterpret_cast<const sockaddr_in*>(buf);}
	char 		buf[Capacity];
};


#ifndef NINLINES
#include "system/socketaddress.ipp"
#endif


#endif
