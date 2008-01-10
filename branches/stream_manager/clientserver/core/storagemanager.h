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
struct StorageMapper;
/*!
	NOTE:
	The mapper method is a little bit more complex than it should be (doGetMapper must also register a mapper)
	because I want to be able to have more than one instance of StorageManager per process. It is not a really
	usefull request, but as long as one can have multiple servers per process it is at least nice to be able to
	have multiple storage managers.
*/

class StorageManager: public clientserver::Object{
	
public:
	typedef std::pair<uint32, uint32> ObjUidTp;
	typedef std::pair<uint32, uint32> FileUidTp;
	
	StorageManager(uint32 _maxfcnt = 0);
	~StorageManager();
	
	int execute(ulong _evs, TimeSpec &_rtout);
	
	int64 fileSize(const char *_fn)const;
	template <class T>
	T* mapper(T *_pm = NULL){
		//doRegisterMapper will assert if _pm is NULL
		static int id(doRegisterMapper(static_cast<StorageMapper*>(_pm)));
		//doGetMapper may also register the mapper - this way one can instantiate more managers per process.
		return static_cast<T*>(doGetMapper(id, static_cast<StorageMapper*>(_pm)));
	}
	
	int stream(StreamPtr<IStream> &_sptr, const ObjUidTp &_robjuid, const char *_fn = NULL, uint32 _flags = 0);
	int stream(StreamPtr<OStream> &_sptr, const ObjUidTp &_robjuid, const char *_fn = NULL, uint32 _flags = 0);
	int stream(StreamPtr<IOStream> &_sptr, const ObjUidTp &_robjuid, const char *_fn = NULL, uint32 _flags = 0);
	
	int stream(StreamPtr<IStream> &_sptr, FileUidTp &_rfuid, const ObjUidTp &_robjuid, const char *_fn = NULL, uint32 _flags = 0);
	int stream(StreamPtr<OStream> &_sptr, FileUidTp &_rfuid, const ObjUidTp &_robjuid, const char *_fn = NULL, uint32 _flags = 0);
	int stream(StreamPtr<IOStream> &_sptr, FileUidTp &_rfuid, const ObjUidTp &_robjuid, const char *_fn = NULL, uint32 _flags = 0);
	
	int stream(StreamPtr<IStream> &_sptr, FileUidTp &_rfuid, const ObjUidTp &_robjuid, const StorageKey &_rk, uint32 _flags = 0);
	int stream(StreamPtr<OStream> &_sptr, FileUidTp &_rfuid, const ObjUidTp &_robjuid, const StorageKey &_rk, uint32 _flags = 0);
	int stream(StreamPtr<IOStream> &_sptr, FileUidTp &_rfuid, const ObjUidTp &_robjuid, const StorageKey &_rk, uint32 _flags = 0);
	
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
	int doRegisterMapper(StorageMapper *_pm);
	StorageMapper* doGetMapper(int _id, StorageMapper *_pm);
	int execute();
	struct Data;
	Data	&d;
};


}//namespace clientserver
#endif
