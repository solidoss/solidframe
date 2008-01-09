/* Implementation file storagemanager.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <vector>
#include <queue>
#include <map>
#include <iostream>

#include "system/debug.h"

#include "utility/sharedcontainer.h"

#include "core/storagemanager.h"
#include "utility/iostream.h"
#include "system/timespec.h"
#include "system/filedevice.h"
#include "system/synchronization.h"
#include "utility/queue.h"
#include "core/server.h"
#include "core/common.h"
#include <string>

namespace cs = clientserver;
using namespace std;
typedef std::string	String;

namespace clientserver{
//	Local Declarations	========================================================
struct LessStrCmp{
	inline bool operator()(const char* const& _s1, const char* const& _s2)const{
		return strcmp(_s1, _s2) < 0;
	}
};
//------------------------------------------------------------------------------
class File: protected FileDevice{
public:
	File(const char* _fn, Mutex &_mtx, uint32 _idx):fn(_fn), mtx(_mtx), usecnt(0), idx(_idx){}
	~File();
	int open();
	int read(char *, uint32, int64);
	int write(const char *, uint32, int64);
	const std::string& name()const{return fn;}
	void name(const char*_str);
	int release();
	int close();
	uint32 index()const{return idx;}
	int stream(StreamPtr<IStream> &_sptr);
	int stream(StreamPtr<OStream> &_sptr);
	int stream(StreamPtr<IOStream> &_sptr);
	int64 size()const;
	int useCount()const;
private:
	std::string	fn;
	Mutex	&mtx;
	int		usecnt;
	uint32	idx;
};
//------------------------------------------------------------------------------
class FileIStream: public IStream{
public:
	FileIStream(File &_rf):rf(_rf),off(0){}
	int read(char *, uint32, uint32 _flags = 0);
	int release();
	int64 seek(int64, SeekRef);
	int64 size()const;
private:
	File 	&rf;
	int64	off;
};
//------------------------------------------------------------------------------
class FileOStream: public OStream{
public:
	FileOStream(File &_rf):rf(_rf),off(0){}
	int write(const char *, uint32, uint32 _flags = 0);
	int release();
	int64 seek(int64, SeekRef);
	int64 size()const;
private:
	File 	&rf;
	int64	off;
};
//------------------------------------------------------------------------------
class FileIOStream: public IOStream{
public:
	FileIOStream(File &_rf):rf(_rf),off(0){}
	~FileIOStream();
	int read(char *, uint32, uint32 _flags = 0);
	int write(const char *, uint32, uint32 _flags = 0);
	int release();
	int64 seek(int64, SeekRef);
	int64 size()const;
private:
	File 	&rf;
	int64	off;
};
//------------------------------------------------------------------------------
struct StorageManager::Data{
	typedef SharedContainer<Mutex>	MutexContainer;
	
	Data(uint32 _maxsz, uint32 _sz):maxsz(_maxsz),sz(_sz), mut(NULL){}
	const uint32	maxsz;
	uint32			sz;
	Mutex			*mut;
	MutexContainer	mutpool;
};
//	StorageManager definitions ==================================================
StorageManager::StorageManager(uint32 _maxfcnt){
}

StorageManager::~StorageManager(){
}

int StorageManager::execute(ulong _evs, TimeSpec &_rtout){
}

int64 StorageManager::fileSize(const char *_fn)const{
}
	
int StorageManager::registerController(const FileUidTp &_rfuid, FileController* _pfc){
}
int StorageManager::unregisterController(const FileUidTp &_rfuid, FileController* &_rpfc){
}
int StorageManager::unregisterController(const FileUidTp &_rfuid){
}

int StorageManager::setFileTimeout(const FileUidTp &_rfuid, const TimeSpec &_rtout){
}

//overload from object
void StorageManager::mutex(Mutex *_pmut){
	d.mut = _pmut;
}

void StorageManager::release(File &_rf){
}

int StorageManager::doUseFreeQueue(File* &_rpf, const char *_fn){
}

int StorageManager::doStream(StreamPtr<IStream> &_sptr, FileUidTp *_pfuid, const ObjUidTp &_robjuid, const char *_fn, uint32 _flags){

}

int StorageManager::doStream(StreamPtr<OStream> &_sptr, FileUidTp *_pfuid, const ObjUidTp &_robjuid, const char *_fn, uint32 _flags){
}

int StorageManager::doStream(StreamPtr<IOStream> &_sptr, FileUidTp *_pfuid, const ObjUidTp &_robjuid, const char *_fn, uint32 _flags){

}

int StorageManager::execute(){
	assert(false);
	return BAD;
}


}//namespace clientserver
