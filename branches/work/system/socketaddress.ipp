/* Inline implementation file socketaddress.ipp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

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

void delete_addrinfo(void *_pv){
	freeaddrinfo(reinterpret_cast<addrinfo*>(_pv));
}

inline ResolveData::ResolveData():pss(NULL){}
inline ResolveData::ResolveData(addrinfo *_pai):pss(NULL){
	if(_pai){
		pss = SharedBackend::the().create(_pai, &delete_addrinfo);
	}
}
inline ResolveData::ResolveData(const ResolveData &_rai):pss(NULL){
	pss = _rai.pss;
	if(pss){
		SharedBackend::the().use(*pss);
	}
}
inline ResolveData::~ResolveData(){
	if(pss){
		SharedBackend::the().release(*pss);
	}
}
inline ResolveData::const_iterator ResolveData::begin()const{
	if(pss){
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
	return pss == NULL;
}
inline ResolveData& ResolveData::operator=(const ResolveData &_rrd){
	if(pss){
		SharedBackend::the().release(*pss);
		pss = NULL;
	}
	pss = _rrd.pss;
	if(pss){
		SharedBackend::the().use(*pss);
	}
	return *this;
}

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
inline SocketAddressStub::SocketAddressStub(const SocketAddressLocal &_rsa):addr(_rsa.sockAddr()), sz(_rsa.size()){
	
}
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
inline SocketAddressStub& SocketAddressStub::operator=(const SocketAddressLocal &_rsa){
	addr = _rsa.sockAddr();
	sz = _rsa.size();
	return *this;
}

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
		edbgx(Dbg::system, "getnameinfo: "<<strerror(errno));
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
			(const void*)this->d.inaddr6.sin6_addr.s6_addr,
			(const void*)_raddr.d.inaddr6.sin6_addr.s6_addr,
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
				(const void*)this->d.inaddr6.sin6_addr.s6_addr,
				(const void*)_raddr.d.inaddr6.sin6_addr.s6_addr,
				sizeof(in6_addr)
			) == 0) && (d.inaddr6.sin6_port == _raddr.d.inaddr6.sin6_port);
		}
		
		return (address4().s_addr == _raddr.address4().s_addr) && (d.inaddr4.sin_port == _raddr.d.inaddr4.sin_port);
	}else{
		return false;
	}
}

inline void SocketAddress::address(const char*_str){
	d.addr.sa_family = 0;
	d.inaddr4.sin_addr.s_addr = 0;
	sz = 0;
	int rv = inet_pton(AF_INET6, _str, (void*)this->d.inaddr6.sin6_addr.s6_addr);
	if(rv == 1){
		sockAddr()->sa_family = AF_INET6;
		sz = sizeof(d.inaddr6);
		return;
	}
	rv = inet_pton(AF_INET, _str, (void*)this->d.inaddr4.sin_addr.s_addr);
	if(rv == 1){
		sockAddr()->sa_family = AF_INET;
		sz = sizeof(d.inaddr4);
		return;
	}
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
		return ::hash(address6()) ^ d.inaddr6.sin6_port;
	}
	return ::hash(address4()) ^ d.inaddr4.sin_port;
}
inline size_t SocketAddress::addressHash()const{
	if(isInet6()){
		return ::hash(address6());
	}
	return ::hash(address4());
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
		edbgx(Dbg::system, "getnameinfo: "<<strerror(errno));
		return false;
	}
	return true;
}

inline bool SocketAddressInet::operator<(const SocketAddressInet &_raddr)const{
	if(sockAddr()->sa_family < _raddr.sockAddr()->sa_family){
		return true;
	}else if(sockAddr()->sa_family > _raddr.sockAddr()->sa_family){
		return false;
	}
	
	if(isInet6()){
		const int rv = memcmp(
			(const void*)this->d.inaddr6.sin6_addr.s6_addr,
			(const void*)_raddr.d.inaddr6.sin6_addr.s6_addr,
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
				(const void*)this->d.inaddr6.sin6_addr.s6_addr,
				(const void*)_raddr.d.inaddr6.sin6_addr.s6_addr,
				sizeof(in6_addr)
			) == 0) && (d.inaddr6.sin6_port == _raddr.d.inaddr6.sin6_port);
		}
		
		return (address4().s_addr == _raddr.address4().s_addr) && (d.inaddr4.sin_port == _raddr.d.inaddr4.sin_port);
	}else{
		return false;
	}
}

inline void SocketAddressInet::address(const char*_str){
	d.addr.sa_family = 0;
	d.inaddr4.sin_addr.s_addr = 0;
	sz = 0;
	int rv = inet_pton(AF_INET6, _str, (void*)this->d.inaddr6.sin6_addr.s6_addr);
	if(rv == 1){
		sockAddr()->sa_family = AF_INET6;
		sz = sizeof(d.inaddr6);
		return;
	}
	rv = inet_pton(AF_INET, _str, (void*)this->d.inaddr4.sin_addr.s_addr);
	if(rv == 1){
		sockAddr()->sa_family = AF_INET;
		sz = sizeof(d.inaddr4);
		return;
	}
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
		return ::hash(address6()) ^ d.inaddr6.sin6_port;
	}
	return ::hash(address4()) ^ d.inaddr4.sin_port;
}
inline size_t SocketAddressInet::addressHash()const{
	if(isInet6()){
		return ::hash(address6());
	}
	return ::hash(address4());
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
		edbgx(Dbg::system, "getnameinfo: "<<strerror(errno));
		return false;
	}
	return true;
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
	d.addr.sa_family = AF_INET;
	d.inaddr4.sin_addr.s_addr = 0;
	int rv = inet_pton(AF_INET, _str, (void*)this->d.inaddr4.sin_addr.s_addr);
	if(rv == 1){
		sockAddr()->sa_family = AF_INET;
		return;
	}
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
	return ::hash(address()) ^ d.inaddr4.sin_port;
}
inline size_t SocketAddressInet4::addressHash()const{
	return ::hash(address());
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
		edbgx(Dbg::system, "getnameinfo: "<<strerror(errno));
		return false;
	}
	return true;
}
inline bool SocketAddressInet6::operator<(const SocketAddressInet6 &_raddr)const{
	const int rv = memcmp(
		(const void*)this->d.inaddr6.sin6_addr.s6_addr,
		(const void*)_raddr.d.inaddr6.sin6_addr.s6_addr,
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
		(const void*)this->d.inaddr6.sin6_addr.s6_addr,
		(const void*)_raddr.d.inaddr6.sin6_addr.s6_addr,
		sizeof(in6_addr)
	) == 0) && (d.inaddr6.sin6_port == _raddr.d.inaddr6.sin6_port);
}

inline void SocketAddressInet6::address(const char*_str){
	d.addr.sa_family = AF_INET6;
	memset(d.inaddr6.sin6_addr.s6_addr, 0, sizeof(d.inaddr6.sin6_addr.s6_addr));
	int rv = inet_pton(AF_INET6, _str, (void*)this->d.inaddr6.sin6_addr.s6_addr);
	if(rv == 1){
		sockAddr()->sa_family = AF_INET6;
		return;
	}
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
	return ::hash(address()) ^ d.inaddr6.sin6_port;
}
inline size_t SocketAddressInet6::addressHash()const{
	return ::hash(address());
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
		(const void*)_inaddr1.s6_addr,
		(const void*)_inaddr2.s6_addr,
		sizeof(in6_addr)
	) < 0;
}

inline bool operator==(const in6_addr &_inaddr1, const in6_addr &_inaddr2){
	return memcmp(
		(const void*)_inaddr1.s6_addr,
		(const void*)_inaddr2.s6_addr,
		sizeof(in6_addr)
	) == 0;
}

inline size_t hash(const in_addr &_inaddr){
	return _inaddr.s_addr;
}

inline size_t hash(const in6_addr &_inaddr){
	//TODO
	return 0;
}

//-----------------------------------------------------------------------
//			SocketAddressLocal
//-----------------------------------------------------------------------

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

#if 0

inline SocketAddressStub::SocketAddressStub(const SocketAddressInfoIterator &_it){
	if(_it){
		addr = _it.addr(); sz = _it.size();
	}else{
		addr = NULL; sz = 0;
	}
}
inline SocketAddressStub::SocketAddressStub(const SocketAddress &_rsa):addr(_rsa.addr()), sz(_rsa.size()){}
inline SocketAddressStub::SocketAddressStub(const SocketAddress4 &_rsa):addr(_rsa.addr()), sz(_rsa.size()){}

inline SocketAddressStub& SocketAddressStub::operator=(const SocketAddressInfoIterator &_it){
	if(_it){
		addr = _it.addr(); sz = _it.size();
	}else{
		addr = NULL; sz = 0;
	}
	return *this;
}

inline SocketAddressStub& SocketAddressStub::operator=(const SocketAddress &_rsa){
	addr = _rsa.addr();	sz = _rsa.size();
	return *this;
}
inline SocketAddressStub& SocketAddressStub::operator=(const SocketAddress4 &_rsa){
	addr = _rsa.addr();	sz = _rsa.size();
	return *this;
}
inline uint16 SocketAddressStub::port()const{
	if(sz == SocketAddressStub4::size()){
		return htons(reinterpret_cast<const sockaddr_in*>(addr)->sin_port);
	}else{
		return htons(reinterpret_cast<const sockaddr_in6*>(addr)->sin6_port);
	}
}
//----------------------------------------------------------------------------
inline /*static*/ bool SocketAddressStub4::equalAdresses(
	SocketAddressStub4 const & _rsa1,
	SocketAddressStub4 const & _rsa2
){
	return _rsa1.addr->sin_addr.s_addr == _rsa2.addr->sin_addr.s_addr;
}
inline /*static*/ bool SocketAddressStub4::compareAddressesLess(
	SocketAddressStub4 const & _rsa1,
	SocketAddressStub4 const & _rsa2
){
	return _rsa1.addr->sin_addr.s_addr < _rsa2.addr->sin_addr.s_addr;
}

inline SocketAddressStub4::SocketAddressStub4(
	const SocketAddressStub &_rsap
):addr((sockaddr_in*)_rsap.addr)/*, size(_rsap.size)*/{
	cassert(_rsap.family() == SocketAddressInfo::Inet4);
	cassert(_rsap.size() == size());
}
inline SocketAddressStub4::SocketAddressStub4(
	const SocketAddress &_rsa
):addr((sockaddr_in*)_rsa.addr())/*, size(_rsa.size())*/{
	cassert(_rsa.family() == SocketAddressInfo::Inet4);
	cassert(_rsa.size() == size());
}
inline SocketAddressStub4::SocketAddressStub4(
	const SocketAddress4 &_rsa
):addr((sockaddr_in*)_rsa.addr()){
}
inline int SocketAddressStub4::port()const{
	return htons(addr->sin_port);
}
inline void SocketAddressStub4::port(uint16 _port){
	addr->sin_port = ntohs(_port);
}
inline bool SocketAddressStub4::operator<(
	const SocketAddressStub4 &_addr
)const{
	//return addr->sin_addr.s_addr < _addr.addr->sin_addr.s_addr;
	if(addr->sin_addr.s_addr < _addr.addr->sin_addr.s_addr){
		return true;
	}else if(addr->sin_addr.s_addr > _addr.addr->sin_addr.s_addr){
		return false;
	}else return port() < _addr.port();
}
inline bool SocketAddressStub4::operator==(
	const SocketAddressStub4 &_addr
)const{
	return (addr->sin_addr.s_addr == _addr.addr->sin_addr.s_addr) && (port() == _addr.port());
}
inline size_t SocketAddressStub4::hash()const{
	return addr->sin_addr.s_addr ^ addr->sin_port;
}
inline size_t SocketAddressStub4::addressHash()const{
	return addr->sin_addr.s_addr;
}
//----------------------------------------------------------------------------

inline /*static*/ bool SocketAddressStub6::equalAdresses(
	SocketAddressStub6 const & _rsa1,
	SocketAddressStub6 const & _rsa2
){
	return memcmp(
		(const void*)_rsa1.addr->sin6_addr.s6_addr,
		(const void*)_rsa2.addr->sin6_addr.s6_addr,
		sizeof(in6_addr)
	) == 0;
}
inline /*static*/ bool SocketAddressStub6::compareAddressesLess(
	SocketAddressStub6 const & _rsa1,
	SocketAddressStub6 const & _rsa2
){
	return memcmp(
		(const void*)_rsa1.addr->sin6_addr.s6_addr,
		(const void*)_rsa2.addr->sin6_addr.s6_addr,
		sizeof(in6_addr)
	) < 0;
}

inline SocketAddressStub6::SocketAddressStub6(
	const SocketAddressStub &_rsap
):addr((sockaddr_in6*)_rsap.addr)/*, size(_rsap.size)*/{
	cassert(_rsap.family() == SocketAddressInfo::Inet6);
	cassert(_rsap.size() == size());
}
inline SocketAddressStub6::SocketAddressStub6(
	const SocketAddress &_rsa
):addr((sockaddr_in6*)_rsa.addr())/*,size(_rsa.size())*/{
	cassert(_rsa.family() == SocketAddressInfo::Inet6);
	cassert(_rsa.size() == size());
}
inline bool SocketAddressStub6::operator<(
	const SocketAddressStub6 &_addr
)const{
	//return addr->sin6_addr.s_addr < _addr.addr->sin6_addr.s_addr;
	//TODO:
	return (memcmp(
		(const void*)addr->sin6_addr.s6_addr,
		(const void*)_addr.addr->sin6_addr.s6_addr,
		sizeof(in6_addr)
	) < 0);
}
inline bool SocketAddressStub6::operator==(
	const SocketAddressStub6 &_addr
)const{
	return (memcmp(
		(const void*)addr->sin6_addr.s6_addr,
		(const void*)_addr.addr->sin6_addr.s6_addr,
		sizeof(in6_addr)
	) < 0) && (port() == _addr.port());
}
inline int SocketAddressStub6::port()const{
	return htons(addr->sin6_port);
}
inline void SocketAddressStub6::port(uint16 _port){
	addr->sin6_port = ntohs(_port);
}

inline size_t SocketAddressStub6::hash()const{
	return 0;
}
inline size_t SocketAddressStub6::addressHash()const{
	return 0;
}
//----------------------------------------------------------------------------
inline SocketAddress::SocketAddress(const SocketAddressInfoIterator &_it){
	if(_it){
		addr(_it.addr(), _it.size());
	}else{
		sz = 0;
	}
}
inline SocketAddress& SocketAddress::operator=(const SocketAddressInfoIterator &_it){
	if(_it){
		addr(_it.addr(), _it.size());
	}else{
		sz = 0;
	}
	return *this;
}
inline SocketAddress& SocketAddress::operator=(const SocketAddressStub &_sp){
	addr(_sp.addr, _sp.size());
	return *this;
}
inline size_t SocketAddress::hash()const{
	if(sz == sizeof(sockaddr_in)){
		//TODO: improve
		return addrin()->sin_addr.s_addr ^ addrin()->sin_port;
	}else{//ipv6
		//TODO:
		return 0;
	}
}

inline size_t SocketAddress::addressHash()const{
	if(sz == sizeof(sockaddr_in)){
		//TODO: improve
		return addrin()->sin_addr.s_addr;
	}else{//ipv6
		//TODO:
		return 0;
	}
}

//----------------------------------------------------------------------------
inline SocketAddress4::SocketAddress4(const SocketAddressInfoIterator &_it){
	if(_it){
		addr(_it.addr(), _it.size());
	}else{
		clear();
	}
}
inline SocketAddress4& SocketAddress4::operator=(const SocketAddressInfoIterator &_it){
	if(_it){
		addr(_it.addr(), _it.size());
	}else{
		clear();
	}
	return *this;
}
inline SocketAddress4& SocketAddress4::operator=(const SocketAddressStub &_sp){
	addr(_sp.addr, _sp.size());
	return *this;
}
inline size_t SocketAddress4::hash()const{
	//TODO: improve
	return addrin()->sin_addr.s_addr ^ addrin()->sin_port;
}

inline size_t SocketAddress4::addressHash()const{
	return addrin()->sin_addr.s_addr;
}
#endif

#ifdef NINLINES
#undef inline
#endif

