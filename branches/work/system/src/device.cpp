/* Implementation file device.cpp
	
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

#ifdef ON_WINDOWS
#include <WinSock2.h>
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <cstdio>
#include <cerrno>
#include <cstring>

#include "system/socketdevice.hpp"
#include "system/filedevice.hpp"
#include "system/directory.hpp"
#include "system/cassert.hpp"
#include "system/debug.hpp"

Device::Device(const Device &_dev):desc(_dev.descriptor()) {
	_dev.desc = invalidDescriptor();
}

Device& Device::operator=(const Device &_dev){
	close();
	desc = _dev.descriptor();
	_dev.desc = invalidDescriptor();
	return *this;
}
Device::Device(DescriptorT _desc):desc(_desc){}

Device::~Device(){
	close();
}

int Device::read(char	*_pb, uint32 _bl){
	cassert(ok());
#ifdef ON_WINDOWS
	DWORD cnt;
	if(ReadFile(desc, _pb, _bl, &cnt, NULL)){
		return cnt;
	}else{
		return -1;
	}
#else
	return ::read(desc, _pb, _bl);
#endif
}

int Device::write(const char* _pb, uint32 _bl){
	cassert(ok());
#ifdef ON_WINDOWS
	DWORD cnt;
	if(WriteFile(desc, const_cast<char*>(_pb), _bl, &cnt, NULL)){
		return cnt;
	}else{
		return -1;
	}
#else
	return ::write(desc, _pb, _bl);
#endif
}

void Device::close(){
	if(ok()){
#ifdef ON_WINDOWS
		CloseHandle(desc);
#else
		::close(desc);
#endif
		desc = invalidDescriptor();
	}
}

int Device::flush(){
	cassert(ok());
#ifdef ON_WINDOWS
	if(FlushFileBuffers(desc)){
		return 0;
	}else{
		return -1;
	}
#else
	return fsync(desc);
#endif
}

//-- SeekableDevice	----------------------------------------
int SeekableDevice::read(char *_pb, uint32 _bl, int64 _off){
#ifdef ON_WINDOWS
	return -1;
#else
	return pread(descriptor(), _pb, _bl, _off);
#endif
}

int SeekableDevice::write(const char *_pb, uint32 _bl, int64 _off){
#ifdef ON_WINDOWS
	return -1;
#else
	return pwrite(descriptor(), _pb, _bl, _off);
#endif
}

const int seekmap[3]={SEEK_SET,SEEK_CUR,SEEK_END};

int64 SeekableDevice::seek(int64 _pos, SeekRef _ref){
#ifdef ON_WINDOWS
	return -1;
#else
	return ::lseek(descriptor(), _pos, seekmap[_ref]);
#endif
}

int SeekableDevice::truncate(int64 _len){
#ifdef ON_WINDOWS
	return -1;
#else
	return ::ftruncate(descriptor(), _len);
#endif
}

//-- File ----------------------------------------

FileDevice::FileDevice(){
}

int FileDevice::open(const char* _fname, int _how){
#ifdef ON_WINDOWS
	return -1;
#else
	descriptor(::open(_fname, _how, 00666));
	return descriptor() >= 0 ? 0 : -1;
#endif
}

int FileDevice::create(const char* _fname, int _how){
	return this->open(_fname, _how | CR | TR);
}

int64 FileDevice::size()const{
#ifdef ON_WINDOWS
	return -1;
#else
	struct stat st;
	if(fstat(descriptor(), &st)) return -1;
	return st.st_size;
#endif
}
bool FileDevice::canRetryOpen()const{
#ifdef ON_WINDOWS
	return false;
#else
	return (errno == EMFILE) || (errno == ENOMEM);
#endif
}
/*static*/ int64 FileDevice::size(const char *_fname){
#ifdef ON_WINDOWS
	return -1;
#else
	struct stat st;
	if(stat(_fname, &st)) return -1;
	return st.st_size;
#endif
}
//-- Directory -------------------------------------
/*static*/ int Directory::create(const char *_path){
#ifdef ON_WINDOWS
	return -1;
#else
	return mkdir(_path,  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
}

/*static*/ int Directory::eraseFile(const char *_path){
#ifdef ON_WINDOWS
	return -1;
#else
	return unlink(_path);
#endif
}
/*static*/ int Directory::renameFile(const char *_to, const char *_from){
#ifdef ON_WINDOWS
	return -1;
#else
	return ::rename(_from, _to);
#endif
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
	close();
}
void SocketDevice::shutdownRead(){
#ifdef ON_WINDOWS
	if(ok()) shutdown(descriptor(), SD_RECEIVE);
#else
	if(ok()) shutdown(descriptor(), SHUT_RD);
#endif
}
void SocketDevice::shutdownWrite(){
#ifdef ON_WINDOWS
	if(ok()) shutdown(descriptor(), SD_SEND);
#else
	if(ok()) shutdown(descriptor(), SHUT_WR);
#endif
}
void SocketDevice::shutdownReadWrite(){
#ifdef ON_WINDOWS
	if(ok()) shutdown(descriptor(), SD_BOTH);
#else
	if(ok()) shutdown(descriptor(), SHUT_RDWR);
#endif
}
void SocketDevice::close(){
#ifdef ON_WINDOWS
	shutdownReadWrite();
	if(ok()){
		closesocket(descriptor());
		descriptor(invalidDescriptor());
	}
#else
	shutdownReadWrite();
	Device::close();
#endif
}
int SocketDevice::create(const SocketAddressInfoIterator &_rai){
#ifdef ON_WINDOWS
	Device::descriptor(socket(_rai.family(), _rai.type(), _rai.protocol()));
	if(ok()) return OK;
	edbgx(Dbg::system, "socket create");
	return BAD;
#else
	Device::descriptor(socket(_rai.family(), _rai.type(), _rai.protocol()));
	if(ok()) return OK;
	edbgx(Dbg::system, "socket create: "<<strerror(errno));
	return BAD;
#endif
}
int SocketDevice::create(SocketAddressInfo::Family _family, SocketAddressInfo::Type _type, int _proto){
#ifdef ON_WINDOWS
	Device::descriptor(socket(_family, _type, _proto));
	if(ok()) return OK;
	edbgx(Dbg::system, "socket create");
	return BAD;
#else
	Device::descriptor(socket(_family, _type, _proto));
	if(ok()) return OK;
	edbgx(Dbg::system, "socket create: "<<strerror(errno));
	return BAD;
#endif
}
int SocketDevice::connect(const SocketAddressInfoIterator &_rai){
#ifdef ON_WINDOWS
	int rv = ::connect(descriptor(), _rai.addr(), _rai.size());
	if (rv < 0) { // sau rv == -1 ...
		if(WSAGetLastError() == WSAEWOULDBLOCK) return NOK;
		edbgx(Dbg::system, "socket connect");
		close();
		return BAD;
	}
	return OK;
#else
	int rv = ::connect(descriptor(), _rai.addr(), _rai.size());
	if (rv < 0) { // sau rv == -1 ...
		if(errno == EINPROGRESS) return NOK;
		edbgx(Dbg::system, "socket connect: "<<strerror(errno));
		close();
		return BAD;
	}
	return OK;
#endif
}
int SocketDevice::prepareAccept(const SocketAddressInfoIterator &_rai, unsigned _listencnt){
#ifdef ON_WINDOWS
	return BAD;
#else
	int yes = 1;
	int rv = setsockopt(descriptor(), SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof(yes));
	if(rv < 0) {
		edbgx(Dbg::system, "socket setsockopt: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}

	rv = ::bind(descriptor(), _rai.addr(), _rai.size());
	if(rv < 0) {
		edbgx(Dbg::system, "socket bind: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	rv = listen(descriptor(), _listencnt);
	if(rv < 0){
		edbgx(Dbg::system, "socket listen: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	return OK;
#endif
}
int SocketDevice::accept(SocketDevice &_dev){
#ifdef ON_WINDOWS
	return BAD;
#else
	if(!ok()) return BAD;
	SocketAddress sa;
	int rv = ::accept(descriptor(), sa.addr(), &sa.size());
	if (rv < 0) {
		if(errno == EAGAIN) return NOK;
		edbgx(Dbg::system, "socket accept: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	_dev.Device::descriptor(rv);
	return OK;
#endif
}
int SocketDevice::bind(const SocketAddressInfoIterator &_rai){
#ifdef ON_WINDOWS
	return BAD;
#else
	if(!ok()) return BAD;
	int rv = ::bind(descriptor(), _rai.addr(), _rai.size());
	if(rv < 0){
		edbgx(Dbg::system, "socket bind: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	return OK;
#endif
}
int SocketDevice::bind(const SocketAddressPair &_rsa){
#ifdef ON_WINDOWS
	return BAD;
#else
	if(!ok()) return BAD;
	int rv = ::bind(descriptor(), _rsa.addr, _rsa.size());
	if(rv < 0){
		edbgx(Dbg::system, "socket bind: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	return OK;
#endif
}
int SocketDevice::makeBlocking(int _msec){
#ifdef ON_WINDOWS
	return BAD;
#else
	if(!ok()) return BAD;
	int flg = fcntl(descriptor(), F_GETFL);
	if(flg == -1){
		edbgx(Dbg::system, "socket fcntl getfl: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	flg &= ~O_NONBLOCK;
	int rv = fcntl(descriptor(), F_SETFL, flg);
	if (rv < 0){
		edbgx(Dbg::system, "error fcntl setfl: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	return OK;
#endif
}
int SocketDevice::makeNonBlocking(){
#ifdef ON_WINDOWS
	return BAD;
#else
	if(!ok()) return BAD;
	int flg = fcntl(descriptor(), F_GETFL);
	if(flg == -1){
		edbgx(Dbg::system, "socket fcntl getfl: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	int rv = fcntl(descriptor(), F_SETFL, flg | O_NONBLOCK);
	if(rv < 0){
		edbgx(Dbg::system, "socket fcntl setfl: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return BAD;
	}
	return OK;
#endif
}
bool SocketDevice::isBlocking(){
#ifdef ON_WINDOWS
	return true;
#else
	int flg = fcntl(descriptor(), F_GETFL);
	if(flg == -1){
		edbgx(Dbg::system, "socket fcntl getfl: "<<strerror(errno));
		shutdownReadWrite();
		close();
		return false;
	}
	return !(flg & O_NONBLOCK);
#endif
}
bool SocketDevice::shouldWait()const{
#ifdef ON_WINDOWS
	return false;
#else
	return errno == EAGAIN;
#endif
}
int SocketDevice::send(const char* _pb, unsigned _ul, unsigned){
#ifdef ON_WINDOWS
	return -1;
#else
	return ::send(descriptor(), _pb, _ul, 0);
#endif
}
int SocketDevice::recv(char *_pb, unsigned _ul, unsigned){
#ifdef ON_WINDOWS
	return -1;
#else
	return ::recv(descriptor(), _pb, _ul, 0);
#endif
}
int SocketDevice::send(const char* _pb, unsigned _ul, const SocketAddressPair &_sap){
#ifdef ON_WINDOWS
	return -1;
#else
	return ::sendto(descriptor(), _pb, _ul, 0, _sap.addr, _sap.size());
#endif
}
int SocketDevice::recv(char *_pb, unsigned _ul, SocketAddress &_rsa){
#ifdef ON_WINDOWS
	return -1;
#else
	_rsa.clear();
	_rsa.size(SocketAddress::Capacity);
	return ::recvfrom(descriptor(), _pb, _ul, 0, _rsa.addr(), &_rsa.size());
#endif
}
int SocketDevice::remoteAddress(SocketAddress &_rsa)const{
#ifdef ON_WINDOWS
	return BAD;
#else
	_rsa.clear();
	_rsa.size(SocketAddress::Capacity);
	int rv = getpeername(descriptor(), _rsa.addr(), &_rsa.size());
	if(rv){
		edbgx(Dbg::system, "socket getpeername: "<<strerror(errno));
		return BAD;
	}
	return OK;
#endif
}
int SocketDevice::localAddress(SocketAddress &_rsa)const{
#ifdef ON_WINDOWS
	return BAD;
#else
	_rsa.clear();
	_rsa.size(SocketAddress::Capacity);
	int rv = getsockname(descriptor(), _rsa.addr(), &_rsa.size());
	if(rv){
		edbgx(Dbg::system, "socket getsockname: "<<strerror(errno));
		return BAD;
	}
	return OK;
#endif
}

int SocketDevice::type()const{
#ifdef ON_WINDOWS
	return BAD;
#else
	int val = 0;
	socklen_t valsz = sizeof(int);
	int rv = getsockopt(descriptor(), SOL_SOCKET, SO_TYPE, &val, &valsz);
	if(rv == 0){
		return val;
	}
	edbgx(Dbg::system, "socket getsockopt: "<<strerror(errno));
	return BAD;
#endif
}

bool SocketDevice::isListening()const{
#ifdef ON_WINDOWS
	return false;
#else
	int val = 0;
	socklen_t valsz = sizeof(int);
	int rv = getsockopt(descriptor(), SOL_SOCKET, SO_ACCEPTCONN, &val, &valsz);
	if(rv == 0){
		return val != 0;
	}
	edbgx(Dbg::system, "socket getsockopt: "<<strerror(errno));
	//try work-arround
	if(this->type() == SocketAddressInfo::Datagram){
		return false;
	}
	SocketAddress	sa;
	rv = ::accept(descriptor(), sa.addr(), &sa.size());
	if(rv < 0){
		if(errno == EINVAL){
			return false;
		}
		return true;
	}
	SocketDevice	sd;
	sd.Device::descriptor(rv);//:( we loose a connection
		
	return true;
#endif
}

#ifdef ON_WINDOWS
void SocketDevice::close(){
}
#endif
