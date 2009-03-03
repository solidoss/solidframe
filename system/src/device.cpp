/* Implementation file device.cpp
	
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

#include <unistd.h>
#include "system/filedevice.hpp"
#include "system/socketdevice.hpp"
#include "system/directory.hpp"
#include "system/cassert.hpp"
#include "system/debug.hpp"
#include <cstdio>
#include <cerrno>
#include <cstring>

Device::Device(const Device &_dev):desc(_dev.descriptor()) {
	_dev.desc = -1;
}

Device& Device::operator=(const Device &_dev){
	close();
	desc = _dev.descriptor();_dev.desc = -1;
	return *this;
}
Device::Device(int _desc):desc(_desc){}

Device::~Device(){
	close();
}

int Device::read(char	*_pb, uint32 _bl){
	cassert(desc >= 0);
	return ::read(desc, _pb, _bl);
}

int Device::write(const char* _pb, uint32 _bl){
	cassert(desc >= 0);
	return ::write(desc, _pb, _bl);
}

void Device::close(){
	if(desc > 0){ ::close(desc); desc = -1;}
}

int Device::flush(){
	cassert(desc >= 0);
	return fsync(desc);
}

//-- SeekableDevice	----------------------------------------
int SeekableDevice::read(char *_pb, uint32 _bl, int64 _off){
	return pread(descriptor(), _pb, _bl, _off);
}

int SeekableDevice::write(const char *_pb, uint32 _bl, int64 _off){
	return pwrite(descriptor(), _pb, _bl, _off);
}

const int seekmap[3]={SEEK_SET,SEEK_CUR,SEEK_END};

int64 SeekableDevice::seek(int64 _pos, SeekRef _ref){
	return ::lseek(descriptor(), _pos, seekmap[_ref]);
}

int SeekableDevice::truncate(int64 _len){
	return ::ftruncate(descriptor(), _len);
}

//-- File ----------------------------------------

FileDevice::FileDevice(){
}

int FileDevice::open(const char* _fname, int _how){
	descriptor(::open(_fname, _how, 00666));
	return descriptor() >= 0 ? 0 : -1;
}

int FileDevice::create(const char* _fname, int _how){
	return this->open(_fname, _how | CR | TR);
}

int64 FileDevice::size()const{
	struct stat st;
	if(fstat(descriptor(), &st)) return -1;
	return st.st_size;
}
bool FileDevice::canRetryOpen()const{
	return (errno == EMFILE) || (errno == ENOMEM);
}
/*static*/ int64 FileDevice::size(const char *_fname){
	struct stat st;
	if(stat(_fname, &st)) return -1;
	return st.st_size;
}
//-- Directory -------------------------------------
/*static*/ int Directory::create(const char *_path){
	return mkdir(_path,  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

/*static*/ int Directory::eraseFile(const char *_path){
	return unlink(_path);
}
/*static*/ int Directory::renameFile(const char *_to, const char *_from){
	return ::rename(_from, _to);
}

//---- SocketDevice ---------------------------------
SocketDevice::SocketDevice(const SocketDevice &_sd):Device(_sd){}
SocketDevice::SocketDevice(){
}
SocketDevice& SocketDevice::operator=(const SocketDevice &_dev){
	*static_cast<Device*>(this) = static_cast<const Device&>(_dev);
	return *this;
}
SocketDevice::~SocketDevice(){
	shutdownReadWrite();
}
void SocketDevice::shutdownRead(){
	if(ok()) shutdown(descriptor(), SHUT_RD);
}
void SocketDevice::shutdownWrite(){
	if(ok()) shutdown(descriptor(), SHUT_WR);
}
void SocketDevice::shutdownReadWrite(){
	if(ok()) shutdown(descriptor(), SHUT_RDWR);
}
int SocketDevice::create(const AddrInfoIterator &_rai){
	Device::descriptor(socket(_rai.family(), _rai.type(), _rai.protocol()));
	if(ok()) return OK;
	edbg("socket create: "<<strerror(errno));
	return BAD;
}
int SocketDevice::create(AddrInfo::Family _family, AddrInfo::Type _type, int _proto){
	Device::descriptor(socket(_family, _type, _proto));
	if(ok()) return OK;
	edbg("socket create: "<<strerror(errno));
	return BAD;
}
int SocketDevice::connect(const AddrInfoIterator &_rai){
	int rv = ::connect(descriptor(), _rai.addr(), _rai.size());
	if (rv < 0) { // sau rv == -1 ...
		if(errno == EINPROGRESS) return NOK;
		edbg("socket connect: "<<strerror(errno));
		close();
		return BAD;
	}
	return OK;
}
int SocketDevice::prepareAccept(const AddrInfoIterator &_rai, unsigned _listencnt){
	int yes = 1;
	int rv = setsockopt(descriptor(), SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof(yes));
	if(rv < 0) {
		edbg("socket setsockopt: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}

	rv = ::bind(descriptor(), _rai.addr(), _rai.size());
	if(rv < 0) {
		edbg("socket bind: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	rv = listen(descriptor(), _listencnt);
	if(rv < 0){
		edbg("socket listen: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	return OK;
}
int SocketDevice::accept(SocketDevice &_dev){
	if(!ok()) return BAD;
	SocketAddress sa;
	int rv = ::accept(descriptor(), sa.addr(), &sa.size());
	if (rv < 0) {
		if(errno == EAGAIN) return NOK;
		edbg("socket accept: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	_dev.Device::descriptor(rv);
	return OK;
}
int SocketDevice::bind(const AddrInfoIterator &_rai){
	if(!ok()) return BAD;
	int rv = ::bind(descriptor(), _rai.addr(), _rai.size());
	if(rv < 0){
		edbg("socket bind: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	return OK;
}
int SocketDevice::bind(const SockAddrPair &_rsa){
	if(!ok()) return BAD;
	int rv = ::bind(descriptor(), _rsa.addr, _rsa.size);
	if(rv < 0){
		edbg("socket bind: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	return OK;
}
int SocketDevice::makeBlocking(int _msec){
	if(!ok()) return BAD;
	int flg = fcntl(descriptor(), F_GETFL);
	if(flg == -1){
		edbg("socket fcntl getfl: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	flg &= ~O_NONBLOCK;
	int rv = fcntl(descriptor(), F_SETFL, flg);
	if (rv < 0){
		edbg("error fcntl setfl: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	return OK;
}
int SocketDevice::makeNonBlocking(){
	if(!ok()) return BAD;
	int flg = fcntl(descriptor(), F_GETFL);
	if(flg == -1){
		edbg("socket fcntl getfl: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	int rv = fcntl(descriptor(), F_SETFL, flg | O_NONBLOCK);
	if(rv < 0){
		edbg("socket fcntl setfl: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	return OK;
}
bool SocketDevice::isBlocking(){
	int flg = fcntl(descriptor(), F_GETFL);
	if(flg == -1){
		edbg("socket fcntl getfl: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return false;
	}
	return !(flg & O_NONBLOCK);
}
bool SocketDevice::shouldWait()const{
	return errno == EAGAIN;
}
int SocketDevice::send(const char* _pb, unsigned _ul, unsigned){
	return ::send(descriptor(), _pb, _ul, 0);
}
int SocketDevice::recv(char *_pb, unsigned _ul, unsigned){
	return ::recv(descriptor(), _pb, _ul, 0);
}
int SocketDevice::send(const char* _pb, unsigned _ul, const SockAddrPair &_sap){
	return ::sendto(descriptor(), _pb, _ul, 0, _sap.addr, _sap.size);
}
int SocketDevice::recv(char *_pb, unsigned _ul, SocketAddress &_rsa){
	_rsa.clear();
	_rsa.size() = SocketAddress::MaxSockAddrSz;
	return ::recvfrom(descriptor(), _pb, _ul, 0, _rsa.addr(), &_rsa.size());
}
int SocketDevice::remoteAddress(SocketAddress &_rsa)const{
	_rsa.clear();
	_rsa.size() = SocketAddress::MaxSockAddrSz;
	int rv = getpeername(descriptor(), _rsa.addr(), &_rsa.size());
	if(rv){
		edbg("socket getpeername: "<<strerror(errno));
		return BAD;
	}
	return OK;
}
int SocketDevice::localAddress(SocketAddress &_rsa)const{
	_rsa.clear();
	_rsa.size() = SocketAddress::MaxSockAddrSz;
	int rv = getsockname(descriptor(), _rsa.addr(), &_rsa.size());
	if(rv){
		edbg("socket getsockname: "<<strerror(errno));
		return BAD;
	}
	return OK;
}

int SocketDevice::type()const{
	int val = 0;
	socklen_t valsz = sizeof(int);
	int rv = getsockopt(descriptor(), SOL_SOCKET, SO_TYPE, &val, &valsz);
	if(rv == 0){
		return val;
	}
	edbg("socket getsockopt: "<<strerror(errno));
	return BAD;
}

bool SocketDevice::isListening()const{
	int val = 0;
	socklen_t valsz = sizeof(int);
	int rv = getsockopt(descriptor(), SOL_SOCKET, SO_ACCEPTCONN, &val, &valsz);
	if(rv == 0){
		return val;
	}
	edbg("socket getsockopt: "<<strerror(errno));
	return false;
}

