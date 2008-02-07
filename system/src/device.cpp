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
#include "filedevice.h"
#include "directory.h"
#include "system/cassert.h"
#include <cerrno>

Device::Device(const Device &_dev):desc(_dev.descriptor()) {
	_dev.desc = -1;
}
Device& Device::operator&(Device &_dev){
	close();
	desc = _dev.descriptor();_dev.descriptor(-1);
	return *this;
}
Device::Device(int _desc):desc(_desc){}

Device::~Device(){
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
	if(desc > 0) ::close(desc);
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
	fstat(descriptor(), &st);
	return st.st_size;
}
bool FileDevice::canRetryOpen()const{
	return (errno == EMFILE) || (errno == ENOMEM);
}
//-- Directory -------------------------------------
/*static*/ int Directory::create(const char *_path){
	return mkdir(_path,  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

/*static*/ int Directory::eraseFile(const char *_path){
	return unlink(_path);
}
