// system/src/device.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifdef ON_WINDOWS
#include <WinSock2.h>
#include <Windows.h>
#else
#define _FILE_OFFSET_BITS 64
#include <unistd.h>
#include <netinet/tcp.h>
#endif

#include <cstdio>
#include <cerrno>
#include <cstring>

#include "system/socketdevice.hpp"
#include "system/filedevice.hpp"
#include "system/directory.hpp"
#include "system/cassert.hpp"
#include "system/debug.hpp"

using namespace std;

namespace solid{

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

int Device::read(char	*_pb, size_t _bl){
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

int Device::write(const char* _pb, size_t _bl){
	cassert(ok());
#ifdef ON_WINDOWS
	DWORD cnt;
	/*OVERLAPPED ovp;
	ovp.Offset = 0;
	ovp.OffsetHigh = 0;
	ovp.hEvent = NULL;*/
	if(WriteFile(desc, const_cast<char*>(_pb), _bl, &cnt, NULL)){
		return cnt;
	}else{
		return -1;
	}
#else
	return ::write(desc, _pb, _bl);
#endif
}

bool Device::cancel(){
#ifdef ON_WINDOWS
	return CancelIoEx(Device::descriptor(), NULL) != 0;
#else
	return true;
#endif
}

void Device::close(){
	if(ok()){
#ifdef ON_WINDOWS
		CloseHandle(desc);
#else
		cverify(!::close(desc));
#endif
		desc = invalidDescriptor();
	}
}

void Device::flush(){
	cassert(ok());
#ifdef ON_WINDOWS
	cverify(FlushFileBuffers(desc));
#else
	cverify(!fsync(desc));
#endif
}

//-- SeekableDevice	----------------------------------------
int SeekableDevice::read(char *_pb, size_t _bl, int64 _off){
#ifdef ON_WINDOWS
	int64 off(seek(0, SeekCur));
	seek(_off);
	int rv = Device::read(_pb, _bl);
	seek(off);
	return rv;
#else
	return pread(descriptor(), _pb, _bl, _off);
#endif
}

int SeekableDevice::write(const char *_pb, size_t _bl, int64 _off){
#ifdef ON_WINDOWS
	int64 off(seek(0, SeekCur));
	seek(_off);
	int rv = Device::write(_pb, _bl);
	seek(off);
	return rv;
#else
	return pwrite(descriptor(), _pb, _bl, _off);
#endif
}

#ifdef ON_WINDOWS
const DWORD seekmap[3]={FILE_BEGIN, FILE_CURRENT, FILE_END};
#else
const int seekmap[3]={SEEK_SET,SEEK_CUR,SEEK_END};
#endif

int64 SeekableDevice::seek(int64 _pos, SeekRef _ref){
#ifdef ON_WINDOWS
	LARGE_INTEGER li;

	li.QuadPart = _pos;
	li.LowPart = SetFilePointer(descriptor(), li.LowPart, &li.HighPart, seekmap[_ref]);
	
	if(
		li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR
	){
		li.QuadPart = -1;
	}
	return li.QuadPart;
#else
	return ::lseek(descriptor(), _pos, seekmap[_ref]);
#endif
}

bool SeekableDevice::truncate(int64 _len){
#ifdef ON_WINDOWS
	seek(_len);
	return SetEndOfFile(descriptor());
#else
	return ::ftruncate(descriptor(), _len) == 0;
#endif
}

//-- File ----------------------------------------

FileDevice::FileDevice(){
}

#ifdef ON_WINDOWS
HANDLE do_open(WCHAR *_pwc, const char *_fname, const size_t _sz, const size_t _wcp, int _how){
	WCHAR *pwctmp(NULL);
	//first convert _fname to _pwc
	int rv = MultiByteToWideChar(CP_UTF8, 0, _fname, _sz, _pwc, _wcp);
	if(rv == 0){
		if( GetLastError() == ERROR_INSUFFICIENT_BUFFER){
			rv = MultiByteToWideChar(CP_UTF8, 0, _fname, _sz, _pwc, 0);
			if(rv == 0){
				return Device::invalidDescriptor();
			}
			pwctmp = new WCHAR[rv + 1];
			rv = MultiByteToWideChar(CP_UTF8, 0, _fname, _sz, _pwc, rv + 1);
			if(rv == 0){
				return Device::invalidDescriptor();
			}
			pwctmp[rv] = 0;
		}else{
			return Device::invalidDescriptor();
		}
	}else{
		_pwc[rv] = 0;
		pwctmp = _pwc;
	}
	DWORD acc(0);
	if(_how & FileDevice::RO){
		acc |= GENERIC_READ;
	}else if(_how & FileDevice::WO){
		acc |= GENERIC_WRITE;
	}else if(_how & FileDevice::RW){
		acc |= (GENERIC_READ | GENERIC_WRITE);
	}

	DWORD creat(0);

	if(_how & FileDevice::CR){
		if(_how & FileDevice::TR){
			creat |= CREATE_ALWAYS;
		}else{
			creat |= CREATE_NEW;
		}
	}else{
		creat |=OPEN_EXISTING;
		if(_how & FileDevice::AP){
		}
		if(_how & FileDevice::TR){
			creat |= TRUNCATE_EXISTING;
		}
	}

	HANDLE h = CreateFileW(pwctmp, acc, FILE_SHARE_READ, NULL, creat, FILE_ATTRIBUTE_NORMAL, NULL);
	if(_pwc != pwctmp){
		delete []pwctmp;
	}
	return h;
}
#endif

bool FileDevice::open(const char* _fname, int _how){
#ifdef ON_WINDOWS
	//_fname is utf-8, so we need to convert it to WCHAR
	const size_t sz(strlen(_fname));
	const size_t szex = sz + 1;
	if(szex < 256){
		WCHAR pwc[512];
		descriptor(do_open(pwc, _fname, sz, 512, _how));
	}else if(szex < 512){
		WCHAR pwc[1024];
		descriptor(do_open(pwc, _fname, sz, 1024, _how));
	}else if(szex < 1024){
		WCHAR pwc[2048];
		descriptor(do_open(pwc, _fname, sz, 2048, _how));
	}else{
		WCHAR pwc[4096];
		descriptor(do_open(pwc, _fname, sz, 4096, _how));
	}
	if(ok()){
		if(_how & AP){
			seek(0, SeekEnd);
		}
	}
	return ok();
#else
	descriptor(::open(_fname, _how, 00666));
	return ok();
#endif
}

bool FileDevice::create(const char* _fname, int _how){
	return this->open(_fname, _how | CreateE | TruncateE);
}

int64 FileDevice::size()const{
#ifdef ON_WINDOWS
	LARGE_INTEGER li;

	li.QuadPart = 0;
	if(GetFileSizeEx(descriptor(), &li)){
		return li.QuadPart;
	}else{
		return -1;
	}
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
	FileDevice fd;
	if(fd.open(_fname, RO)){
		return -1;
	}
	return fd.size();
#else
	struct stat st;
	if(stat(_fname, &st)) return -1;
	return st.st_size;
#endif
}
//-- Directory -------------------------------------
#ifdef ON_WINDOWS
int do_create_directory(WCHAR *_pwc, const char *_path, size_t _sz, size_t _wcp){
	WCHAR *pwctmp(NULL);
	//first convert _fname to _pwc
	int rv = MultiByteToWideChar(CP_UTF8, 0, _path, _sz, _pwc, _wcp);
	if(rv == 0){
		if( GetLastError() == ERROR_INSUFFICIENT_BUFFER){
			rv = MultiByteToWideChar(CP_UTF8, 0, _path, _sz, _pwc, 0);
			if(rv == 0){
				return -1;
			}
			pwctmp = new WCHAR[rv + 1];
			rv = MultiByteToWideChar(CP_UTF8, 0, _path, _sz, _pwc, rv + 1);
			if(rv == 0){
				return -1;
			}
			pwctmp[rv] = 0;
		}else{
			return -1;
		}
	}else{
		_pwc[rv] = 0;
		pwctmp = _pwc;
	}
	BOOL brv = CreateDirectoryW(pwctmp, NULL);
	if(_pwc != pwctmp){
		delete []pwctmp;
	}
	if(brv){
		return 0;
	}
	return -1;
}
#endif
/*static*/ bool Directory::create(const char *_fname){
#ifdef ON_WINDOWS
	const size_t sz(strlen(_fname));
	const size_t szex = sz + 1;
	if(szex < 256){
		WCHAR pwc[512];
		return do_create_directory(pwc, _fname, sz, 512) == 0;
	}else if(szex < 512){
		WCHAR pwc[1024];
		return do_create_directory(pwc, _fname, sz, 1024) == 0;
	}else if(szex < 1024){
		WCHAR pwc[2048];
		return do_create_directory(pwc, _fname, sz, 2048) == 0;
	}else{
		WCHAR pwc[4096];
		return do_create_directory(pwc, _fname, sz, 4096) == 0;
	}
	return false;
#else
	return mkdir(_fname,  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
#endif
}

#ifdef ON_WINDOWS
int do_erase_file(WCHAR *_pwc, const char *_path, size_t _sz, size_t _wcp){
	WCHAR *pwctmp(NULL);
	//first convert _fname to _pwc
	int rv = MultiByteToWideChar(CP_UTF8, 0, _path, _sz, _pwc, _wcp);
	if(rv == 0){
		if( GetLastError() == ERROR_INSUFFICIENT_BUFFER){
			rv = MultiByteToWideChar(CP_UTF8, 0, _path, _sz, _pwc, 0);
			if(rv == 0){
				return -1;
			}
			pwctmp = new WCHAR[rv + 1];
			rv = MultiByteToWideChar(CP_UTF8, 0, _path, _sz, _pwc, rv + 1);
			if(rv == 0){
				return -1;
			}
			pwctmp[rv] = 0;
		}else{
			return -1;
		}
	}else{
		_pwc[rv] = 0;
		pwctmp = _pwc;
	}
	BOOL brv = DeleteFileW(pwctmp);
	if(_pwc != pwctmp){
		delete []pwctmp;
	}
	if(brv){
		return 0;
	}
	return -1;
}
#endif


/*static*/ bool Directory::eraseFile(const char *_fname){
#ifdef ON_WINDOWS
	const size_t sz(strlen(_fname));
	const size_t szex = sz + 1;
	if(szex < 256){
		WCHAR pwc[512];
		return do_erase_file(pwc, _fname, sz, 512) == 0;
	}else if(szex < 512){
		WCHAR pwc[1024];
		return do_erase_file(pwc, _fname, sz, 1024) == 0;
	}else if(szex < 1024){
		WCHAR pwc[2048];
		return do_erase_file(pwc, _fname, sz, 2048) == 0;
	}else{
		WCHAR pwc[4096];
		return do_erase_file(pwc, _fname, sz, 4096) == 0;
	}
	return false;
#else
	return unlink(_fname) == 0;
#endif
}
/*static*/ bool Directory::renameFile(const char *_to, const char *_from){
#ifdef ON_WINDOWS
	const size_t szto(strlen(_to));
	const size_t szfr(strlen(_from));
	WCHAR pwcto[4096];
	WCHAR pwcfr[4096];
	WCHAR *pwctmpto(NULL);
	WCHAR *pwctmpfr(NULL);
	
	//first convert _to to _pwc
	int rv = MultiByteToWideChar(CP_UTF8, 0, _to, szto, pwcto, 4096);
	if(rv == 0){
		if( GetLastError() == ERROR_INSUFFICIENT_BUFFER){
			rv = MultiByteToWideChar(CP_UTF8, 0, _to, szto, pwcto, 0);
			if(rv == 0){
				return false;
			}
			pwctmpto = new WCHAR[rv + 1];
			rv = MultiByteToWideChar(CP_UTF8, 0, _to, szto, pwcto, rv + 1);
			if(rv == 0){
				return false;
			}
			pwctmpto[rv] = 0;
		}else{
			return false;
		}
	}else{
		pwcto[rv] = 0;
		pwctmpto = pwcto;
	}

	rv = MultiByteToWideChar(CP_UTF8, 0, _from, szfr, pwcfr, 4096);
	if(rv == 0){
		if( GetLastError() == ERROR_INSUFFICIENT_BUFFER){
			rv = MultiByteToWideChar(CP_UTF8, 0, _from, szfr, pwcfr, 0);
			if(rv == 0){
				return false;
			}
			pwctmpfr = new WCHAR[rv + 1];
			rv = MultiByteToWideChar(CP_UTF8, 0, _from, szfr, pwcfr, rv + 1);
			if(rv == 0){
				return false;
			}
			pwctmpfr[rv] = 0;
		}else{
			return false;
		}
	}else{
		pwcfr[rv] = 0;
		pwctmpfr = pwcfr;
	}

	BOOL brv = MoveFileW(pwctmpfr, pwctmpto);

	if(pwctmpfr != pwcfr){
		delete []pwctmpfr;
	}
	if(pwctmpto != pwcto){
		delete []pwctmpto;
	}
	if(brv){
		return true;
	}else{
		return false;
	}
#else
	return ::rename(_from, _to) == 0;
#endif
}

#ifndef UDEBUG
#ifdef ON_WINDOWS
struct wsa_cleaner{
	~wsa_cleaner(){
		WSACleanup();
	}
};
#endif
#endif

//---- SocketDevice ---------------------------------
/*static*/ ErrorCodeT last_socket_error(){
#ifdef ON_WINDOWS
	const DWORD err = WSAGetLastError();
	return ErrorCodeT(err, ERROR_NS::system_category());
#else
	return solid::last_system_error();
#endif
}
SocketDevice::SocketDevice(const SocketDevice &_sd):Device(_sd){
#ifndef UDEBUG
#ifdef ON_WINDOWS
	static const wsa_cleaner wsaclean;
#endif
#endif
}
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
		Device::descriptor((HANDLE)invalidDescriptor());
	}
#else
	shutdownReadWrite();
	Device::close();
#endif
}

ErrorCodeT SocketDevice::create(const ResolveIterator &_rri){
#ifdef ON_WINDOWS
	//SOCKET s = socket(_rai.family(), _rai.type(), _rai.protocol());
	SOCKET s = WSASocket(_rri.family(), _rri.type(), _rri.protocol(), NULL, 0, 0);
	Device::descriptor((HANDLE)s);
	return last_socket_error();
#else
	Device::descriptor(socket(_rri.family(), _rri.type(), _rri.protocol()));
	return last_socket_error();
#endif
}

ErrorCodeT SocketDevice::create(
	SocketInfo::Family _family,
	SocketInfo::Type _type,
	int _proto
){
#ifdef ON_WINDOWS
	//SOCKET s = socket(_family, _type, _proto);
	SOCKET s = WSASocket(_family, _type, _proto, NULL, 0, 0);
	Device::descriptor((HANDLE)s);
	return last_socket_error();
#else
	Device::descriptor(socket(_family, _type, _proto));
	return last_socket_error();
#endif
}

ErrorCodeT SocketDevice::connect(const SocketAddressStub &_rsas, bool &_rcan_wait){
#ifdef ON_WINDOWS
	/*const int rv = */::connect(descriptor(), _rsas.sockAddr(), _rsas.size());
	_rcan_retry = (WSAGetLastError() == WSAEWOULDBLOCK);
	return last_socket_error();
#else
	/*const int rv = */::connect(descriptor(), _rsas.sockAddr(), _rsas.size());
	_rcan_wait = (errno == EINPROGRESS);
	return last_socket_error();
#endif
}

ErrorCodeT SocketDevice::connect(const SocketAddressStub &_rsas){
#ifdef ON_WINDOWS
	/*int rv = */::connect(descriptor(), _rsas.sockAddr(), _rsas.size());
	
	return last_socket_error();
#else
	/*int rv = */::connect(descriptor(), _rsas.sockAddr(), _rsas.size());
	return last_socket_error();
#endif
}

// int SocketDevice::connect(const ResolveIterator &_rai){
// #ifdef ON_WINDOWS
// 	int rv = ::connect(descriptor(), _rai.addr(), _rai.size());
// 	if (rv < 0) { // sau rv == -1 ...
// 		if(WSAGetLastError() == WSAEWOULDBLOCK) return NOK;
// 		edbgx(Debug::system, "socket connect");
// 		close();
// 		return BAD;
// 	}
// 	return OK;
// #else
// 	int rv = ::connect(descriptor(), _rai.addr(), _rai.size());
// 	if (rv < 0) { // sau rv == -1 ...
// 		if(errno == EINPROGRESS) return NOK;
// 		edbgx(Debug::system, "socket connect: "<<strerror(errno));
// 		close();
// 		return BAD;
// 	}
// 	return OK;
// #endif
// }
ErrorCodeT SocketDevice::prepareAccept(const SocketAddressStub &_rsas, size_t _listencnt){
#ifdef ON_WINDOWS
	int yes = 1;
	int rv = setsockopt(descriptor(), SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof(yes));
	if(rv < 0) {
		return last_socket_error();
	}

	rv = ::bind(descriptor(), _rsas.sockAddr(), _rsas.size());
	if(rv < 0) {
		return last_socket_error();
	}
	rv = listen(descriptor(), _listencnt);
	if(rv < 0){
		return last_socket_error();
	}
	return last_socket_error();
#else
	int yes = 1;
	int rv = setsockopt(descriptor(), SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof(yes));
	if(rv < 0) {
		return last_socket_error();
	}

	rv = ::bind(descriptor(), _rsas.sockAddr(), _rsas.size());
	if(rv < 0) {
		return last_socket_error();
	}
	rv = listen(descriptor(), _listencnt);
	if(rv < 0){
		return last_socket_error();
	}
	return last_socket_error();
#endif
}

ErrorCodeT SocketDevice::accept(SocketDevice &_dev, bool &_rcan_retry){
#ifdef ON_WINDOWS
	SocketAddress sa;
	const SOCKET rv = ::accept(descriptor(), sa, &sa.sz);
	_rcan_retry = (WSAGetLastError() == WSAEWOULDBLOCK);
	_dev.Device::descriptor((HANDLE)rv);
	return last_socket_error();
#else
	SocketAddress sa;
	const int rv = ::accept(descriptor(), sa, &sa.sz);
	_rcan_retry = (errno == EAGAIN);
	_dev.Device::descriptor(rv);
	return last_socket_error();
#endif
}

ErrorCodeT SocketDevice::accept(SocketDevice &_dev){
#ifdef ON_WINDOWS
	SocketAddress sa;
	SOCKET rv = ::accept(descriptor(), sa, &sa.sz);
	_dev.Device::descriptor((HANDLE)rv);
	return last_socket_error();
#else
	SocketAddress sa;
	int rv = ::accept(descriptor(), sa, &sa.sz);
	_dev.Device::descriptor(rv);
	return last_socket_error();
#endif
}


ErrorCodeT SocketDevice::bind(const SocketAddressStub &_rsa){
	specific_error_clear();
#ifdef ON_WINDOWS
	/*int rv = */::bind(descriptor(), _rsa.sockAddr(), _rsa.size());
	return last_socket_error();
#else
	/*int rv = */::bind(descriptor(), _rsa.sockAddr(), _rsa.size());
	return last_socket_error();
#endif
}

ErrorCodeT SocketDevice::makeBlocking(){
#ifdef ON_WINDOWS
	u_long mode = 0;
	int rv = ioctlsocket(descriptor(), FIONBIO, &mode);
	if (rv != NO_ERROR){
		return last_socket_error();
	}
	return ErrorCodeT();
#else
	int flg = fcntl(descriptor(), F_GETFL);
	if(flg == -1){
		return last_socket_error();
	}
	flg &= ~O_NONBLOCK;
	/*int rv = */fcntl(descriptor(), F_SETFL, flg);
	return last_socket_error();
#endif
}


ErrorCodeT SocketDevice::makeBlocking(size_t _msec){
	specific_error_clear();
#ifdef ON_WINDOWS
	u_long mode = 0;
	int rv = ioctlsocket(descriptor(), FIONBIO, &mode);
	if (rv != NO_ERROR){
		return last_socket_error();
	}
	DWORD tout(_msec);
	rv = setsockopt(descriptor(), SOL_SOCKET, SO_RCVTIMEO, (char *) &tout, sizeof(tout));
	if (rv == SOCKET_ERROR){
		return last_socket_error();
	}
	tout = _msec;
	rv = setsockopt(descriptor(), SOL_SOCKET, SO_SNDTIMEO, (char *) &tout, sizeof(tout));
	if (rv == SOCKET_ERROR){
		return last_socket_error();
	}
	return ErrorCodeT();
#else
	int flg = fcntl(descriptor(), F_GETFL);
	if(flg == -1){
		return last_socket_error();
	}
	flg &= ~O_NONBLOCK;
	int rv = fcntl(descriptor(), F_SETFL, flg);
	if (rv < 0){
		return last_socket_error();
	}
	struct timeval timeout;      
	timeout.tv_sec = _msec / 1000;
	timeout.tv_usec = _msec % 1000;
	rv = setsockopt(descriptor(), SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
	if(rv != 0){
		return last_socket_error();
	}
	rv = setsockopt(descriptor(), SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
	if(rv != 0){
		return last_socket_error();
	}
	return ErrorCodeT();
#endif
}

ErrorCodeT SocketDevice::makeNonBlocking(){
	specific_error_clear();
#ifdef ON_WINDOWS
	u_long mode = 1;
	int rv = ioctlsocket(descriptor(), FIONBIO, &mode);
	
	if (rv == NO_ERROR){
		return ErrorCodeT();
	}
	return last_socket_error();
#else
	int flg = fcntl(descriptor(), F_GETFL);
	if(flg == -1){
		return last_socket_error();
	}
	int rv = fcntl(descriptor(), F_SETFL, flg | O_NONBLOCK);
	if(rv >= 0){
		return ErrorCodeT();
	}
	return last_socket_error();
#endif
}

 ErrorCodeT SocketDevice::isBlocking(bool &_rrv)const{
	specific_error_clear();
#ifdef ON_WINDOWS
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
#else

	const int flg = fcntl(descriptor(), F_GETFL);
	
	if(flg != -1){
		_rrv = ((flg & O_NONBLOCK) == 0);
		return ErrorCodeT();
	}
	return last_socket_error();
#endif
}

int SocketDevice::send(const char* _pb, size_t _ul, bool &_rcan_retry, ErrorCodeT &_rerr, unsigned){
#ifdef ON_WINDOWS
	return -1;
#else
	int rv = ::send(descriptor(), _pb, _ul, 0);
	_rcan_retry = (errno == EAGAIN/* || errno == EWOULDBLOCK*/);
	_rerr = last_socket_error();
	return rv;
#endif
}
int SocketDevice::recv(char *_pb, size_t _ul, bool &_rcan_retry, ErrorCodeT &_rerr, unsigned){
#ifdef ON_WINDOWS
	return -1;
#else
	int rv = ::recv(descriptor(), _pb, _ul, 0);
	_rcan_retry = (errno == EAGAIN/* || errno == EWOULDBLOCK*/);
	_rerr = last_socket_error();
	return rv;
#endif
}
int SocketDevice::send(const char* _pb, size_t _ul, const SocketAddressStub &_sap, bool &_rcan_retry, ErrorCodeT &_rerr){
#ifdef ON_WINDOWS
	return -1;
#else
	int rv = ::sendto(descriptor(), _pb, _ul, 0, _sap.sockAddr(), _sap.size());
	_rcan_retry = (errno == EAGAIN/* || errno == EWOULDBLOCK*/);
	_rerr = last_socket_error();
	return rv;
#endif
}
int SocketDevice::recv(char *_pb, size_t _ul, SocketAddress &_rsa, bool &_rcan_retry, ErrorCodeT &_rerr){
#ifdef ON_WINDOWS
	return -1;
#else
	_rsa.clear();
	_rsa.sz = SocketAddress::Capacity;
	int rv = ::recvfrom(descriptor(), _pb, _ul, 0, _rsa.sockAddr(), &_rsa.sz);
	_rcan_retry = (errno == EAGAIN/* || errno == EWOULDBLOCK*/);
	_rerr = last_socket_error();
	return rv;
#endif
}

ErrorCodeT SocketDevice::remoteAddress(SocketAddress &_rsa)const{
#ifdef ON_WINDOWS
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
#else
	_rsa.clear();
	_rsa.sz = SocketAddress::Capacity;
	int rv = getpeername(descriptor(), _rsa.sockAddr(), &_rsa.sz);
	if(!rv){
		return ErrorCodeT();
	}
	return last_socket_error();
#endif
}

ErrorCodeT SocketDevice::localAddress(SocketAddress &_rsa)const{
#ifdef ON_WINDOWS
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
#else
	_rsa.clear();
	_rsa.sz = SocketAddress::Capacity;
	int rv = getsockname(descriptor(), _rsa.sockAddr(), &_rsa.sz);
	if(!rv){
		return ErrorCodeT();
	}
	return last_socket_error();
#endif
}

ErrorCodeT SocketDevice::type(int &_rrv)const{
#ifdef ON_WINDOWS
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
#else
	int			val = 0;
	socklen_t	valsz = sizeof(int);
	int rv = getsockopt(descriptor(), SOL_SOCKET, SO_TYPE, &val, &valsz);
	if(rv == 0){
		_rrv = val;
		return ErrorCodeT();
	}
	
	return last_socket_error();
#endif
}

// SocketDevice::RetValE SocketDevice::isListening(ERROR_NS::error_code &_rerr)const{
// #ifdef ON_WINDOWS
// 	return Error;
// #else
// 	int val = 0;
// 	socklen_t valsz = sizeof(int);
// 	int rv = getsockopt(descriptor(), SOL_SOCKET, SO_ACCEPTCONN, &val, &valsz);
// 	if(rv == 0){
// 		_rerr = last_error();
// 		return val != 0 ? Success : Failure;
// 	}
// 
// 	if(this->type() == SocketInfo::Datagram){
// 		return Failure;
// 	}
// 	return Error;
// #endif
// }

ErrorCodeT SocketDevice::enableNoDelay(){
#ifdef ON_WINDOWS
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
#else
	int flag = 1;
	int rv = setsockopt(descriptor(), IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
	if(rv == 0){
		return ErrorCodeT();
	}
	return last_socket_error();
#endif
}

ErrorCodeT SocketDevice::disableNoDelay(){
#ifdef ON_WINDOWS
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
#else
	int flag = 0;
	int rv = setsockopt(descriptor(), IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
	if(rv == 0){
		return ErrorCodeT();
	}
	return last_socket_error();
#endif
}


ErrorCodeT SocketDevice::enableLinger(){
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
}

ErrorCodeT SocketDevice::disableLinger(){
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
}

ErrorCodeT SocketDevice::hasNoDelay(bool &_rrv)const{
#ifdef ON_WINDOWS
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
#else
	int			flag = 0;
	socklen_t	sz(sizeof(flag));
	int			rv = getsockopt(descriptor(), IPPROTO_TCP, TCP_NODELAY, (char*)&flag, &sz);
	if(rv == 0){
		_rrv = (flag != 0);
		return ErrorCodeT();
	}
	return last_socket_error();
#endif
}
	
ErrorCodeT SocketDevice::enableCork(){
#ifdef ON_WINDOWS
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
#elif defined(ON_LINUX)
	int flag = 1;
	int rv = setsockopt(descriptor(), IPPROTO_TCP, TCP_CORK, (char*)&flag, sizeof(flag));
	if(rv == 0){
		return ErrorCodeT();
	}
	return last_socket_error();
#else
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
#endif
}

ErrorCodeT SocketDevice::disableCork(){
#ifdef ON_WINDOWS
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
#elif defined(ON_LINUX)
	int flag = 0;
	int rv = setsockopt(descriptor(), IPPROTO_TCP, TCP_CORK, (char*)&flag, sizeof(flag));
	if(rv == 0){
		return ErrorCodeT();
	}
	return last_socket_error();
#else
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
#endif
}

ErrorCodeT SocketDevice::hasCork(bool &_rrv)const{
#ifdef ON_WINDOWS
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
#elif defined(ON_LINUX)
	int			flag = 0;
	socklen_t	sz(sizeof(flag));
	int rv = getsockopt(descriptor(), IPPROTO_TCP, TCP_CORK, (char*)&flag, &sz);
	if(rv == 0){
		_rrv = (flag != 0);
		return ErrorCodeT();
	}
	return last_socket_error();
#else
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
#endif
}

ErrorCodeT SocketDevice::sendBufferSize(int &_rsz){
#ifdef ON_WINDOWS
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
#else
	if(_rsz >= 0){
		int sockbufsz(_rsz);
		int rv = setsockopt(descriptor(), SOL_SOCKET, SO_SNDBUF, (char*)&sockbufsz, sizeof(sockbufsz));
		if(rv == 0){
			return ErrorCodeT();
		}
		return last_socket_error();
	}else{
		int 		sockbufsz(0);
		socklen_t	sz(sizeof(sockbufsz));
		int 		rv = getsockopt(descriptor(), SOL_SOCKET, SO_SNDBUF, (char*)&sockbufsz, &sz);
		
		if(rv == 0){
			_rsz = sockbufsz;
			return ErrorCodeT();
		}
		return last_socket_error();
	}
#endif
}

ErrorCodeT SocketDevice::recvBufferSize(int &_rsz){
#ifdef ON_WINDOWS
	return solid::error_make(solid::ERROR_NOT_IMPLEMENTED);
#else
	if(_rsz >= 0){
		int sockbufsz(_rsz);
		int rv = setsockopt(descriptor(), SOL_SOCKET, SO_RCVBUF, (char*)&sockbufsz, sizeof(sockbufsz));
		if(rv == 0){
			return ErrorCodeT();
		}
	}else{
		int 		sockbufsz(0);
		socklen_t	sz(sizeof(sockbufsz));
		int 		rv = getsockopt(descriptor(), SOL_SOCKET, SO_RCVBUF, (char*)&sockbufsz, &sz);
		if(rv == 0){
			_rsz = sockbufsz;
			return ErrorCodeT();
		}
	}
	return last_socket_error();
#endif
}

}//namespace solid

