/* Declarations file storagemanager.h
	
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

#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "utility/streamptr.h"

#include "clientserver/core/object.h"

#include "common.h"

class IStream;
class OStream;
class IOStream;

class Mutex;

namespace clientserver{
class File;

struct FileController{
	virtual int afterRead(int64 _from, uint32 _sz, const StreamFlags &_rff) = 0;
	virtual int afterWrite(int64 _at, uint32 _sz, const StreamFlags &_rff) = 0;
};

struct StorageKey;

class StorageMapper{
public:
	StorageMapper();
	virtual ~StorageMapper();
};

struct StorageKey{
	virtual ~StorageKey();
	virtual void name(string &_fname) = 0;
	virtual int find(const StorageManager &_sm) = 0;
	virtual int insert(const StorageManager &_sm) = 0;
	virtual int erase(const StorageManager &_sm) = 0;
	virtual StorageKey* clone() = 0;
};

class StorageManager: public clientserver::Object{
#ifdef USAFETY_CHECK
	
	static 
public:
	typedef std::pair<uint32, uint32> ObjUidTp;
	typedef std::pair<uint32, uint32> FileUidTp;
	
	StorageManager(uint32 _maxfcnt = 0);
	~StorageManager();
	
	int execute(ulong _evs, TimeSpec &_rtout);
	
	int64 fileSize(const char *_fn)const;
	template <class T>
	void registerMapper(T *_pm){
		doRegisterMapper(static_cast<StorageMapper*>(_pm));
	}
	
	template <class T>
	T& mapper(){
		return *static_cast<T*>(doGetMapper());
	}
	
	int stream(StreamPtr<IStream> &_sptr, const ObjUidTp &_robjuid, const char *_fn = NULL, uint32 _flags = 0);
	int stream(StreamPtr<OStream> &_sptr, const ObjUidTp &_robjuid, const char *_fn = NULL, uint32 _flags = 0);
	int stream(StreamPtr<IOStream> &_sptr, const ObjUidTp &_robjuid, const char *_fn = NULL, uint32 _flags = 0);
	
	int stream(StreamPtr<IStream> &_sptr, FileUidTp &_rfuid, const ObjUidTp &_robjuid, const char *_fn = NULL, uint32 _flags = 0);
	int stream(StreamPtr<OStream> &_sptr, FileUidTp &_rfuid, const ObjUidTp &_robjuid, const char *_fn = NULL, uint32 _flags = 0);
	int stream(StreamPtr<IOStream> &_sptr, FileUidTp &_rfuid, const ObjUidTp &_robjuid, const char *_fn = NULL, uint32 _flags = 0);
	
	int registerController(const FileUidTp &_rfuid, FileController* _pfc);
	int unregisterController(const FileUidTp &_rfuid, FileController* &_rpfc);
	int unregisterController(const FileUidTp &_rfuid);
	
	int setFileTimeout(const FileUidTp &_rfuid, const TimeSpec &_rtout);
	
	//overload from object
	void mutex(Mutex *_pmut);
	void release(File &_rf);
	int doUseFreeQueue(File* &_rpf, const char *_fn);
protected:
	virtual void sendStream(StreamPtr<IStream> &_sptr, const FileUidTp &_rfuid, const ObjUidTp& _robjuid) = 0;
	virtual void sendStream(StreamPtr<OStream> &_sptr, const FileUidTp &_rfuid, const ObjUidTp& _robjuid) = 0;
	virtual void sendStream(StreamPtr<IOStream> &_sptr, const FileUidTp &_rfuid, const ObjUidTp& _robjuid) = 0;
	virtual void sendError(const ObjUidTp& _robjuid);
private:
	int doStream(StreamPtr<IStream> &_sptr, FileUidTp *_pfuid, const ObjUidTp &_robjuid, const char *_fn, uint32 _flags);
	int doStream(StreamPtr<OStream> &_sptr, FileUidTp *_pfuid, const ObjUidTp &_robjuid, const char *_fn, uint32 _flags);
	int doStream(StreamPtr<IOStream> &_sptr, FileUidTp *_pfuid, const ObjUidTp &_robjuid, const char *_fn, uint32 _flags);
	
	int execute();
	struct Data;
	Data	&d;
};


int StorageManager::stream(StreamPtr<IStream> &_sptr, const ObjUidTp &_robjuid, const char *_fn, uint32 _flags){
	return doStream(_sptr, NULL, _robjuid, _fn, _flags);
}

int StorageManager::stream(StreamPtr<OStream> &_sptr, const ObjUidTp &_robjuid, const char *_fn, uint32 _flags){
	return doStream(_sptr, NULL, _robjuid, _fn, _flags);
}

int StorageManager::stream(StreamPtr<IOStream> &_sptr, const ObjUidTp &_robjuid, const char *_fn, uint32 _flags){
	return doStream(_sptr, NULL, _robjuid, _fn, _flags);
}

int StorageManager::stream(StreamPtr<IStream> &_sptr, FileUidTp &_rfuid, const ObjUidTp &_robjuid, const char *_fn, uint32 _flags){
	return doStream(_sptr, &_rfuid, _robjuid, _fn, _flags);
}
int StorageManager::stream(StreamPtr<OStream> &_sptr, FileUidTp &_rfuid, const ObjUidTp &_robjuid, const char *_fn, uint32 _flags){
	return doStream(_sptr, &_rfuid, _robjuid, _fn, _flags);
}
int StorageManager::stream(StreamPtr<IOStream> &_sptr, FileUidTp &_rfuid, const ObjUidTp &_robjuid, const char *_fn, uint32 _flags){
	return doStream(_sptr, &_rfuid, _robjuid, _fn, _flags);
}


}//namespace clientserver
#endif
