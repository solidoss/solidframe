// system/socketaddress.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_SOCKETADDRESS_HPP
#define SYSTEM_SOCKETADDRESS_HPP

#ifndef ON_WINDOWS
#include <sys/un.h>
#include <arpa/inet.h>
#endif

#include <ostream>

#include "system/common.hpp"
#include "system/socketinfo.hpp"
#include "system/binary.hpp"

#if defined(HAS_CPP11) && !defined(USHAREDBACKEND)
#include <memory>
#elif defined(UBOOSTSHAREDPTR) && !defined(USHAREDBACKEND)
#include "boost/shared_ptr.hpp"
#else
#include "system/sharedbackend.hpp"
#endif

#ifndef NINLINES
#include "system/cassert.hpp"
#include "system/debug.hpp"
#endif

namespace solid{

class SocketDevice;
//struct sockaddr_in;
//struct sockaddr_in6;
//==================================================================
//! An interator for POSIX addrinfo (see man getaddrinfo)
/*!
	Usually it will hold all data needed for creating and connecting 
	a socket. Use ResolverData::begin() to get started.
*/
struct ResolveIterator{
	ResolveIterator();
	ResolveIterator& next();
	int family()const;
	int type()const;
	int protocol()const;
	size_t size()const;
	sockaddr* sockAddr()const;
	operator bool()const;
	ResolveIterator &operator++();
	bool operator==(const ResolveIterator &_rrit)const;
private:
	friend struct ResolveData;
	ResolveIterator(addrinfo *_pa);
	const addrinfo	*paddr;
};
//==================================================================
//! A shared pointer for POSIX addrinfo (see man getaddrinfo)
/*!
	Use synchronous_resolve to create ResolveData objects.
*/
struct ResolveData{
	enum Flags{
#ifndef ON_WINDOWS
		CannonName = AI_CANONNAME,
		NumericHost = AI_NUMERICHOST,
		All	= AI_ALL,
		AddrConfig = AI_ADDRCONFIG,
		V4Mapped  = AI_V4MAPPED,
		NumericService = AI_NUMERICSERV
#else
		CannonName,
		NumericHost,
		All,
		AddrConfig,
		V4Mapped,
		NumericService
#endif
	};
	typedef ResolveIterator const_iterator;
	
	ResolveData();
	ResolveData(addrinfo *_pai);
	ResolveData(const ResolveData &_rai);
	
	
	~ResolveData();
	//! Get an iterator to he first resolved ip address
	const_iterator begin()const;
	const_iterator end()const;
	//! Check if the returned list of ip addresses is empty
	bool empty()const;
	void clear();
	ResolveData& operator=(const ResolveData &_rrd);
private:
	static void delete_addrinfo(void *_pv);
#if defined(HAS_CPP11) && !defined(USHAREDBACKEND)
	typedef std::shared_ptr<addrinfo>	AddrInfoSharedPtrT;
	AddrInfoSharedPtrT		aiptr;
#elif defined(UBOOSTSHAREDPTR) && !defined(USHAREDBACKEND)
	typedef boost::shared_ptr<addrinfo>	AddrInfoSharedPtrT;
	AddrInfoSharedPtrT		aiptr;
#else
	SharedStub				*pss;
#endif
};

ResolveData synchronous_resolve(const char *_node, const char *_service);
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
	structure with SocketAddress and ResolveIterator
*/
struct SocketAddressStub{
	SocketAddressStub(sockaddr *_pa = NULL, size_t _sz = 0);
	
	SocketAddressStub(const ResolveIterator &_it);
	SocketAddressStub(const SocketAddress &_rsa);
	SocketAddressStub(const SocketAddressInet &_rsa);
	SocketAddressStub(const SocketAddressInet4 &_rsa);
	SocketAddressStub(const SocketAddressInet6 &_rsa);
	SocketAddressStub(const SocketAddressLocal &_rsa);
	
	SocketAddressStub& operator=(const ResolveIterator &_it);
	SocketAddressStub& operator=(const SocketAddress &_rsa);
	SocketAddressStub& operator=(const SocketAddressInet &_rsa);
	SocketAddressStub& operator=(const SocketAddressInet4 &_rsa);
	SocketAddressStub& operator=(const SocketAddressInet6 &_rsa);
	SocketAddressStub& operator=(const SocketAddressLocal &_rsa);
	
	operator const sockaddr*()const;
	
	void clear();
	
	SocketInfo::Family family()const;
	
	bool isInet4()const;
	bool isInet6()const;
	bool isLocal()const;
	
	socklen_t size()const;
	int port()const;
	
	const sockaddr	*sockAddr()const;
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
#ifndef ON_WINDOWS
		sockaddr_un		localaddr;
#endif
	};
public:
	enum {Capacity = sizeof(AddrUnion)};
	
	SocketAddress();
	SocketAddress(const SocketAddressStub &);
	SocketAddress(const char* _addr, int _port);
	SocketAddress(const char* _path);
	
	SocketAddress& operator=(const SocketAddressStub &);
	
	SocketInfo::Family family()const;
	
	bool isInet4()const;
	bool isInet6()const;
	bool isLocal()const;
	
	bool isLoopback()const;
	bool isInvalid()const;
	
	bool empty()const;
	
	const socklen_t&	size()const;

	const sockaddr* sockAddr()const;
	operator const sockaddr*()const;
	//! Get the name associated to the address
	/*!
		Generates the string name associated to a specific address
		filling the given buffers. It is a wrapper for POSIX,
		getnameinfo.
		Usage:<br>
		<CODE>
		char			host[SocketAddress::HostStringCapacity];<br>
		char			port[SocketAddress::ServerNameCapacity];<br>
		SocketAddress	addr;<br>
		channel().localAddress(addr);<br>
		addr.name(<br>
			host,<br>
			SocketAddress::HostStringCapacity,<br>
			port,<br>
			SocketAddress::ServiceStringCapacity,<br>
			SocketAddress::NumericService<br>
		);<br>
		</CODE>
		\retval true for success, false for error.
		\param _host An output buffer to keep the host name.
		\param _hostcp The capacity of the output host buffer.
		\param _serv An output buffer to keep the service name/port.
		\param _servcp The capacity of the output service buffer.
		\param _flags Some request flags
	*/
	bool toString(
		char* _host,
		unsigned _hostcp,
		char* _serv,
		unsigned _servcp,
		uint32	_flags = 0
	)const;
	
	bool operator<(const SocketAddress &_raddr)const;
	bool operator==(const SocketAddress &_raddr)const;
	
	void address(const char*_str);
	
	const in_addr& address4()const;
	const in6_addr& address6()const;
	int port()const;
	bool port(int _port);
	void clear();
	size_t hash()const;
	size_t addressHash()const;
	
	void path(const char*_pth);
	const char* path()const;
private:
	friend class SocketDevice;
	operator sockaddr*();
	sockaddr* sockAddr();
	AddrUnion	d;
	socklen_t	sz;
};
//==================================================================
struct SocketAddressInet{
private:
	union AddrUnion{
		sockaddr		addr;
		sockaddr_in 	inaddr4;
		sockaddr_in6 	inaddr6;
	};
public:
	enum {Capacity = sizeof(AddrUnion)};
	
	typedef Binary<4>	Binary4T;
	typedef Binary<16>	Binary6T;
	
	SocketAddressInet();
	SocketAddressInet(const SocketAddressStub &);
	SocketAddressInet(const char* _addr, int _port = 0);
	
	SocketAddressInet& operator=(const SocketAddressStub &);
	
	SocketInfo::Family family()const;
	
	bool isInet4()const;
	bool isInet6()const;
	
	bool isLoopback()const;
	bool isInvalid()const;
	
	bool empty()const;
	
	const socklen_t&	size()const;

	const sockaddr* sockAddr()const;
	operator const sockaddr*()const;
	//! Get the name associated to the address
	//! \see SocketAddress::toString
	bool toString(
		char* _host,
		unsigned _hostcp,
		char* _serv,
		unsigned _servcp,
		uint32	_flags = 0
	)const;
	
	bool toBinary(Binary4T &_bin, uint16 &_port)const;
	bool toBinary(Binary6T &_bin, uint16 &_port)const;
	void fromBinary(const Binary4T &_bin, uint16 _port = 0);
	void fromBinary(const Binary6T &_bin, uint16 _port = 0);
	
	bool operator<(const SocketAddressInet &_raddr)const;
	bool operator==(const SocketAddressInet &_raddr)const;
	
	void address(const char*_str);
	
	const in_addr& address4()const;
	const in6_addr& address6()const;
	
	int port()const;
	bool port(int _port);
	void clear();
	size_t hash()const;
	size_t addressHash()const;
	
private:
	friend class SocketDevice;
	operator sockaddr*();
	sockaddr* sockAddr();
	AddrUnion	d;
	socklen_t	sz;
};
//==================================================================
struct SocketAddressInet4{
private:
	union AddrUnion{
		sockaddr		addr;
		sockaddr_in 	inaddr4;
	};
public:
	enum {Capacity = sizeof(AddrUnion)};
	typedef Binary<4>	BinaryT;
	
	SocketAddressInet4();
	SocketAddressInet4(const SocketAddressStub &);
	SocketAddressInet4(const char* _addr, int _port = 0);
	SocketAddressInet4(uint32 _addr, int _port);
	SocketAddressInet4(const BinaryT &_addr, int _port = 0);
	
	//SocketAddressInet4& operator=(const ResolveIterator &);
	SocketAddressInet4& operator=(const SocketAddressStub &);
			
	SocketInfo::Family family()const;
	bool isInet4()const;
		
	bool isLoopback()const;
	bool isInvalid()const;
	
	socklen_t	size()const;

	const sockaddr* sockAddr()const;
	//operator sockaddr*(){return sockAddr();}
	operator const sockaddr*()const;
	//! Get the name associated to the address
	//! \see SocketAddress::toString
	bool toString(
		char* _host,
		unsigned _hostcp,
		char* _serv,
		unsigned _servcp,
		uint32	_flags = 0
	)const;
	
	void toBinary(BinaryT &_bin, uint16 &_port)const;
	
	void toUInt(uint32 &_addr, uint16 &_port)const;
	
	void fromBinary(const BinaryT &_bin, uint16 _port = 0);
	void fromUInt(uint32 _addr, uint16 _port = 0);
	
	
	bool operator<(const SocketAddressInet4 &_raddr)const;
	bool operator==(const SocketAddressInet4 &_raddr)const;
	
	void address(const char*_str);
	
	const in_addr& address()const;
	
	int port()const;
	void port(int _port);
	void clear();
	size_t hash()const;
	size_t addressHash()const;
	
private:
	friend class SocketDevice;
	operator sockaddr*();
	sockaddr* sockAddr();
	AddrUnion	d;
};
//==================================================================
struct SocketAddressInet6{
private:
	union AddrUnion{
		sockaddr		addr;
		sockaddr_in6 	inaddr6;
	};
public:
	enum {Capacity = sizeof(AddrUnion)};
	typedef Binary<16>	BinaryT;
	
	SocketAddressInet6();
	SocketAddressInet6(const SocketAddressStub &);
	SocketAddressInet6(const char* _addr, int _port = 0);
	SocketAddressInet6(const BinaryT &_addr, int _port = 0);
	
	SocketAddressInet6& operator=(const SocketAddressStub &);
		
	SocketInfo::Family family()const;
	bool isInet6()const;
	
	bool isLoopback()const;
	bool isInvalid()const;
	
	bool empty()const;
	
	socklen_t size()const;

	const sockaddr* sockAddr()const;
	operator const sockaddr*()const;
	
	//! Get the name associated to the address
	//! \see SocketAddress::toString
	bool toString(
		char* _host,
		unsigned _hostcp,
		char* _serv,
		unsigned _servcp,
		uint32	_flags = 0
	)const;
	
	void toBinary(BinaryT &_bin, uint16 &_port)const;
	
	void fromBinary(const BinaryT &_bin, uint16 _port = 0);
	
	bool operator<(const SocketAddressInet6 &_raddr)const;
	bool operator==(const SocketAddressInet6 &_raddr)const;
	
	void address(const char*_str);
	
	const in6_addr& address()const;
	
	int port()const;
	void port(int _port);
	void clear();
	size_t hash()const;
	size_t addressHash()const;
	
private:
	friend class SocketDevice;
	operator sockaddr*();
	sockaddr* sockAddr();
	AddrUnion	d;
};
//==================================================================
bool operator<(const in_addr &_inaddr1, const in_addr &_inaddr2);

bool operator==(const in_addr &_inaddr1, const in_addr &_inaddr2);

bool operator<(const in6_addr &_inaddr1, const in6_addr &_inaddr2);

bool operator==(const in6_addr &_inaddr1, const in6_addr &_inaddr2);

size_t in_addr_hash(const in_addr &_inaddr);

size_t in_addr_hash(const in6_addr &_inaddr);

std::ostream& operator<<(std::ostream& _ros, const SocketAddressInet4& _rsa);

std::ostream& operator<<(std::ostream& _ros, const SocketAddressInet& _rsa);

//==================================================================
#ifndef ON_WINDOWS
struct SocketAddressLocal{
private:
	union AddrUnion{
		sockaddr		addr;
		sockaddr_un		localaddr;
	};
public:
	enum {Capacity = sizeof(AddrUnion)};
	
	SocketAddressLocal();
	SocketAddressLocal(const char* _path);
	
	SocketAddressLocal& operator=(const SocketAddressStub &);
	
	SocketInfo::Family family()const;
	
	bool empty()const;
	
	const socklen_t&	size()const;

	const sockaddr* sockAddr()const;
	operator const sockaddr*()const;
	
	bool operator<(const SocketAddressLocal &_raddr)const;
	bool operator==(const SocketAddressLocal &_raddr)const;
	
	void clear();
	size_t hash()const;
	size_t addressHash()const;
	
	void path(const char*_pth);
	const char* path()const;
private:
	friend struct SocketDevice;
	operator sockaddr*();
	sockaddr* sockAddr();
	AddrUnion	d;
	socklen_t	sz;
};
#endif
//==================================================================
#ifndef NINLINES
#include "system/socketaddress.ipp"
#endif
//==================================================================

}//namespace solid

#endif
