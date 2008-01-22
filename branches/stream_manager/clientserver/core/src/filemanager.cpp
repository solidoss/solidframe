/* Implementation file filemanager.cpp
	
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
#include <string>

#include "system/debug.h"
#include "system/timespec.h"
#include "system/filedevice.h"
#include "system/synchronization.h"
#include "system/timespec.h"

#include "utility/sharedcontainer.h"
#include "utility/iostream.h"
#include "utility/queue.h"
#include "utility/stack.h"

#include "core/filemanager.h"
#include "core/server.h"
#include "core/common.h"
#include "core/filekeys.h"
#include "core/filemapper.h"

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

typedef FileManager::RequestUid RequestUidTp;

//------------------------------------------------------------------------------
class File: public FileDevice{
	enum {
		NotOpened = 0,
		Opened,
		Timeout = 1,
	};
public:
	File(FileKey &_rk, uint32 _flags = 0):rk(_rk), ousecnt(0), iusecnt(0), flags(_flags & 0xffff), state(NotOpened){}
	~File();
	FileKey &key(){return rk;}
	int open(FileManager &_rsm, uint32 _flags);
// 	int read(char *, uint32, int64);
// 	int write(const char *, uint32, int64);
	int stream(FileManager& _rsm, uint _fileid, StreamPtr<IStream> &_sptr, const RequestUidTp &_roi, uint32 _flags);
	int stream(FileManager& _rsm, uint _fileid, StreamPtr<OStream> &_sptr, const RequestUidTp &_roi, uint32 _flags);
	int stream(FileManager& _rsm, uint _fileid, StreamPtr<IOStream> &_sptr, const RequestUidTp &_roi, uint32 _flags);
	//int64 size()const;
	bool decreaseOutCount();
	bool decreaseInCount();
	int execute(FileManager &_rsm, const FileUidTp &_fuid, uint32 _flags, TimeSpec &_rtout);
	void clear(FileManager &_rsm, const FileUidTp &_fuid);
private:
	bool signalStreams(FileManager &_rsm, const FileUidTp &_fuid);
	struct WaitData{
		WaitData(const RequestUidTp &_rrequid, uint32 _flags):requid(_rrequid), flags(_flags){}
		RequestUidTp	requid;
		uint32			flags;
	};
	typedef Queue<WaitData>		WaitQueueTp;
	FileKey			&rk;
	uint32			ousecnt;
	uint32			iusecnt;
	uint32			msectout;
	uint16			flags;
	uint16			state;
	WaitQueueTp		iwq;
	WaitQueueTp		owq;
};
//------------------------------------------------------------------------------
class FileIStream: public IStream{
public:
	FileIStream(FileManager::Data &_rd, uint _fileid):rd(_rd), fileid(_fileid), off(0){}
	int read(char *, uint32, uint32 _flags = 0);
	int release();
	int64 seek(int64, SeekRef);
	int64 size()const;
private:
	FileManager::Data	&rd;
	uint				fileid;
	int64				off;
};
//------------------------------------------------------------------------------
class FileOStream: public OStream{
public:
	FileOStream(FileManager::Data &_rd, uint _fileid):rd(_rd), fileid(_fileid), off(0){}
	int write(const char *, uint32, uint32 _flags = 0);
	int release();
	int64 seek(int64, SeekRef);
	int64 size()const;
private:
	FileManager::Data	&rd;
	uint				fileid;
	int64				off;
};
//------------------------------------------------------------------------------
class FileIOStream: public IOStream{
public:
	FileIOStream(FileManager::Data &_rd, uint _fileid):rd(_rd), fileid(_fileid), off(0){}
	int read(char *, uint32, uint32 _flags = 0);
	int write(const char *, uint32, uint32 _flags = 0);
	int release();
	int64 seek(int64, SeekRef);
	int64 size()const;
private:
	FileManager::Data	&rd;
	uint					fileid;
	int64					off;
};
//------------------------------------------------------------------------------
struct FileManager::Data{
	enum {Running = 1, Stopping, Stopped = -1};
	typedef SharedContainer<Mutex>		MutexContainer;
	typedef Queue<uint>					SignalQueueTp;
	typedef Stack<uint>					FreeStackTp;
	struct FileData{
		FileData(File *_pfile = NULL):pfile(_pfile), toutidx(-1), uid(0){}
		File		*pfile;
		int32		toutidx;
		uint32		uid;
		TimeSpec	tout;
	};
	typedef std::deque<FileData>		FileVectorTp;
	typedef std::deque<int32>			TimeoutVectorTp;
	typedef std::vector<FileMapper*>	FileMapperVectorTp;
	Data(uint32 _maxsz):maxsz(_maxsz),sz(0), mut(NULL){}
	int  fileWrite(uint _fileid, const char *_pb, uint32 _bl, uint32 _flags);
	int  fileRead(uint _fileid, char *_pb, uint32 _bl, uint32 _flags);
	int  fileSize(uint _fileid);
	unsigned createFilePosition();
	void collectFilePosition(unsigned _pos);
	
	const uint32			maxsz;
	uint32					sz;
	Mutex					*mut;
	MutexContainer			mutpool;
	FileVectorTp			fv;//file vector
	SignalQueueTp			sq;//stream queue
	SignalQueueTp			oq;//open queue
	FileMapperVectorTp		mv;
	FreeStackTp				fs;//free stack
	TimeoutVectorTp			toutv;
};


//	FileManager definitions ==================================================
FileManager::FileManager(uint32 _maxfcnt):d(*(new Data(_maxfcnt))){
	state(Data::Running);
}

FileManager::~FileManager(){
	delete &d;
}

/*
NOTE:
	The algorithm resembles somehow the one from ConnectionSelector. But, while for connections the raport of (total number of connections) / (connections waitin for timeout) is far closer to 1 than it is in case of files i.e. fewer files will wait for timeout.
	That is why we'll complicate a little the algorithm, using toutv to hold the indexes of files waiting for timeout.
*/

int FileManager::execute(ulong _evs, TimeSpec &_rtout){
	d.mut->lock();
	if(signaled()){
		ulong sm = grabSignalMask(0);
		idbg("signalmask "<<sm);
		if(sm & cs::S_KILL){
			state(Data::Stopping);
			if(!d.sz){//no file
				d.mut->unlock();
				Server::the().removeFileManager();
				return BAD;
			}
		}
	}
	
	uint sz = 0;
	uint32	fileids[256];
	while(sz < 256 && d.sq.size()){
		fileids[sz] = d.sq.front();
		d.sq.pop();
		++sz;
	}
	d.mut->unlock();
	const uint32	*pfid = fileids;
	while(sz){
		FileUidTp fuid(*pfid, d.fv[*pfid].uid);
		Data::FileData &rf(d.fv[*pfid]);
		Mutex::Locker lock(d.mutpool.object(*pfid));
		switch(rf.pfile->execute(*this, fuid, 0, rf.tout)){
		case BAD://delete the file and collect the
			rf.pfile->clear(*this, fuid);//send errors
			delete rf.pfile;
			rf.pfile = NULL;
			d.mut->lock();
			d.collectFilePosition(*pfid);
			d.mut->unlock();
			break;
		case NOK:
			if(rf.tout == 0){
				rf.tout = 0xffffffff;
			}
		}
		++pfid;
		--sz;
	}
	if(_evs & TIMEOUT){
		//scan all files for timeout
	}
	return NOK;
}

int64 FileManager::fileSize(const char *_fn)const{
	//TODO:
}
	
int FileManager::setFileTimeout(const FileUidTp &_rfuid, const TimeSpec &_rtout){
	//TODO:
}

//overload from object
void FileManager::mutex(Mutex *_pmut){
	d.mut = _pmut;
}

int FileManager::stream(
	StreamPtr<IStream> &_sptr,
	const RequestUid &_rrequid,
	const char *_fn,
	uint32 _flags
){
	FileUidTp fuid;
	return stream(_sptr, fuid, _rrequid, _fn, _flags);
}

int FileManager::stream(
	StreamPtr<OStream> &_sptr,
	const RequestUid &_rrequid,
	const char *_fn,
	uint32 _flags
){
	FileUidTp fuid;
	return stream(_sptr, fuid, _rrequid, _fn, _flags);
}

int FileManager::stream(
	StreamPtr<IOStream> &_sptr,
	const RequestUid &_rrequid,
	const char *_fn,
	uint32 _flags
){
	FileUidTp fuid;
	return stream(_sptr, fuid, _rrequid, _fn, _flags);
}

int FileManager::stream(
	StreamPtr<IStream> &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_rrequid,
	const char *_fn,
	uint32 _flags
){
	if(_fn){
		FastNameFileKey k(_fn);
		return stream(_sptr, _rfuid, _rrequid, k, _flags);
	}else{
		TempFileKey k;
		return stream(_sptr, _rfuid, _rrequid, k, _flags);
	}
}

int FileManager::stream(
	StreamPtr<OStream> &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_rrequid,
	const char *_fn,
	uint32 _flags
){
	if(_fn){
		FastNameFileKey k(_fn);
		return stream(_sptr, _rfuid, _rrequid, k, _flags);
	}else{
		TempFileKey k;
		return stream(_sptr, _rfuid, _rrequid, k, _flags);
	}
}

int FileManager::stream(
	StreamPtr<IOStream> &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_rrequid,
	const char *_fn,
	uint32 _flags
){
	if(_fn){
		FastNameFileKey k(_fn);
		return stream(_sptr, _rfuid, _rrequid, k, _flags);
	}else{
		TempFileKey k;
		return stream(_sptr, _rfuid, _rrequid, k, _flags);
	}
}
//-------------------------------------------------------------------------------------
int FileManager::stream(StreamPtr<IStream> &_sptr, const char *_fn, uint32 _flags){
	FileUidTp	fuid;
	RequestUid	requid;
	if(_fn){
		FastNameFileKey k(_fn);
		return stream(_sptr, fuid, requid, k, _flags | NoWait);
	}else{
		TempFileKey k;
		return stream(_sptr, fuid, requid, k, _flags | NoWait);
	}
}

int FileManager::stream(StreamPtr<OStream> &_sptr, const char *_fn, uint32 _flags){
	FileUidTp	fuid;
	RequestUid	requid;
	if(_fn){
		FastNameFileKey k(_fn);
		return stream(_sptr, fuid, requid, k, _flags | NoWait);
	}else{
		TempFileKey k;
		return stream(_sptr, fuid, requid, k, _flags | NoWait);
	}
}

int FileManager::stream(StreamPtr<IOStream> &_sptr, const char *_fn, uint32 _flags){
	FileUidTp	fuid;
	RequestUid	requid;
	if(_fn){
		FastNameFileKey k(_fn);
		return stream(_sptr, fuid, requid, k, _flags | NoWait);
	}else{
		TempFileKey k;
		return stream(_sptr, fuid, requid, k, _flags | NoWait);
	}
}
//-------------------------------------------------------------------------------------
int FileManager::stream(StreamPtr<IStream> &_sptr, const FileKey &_rk, uint32 _flags){
	FileUidTp	fuid;
	RequestUid	requid;
	return stream(_sptr, fuid, requid, _rk, _flags | NoWait);
}

int FileManager::stream(StreamPtr<OStream> &_sptr, const FileKey &_rk, uint32 _flags){
	FileUidTp	fuid;
	RequestUid	requid;
	return stream(_sptr, fuid, requid, _rk, _flags | NoWait);
}

int FileManager::stream(StreamPtr<IOStream> &_sptr, const FileKey &_rk, uint32 _flags){
	FileUidTp	fuid;
	RequestUid	requid;
	return stream(_sptr, fuid, requid, _rk, _flags | NoWait);
}


//-------------------------------------------------------------------------------------
int FileManager::stream(
	StreamPtr<IStream> &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_requid,
	const FileKey &_rk,
	uint32 _flags
){
	//TODO:
}
int FileManager::stream(
	StreamPtr<OStream> &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_requid,
	const FileKey &_rk,
	uint32 _flags
){
	//TODO:
}

/*
	NOTE:
	The tmp file creation will be handled by TempFileKey
	which:
		- on find will allways return -1
		-
*/
int FileManager::stream(
	StreamPtr<IOStream> &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_requid,
	const FileKey &_rk,
	uint32 _flags
){	
	Mutex::Locker	lock(*d.mut);
	if(state() != Data::Running) return BAD;
	int fid = _rk.find(*this);
	if(fid >= 0){//the file is already open
		//try to get a stream from it.
		switch(d.fv[fid].pfile->stream(*this, fid, _sptr, _requid, _flags)){
			case BAD:
				//we cant get the stream - not now and not ever
				return BAD;
			case OK:
				_rfuid.first = fid;
				_rfuid.second = d.fv[fid].uid;
				return OK;
			case NOK:
				//we should wait for the stream
				return NOK;
		}
	}else{//the file is not open
		//first create the file object:
		unsigned pos = d.createFilePosition();
		d.fv[pos].pfile = new File(*_rk.clone(), _flags);
		//now try to open the file:
		if(d.sz < d.maxsz){
			switch(d.fv[pos].pfile->open(*this, pos)){
				case BAD:
					//unable to open file - not now not ever
					delete d.fv[pos].pfile;
					d.fv[pos].pfile = NULL;
					d.collectFilePosition(pos);
					return BAD;
				case OK:{
					//register into a mapper
					d.fv[pos].pfile->key().insert(*this, pos);
					_rfuid.first = pos;
					_rfuid.second = d.fv[pos].uid;
					d.fv[pos].pfile->stream(*this, pos, _sptr, _requid, _flags);//MUST NOT FAIL
					assert(_sptr.ptr());
					++d.sz;
					return OK;
				}
				case NOK:{
					//we cant open file for now - no more fds etc
					if(_flags & NoWait){
						delete d.fv[pos].pfile;
						d.fv[pos].pfile = NULL;
						d.collectFilePosition(pos);
						return BAD;
					}
					//register into a mapper
					d.fv[pos].pfile->key().insert(*this, pos);
					_rfuid.first = pos;
					_rfuid.second = d.fv[pos].uid;
					d.fv[pos].pfile->stream(*this, pos, _sptr, _requid, _flags);//WILL FAIL
					d.oq.push(pos);
					return NOK;
				}
			}
		}else{
			if(_flags & NoWait){
				delete d.fv[pos].pfile;
				d.fv[pos].pfile = NULL;
				d.collectFilePosition(pos);
				return BAD;
			}
			//register into a mapper
			d.fv[pos].pfile->key().insert(*this, pos);
			_rfuid.first = pos;
			_rfuid.second = d.fv[pos].uid;
			d.fv[pos].pfile->stream(*this, pos, _sptr, _requid, _flags);//WILL FAIL
			d.oq.push(pos);
			return NOK;
		}
	}
	assert(false);
	return BAD;
}

int FileManager::stream(StreamPtr<IStream> &_sptr, const FileUidTp &_rfuid, const RequestUid &_requid, uint32 _flags){
	Mutex::Locker	lock(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].uid == _rfuid.second){
		assert(d.fv[_rfuid.first].pfile);
		return d.fv[_rfuid.first].pfile->stream(*this, _rfuid.first, _sptr, _requid, _flags);
	}
	return BAD;
}
int FileManager::stream(StreamPtr<OStream> &_sptr, const FileUidTp &_rfuid, const RequestUid &_requid, uint32 _flags){
	Mutex::Locker	lock(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].uid == _rfuid.second){
		assert(d.fv[_rfuid.first].pfile);
		return d.fv[_rfuid.first].pfile->stream(*this, _rfuid.first, _sptr, _requid, _flags);
	}
	return BAD;
}
int FileManager::stream(StreamPtr<IOStream> &_sptr, const FileUidTp &_rfuid, const RequestUid &_requid, uint32 _flags){
	Mutex::Locker	lock(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].uid == _rfuid.second){
		assert(d.fv[_rfuid.first].pfile);
		return d.fv[_rfuid.first].pfile->stream(*this, _rfuid.first, _sptr, _requid, _flags);
	}
	return BAD;
}


void FileManager::releaseIStream(uint _fileid){
	bool b;
	{
		Mutex::Locker lock(d.mutpool.object(_fileid));
		b = d.fv[_fileid].pfile->decreaseInCount();
	}
	if(b){
		Mutex::Locker	lock(*d.mut);
		//we must signal the filemanager
		d.sq.push(_fileid);
		if(static_cast<cs::Object*>(this)->signal((int)cs::S_RAISE)){
			Server::the().raiseObject(*this);
		}
	}
}
void FileManager::releaseOStream(uint _fileid){
	bool b;
	{
		Mutex::Locker lock(d.mutpool.object(_fileid));
		b = d.fv[_fileid].pfile->decreaseOutCount();
	}
	if(b){
		Mutex::Locker	lock(*d.mut);
		//we must signal the filemanager
		d.sq.push(_fileid);
		if(static_cast<cs::Object*>(this)->signal((int)cs::S_RAISE)){
			Server::the().raiseObject(*this);
		}
	}
}
void FileManager::releaseIOStream(uint _fileid){
	releaseOStream(_fileid);
}

int FileManager::doRegisterMapper(FileMapper *_pm){
	Mutex::Locker	lock(*d.mut);
	d.mv.push_back(_pm);
	return d.mv.size() - 1;
}

FileMapper* FileManager::doGetMapper(unsigned _id, FileMapper *_pm){
	if(!_pm){
		assert(_id < d.mv.size() && d.mv[_id]);
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
uint32 FileManager::fileOpenTimeout()const{
	//TODO: make it configurable
	return 1000 * 60 * 3;//three minutes
}
int FileManager::execute(){
	assert(false);
	return BAD;
}
//--------------------------------------------------------------------------
// FileManager::Data
int FileManager::Data::fileWrite(uint _fileid, const char *_pb, uint32 _bl, uint32 _flags){
	Mutex::Locker lock(mutpool.object(_fileid));
	return fv[_fileid].pfile->write(_pb, _bl, _flags);
}
int FileManager::Data::fileRead(uint _fileid, char *_pb, uint32 _bl, uint32 _flags){
	Mutex::Locker lock(mutpool.object(_fileid));
	return fv[_fileid].pfile->read(_pb, _bl, _flags);
}
int FileManager::Data::fileSize(uint _fileid){
	Mutex::Locker lock(mutpool.object(_fileid));
	return fv[_fileid].pfile->size();
}
unsigned FileManager::Data::createFilePosition(){
	if(fs.size()){
		uint t = fs.top();
		fs.pop();
		return t;
	}else{
		fv.push_back(FileData());
		return fv.size() - 1;
	}
}
void FileManager::Data::collectFilePosition(unsigned _pos){
	fs.push(_pos);
	++fv[_pos].uid;
}
//--------------------------------------------------------------------------
// FileIStream

int FileIStream::read(char * _pb, uint32 _bl, uint32 _flags){
	return rd.fileRead(fileid, _pb, _bl, _flags);
}

int FileIStream::release(){	
	Server::the().fileManager().releaseIStream(fileid);
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
	Server::the().fileManager().releaseOStream(fileid);
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
	Server::the().fileManager().releaseIOStream(fileid);
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

int File::open(FileManager &_rsm, uint _fileid){
	//TODO: do it right/better
	string fn;
	rk.fileName(_rsm, _fileid, fn);
	if(FileDevice::open(fn.c_str(), FileDevice::RW) < 0){
		if(canRetryOpen()) return NOK;//will return true if errno is ENFILE or ENOMEM
		if(flags & FileManager::Create){
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

int File::stream(FileManager& _rsm, uint _fileid, StreamPtr<IStream> &_sptr, const FileManager::RequestUid &_roi, uint32 _flags){
	{
		//TODO: consider FileManager::Forced 
		Mutex::Locker lock(_rsm.d.mutpool.object(_fileid));
		if(!state || ousecnt || owq.size()){//no writers and no writers waiting
			iwq.push(WaitData(_roi, _flags));
			return NOK;
		}
		++iusecnt;
	}
	_sptr = new FileIStream(_rsm.d, _fileid);
	return OK;
}

int File::stream(FileManager& _rsm, uint _fileid, StreamPtr<OStream> &_sptr, const FileManager::RequestUid &_roi, uint32 _flags){
	{
		Mutex::Locker lock(_rsm.d.mutpool.object(_fileid));
		if(_flags & FileManager::Forced){
			//TODO: maybe you dont need owq.size() too
			if(!state || ousecnt || owq.size()) return BAD;
		}else if(!state || ousecnt || iusecnt || owq.size()){
			owq.push(WaitData(_roi, _flags));
			return NOK;
		}
		++ousecnt;
	}
	_sptr = new FileOStream(_rsm.d, _fileid);
	return OK;
}

int File::stream(FileManager& _rsm, uint _fileid, StreamPtr<IOStream> &_sptr, const FileManager::RequestUid &_roi, uint32 _flags){
	{
		Mutex::Locker lock(_rsm.d.mutpool.object(_fileid));
		if(_flags & FileManager::Forced){
			//TODO: maybe you dont need owq.size() too
			if(!state || ousecnt || owq.size()) return BAD;
		}else if(!state || ousecnt || iusecnt || owq.size()){
			owq.push(WaitData(_roi, _flags | FileManager::IOStreamRequest));
			return NOK;
		}
		++ousecnt;
	}
	_sptr = new FileIOStream(_rsm.d, _fileid);
	return OK;
}

// int File::read(char *, uint32, int64){
// 	//TODO:
// }
// 
// int File::write(const char *, uint32, int64){
// 	//TODO:
// }
/*
int64 File::size()const{
	//TODO:
	return 0;
}*/

bool File::decreaseOutCount(){
	--ousecnt;
	if(!ousecnt){
		return iwq.size() || owq.size() || (owq.empty() && iwq.empty());//return true if there are waiting readers or writers or noone waiting
	}
	return true;//we must signal the filemanager to take the next writer
}

bool File::decreaseInCount(){
	--iusecnt;
	if(!iusecnt){
		return owq.size() || (owq.empty() && iwq.empty());//return true if there are waiting writers or noone waiting
	}
	return false;
}

int File::execute(FileManager &_rsm, const FileUidTp &_fuid, uint32 _flags, TimeSpec &_rtout){
	Mutex &m(_rsm.d.mutpool.object(_fuid.first));
	int		inuse;
	uint32	ttout;
	if(_flags & Timeout){
		assert(!ousecnt && !iusecnt && iwq.empty() && owq.empty());
		return BAD;
	}
	m.lock();
	if(state){//file is opened
		inuse = signalStreams(_rsm, _fuid);
		ttout = msectout;
		m.unlock();
		
		if(inuse){
			_rtout = 0;
			return NOK;//wait indefinetly
		}else{
			if(msectout){
				_rtout = ttout;
				return NOK;//wait
			}else return BAD;
		}
	}else{//we must still try to open file
		m.unlock();
		
		switch(this->open(_rsm, _fuid.first)){
			case BAD:return BAD;
			case OK:
				_rtout = 0;//infinite
				m.lock();
				state = Opened;
				inuse = signalStreams(_rsm, _fuid);
				ttout = msectout;
				m.unlock();
				if(inuse){
					_rtout = 0;
					return NOK;//wait indefinetly
				}else{
					if(ttout){
						_rtout = ttout;
						return NOK;//wait
					}else return BAD;
				}
			case NOK:
				_rtout += _rsm.fileOpenTimeout();
				return NOK;
		}
	}
	assert(false);
	return BAD;
}

void File::clear(FileManager &_rsm, const FileUidTp &_fuid){
	//TODO:
}

bool File::signalStreams(FileManager &_rsm, const FileUidTp &_fuid){
	if(ousecnt) return true;//can do nothing
	if(this->owq.size()){//we have writers waiting
		if(iusecnt) return true;
		if(owq.front().flags & FileManager::IOStreamRequest){
			StreamPtr<IOStream> sptr(new FileIOStream(_rsm.d, _fuid.first));
			_rsm.sendStream(sptr, _fuid, iwq.front().requid);
		}else{
			StreamPtr<OStream> sptr(new FileOStream(_rsm.d, _fuid.first));
			_rsm.sendStream(sptr, _fuid, iwq.front().requid);
		}
		++ousecnt;
		owq.pop();
	}else while(this->iwq.size()){//signal all istreams
		StreamPtr<IStream> sptr(new FileIStream(_rsm.d, _fuid.first));
		_rsm.sendStream(sptr, _fuid, iwq.front().requid);
		++iusecnt;
		iwq.pop();
	}
	return ousecnt || iusecnt;
}

}//namespace clientserver
