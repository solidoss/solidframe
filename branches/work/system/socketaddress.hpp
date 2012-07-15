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
	friend struct SocketAddressInfo;
	SocketAddressInfoIterator(addrinfo *_pa):paddr(_pa){}
	mutable addrinfo	*paddr;
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
	
	SocketAddressInfo(){}
	SocketAddressInfo(const SocketAddressInfo &_rai):ib(_rai.ib.paddr){
		_rai.ib.paddr = NULL;
	}
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
	SocketAddressInfo& operator=(const SocketAddressInfo &_rai){
		ib.paddr = _rai.ib.paddr;
		_rai.ib.paddr = NULL;
		return *this;
	}
private:
	SocketAddressInfoIterator	ib;
};

struct SocketAddress;
struct SocketAddress4;
#ifdef ON_WINDOWS
typedef int socklen_t;
#endif
//! A pair of a sockaddr pointer and a size
/*!
	It is a commodity structure, it will not allocate data for sockaddr pointer
	nor it will delete it. Use this structure with SocketAddress and SocketAddressInfoIterator
*/
struct SocketAddressStub{
	SocketAddressStub(sockaddr *_pa = NULL, size_t _sz = 0):addr(_pa),sz(_sz){}
	SocketAddressStub(const SocketAddressInfoIterator &_it);
	SocketAddressStub(const SocketAddress &_rsa);
	SocketAddressStub(const SocketAddress4 &_rsa);
	SocketInfo::Family family()const{return (SocketInfo::Family)addr->sa_family;}
	SocketAddressStub& operator=(const SocketAddressInfoIterator &_it);
	SocketAddressStub& operator=(const SocketAddress &_rsa);
	SocketAddressStub& operator=(const SocketAddress4 &_rsa);
	bool isInet4()const{
		return sz == sizeof(sockaddr_in);
	}
	bool isInet6()const{
		return sz == sizeof(sockaddr_in6);
	}
	bool isLocal()const{
		return false;
	}
	socklen_t size()const{
		return sz;
	}
	void size(socklen_t _sz){
		sz = _sz;
	}
	uint16 port()const;
	const sockaddr	*address()const{
		return addr;
	}
private:
	const sockaddr	*addr;
	socklen_t		sz;
};


//! Holds a generic socket address
/*!
	The address will be hold within a buffer of size.
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
	
	static bool equalAdresses(
		SocketAddress const & _rsa1,
		SocketAddress const & _rsa2
	);
	static bool compareAddressesLess(
		SocketAddress const & _rsa1,
		SocketAddress const & _rsa2
	);
	
	SocketAddress():sz(0){clear();}
	SocketAddress(const SocketAddressInfoIterator &);
	SocketAddress(const SocketAddressStub &);
	SocketAddress& operator=(const SocketAddressInfoIterator &);
	SocketAddress& operator=(const SocketAddressStub &);
	SocketAddressInfo::Family family()const{return (SocketAddressInfo::Family)addr()->sa_family;}
	const socklen_t&	size()const {return sz;}
	socklen_t&	size(){return sz;}
	socklen_t&	size(const socklen_t &_rsz){
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
	size_t addressHash()const;
private:
	sockaddr_in* addrin(){return reinterpret_cast<sockaddr_in*>(buf);}
	const sockaddr_in* addrin()const{return reinterpret_cast<const sockaddr_in*>(buf);}
	socklen_t	sz;
	char 		buf[Capacity];
};

struct SocketAddressInet{
};

struct SocketAddressInet4{
};

struct SocketAddressInet6{
};

struct SocketAddressLocal{
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
	
	static bool equalAdresses(
		SocketAddress4 const & _rsa1,
		SocketAddress4 const & _rsa2
	);
	static bool compareAddressesLess(
		SocketAddress4 const & _rsa1,
		SocketAddress4 const & _rsa2
	);
	
	SocketAddress4(){clear();}
	SocketAddress4(const SocketAddressInfoIterator &);
	SocketAddress4(const SocketAddressStub &);
	
	SocketAddress4& operator=(const SocketAddressInfoIterator &);
	SocketAddress4& operator=(const SocketAddressStub &);
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
	size_t addressHash()const;
private:
	sockaddr_in* addrin(){return reinterpret_cast<sockaddr_in*>(buf);}
	const sockaddr_in* addrin()const{return reinterpret_cast<const sockaddr_in*>(buf);}
	char 		buf[Capacity];
};

typedef SocketAddress SocketAddress6;

#ifndef NINLINES
#include "system/socketaddress.ipp"
#endif


#endif
