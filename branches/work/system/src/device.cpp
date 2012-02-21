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
	int64 off(seek(0, SeekCur));
	seek(_off);
	int rv = Device::read(_pb, _bl);
	seek(off);
	return rv;
#else
	return pread(descriptor(), _pb, _bl, _off);
#endif
}

int SeekableDevice::write(const char *_pb, uint32 _bl, int64 _off){
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

int SeekableDevice::truncate(int64 _len){
#ifdef ON_WINDOWS
	seek(_len);
	if(SetEndOfFile(descriptor())){
		return 0;
	}
	return -1;
#else
	return ::ftruncate(descriptor(), _len);
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

int FileDevice::open(const char* _fname, int _how){
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
	return ok() ? 0 : -1;
#else
	descriptor(::open(_fname, _how, 00666));
	return ok() ? 0 : -1;
#endif
}

int FileDevice::create(const char* _fname, int _how){
	return this->open(_fname, _how | CR | TR);
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
/*static*/ int Directory::create(const char *_fname){
#ifdef ON_WINDOWS
	const size_t sz(strlen(_fname));
	const size_t szex = sz + 1;
	if(szex < 256){
		WCHAR pwc[512];
		return do_create_directory(pwc, _fname, sz, 512);
	}else if(szex < 512){
		WCHAR pwc[1024];
		return do_create_directory(pwc, _fname, sz, 1024);
	}else if(szex < 1024){
		WCHAR pwc[2048];
		return do_create_directory(pwc, _fname, sz, 2048);
	}else{
		WCHAR pwc[4096];
		return do_create_directory(pwc, _fname, sz, 4096);
	}
	return -1;
#else
	return mkdir(_fname,  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
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


/*static*/ int Directory::eraseFile(const char *_fname){
#ifdef ON_WINDOWS
	const size_t sz(strlen(_fname));
	const size_t szex = sz + 1;
	if(szex < 256){
		WCHAR pwc[512];
		return do_erase_file(pwc, _fname, sz, 512);
	}else if(szex < 512){
		WCHAR pwc[1024];
		return do_erase_file(pwc, _fname, sz, 1024);
	}else if(szex < 1024){
		WCHAR pwc[2048];
		return do_erase_file(pwc, _fname, sz, 2048);
	}else{
		WCHAR pwc[4096];
		return do_erase_file(pwc, _fname, sz, 4096);
	}
	return -1;
#else
	return unlink(_fname);
#endif
}
/*static*/ int Directory::renameFile(const char *_to, const char *_from){
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
				return -1;
			}
			pwctmpto = new WCHAR[rv + 1];
			rv = MultiByteToWideChar(CP_UTF8, 0, _to, szto, pwcto, rv + 1);
			if(rv == 0){
				return -1;
			}
			pwctmpto[rv] = 0;
		}else{
			return -1;
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
				return -1;
			}
			pwctmpfr = new WCHAR[rv + 1];
			rv = MultiByteToWideChar(CP_UTF8, 0, _from, szfr, pwcfr, rv + 1);
			if(rv == 0){
				return -1;
			}
			pwctmpfr[rv] = 0;
		}else{
			return -1;
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
		return 0;
	}else{
		return -1;
	}
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
		Device::descriptor((HANDLE)invalidDescriptor());
	}
#else
	shutdownReadWrite();
	Device::close();
#endif
}
int SocketDevice::create(const SocketAddressInfoIterator &_rai){
#ifdef ON_WINDOWS
	//SOCKET s = socket(_rai.family(), _rai.type(), _rai.protocol());
	SOCKET s = WSASocket(_rai.family(), _rai.type(), _rai.protocol(), NULL, 0, 0);
	Device::descriptor((HANDLE)s);
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
	//SOCKET s = socket(_family, _type, _proto);
	SOCKET s = WSASocket(_family, _type, _proto, NULL, 0, 0);
	Device::descriptor((HANDLE)s);
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
	int yes = 1;
	int rv = setsockopt(descriptor(), SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof(yes));
	if(rv < 0) {
		edbgx(Dbg::system, "socket setsockopt: "<<strerror(errno));
		close();
		return BAD;
	}

	rv = ::bind(descriptor(), _rai.addr(), _rai.size());
	if(rv < 0) {
		edbgx(Dbg::system, "socket bind: "<<strerror(errno));
		close();
		return BAD;
	}
	rv = listen(descriptor(), _listencnt);
	if(rv < 0){
		edbgx(Dbg::system, "socket listen: "<<strerror(errno));
		close();
		return BAD;
	}
	return OK;
#else
	int yes = 1;
	int rv = setsockopt(descriptor(), SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof(yes));
	if(rv < 0) {
		edbgx(Dbg::system, "socket setsockopt: "<<strerror(errno));
		close();
		return BAD;
	}

	rv = ::bind(descriptor(), _rai.addr(), _rai.size());
	if(rv < 0) {
		edbgx(Dbg::system, "socket bind: "<<strerror(errno));
		close();
		return BAD;
	}
	rv = listen(descriptor(), _listencnt);
	if(rv < 0){
		edbgx(Dbg::system, "socket listen: "<<strerror(errno));
		close();
		return BAD;
	}
	return OK;
#endif
}
int SocketDevice::accept(SocketDevice &_dev){
#ifdef ON_WINDOWS
	if(!ok()) return BAD;
	SocketAddress sa;
	SOCKET rv = ::accept(descriptor(), sa.addr(), &sa.size(SocketAddress::Capacity));
	if (rv == invalidDescriptor()) {
		if(WSAGetLastError() == WSAEWOULDBLOCK) return NOK;
		edbgx(Dbg::system, "socket accept: "<<WSAGetLastError());
		close();
		return BAD;
	}
	_dev.Device::descriptor((HANDLE)rv);
	return OK;
#else
	if(!ok()) return BAD;
	SocketAddress sa;
	int rv = ::accept(descriptor(), sa.addr(), &sa.size());
	if (rv < 0) {
		if(errno == EAGAIN) return NOK;
		edbgx(Dbg::system, "socket accept: "<<strerror(errno));
		close();
		return BAD;
	}
	_dev.Device::descriptor(rv);
	return OK;
#endif
}
int SocketDevice::bind(const SocketAddressInfoIterator &_rai){
#ifdef ON_WINDOWS
	if(!ok()) return BAD;
	int rv = ::bind(descriptor(), _rai.addr(), _rai.size());
	if(rv < 0){
		edbgx(Dbg::system, "socket bind: "<<strerror(errno));
		close();
		return BAD;
	}
	return OK;
#else
	if(!ok()) return BAD;
	int rv = ::bind(descriptor(), _rai.addr(), _rai.size());
	if(rv < 0){
		edbgx(Dbg::system, "socket bind: "<<strerror(errno));
		close();
		return BAD;
	}
	return OK;
#endif
}
int SocketDevice::bind(const SocketAddressPair &_rsa){
#ifdef ON_WINDOWS
	if(!ok()) return BAD;
	int rv = ::bind(descriptor(), _rsa.addr, _rsa.size());
	if(rv < 0){
		edbgx(Dbg::system, "socket bind: "<<strerror(errno));
		close();
		return BAD;
	}
	return OK;
#else
	if(!ok()) return BAD;
	int rv = ::bind(descriptor(), _rsa.addr, _rsa.size());
	if(rv < 0){
		edbgx(Dbg::system, "socket bind: "<<strerror(errno));
		close();
		return BAD;
	}
	return OK;
#endif
}
int SocketDevice::makeBlocking(int _msec){
#ifdef ON_WINDOWS
	if(!ok()) return BAD;
	u_long mode = 0;
	int rv = ioctlsocket(descriptor(), FIONBIO, &mode);
	if (rv != NO_ERROR){
		edbgx(Dbg::system, "socket ioctlsocket fionbio: "<<WSAGetLastError());
		close();
		return BAD;
	}
	if(_msec < 0){
		return OK;
	}
	DWORD tout(_msec);
	rv = setsockopt(descriptor(), SOL_SOCKET, SO_RCVTIMEO, (char *) &tout, sizeof(tout));
	if (rv == SOCKET_ERROR){
		edbgx(Dbg::system, "error setsockopt rcvtimeo: "<<WSAGetLastError());
		close();
		return BAD;
	}
	tout = _msec;
	rv = setsockopt(descriptor(), SOL_SOCKET, SO_SNDTIMEO, (char *) &tout, sizeof(tout));
	if (rv == SOCKET_ERROR){
		edbgx(Dbg::system, "error setsockopt sndtimeo: "<<WSAGetLastError());
		close();
		return BAD;
	}
	return OK;
#else
	if(!ok()) return BAD;
	int flg = fcntl(descriptor(), F_GETFL);
	if(flg == -1){
		edbgx(Dbg::system, "socket fcntl getfl: "<<strerror(errno));
		close();
		return BAD;
	}
	flg &= ~O_NONBLOCK;
	int rv = fcntl(descriptor(), F_SETFL, flg);
	if (rv < 0){
		edbgx(Dbg::system, "error fcntl setfl: "<<strerror(errno));
		close();
		return BAD;
	}
	return OK;
#endif
}
int SocketDevice::makeNonBlocking(){
#ifdef ON_WINDOWS
	if(!ok()) return BAD;
	u_long mode = 1;
	int rv = ioctlsocket(descriptor(), FIONBIO, &mode);
	if (rv != NO_ERROR){
		edbgx(Dbg::system, "socket fcntl ioctlsocket "<<WSAGetLastError());
		close();
		return BAD;
	}
	return OK;
#else
	if(!ok()) return BAD;
	int flg = fcntl(descriptor(), F_GETFL);
	if(flg == -1){
		edbgx(Dbg::system, "socket fcntl getfl: "<<strerror(errno));
		close();
		return BAD;
	}
	int rv = fcntl(descriptor(), F_SETFL, flg | O_NONBLOCK);
	if(rv < 0){
		edbgx(Dbg::system, "socket fcntl setfl: "<<strerror(errno));
		close();
		return BAD;
	}
	return OK;
#endif
}
#ifdef ON_WINDOWS
#else
bool SocketDevice::isBlocking(){
	int flg = fcntl(descriptor(), F_GETFL);
	if(flg == -1){
		edbgx(Dbg::system, "socket fcntl getfl: "<<strerror(errno));
		close();
		return false;
	}
	return !(flg & O_NONBLOCK);
}
#endif
bool SocketDevice::shouldWait()const{
#ifdef ON_WINDOWS
	return WSAGetLastError() == WSAEWOULDBLOCK;
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

