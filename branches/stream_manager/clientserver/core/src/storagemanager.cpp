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
	enum {
		NotReady,
		Ready,//opened
	};
public:
	File(StorageKey &_rk, uint32 _flags = 0):rk(_rk), ousecnt(0), iusecnt(0), flags(_flags & 0xffff), state(NotReady){}
	~File();
	int open(StorageManager &_rsm, uint32 _flags);
	int read(char *, uint32, int64);
	int write(const char *, uint32, int64);
	int stream(StreamPtr<IStream> &_sptr, const StorageManager::ObjUidTp &_roi, uint32 _flags);
	int stream(StreamPtr<OStream> &_sptr, const StorageManager::ObjUidTp &_roi, uint32 _flags);
	int stream(StreamPtr<IOStream> &_sptr, const StorageManager::ObjUidTp &_roi, uint32 _flags);
	int64 size()const;
	bool decreaseOutCount();
	bool decreaseInCount();
	void execute();
private:
	struct WaitData{
		WaitData(uint32 _objidx, uint32 _objuid, uint32 _flags):objidx(_objidx), objuid(_objuid), flags(_flags){}
		uint32	objidx;
		uint32	objuid;
		uint32	flags;
	};
	typedef Queue<WaitData>		WaitQueueTp;
	StorageKey		&rk;
	uint32			ousecnt;
	uint32			iusecnt;
	uint16			flags;
	uint16			state;
	WaitQueueTp		iwq;
	WaitQueueTp		owq;
	TimeSpec 		tout;
};
//------------------------------------------------------------------------------
class FileIStream: public IStream{
public:
	FileIStream(StorageManager::Data &_rd):rd(_rd),off(0){}
	int read(char *, uint32, uint32 _flags = 0);
	int release();
	int64 seek(int64, SeekRef);
	int64 size()const;
private:
	StorageManager::Data	&rd;
	uint					fileid;
	int64					off;
};
//------------------------------------------------------------------------------
class FileOStream: public OStream{
public:
	FileOStream(StorageManager::Data &_rd):rd(_rd),off(0){}
	int write(const char *, uint32, uint32 _flags = 0);
	int release();
	int64 seek(int64, SeekRef);
	int64 size()const;
private:
	StorageManager::Data	&rd;
	uint					fileid;
	int64					off;
};
//------------------------------------------------------------------------------
class FileIOStream: public IOStream{
public:
	FileIOStream(StorageManager::Data &_rd):rd(_rd),off(0){}
	~FileIOStream();
	int read(char *, uint32, uint32 _flags = 0);
	int write(const char *, uint32, uint32 _flags = 0);
	int release();
	int64 seek(int64, SeekRef);
	int64 size()const;
private:
	StorageManager::Data	&rd;
	uint					fileid;
	int64					off;
};
//------------------------------------------------------------------------------
struct StorageManager::Data{
	typedef SharedContainer<Mutex>		MutexContainer;
	typedef Queue<uint>					FreeQueueTp;
	typedef Queue<uint>					SignalQueueTp;
	typedef std::pair<File*, uint32>	FilePairTp;
	typedef std::deque<FilePairTp>		FileVectorTp;
	Data(uint32 _maxsz, uint32 _sz):maxsz(_maxsz),sz(_sz), mut(NULL){}
	int  fileWrite(const char *_pb, uint32 _bl, uint32 _flags);
	int  fileRead(uint _fileid, const char *_pb, uint32 _bl, uint32 _flags);
	int  fileSize(uint _fileid);
	const uint32	maxsz;
	uint32			sz;
	Mutex			*mut;
	MutexContainer	mutpool;
	FileVectorTp	fv;//file vector
	SignalQueueTp	sq;//stream queue
	SignalQueueTp	oq;//open queue
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

int StorageManager::stream(
	StreamPtr<IStream> &_sptr,
	const ObjUidTp &_robjuid,
	const char *_fn,
	uint32 _flags
){
	FileUidTp fuid;
	return stream(_sptr, fuid, _robjuid, fn, _flags);
}

int StorageManager::stream(
	StreamPtr<OStream> &_sptr,
	const ObjUidTp &_robjuid,
	const char *_fn,
	uint32 _flags
){
	FileUidTp fuid;
	return stream(_sptr, fuid, _robjuid, fn, _flags);
}

int StorageManager::stream(
	StreamPtr<IOStream> &_sptr,
	const ObjUidTp &_robjuid,
	const char *_fn,
	uint32 _flags
){
	FileUidTp fuid;
	return stream(_sptr, fuid, _robjuid, fn, _flags);
}

int StorageManager::stream(
	StreamPtr<IStream> &_sptr,
	FileUidTp &_rfuid,
	const ObjUidTp &_robjuid,
	const char *_fn,
	uint32 _flags
){
	if(_fn){
		FastNameStorageKey k(_fn);
		return stream(_sptr, _rfuid, _robjuid, k, _flags);
	}else{
		TempStorageKey k;
		return stream(_sptr, _rfuid, _robjuid, k, _flags);
	}
}

int StorageManager::stream(
	StreamPtr<OStream> &_sptr,
	FileUidTp &_rfuid,
	const ObjUidTp &_robjuid,
	const char *_fn,
	uint32 _flags
){
	if(_fn){
		FastNameStorageKey k(_fn);
		return stream(_sptr, _rfuid, _robjuid, k, _flags);
	}else{
		TempStorageKey k;
		return stream(_sptr, _rfuid, _robjuid, k, _flags);
	}
}

int StorageManager::stream(
	StreamPtr<IOStream> &_sptr,
	FileUidTp &_rfuid,
	const ObjUidTp &_robjuid,
	const char *_fn,
	uint32 _flags
){
	if(_fn){
		FastNameStorageKey k(_fn);
		return stream(_sptr, _rfuid, _robjuid, k, _flags);
	}else{
		TempStorageKey k;
		return stream(_sptr, _rfuid, _robjuid, k, _flags);
	}
}

int StorageManager::stream(
	StreamPtr<IStream> &_sptr,
	FileUidTp &_rfuid,
	const ObjUidTp &_robjuid,
	const StorageKey &_rk,
	uint32 _flags
){
	//TODO:
}
int StorageManager::stream(
	StreamPtr<OStream> &_sptr,
	FileUidTp &_rfuid,
	const ObjUidTp &_robjuid,
	const StorageKey &_rk,
	uint32 _flags
){
	//TODO:
}

/*
	NOTE:
	The tmp file creation will be handled by TempStorageKey
	which:
		- on find will allways return -1
		-
*/
int StorageManager::stream(
	StreamPtr<IOStream> &_sptr,
	FileUidTp &_rfuid,
	const ObjUidTp &_robjuid,
	const StorageKey &_rk,
	uint32 _flags
){	
	Mutex::Locker	lock(*d.mut);
	int fid = _rk.find(*this);
	if(fid >= 0){//the file is already open
		//try to get a stream from it.
		switch(d.fv[fid].first->stream(*this, fid, _sptr, _robjuid, _flags)){
			case BAD:
				//we cant get the stream - not now and not ever
				return BAD;
			case OK:
				_rfuid.first = fid;
				_rfuid.second = d.fv[fid].second;
				return OK;
			case NOK:
				//we should wait for the stream
				return NOK;
		}
	}else{//the file is not open
		//first create the file object:
		unsigned pos = d.createFilePosition();
		d.fv[pos].first = new File(_rk.clone(), flags);
		//now try to open the file:
		if(d.fcnt < d.maxfcnt){
			switch(d.fv[pos].open(*this)){
				case BAD:
					//unable to open file - not now not ever
					delete d.fv[pos].first;
					d.fv[pos].first = NULL;
					d.collectFilePosition(pos);
					return BAD;
				case OK:{
					//register into a mapper
					d.fv[pos].first->key().insert(*this);
					_rfuid.first = pos;
					_rfuid.second = d.fv[pos].second;
					d.fv[pos].first->stream(*this, pos, _sptr, _robjuid, _flags);//MUST NOT FAIL
					assert(_sptr.ptr());
					return OK;
				}
				case NOK:{
					//we cant open file for now - no more fds etc
					//register into a mapper
					d.fv[pos].first->key().insert(*this);
					_rfuid.first = pos;
					_rfuid.second = d.fv[pos].second;
					//Mutex::Locker	lock2(d.mutpool.object(_rfuid.first));
					d.fv[pos].first->stream(*this, pos, _sptr, _robjuid, _flags);//WILL FAIL
					d.fq.push(pos);
					return NOK;
				}
			}
		}else{
			//register into a mapper
			d.fv[pos]->key().insert(*this);
			_rfuid.first = pos;
			_rfuid.second = d.fv[pos].second;
			//Mutex::Locker	lock2(d.mutpool.object(pos));
			d.fv[pos].first->stream(*this, pos, _sptr, _robjuid, _flags);//WILL FAIL
			d.oq.push(pos);
			return NOK;
		}
	}
	assert(false);
	return BAD;
}

int StorageManager::stream(StreamPtr<IStream> &_sptr, const FileUidTp &_rfuid, const ObjUidTp &_robjuid, uint32 _flags){
	Mutex::Locker	lock(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].second == _rfuid.second){
		assert(d.fv[_rfuid.first].first);
		return d.fv[_rfuid.first].first->stream(*this, _rfuid.first, _sptr, _robjuid, _flags);
	}
	return BAD;
}
int StorageManager::stream(StreamPtr<OStream> &_sptr, const FileUidTp &_rfuid, const ObjUidTp &_robjuid, uint32 _flags){
	Mutex::Locker	lock(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].second == _rfuid.second){
		assert(d.fv[_rfuid.first].first);
		return d.fv[_rfuid.first].first->stream(*this, _rfuid.first, _sptr, _robjuid, _flags);
	}
	return BAD;
}
int StorageManager::stream(StreamPtr<IOStream> &_sptr, const FileUidTp &_rfuid, const ObjUidTp &_robjuid, uint32 _flags){
	Mutex::Locker	lock(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].second == _rfuid.second){
		assert(d.fv[_rfuid.first].first);
		return d.fv[_rfuid.first].first->stream(*this, _rfuid.first, _sptr, _robjuid, _flags);
	}
	return BAD;
}


void StorageManager::releaseIStream(uint _fileid){
}
void StorageManager::releaseOStream(uint _fileid){
}
void StorageManager::releaseIOStream(uint _fileid){
}

int StorageManager::doRegisterMapper(StorageMapper *_pm){
	d.mv.push_back(_pm);
	return d.mv.size() - 1;
}

StorageMapper* StorageManager::doGetMapper(unsigned _id, StorageMapper *_pm){
	if(!_pm){
		assert(_id < d.mv.size() && d.mv[_i]);
		return d.mv[_id];
	}else{
		if(_id < d.mv.size()){
			unsigned oldsize = d.mv.size();
			d.mv.resize(_id + 1);
			for(unsigned i = oldsize; i < _id; ++i){
				d.mv[i] = NULL;
			}
			d.mv[_id] = _pm;
			return _pm;
		}
	}
}

int StorageManager::execute(){
	assert(false);
	return BAD;
}
//--------------------------------------------------------------------------
// StorageManager::Data
int StorageManager::Data::fileWrite(uint _fileid, const char *_pb, uint32 _bl, uint32 _flags){
	Mutex::Locker lock(mutpool.object(_fileid));
	return fv[_fileid].first->write(_pb, _bl, _flags);
}
int StorageManager::Data::fileRead(uint _fileid, const char *_pb, uint32 _bl, uint32 _flags){
	Mutex::Locker lock(mutpool.object(_fileid));
	return fv[_fileid].first->read(_pb, _bl, _flags);
}
int StorageManager::Data::fileSize(uint _fileid){
	Mutex::Locker lock(mutpool.object(_fileid));
	return fv[_fileid].first->size();
}

//--------------------------------------------------------------------------
// FileIStream

int FileIStream::read(char * _pb, uint32 _bl, uint32 _flags){
	return rd.fileRead(fileid, _pb, _bl, _flags);
}

int FileIStream::release(){	
	Server::the().storage().releaseIStream(fileid);
	return -1;
}

int64 FileIStream::seek(int64 _off, SeekRef _ref){
	assert(_ref == SeekBeg);
	off = _off;
	return off;
}
int64 FileIStream::size()const{
	rd.fileSize(fileid);
}

//--------------------------------------------------------------------------
// FileOStream
int  FileOStream::write(const char *_pb, uint32 _bl, uint32 _flags){
	return rd.fileWrite(fileid, _pb, _bl, _flags);
}

int FileOStream::release(){
	Server::the().storage().releaseOStream(fileid);
	return -1;
}

int64 FileOStream::seek(int64 _off, SeekRef _ref){
	assert(_ref == SeekBeg);
	off = _off;
	return off;
}

int64 FileOStream::size()const{
	rd.fileSize(fileid);
}

//--------------------------------------------------------------------------
// FileIOStream

int FileIOStream::read(char * _pb, uint32 _bl, uint32 _flags){
	return rd.fileRead(fileid, _pb, _bl, _flags);
}

int  FileIOStream::write(const char *_pb, uint32 _bl, uint32 _flags){
	return rd.fileWrite(fileid, _pb, _bl, _flags);
}

int FileIOStream::release(){
	Server::the().storage().releaseIOStream(fileid);
	return -1;
}

int64 FileIOStream::seek(int64 _off, SeekRef _ref){
	assert(_ref == SeekBeg);
	off = _off;
	return off;
}

int64 FileIOStream::size()const{
	rd.fileSize(fileid);
}

//--------------------------------------------------------------------------
// File

File::~File(){
	if(rk.release()){
		delete &rk;
	}
}

int File::open(StorageManager &_rsm, uint _fileid){
	//TODO: do it right/better
	string fn;
	rk.fileName(_rsm, fileid, fn);
	if(FileDevice::open(fn.c_str(), FileDevice::RW) < 0){
		if(canRetryOpen()) return NOK;//will return true if errno is ENFILE or ENOMEM
		if(flags & StorageManager::Create){
			if(FileDevice::create(fn.c_str(), FileDevice::RW)){
				if(canRetryOpen()) return NOK;//will return true if errno is ENFILE or ENOMEM
				return BAD;
			}else{
				return OK;
			}
		}
	}
	return OK;
}

int File::stream(StorageManager& _rsm, uint _fileid, StreamPtr<IStream> &_sptr, const StorageManager::ObjUidTp &_roi, uint32 _flags){
	{
		Mutex::Locker lock(_rsm.d.mutpool.object(_fileid));
		if(state < Ready || ousecnt || owq.size()){//no writers and no writers waiting
			iwq.push(WaitData(_roi.first, _roi.second, _flags));
			return NOK;
		}
		++iusecnt;
	}
	_sptr = new FileIStream(_rsm.d, _fileid);
	return OK;
}

int File::stream(StorageManager& _rsm, uint _fileid, StreamPtr<OStream> &_sptr, const StorageManager::ObjUidTp &_roi, uint32 _flags){
	{
		Mutex::Locker lock(_rsm.d.mutpool.object(_fileid));
		if(state < Ready || ousecnt || isusecnt || owq.size()){
			owq.push(WaitData(_roi.first, _roi.second, _flags));
			return NOK;
		}
		++ousecnt;
	}
	_sptr = new FileOStream(_rsm.d, _fileid);
	return OK;
}

int File::stream(StorageManager& _rsm, uint _fileid, StreamPtr<IOStream> &_sptr, const StorageManager::ObjUidTp &_roi, uint32 _flags){
	{
		Mutex::Locker lock(_rsm.d.mutpool.object(_fileid));
		if(state < Ready || ousecnt || isusecnt || owq.size()){
			owq.push(WaitData(_roi.first, _roi.second, _flags | StorageManager::IOStreamFlag));
			return NOK;
		}
		++ousecnt;
	}
	_sptr = new FileIOStream(_rsm.d, _fileid);
	return OK;
}

// int64 File::size()const{
// }

bool File::decreaseOutCount(){
	--ousecnt;
	if(!ousecnt){
		return iwq.size() || owq.size() || (owq.empty() && iwq.empty());//return true if there are waiting readers or writers or noone waiting
	}
	return false;
}

bool File::decreaseInCount(){
	--iusecnt;
	if(!iusecnt){
		return owq.size() || (owq.empty() && iwq.empty());//return true if there are waiting writers or noone waiting
	}
	return false;
}

int File::execute(StorageManager &_rsm, const FileUidTp &_fuid, uint32 _flags, const TimeSpec &_rtout){
	bool inuse = false;
	Mutex &m(_rsm.d.mutpool.object(_fuid.first));
	//if(_flags & Timeout) return BAD;
	m.lock();//we really need smart locking!!!
	switch(state){
		case NotReady://we must try to open the file
			m.unlock();
			//open can take a long time
			switch(this->open(_rsm, _fuid.first)){
				case BAD:return BAD;
				case OK:
					tout = 0;
					m.lock()
					state = Ready;
					m.unlock();
					return OK;//no timeout
				case NOK:
					tout = _rtout + 1000 * 30;//30 sec timeout
					return NOK;
			}
			assert(false);
			break;
		case Ready:
			inuse = ousecnt || iusecnt;
			if(owq.size()){
				if(owq.front().flags & StorageManager::IOStreamFlag){
					++ousecnt;
					StorageManager::ObjUidTp	ouid(owq.front.objuid);
					owq.pop();
					m.unlock();
					inuse = true;
					StreamPtr<IOStream> sp(new FileIOStream(_rsm.d, _fuid.first));
					_rsm.sendStream(sp, _fuid, ouid);//send stream must be 
				}else{
					++ousecnt;
					StorageManager::ObjUidTp	ouid(owq.front.objuid);
					owq.pop();
					m.unlock();
					inuse = true;
					StreamPtr<OStream> sp(new FileOStream(_rsm.d, _fuid.first));
					_rsm.sendStream(sp, _fuid, ouid);
				}
			}else{
				while(iwq.size()){
					StorageManager::ObjUidTp	ouid(iwq.front.objuid);
					++iusecnt;
					iwq.pop();
					m.unlock()
					inuse = true;;
					StreamPtr<IStream> sp(new FileIStream(_rsm.d, _fuid.first));
					_rsm.sendStream(sp, _fuid, ouid);
					m.lock();
				}
				m.unlock();
			}
	}
	return OK;
}


}//namespace clientserver
