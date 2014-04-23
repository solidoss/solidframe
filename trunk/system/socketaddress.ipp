// system/socketaddress.ipp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifdef NINLINES
#define inline
#endif

//-----------------------------------------------------------------------
//			ResolveIterator
//-----------------------------------------------------------------------

inline ResolveIterator::ResolveIterator():paddr(NULL){
}
inline ResolveIterator& ResolveIterator::next(){
	paddr = paddr->ai_next;
	return *this;
}
inline int ResolveIterator::family()const{
	return paddr->ai_family;
}
inline int ResolveIterator::type()const{
	return paddr->ai_socktype;
}
inline int ResolveIterator::protocol()const{
	return paddr->ai_protocol;
}
inline size_t ResolveIterator::size()const{
	return paddr->ai_addrlen;
}
inline sockaddr* ResolveIterator::sockAddr()const{
	return paddr->ai_addr;
}
inline ResolveIterator::operator bool()const{
	return paddr != NULL;
}
inline ResolveIterator &ResolveIterator::operator++(){
	return next();
}
inline bool ResolveIterator::operator==(const ResolveIterator &_rrit)const{
	return _rrit.paddr == this->paddr;
}
inline ResolveIterator::ResolveIterator(addrinfo *_pa):paddr(_pa){
}

//-----------------------------------------------------------------------
//			ResolveData
//-----------------------------------------------------------------------
#if (defined(HAS_CPP11) || defined(UBOOSTSHAREDPTR)) && !defined(USHAREDBACKEND)
inline ResolveData::ResolveData(){}
inline ResolveData::ResolveData(addrinfo *_pai):aiptr(_pai, &delete_addrinfo){
}
inline ResolveData::ResolveData(const ResolveData &_rai):aiptr(_rai.aiptr){
}
inline ResolveData::~ResolveData(){
}
inline ResolveData::const_iterator ResolveData::begin()const{
	if(aiptr.get()){
		return const_iterator(aiptr.get());
	}else{
		return end();
	}
}
inline ResolveData::const_iterator ResolveData::end()const{
	const static const_iterator emptyit;
	return emptyit; 
}
inline bool ResolveData::empty()const{
	return aiptr.get() == NULL;
}
inline void ResolveData::clear(){
	aiptr.reset();
}
inline ResolveData& ResolveData::operator=(const ResolveData &_rrd){
	aiptr = _rrd.aiptr;
	return *this;
}
#else
inline ResolveData::ResolveData():pss(&SharedBackend::emptyStub()){
	SharedBackend::use(*pss);
}
inline ResolveData::ResolveData(addrinfo *_pai):pss(SharedBackend::create(_pai, &delete_addrinfo)){
}
inline ResolveData::ResolveData(const ResolveData &_rai):pss(_rai.pss){
	SharedBackend::use(*pss);
}
inline ResolveData::~ResolveData(){
	SharedBackend::release(*pss);
}
inline ResolveData::const_iterator ResolveData::begin()const{
	if(pss->ptr){
		return const_iterator(reinterpret_cast<addrinfo*>(pss->ptr));
	}else{
		return end();
	}
}
inline ResolveData::const_iterator ResolveData::end()const{
	const static const_iterator emptyit;
	return emptyit; 
}
inline bool ResolveData::empty()const{
	return pss->ptr == NULL;
}
inline void ResolveData::clear(){
	SharedBackend::release(*pss);
	pss = &SharedBackend::emptyStub();
}
inline ResolveData& ResolveData::operator=(const ResolveData &_rrd){
	pss = _rrd.pss;
	SharedBackend::use(*pss);
	return *this;
}
#endif

//-----------------------------------------------------------------------
//			SocketAddressStub
//-----------------------------------------------------------------------

inline SocketAddressStub::SocketAddressStub(
	sockaddr *_pa, size_t _sz
):addr(_pa),sz(_sz){}

inline SocketAddressStub::SocketAddressStub(const ResolveIterator &_it){
	if(_it){
		addr = _it.sockAddr(); sz = _it.size();
	}else{
		addr = NULL; sz = 0;
	}
}
inline SocketAddressStub::SocketAddressStub(const SocketAddress &_rsa):addr(_rsa.sockAddr()), sz(_rsa.size()){
	
}
inline SocketAddressStub::SocketAddressStub(const SocketAddressInet &_rsa):addr(_rsa.sockAddr()), sz(_rsa.size()){
	
}
inline SocketAddressStub::SocketAddressStub(const SocketAddressInet4 &_rsa):addr(_rsa.sockAddr()), sz(_rsa.size()){
	
}
inline SocketAddressStub::SocketAddressStub(const SocketAddressInet6 &_rsa):addr(_rsa.sockAddr()), sz(_rsa.size()){
	
}
#ifndef ON_WINDOWS
inline SocketAddressStub::SocketAddressStub(const SocketAddressLocal &_rsa):addr(_rsa.sockAddr()), sz(_rsa.size()){
	
}
#endif
inline SocketAddressStub& SocketAddressStub::operator=(const ResolveIterator &_it){
	if(_it){
		addr = _it.sockAddr(); sz = _it.size();
	}else{
		addr = NULL; sz = 0;
	}
	return *this;
}
inline SocketAddressStub& SocketAddressStub::operator=(const SocketAddress &_rsa){
	addr = _rsa.sockAddr();
	sz = _rsa.size();
	return *this;
}
inline SocketAddressStub& SocketAddressStub::operator=(const SocketAddressInet &_rsa){
	addr = _rsa.sockAddr();
	sz = _rsa.size();
	return *this;
}
inline SocketAddressStub& SocketAddressStub::operator=(const SocketAddressInet4 &_rsa){
	addr = _rsa.sockAddr();
	sz = _rsa.size();
	return *this;
}
inline SocketAddressStub& SocketAddressStub::operator=(const SocketAddressInet6 &_rsa){
	addr = _rsa.sockAddr();
	sz = _rsa.size();
	return *this;
}
#ifndef ON_WINDOWS
inline SocketAddressStub& SocketAddressStub::operator=(const SocketAddressLocal &_rsa){
	addr = _rsa.sockAddr();
	sz = _rsa.size();
	return *this;
}
#endif
inline SocketAddressStub::operator const sockaddr*()const{
	return sockAddr();
}
inline void SocketAddressStub::clear(){
	addr = NULL;
	sz = 0;
}
inline SocketInfo::Family SocketAddressStub::family()const{
	cassert(addr);
	return (SocketInfo::Family)addr->sa_family;
}
inline bool SocketAddressStub::isInet4()const{
	cassert(addr);
	if(addr){
		return addr->sa_family == AF_INET;
	}else{
		return false;
	}
}
inline bool SocketAddressStub::isInet6()const{
	cassert(addr);
	if(addr){
		return addr->sa_family == AF_INET6;
	}else{
		return false;
	}
}
inline bool SocketAddressStub::isLocal()const{
	cassert(addr);
	if(addr){
		return addr->sa_family == AF_UNIX;
	}else{
		return false;
	}
}
inline socklen_t SocketAddressStub::size()const{
	return sz;
}
inline int SocketAddressStub::port()const{
	if(isInet4()){
		return htons(reinterpret_cast<const sockaddr_in*>(addr)->sin_port);
	}else if(isInet6()){
		return htons(reinterpret_cast<const sockaddr_in6*>(addr)->sin6_port);
	}else return -1;
}
inline const sockaddr* SocketAddressStub::sockAddr()const{
	return addr;
}
//-----------------------------------------------------------------------
//			SocketAddress
//-----------------------------------------------------------------------

inline SocketAddress::SocketAddress():sz(0){
	clear();
}
inline SocketAddress::SocketAddress(const SocketAddressStub &_rsas):sz(0){
	clear();
	memcpy(&d.addr, _rsas.sockAddr(), _rsas.size());
	sz = _rsas.size();
}
inline SocketAddress::SocketAddress(const char* _addr, int _port):sz(0){
	clear();
	address(_addr);
	port(_port);
}
inline SocketAddress::SocketAddress(const char* _path):sz(0){
	clear();
	path(_path);
}

inline SocketAddress& SocketAddress::operator=(const SocketAddressStub &_rsas){
	clear();
	memcpy(&d.addr, _rsas.sockAddr(), _rsas.size());
	sz = _rsas.size();
	return *this;
}

inline SocketInfo::Family SocketAddress::family()const{
	return (SocketInfo::Family)sockAddr()->sa_family;
}

inline bool SocketAddress::isInet4()const{
	return sockAddr()->sa_family == AF_INET;
}
inline bool SocketAddress::isInet6()const{
	return sockAddr()->sa_family == AF_INET6;
}
inline bool SocketAddress::isLocal()const{
	return sockAddr()->sa_family == AF_UNIX;
}

inline bool SocketAddress::isLoopback()const{
	if(isInet4()){
		return ((uint32)ntohl(address4().s_addr) & 0xFF000000) == 0x7F000000;
	}else if(isInet6()){
		return ((address6().s6_addr[0] == 0) && (address6().s6_addr[1] == 0)
				&& (address6().s6_addr[2] == 0) && (address6().s6_addr[3] == 0)
				&& (address6().s6_addr[4] == 0) && (address6().s6_addr[5] == 0)
				&& (address6().s6_addr[6] == 0) && (address6().s6_addr[7] == 0)
				&& (address6().s6_addr[8] == 0) && (address6().s6_addr[9] == 0)
				&& (address6().s6_addr[10] == 0) && (address6().s6_addr[11] == 0)
				&& (address6().s6_addr[12] == 0) && (address6().s6_addr[13] == 0)
				&& (address6().s6_addr[14] == 0) && (address6().s6_addr[15] == 1));
	}
	return false;
}
inline bool SocketAddress::isInvalid()const{
	if(isInet4()){
		return (uint32)ntohl(address4().s_addr) == 0;
	}else if(isInet6()){
		return ((address6().s6_addr[0] == 0) && (address6().s6_addr[1] == 0)
				&& (address6().s6_addr[2] == 0) && (address6().s6_addr[3] == 0)
				&& (address6().s6_addr[4] == 0) && (address6().s6_addr[5] == 0)
				&& (address6().s6_addr[6] == 0) && (address6().s6_addr[7] == 0)
				&& (address6().s6_addr[8] == 0) && (address6().s6_addr[9] == 0)
				&& (address6().s6_addr[10] == 0) && (address6().s6_addr[11] == 0)
				&& (address6().s6_addr[12] == 0) && (address6().s6_addr[13] == 0)
				&& (address6().s6_addr[14] == 0) && (address6().s6_addr[15] == 0));
	}
	return false;
}

inline bool SocketAddress::empty()const{
	return sz == 0;
}

inline const socklen_t&	SocketAddress::size()const {
	return sz;
}

inline const sockaddr* SocketAddress::sockAddr()const{
	return &d.addr;
}
//operator sockaddr*(){return sockAddr();}
inline SocketAddress::operator const sockaddr*()const{
	return sockAddr();
}
inline bool SocketAddress::toString(
	char* _host,
	unsigned _hostcp,
	char* _serv,
	unsigned _servcp,
	uint32	_flags
)const{
	if(!_hostcp || !_servcp) return false;
	if(empty()) return false;
	_host[0] = 0;
	_serv[0] = 0;
	int rv = getnameinfo(sockAddr(), size(), _host, _hostcp, _serv, _servcp, _flags);
	if(rv){
		edbgx(Debug::system, "getnameinfo: "<<strerror(errno));
		return false;
	}
	return true;
}

inline bool SocketAddress::operator<(const SocketAddress &_raddr)const{
	if(sockAddr()->sa_family < _raddr.sockAddr()->sa_family){
		return true;
	}else if(sockAddr()->sa_family > _raddr.sockAddr()->sa_family){
		return false;
	}
	
	if(isInet6()){
		const int rv = memcmp(
			(const void*)&this->d.inaddr6.sin6_addr.s6_addr,
			(const void*)&_raddr.d.inaddr6.sin6_addr.s6_addr,
			sizeof(in6_addr)
		);
		if(rv < 0){
			return true;
		}else if(rv > 0){
			return false;
		}
		return d.inaddr6.sin6_port < _raddr.d.inaddr6.sin6_port;
	}
	
	if(address4().s_addr < _raddr.address4().s_addr){
		return true;
	}else if(address4().s_addr > _raddr.address4().s_addr){
		return false;
	}
	return d.inaddr4.sin_port < _raddr.d.inaddr4.sin_port;
}
inline bool SocketAddress::operator==(const SocketAddress &_raddr)const{
	if(sockAddr()->sa_family == _raddr.sockAddr()->sa_family){
		if(isInet6()){
			return (memcmp(
				(const void*)&this->d.inaddr6.sin6_addr.s6_addr,
				(const void*)&_raddr.d.inaddr6.sin6_addr.s6_addr,
				sizeof(in6_addr)
			) == 0) && (d.inaddr6.sin6_port == _raddr.d.inaddr6.sin6_port);
		}
		
		return (address4().s_addr == _raddr.address4().s_addr) && (d.inaddr4.sin_port == _raddr.d.inaddr4.sin_port);
	}else{
		return false;
	}
}

inline void SocketAddress::address(const char*_str){
#ifndef ON_WINDOWS
	d.addr.sa_family = 0;
	d.inaddr4.sin_addr.s_addr = 0;
	sz = 0;
	int rv = inet_pton(AF_INET6, _str, (void*)&this->d.inaddr6.sin6_addr.s6_addr);
	if(rv == 1){
		sockAddr()->sa_family = AF_INET6;
		sz = sizeof(d.inaddr6);
		return;
	}
	rv = inet_pton(AF_INET, _str, (void*)&this->d.inaddr4.sin_addr.s_addr);
	if(rv == 1){
		sockAddr()->sa_family = AF_INET;
		sz = sizeof(d.inaddr4);
		return;
	}
#else
#endif
}

inline const in_addr& SocketAddress::address4()const{
	return d.inaddr4.sin_addr;
}
inline const in6_addr& SocketAddress::address6()const{
	return d.inaddr6.sin6_addr;
}
inline int SocketAddress::port()const{
	if(isInet4()){
		return htons(d.inaddr4.sin_port);
	}else if(isInet6()){
		return htons(d.inaddr6.sin6_port);
	}else return -1;
}
inline bool SocketAddress::port(int _port){
	if(isInet4()){
		d.inaddr4.sin_port = ntohs(_port);
	}else if(isInet6()){
		d.inaddr6.sin6_port = ntohs(_port);
	}else return false;
	return true;
}
inline void SocketAddress::clear(){
	d.addr.sa_family = 0;
	d.inaddr4.sin_addr.s_addr = 0;
	sz = 0;
	port(0);
}
inline size_t SocketAddress::hash()const{
	if(isInet6()){
		return in_addr_hash(address6()) ^ d.inaddr6.sin6_port;
	}
	return in_addr_hash(address4()) ^ d.inaddr4.sin_port;
}
inline size_t SocketAddress::addressHash()const{
	if(isInet6()){
		return in_addr_hash(address6());
	}
	return in_addr_hash(address4());
}

inline void SocketAddress::path(const char*_pth){
	//TODO
}
inline const char* SocketAddress::path()const{
	//TODO
	return NULL;
}
inline SocketAddress::operator sockaddr*(){
	return sockAddr();
}
inline sockaddr* SocketAddress::sockAddr(){
	return &d.addr;
}

//-----------------------------------------------------------------------
//			SocketAddressInet
//-----------------------------------------------------------------------

inline SocketAddressInet::SocketAddressInet():sz(0){
	clear();
}
inline SocketAddressInet::SocketAddressInet(const SocketAddressStub &_rsas):sz(0){
	clear();
	if(_rsas.isInet4() || _rsas.isInet6()){
		memcpy(&d.addr, _rsas.sockAddr(), _rsas.size());
		sz = _rsas.size();
	}
}
inline SocketAddressInet::SocketAddressInet(const char* _addr, int _port):sz(0){
	clear();
	address(_addr);
	port(_port);
}

inline SocketAddressInet& SocketAddressInet::operator=(const SocketAddressStub &_rsas){
	clear();
	if(_rsas.isInet4() || _rsas.isInet6()){
		memcpy(&d.addr, _rsas.sockAddr(), _rsas.size());
		sz = _rsas.size();
	}
	return *this;
}

inline SocketInfo::Family SocketAddressInet::family()const{
	return (SocketInfo::Family)sockAddr()->sa_family;
}

inline bool SocketAddressInet::isInet4()const{
	return sockAddr()->sa_family == AF_INET;
}
inline bool SocketAddressInet::isInet6()const{
	return sockAddr()->sa_family == AF_INET6;
}

inline bool SocketAddressInet::isLoopback()const{
	if(isInet4()){
		return ((uint32)ntohl(address4().s_addr) & 0xFF000000) == 0x7F000000;
	}else if(isInet6()){
		return ((address6().s6_addr[0] == 0) && (address6().s6_addr[1] == 0)
				&& (address6().s6_addr[2] == 0) && (address6().s6_addr[3] == 0)
				&& (address6().s6_addr[4] == 0) && (address6().s6_addr[5] == 0)
				&& (address6().s6_addr[6] == 0) && (address6().s6_addr[7] == 0)
				&& (address6().s6_addr[8] == 0) && (address6().s6_addr[9] == 0)
				&& (address6().s6_addr[10] == 0) && (address6().s6_addr[11] == 0)
				&& (address6().s6_addr[12] == 0) && (address6().s6_addr[13] == 0)
				&& (address6().s6_addr[14] == 0) && (address6().s6_addr[15] == 1));
	}
	return false;
}
inline bool SocketAddressInet::isInvalid()const{
	if(isInet4()){
		return (uint32)ntohl(address4().s_addr) == 0;
	}else if(isInet6()){
		return ((address6().s6_addr[0] == 0) && (address6().s6_addr[1] == 0)
				&& (address6().s6_addr[2] == 0) && (address6().s6_addr[3] == 0)
				&& (address6().s6_addr[4] == 0) && (address6().s6_addr[5] == 0)
				&& (address6().s6_addr[6] == 0) && (address6().s6_addr[7] == 0)
				&& (address6().s6_addr[8] == 0) && (address6().s6_addr[9] == 0)
				&& (address6().s6_addr[10] == 0) && (address6().s6_addr[11] == 0)
				&& (address6().s6_addr[12] == 0) && (address6().s6_addr[13] == 0)
				&& (address6().s6_addr[14] == 0) && (address6().s6_addr[15] == 0));
	}
	return false;
}

inline bool SocketAddressInet::empty()const{
	return sz == 0;
}

inline const socklen_t&	SocketAddressInet::size()const {
	return sz;
}

inline const sockaddr* SocketAddressInet::sockAddr()const{
	return &d.addr;
}
//operator sockaddr*(){return sockAddr();}
inline SocketAddressInet::operator const sockaddr*()const{
	return sockAddr();
}
inline bool SocketAddressInet::toString(
	char* _host,
	unsigned _hostcp,
	char* _serv,
	unsigned _servcp,
	uint32	_flags
)const{
	if(!_hostcp || !_servcp) return false;
	if(empty()) return false;
	_host[0] = 0;
	_serv[0] = 0;
	int rv = getnameinfo(sockAddr(), size(), _host, _hostcp, _serv, _servcp, _flags);
	if(rv){
		edbgx(Debug::system, "getnameinfo: "<<strerror(errno));
		return false;
	}
	return true;
}

inline bool SocketAddressInet::toBinary(Binary4T &_bin, uint16 &_port)const{
	if(isInet4()){
		memcpy(_bin.data, &address4().s_addr, 4);
		_port = d.inaddr4.sin_port;
		return true;
	}
	return false;
}

inline bool SocketAddressInet::toBinary(Binary6T &_bin, uint16 &_port)const{
	if(isInet6()){
		memcpy(_bin.data, &address6().s6_addr, 16);
		_port = d.inaddr6.sin6_port;
		return true;
	}
	return false;
}

inline void SocketAddressInet::fromBinary(const Binary4T &_bin, uint16 _port){
	sockAddr()->sa_family = AF_INET;
	sz = sizeof(d.inaddr4);
	memcpy(&d.inaddr4.sin_addr.s_addr, _bin.data, 4);
	d.inaddr4.sin_port = _port;
}

inline void SocketAddressInet::fromBinary(const Binary6T &_bin, uint16 _port){
	sockAddr()->sa_family = AF_INET6;
	sz = sizeof(d.inaddr6);
	memcpy(&d.inaddr6.sin6_addr.s6_addr, _bin.data, 16);
	d.inaddr6.sin6_port = _port;
}

inline bool SocketAddressInet::operator<(const SocketAddressInet &_raddr)const{
	if(sockAddr()->sa_family < _raddr.sockAddr()->sa_family){
		return true;
	}else if(sockAddr()->sa_family > _raddr.sockAddr()->sa_family){
		return false;
	}
	
	if(isInet6()){
		const int rv = memcmp(
			(const void*)&this->d.inaddr6.sin6_addr.s6_addr,
			(const void*)&_raddr.d.inaddr6.sin6_addr.s6_addr,
			sizeof(in6_addr)
		);
		if(rv < 0){
			return true;
		}else if(rv > 0){
			return false;
		}
		return d.inaddr6.sin6_port < _raddr.d.inaddr6.sin6_port;
	}
	
	if(address4().s_addr < _raddr.address4().s_addr){
		return true;
	}else if(address4().s_addr > _raddr.address4().s_addr){
		return false;
	}
	return d.inaddr4.sin_port < _raddr.d.inaddr4.sin_port;
}
inline bool SocketAddressInet::operator==(const SocketAddressInet &_raddr)const{
	if(sockAddr()->sa_family == _raddr.sockAddr()->sa_family){
		if(isInet6()){
			return (memcmp(
				(const void*)&this->d.inaddr6.sin6_addr.s6_addr,
				(const void*)&_raddr.d.inaddr6.sin6_addr.s6_addr,
				sizeof(in6_addr)
			) == 0) && (d.inaddr6.sin6_port == _raddr.d.inaddr6.sin6_port);
		}
		
		return (address4().s_addr == _raddr.address4().s_addr) && (d.inaddr4.sin_port == _raddr.d.inaddr4.sin_port);
	}else{
		return false;
	}
}

inline void SocketAddressInet::address(const char*_str){
#ifndef ON_WINDOWS
	d.addr.sa_family = 0;
	d.inaddr4.sin_addr.s_addr = 0;
	sz = 0;
	int rv = inet_pton(AF_INET6, _str, (void*)&this->d.inaddr6.sin6_addr.s6_addr);
	if(rv == 1){
		sockAddr()->sa_family = AF_INET6;
		sz = sizeof(d.inaddr6);
		return;
	}
	rv = inet_pton(AF_INET, _str, (void*)&this->d.inaddr4.sin_addr.s_addr);
	if(rv == 1){
		sockAddr()->sa_family = AF_INET;
		sz = sizeof(d.inaddr4);
		return;
	}
#else
#endif
}

inline const in_addr& SocketAddressInet::address4()const{
	return d.inaddr4.sin_addr;
}
inline const in6_addr& SocketAddressInet::address6()const{
	return d.inaddr6.sin6_addr;
}
inline int SocketAddressInet::port()const{
	if(isInet4()){
		return htons(d.inaddr4.sin_port);
	}else if(isInet6()){
		return htons(d.inaddr6.sin6_port);
	}else return -1;
}
inline bool SocketAddressInet::port(int _port){
	if(isInet4()){
		d.inaddr4.sin_port = ntohs(_port);
	}else if(isInet6()){
		d.inaddr6.sin6_port = ntohs(_port);
	}else return false;
	return true;
}
inline void SocketAddressInet::clear(){
	d.addr.sa_family = 0;
	d.inaddr4.sin_addr.s_addr = 0;
	sz = 0;
	port(0);
}
inline size_t SocketAddressInet::hash()const{
	if(isInet6()){
		return in_addr_hash(address6()) ^ d.inaddr6.sin6_port;
	}
	return in_addr_hash(address4()) ^ d.inaddr4.sin_port;
}
inline size_t SocketAddressInet::addressHash()const{
	if(isInet6()){
		return in_addr_hash(address6());
	}
	return in_addr_hash(address4());
}

inline SocketAddressInet::operator sockaddr*(){
	return sockAddr();
}
inline sockaddr* SocketAddressInet::sockAddr(){
	return &d.addr;
}

//-----------------------------------------------------------------------
//			SocketAddressInet4
//-----------------------------------------------------------------------

inline SocketAddressInet4::SocketAddressInet4(){
	clear();
}
inline SocketAddressInet4::SocketAddressInet4(const SocketAddressStub &_rsas){
	clear();
	if(_rsas.isInet4()){
		memcpy(&d.addr, _rsas.sockAddr(), _rsas.size());
	}
}
inline SocketAddressInet4::SocketAddressInet4(const char* _addr, int _port){
	clear();
	address(_addr);
	port(_port);
}
inline SocketAddressInet4::SocketAddressInet4(uint32 _addr, int _port){
	clear();
	fromUInt(_addr, _port);
}
inline SocketAddressInet4::SocketAddressInet4(const BinaryT &_addr, int _port){
	clear();
	fromBinary(_addr, _port);
}

inline SocketAddressInet4& SocketAddressInet4::operator=(const SocketAddressStub &_rsas){
	clear();
	if(_rsas.isInet4()){
		memcpy(&d.addr, _rsas.sockAddr(), _rsas.size());
	}
	return *this;
}

inline SocketInfo::Family SocketAddressInet4::family()const{
	return (SocketInfo::Family)sockAddr()->sa_family;
}

inline bool SocketAddressInet4::isInet4()const{
	return sockAddr()->sa_family == AF_INET;
}

inline bool SocketAddressInet4::isLoopback()const{
	if(isInet4()){
		return ((uint32)ntohl(address().s_addr) & 0xFF000000) == 0x7F000000;
	}
	return false;
}
inline bool SocketAddressInet4::isInvalid()const{
	if(isInet4()){
		return (uint32)ntohl(address().s_addr) == 0;
	}
	return false;
}

inline socklen_t	SocketAddressInet4::size()const {
	return sizeof(d);
}

inline const sockaddr* SocketAddressInet4::sockAddr()const{
	return &d.addr;
}
//operator sockaddr*(){return sockAddr();}
inline SocketAddressInet4::operator const sockaddr*()const{
	return sockAddr();
}
inline bool SocketAddressInet4::toString(
	char* _host,
	unsigned _hostcp,
	char* _serv,
	unsigned _servcp,
	uint32	_flags
)const{
	if(!_hostcp || !_servcp) return false;
	_host[0] = 0;
	_serv[0] = 0;
	int rv = getnameinfo(sockAddr(), size(), _host, _hostcp, _serv, _servcp, _flags);
	if(rv){
		edbgx(Debug::system, "getnameinfo: "<<strerror(errno));
		return false;
	}
	return true;
}
inline void SocketAddressInet4::toBinary(BinaryT &_bin, uint16 &_port)const{
	memcpy(_bin.data, &address().s_addr, 4);
	_port = d.inaddr4.sin_port;
}
	
inline void SocketAddressInet4::toUInt(uint32 &_addr, uint16 &_port)const{
	_addr = ntohl(address().s_addr);
	_port = static_cast<uint16>(port());
}

inline void SocketAddressInet4::fromBinary(const BinaryT &_bin, uint16 _port){
	memcpy(&d.inaddr4.sin_addr.s_addr, _bin.data, 4);
	d.inaddr4.sin_port = _port;
}
inline void SocketAddressInet4::fromUInt(uint32 _addr, uint16 _port){
	d.inaddr4.sin_addr.s_addr = htonl(_addr);
	port(_port);
}
inline bool SocketAddressInet4::operator<(const SocketAddressInet4 &_raddr)const{
	if(address().s_addr < _raddr.address().s_addr){
		return true;
	}else if(address().s_addr > _raddr.address().s_addr){
		return false;
	}
	return d.inaddr4.sin_port < _raddr.d.inaddr4.sin_port;
}
inline bool SocketAddressInet4::operator==(const SocketAddressInet4 &_raddr)const{
	return (address().s_addr == _raddr.address().s_addr) && (d.inaddr4.sin_port == _raddr.d.inaddr4.sin_port);
}

inline void SocketAddressInet4::address(const char*_str){
#ifndef ON_WINDOWS
	d.addr.sa_family = AF_INET;
	d.inaddr4.sin_addr.s_addr = 0;
	int rv = inet_pton(AF_INET, _str, (void*)&this->d.inaddr4.sin_addr.s_addr);
	if(rv == 1){
		sockAddr()->sa_family = AF_INET;
		return;
	}
#else
#endif
}

inline const in_addr& SocketAddressInet4::address()const{
	return d.inaddr4.sin_addr;
}
inline int SocketAddressInet4::port()const{
	return htons(d.inaddr4.sin_port);
}
inline void SocketAddressInet4::port(int _port){
	d.inaddr4.sin_port = ntohs(_port);
}
inline void SocketAddressInet4::clear(){
	d.addr.sa_family = AF_INET;
	d.inaddr4.sin_addr.s_addr = 0;
	port(0);
}
inline size_t SocketAddressInet4::hash()const{
	return in_addr_hash(address()) ^ d.inaddr4.sin_port;
}
inline size_t SocketAddressInet4::addressHash()const{
	return in_addr_hash(address());
}

inline SocketAddressInet4::operator sockaddr*(){
	return sockAddr();
}
inline sockaddr* SocketAddressInet4::sockAddr(){
	return &d.addr;
}

//-----------------------------------------------------------------------
//			SocketAddressInet6
//-----------------------------------------------------------------------
inline SocketAddressInet6::SocketAddressInet6(){
	clear();
}
inline SocketAddressInet6::SocketAddressInet6(const SocketAddressStub &_rsas){
	clear();
	if(_rsas.isInet6()){
		memcpy(&d.addr, _rsas.sockAddr(), _rsas.size());
	}
}
inline SocketAddressInet6::SocketAddressInet6(const char* _addr, int _port){
	clear();
	address(_addr);
	port(_port);
}

inline SocketAddressInet6::SocketAddressInet6(const BinaryT &_addr, int _port){
	clear();
	fromBinary(_addr, _port);
}

inline SocketAddressInet6& SocketAddressInet6::operator=(const SocketAddressStub &_rsas){
	clear();
	if(_rsas.isInet6()){
		memcpy(&d.addr, _rsas.sockAddr(), _rsas.size());
	}
	return *this;
}

inline SocketInfo::Family SocketAddressInet6::family()const{
	return (SocketInfo::Family)sockAddr()->sa_family;
}

inline bool SocketAddressInet6::isInet6()const{
	return sockAddr()->sa_family == AF_INET6;
}

inline bool SocketAddressInet6::isLoopback()const{
	return ((address().s6_addr[0] == 0) && (address().s6_addr[1] == 0)
			&& (address().s6_addr[2] == 0) && (address().s6_addr[3] == 0)
			&& (address().s6_addr[4] == 0) && (address().s6_addr[5] == 0)
			&& (address().s6_addr[6] == 0) && (address().s6_addr[7] == 0)
			&& (address().s6_addr[8] == 0) && (address().s6_addr[9] == 0)
			&& (address().s6_addr[10] == 0) && (address().s6_addr[11] == 0)
			&& (address().s6_addr[12] == 0) && (address().s6_addr[13] == 0)
			&& (address().s6_addr[14] == 0) && (address().s6_addr[15] == 1));
}
inline bool SocketAddressInet6::isInvalid()const{
	return ((address().s6_addr[0] == 0) && (address().s6_addr[1] == 0)
			&& (address().s6_addr[2] == 0) && (address().s6_addr[3] == 0)
			&& (address().s6_addr[4] == 0) && (address().s6_addr[5] == 0)
			&& (address().s6_addr[6] == 0) && (address().s6_addr[7] == 0)
			&& (address().s6_addr[8] == 0) && (address().s6_addr[9] == 0)
			&& (address().s6_addr[10] == 0) && (address().s6_addr[11] == 0)
			&& (address().s6_addr[12] == 0) && (address().s6_addr[13] == 0)
			&& (address().s6_addr[14] == 0) && (address().s6_addr[15] == 0));
}


inline socklen_t	SocketAddressInet6::size()const {
	return sizeof(d);
}

inline const sockaddr* SocketAddressInet6::sockAddr()const{
	return &d.addr;
}
//operator sockaddr*(){return sockAddr();}
inline SocketAddressInet6::operator const sockaddr*()const{
	return sockAddr();
}
inline bool SocketAddressInet6::toString(
	char* _host,
	unsigned _hostcp,
	char* _serv,
	unsigned _servcp,
	uint32	_flags
)const{
	if(!_hostcp || !_servcp) return false;
	_host[0] = 0;
	_serv[0] = 0;
	int rv = getnameinfo(sockAddr(), size(), _host, _hostcp, _serv, _servcp, _flags);
	if(rv){
		edbgx(Debug::system, "getnameinfo: "<<strerror(errno));
		return false;
	}
	return true;
}

inline void SocketAddressInet6::toBinary(BinaryT &_bin, uint16 &_port)const{
	memcpy(_bin.data, &address().s6_addr, 16);
	_port = d.inaddr6.sin6_port;
}
	
inline void SocketAddressInet6::fromBinary(const BinaryT &_bin, uint16 _port){
	memcpy(&d.inaddr6.sin6_addr.s6_addr, _bin.data, 16);
	d.inaddr6.sin6_port = _port;
}

inline bool SocketAddressInet6::operator<(const SocketAddressInet6 &_raddr)const{
	const int rv = memcmp(
		(const void*)&this->d.inaddr6.sin6_addr.s6_addr,
		(const void*)&_raddr.d.inaddr6.sin6_addr.s6_addr,
		sizeof(in6_addr)
	);
	if(rv < 0){
		return true;
	}else if(rv > 0){
		return false;
	}
	return d.inaddr6.sin6_port < _raddr.d.inaddr6.sin6_port;
}
inline bool SocketAddressInet6::operator==(const SocketAddressInet6 &_raddr)const{
	return (memcmp(
		(const void*)&this->d.inaddr6.sin6_addr.s6_addr,
		(const void*)&_raddr.d.inaddr6.sin6_addr.s6_addr,
		sizeof(in6_addr)
	) == 0) && (d.inaddr6.sin6_port == _raddr.d.inaddr6.sin6_port);
}

inline void SocketAddressInet6::address(const char*_str){
#ifndef ON_WINDOWS
	d.addr.sa_family = AF_INET6;
	memset(d.inaddr6.sin6_addr.s6_addr, 0, sizeof(d.inaddr6.sin6_addr.s6_addr));
	int rv = inet_pton(AF_INET6, _str, (void*)&this->d.inaddr6.sin6_addr.s6_addr);
	if(rv == 1){
		sockAddr()->sa_family = AF_INET6;
		return;
	}
#else
#endif
}

inline const in6_addr& SocketAddressInet6::address()const{
	return d.inaddr6.sin6_addr;
}
inline int SocketAddressInet6::port()const{
	return htons(d.inaddr6.sin6_port);
}
inline void SocketAddressInet6::port(int _port){
	d.inaddr6.sin6_port = ntohs(_port);
}
inline void SocketAddressInet6::clear(){
	d.addr.sa_family = AF_INET6;
	memset(d.inaddr6.sin6_addr.s6_addr, 0, sizeof(d.inaddr6.sin6_addr.s6_addr));
	port(0);
}
inline size_t SocketAddressInet6::hash()const{
	return in_addr_hash(address()) ^ d.inaddr6.sin6_port;
}
inline size_t SocketAddressInet6::addressHash()const{
	return in_addr_hash(address());
}

inline SocketAddressInet6::operator sockaddr*(){
	return sockAddr();
}
inline sockaddr* SocketAddressInet6::sockAddr(){
	return &d.addr;
}

//-----------------------------------------------------------------------
//			hash
//-----------------------------------------------------------------------

inline bool operator<(const in_addr &_inaddr1, const in_addr &_inaddr2){
	return _inaddr1.s_addr < _inaddr2.s_addr;
}

inline bool operator==(const in_addr &_inaddr1, const in_addr &_inaddr2){
	return _inaddr1.s_addr == _inaddr2.s_addr;
}

inline bool operator<(const in6_addr &_inaddr1, const in6_addr &_inaddr2){
	return memcmp(
		(const void*)&_inaddr1.s6_addr,
		(const void*)&_inaddr2.s6_addr,
		sizeof(in6_addr)
	) < 0;
}

inline bool operator==(const in6_addr &_inaddr1, const in6_addr &_inaddr2){
	return memcmp(
		(const void*)&_inaddr1.s6_addr,
		(const void*)&_inaddr2.s6_addr,
		sizeof(in6_addr)
	) == 0;
}

inline size_t in_addr_hash(const in_addr &_inaddr){
	return _inaddr.s_addr;
}

inline size_t in_addr_hash(const in6_addr &_inaddr){
	//TODO
	return 0;
}

//-----------------------------------------------------------------------
//			SocketAddressLocal
//-----------------------------------------------------------------------
#ifndef ON_WINDOWS
inline SocketAddressLocal::SocketAddressLocal(){
	
}
inline SocketAddressLocal::SocketAddressLocal(const char* _path){
	
}

inline SocketAddressLocal& SocketAddressLocal::operator=(const SocketAddressStub &){
	return *this;
}

inline SocketInfo::Family SocketAddressLocal::family()const{
	return (SocketInfo::Family)sockAddr()->sa_family;
}

inline bool SocketAddressLocal::empty()const{
	return sz == 0;
}

inline const socklen_t&	SocketAddressLocal::size()const{
	return sz;
}

inline const sockaddr* SocketAddressLocal::sockAddr()const{
	return &d.addr;
}
inline SocketAddressLocal::operator const sockaddr*()const{
	return sockAddr();
}

inline bool SocketAddressLocal::operator<(const SocketAddressLocal &_raddr)const{
	//TODO
	return false;
}
inline bool SocketAddressLocal::operator==(const SocketAddressLocal &_raddr)const{
	//TODO
	return false;
}

inline void SocketAddressLocal::clear(){
	//TODO
}
inline size_t SocketAddressLocal::hash()const{
	//TODO
	return 0;
}
inline size_t SocketAddressLocal::addressHash()const{
	//TODO
	return 0;
}

inline void SocketAddressLocal::path(const char*_pth){
	//TODO
}
inline const char* SocketAddressLocal::path()const{
	//TODO
	return NULL;
}
inline SocketAddressLocal::operator sockaddr*(){
	return sockAddr();
}
inline sockaddr* SocketAddressLocal::sockAddr(){
	return &d.addr;
}
#endif

#ifdef NINLINES
#undef inline
#endif

