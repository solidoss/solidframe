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
/*#include "core/connection.h"
#include "core/command.h"*/
#include "core/server.h"
#include "core/common.h"
#include <string>

namespace cs = clientserver;
using namespace std;


namespace clientserver{
struct LessStrCmp{
	inline bool operator()(const char* const& _s1, const char* const& _s2)const{
		return strcmp(_s1, _s2) < 0;
	}
};
typedef std::string	String;
//==================================================================
class File: protected FileDevice{
public:
	File(const char* _fn, Mutex &_mtx, uint32 _idx):fn(_fn), mtx(_mtx), usecnt(0), idx(_idx){}
	~File();
	int open();
	int read(char *, uint32, int64);
	int write(const char *, uint32, int64);
	const std::string& name()const;
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

struct StorageManager::Data{
	Data(uint32 _maxsz, uint32 _sz):maxsz(_maxsz),sz(_sz), mut(NULL){}
	enum {Running = 1, Stopping, Stopped};
	typedef Queue<uint32> FreeQueueTp;
	struct WaitData{
		WaitData(uint32 _idx, uint32 _uid, uint16 _req):idx(_idx), uid(_uid), req(_req){}
		uint32	idx;
		uint32	uid;
		uint16	req;//0 for istream, 1 for ostream, 2 for iostream
		String	fn;
	};
	typedef SharedContainer<Mutex>	MutexContainer;
	typedef Queue<WaitData> WaitQueueTp;
	typedef std::pair<File*, int>	FilePairTp;
	typedef std::vector<FilePairTp> FileVectorTp;
	typedef std::map<const char*, uint32, LessStrCmp> FileMapTp;
	FileVectorTp	fv;
	FileMapTp		fm;
	FreeQueueTp		fq;
	WaitQueueTp		wq;
	const uint32	maxsz;
	uint32			sz;
	Mutex			*mut;
	MutexContainer	mutpool;
};
//==================================================================
File::~File(){
	assert(!usecnt);
}
inline int File::open(){
	usecnt = 0;
	if(FileDevice::open(fn.c_str(), FileDevice::RW) < 0){
		//try create it
		return FileDevice::create(fn.c_str(), FileDevice::RW);
	}
	return OK;
}

inline int File::read(char *_pb, uint32 _bl, int64 _off){
	Mutex::Locker lock(mtx);
	return FileDevice::read(_pb, _bl, _off);
}

inline int File::write(const char *_pb, uint32 _bl, int64 _off){
	Mutex::Locker lock(mtx);
	return FileDevice::write(_pb, _bl, _off);
}

inline const String& File::name()const{
	return fn;
}
inline void File::name(const char* _fn){
	fn = _fn;
}

int File::release(){
	Mutex::Locker lock(mtx);
	return --usecnt;
}
int File::close(){
	Mutex::Locker lock(mtx);
	if(usecnt) return BAD;
	FileDevice::close();
	return OK;
}
int File::stream(StreamPtr<IStream> &_sptr){
	Mutex::Locker lock(mtx);
	++usecnt;
	_sptr = new FileIStream(*this);
	return OK;
}

int File::stream(StreamPtr<OStream> &_sptr){
	Mutex::Locker lock(mtx);
	++usecnt;
	_sptr = new FileOStream(*this);
	return OK;
}

int File::stream(StreamPtr<IOStream> &_sptr){
	Mutex::Locker lock(mtx);
	++usecnt;
	_sptr = new FileIOStream(*this);
	return OK;
}


int64 File::size()const{
	Mutex::Locker lock(mtx);
	return FileDevice::size();
}
int File::useCount()const{
	Mutex::Locker lock(mtx);
	return usecnt;
}
//------------------------------------------------------------------
int FileIOStream::read(char *_pb, uint32 _bl, uint32){
	int rv = rf.read(_pb, _bl, off);
	if(rv > 0){off += rv;}
	return rv;
}
int FileIOStream::write(const char *_pb, uint32 _bl, uint32){
	int rv = rf.write(_pb, _bl, off);
	if(rv > 0){off += rv;}
	return rv;
}

int FileIOStream::release(){
	StorageManager &ss = Server::the().storage();
	ss.release(rf);
	return -1;
}
int64 FileIOStream::seek(int64 _off, SeekRef _ref){
	assert(_ref == SeekBeg);
	off = _off;
	return off;
}
int64 FileIOStream::size()const{
	return rf.size();
}
FileIOStream::~FileIOStream(){
	idbg("");
}
//------------------------------------------------------------------
int FileOStream::write(const char *_pb, uint32 _bl, uint32){
	int rv = rf.write(_pb, _bl, off);
	if(rv > 0){off += rv;}
	return rv;
}

int FileOStream::release(){
	StorageManager &ss = Server::the().storage();
	ss.release(rf);
	return -1;
}
int64 FileOStream::seek(int64 _off, SeekRef _ref){
	assert(_ref == SeekBeg);
	off = _off;
	return off;
}
int64 FileOStream::size()const{
	return rf.size();
}
//------------------------------------------------------------------
int FileIStream::read(char *_pb, uint32 _bl, uint32){
	int rv = rf.read(_pb, _bl, off);
	if(rv > 0){off += rv;}
	return rv;
}

int FileIStream::release(){
	StorageManager &ss = Server::the().storage();
	ss.release(rf);
	return -1;
}
int64 FileIStream::seek(int64 _off, SeekRef _ref){
	assert(_ref == SeekBeg);
	off = _off;
	return off;
}
int64 FileIStream::size()const{
	return rf.size();
}

//------------------------------------------------------------------
// int IStreamCommand::execute(Connection &_rcon){
// 	return _rcon.receiveIStream(sptr);
// }
// //------------------------------------------------------------------
// int OStreamCommand::execute(Connection &_rcon){
// 	return _rcon.receiveOStream(sptr);
// }
// //------------------------------------------------------------------
// int IOStreamCommand::execute(Connection &_rcon){
// 	return _rcon.receiveIOStream(sptr);
// }

//------------------------------------------------------------------
StorageManager::StorageManager(uint32 _maxsz): d(*(new Data(_maxsz, 0))){
	state(Data::Running);
}

StorageManager::~StorageManager(){
	for(Data::FileVectorTp::iterator it(d.fv.begin()); it != d.fv.end(); ++it){
		delete it->first;
	}
	delete &d;
}

void StorageManager::mutex(Mutex *_pmut){
	d.mut = _pmut;
}

int StorageManager::execute(){
	return BAD;
}

void StorageManager::release(File &_rf){
	if(!_rf.release()){
		Mutex::Locker	lock(*d.mut);
		assert(!_rf.useCount());
		if(state() == Data::Running){
			if(!d.fv[_rf.index()].second){
				d.fq.push(_rf.index());
				d.fv[_rf.index()].second = 1;
			}
			if(d.wq.size()){
				static_cast<cs::Object*>(this)->signal((int)cs::S_RAISE);
				Server::the().raiseObject(*this);
			}
 		}else{
			if(!d.fv[_rf.index()].second){
				d.fq.push(_rf.index());
				d.fv[_rf.index()].second = 1;
			}
			if(d.fq.size() == d.sz){
				static_cast<cs::Object*>(this)->signal((int)cs::S_RAISE | cs::S_KILL);
				Server::the().raiseObject(*this);
			}
		}
	}
}

int StorageManager::istream(StreamPtr<IStream> &_sptr, const char* _fn, uint32 _idx, uint32 _uid){
	Mutex::Locker	lock(*d.mut);
	if(state() != Data::Running) return BAD;
	{
		Data::FileMapTp::const_iterator it(d.fm.find(_fn));
		if(it != d.fm.end()){//file already open
			return d.fv[it->second].first->stream(_sptr);
		}
	}
	if(d.sz < d.maxsz){//open a new file
		File *pf = new File(_fn, d.mutpool.safeObject(d.fv.size()), d.fv.size());
		if(pf->open()){delete pf; return BAD;}
		d.fv.push_back(Data::FilePairTp(pf, 0));
		d.fm[d.fv.back().first->name().c_str()] = d.fv.size() - 1;
		++d.sz;
		return d.fv.back().first->stream(_sptr);
	}
	File *pf;
	//see if we can free something from the queue
	switch(doUseFreeQueue(pf, _fn)){
		case BAD:	return BAD;
		case OK:	return pf->stream(_sptr);
		default:	break;
	}
	d.wq.push(Data::WaitData(_idx,_uid, 0));
	d.wq.back().fn = _fn;
	return NOK;
}

int StorageManager::ostream(StreamPtr<OStream> &_sptr, const char* _fn, uint32 _idx, uint32 _uid){
	Mutex::Locker	lock(*d.mut);
	if(state() != Data::Running) return BAD;
	{
		Data::FileMapTp::const_iterator it(d.fm.find(_fn));
		if(it != d.fm.end()){//file already open
			return d.fv[it->second].first->stream(_sptr);
		}
	}
	if(d.sz < d.maxsz){//open a new file
		File *pf = new File(_fn, d.mutpool.safeObject(d.fv.size()), d.fv.size());
		if(pf->open()){delete pf; return BAD;}
		d.fv.push_back(Data::FilePairTp(pf, 0));
		d.fm[d.fv.back().first->name().c_str()] = d.fv.size() - 1;
		++d.sz;
		return d.fv.back().first->stream(_sptr);
	}
	File *pf;
	//see if we can free something from the queue
	switch(doUseFreeQueue(pf, _fn)){
		case BAD: return BAD;
		case OK: return pf->stream(_sptr);
		default: break;
	}
	d.wq.push(Data::WaitData(_idx,_uid, 1));
	d.wq.back().fn = _fn;
	return NOK;
}

int StorageManager::iostream(StreamPtr<IOStream> &_sptr, const char* _fn){
	Mutex::Locker	lock(*d.mut);
	if(state() != Data::Running) return BAD;
	{
		Data::FileMapTp::const_iterator it(d.fm.find(_fn));
		if(it != d.fm.end()){//file already open
			return d.fv[it->second].first->stream(_sptr);
		}
	}
	if(d.sz < d.maxsz){//open a new file
		File *pf = new File(_fn, d.mutpool.safeObject(d.fv.size()), d.fv.size());
		if(pf->open()){delete pf; return BAD;}
		d.fv.push_back(Data::FilePairTp(pf, 0));
		d.fm[d.fv.back().first->name().c_str()] = d.fv.size() - 1;
		++d.sz;
		return d.fv.back().first->stream(_sptr);
	}
	return BAD;
}

int StorageManager::doUseFreeQueue(File* &_rf, const char* _fn){
	uint32 pos;
	_rf = NULL;
	while(d.fq.size()){
		pos = d.fq.front(); d.fq.pop(); d.fv[pos].second = 0;
		if(!d.fv[pos].first || !d.fv[pos].first->close()){
			idbg("closed a filedesc");
			goto Done;
		}
	}
	return NOK;
	Done:
		d.fm.erase(d.fv[pos].first->name().c_str());
		d.fv[pos].first->name(_fn);
		if(!d.fv[pos].first->open()){
			d.fm[d.fv[pos].first->name().c_str()] = pos;
			_rf =d.fv[pos].first;
			return OK;
			//return fv[pos].first->stream(_sptr);
		}
		assert(0);
		delete d.fv[pos].first;
		d.fv[pos].first = NULL;
		d.fq.push(pos);
		return BAD;
}

int StorageManager::execute(ulong _evs, TimeSpec &_rtout){
	idbg("");//_rtout = 0;
	if(signaled()){
		d.mut->lock();
		ulong sm = grabSignalMask(0);
		idbg("signalmask "<<sm);
		if(sm & cs::S_KILL){
			state(Data::Stopping);
			assert(d.fq.size() <= d.sz);
			if(d.fq.size() == d.sz){
				d.mut->unlock();
				Server::the().removeStorage();
				return BAD;
			}else{
				d.mut->unlock();
				return NOK;
			}
		}
		if(state() != Data::Running){d.mut->unlock(); return NOK;}
		File *pf;
		while(d.wq.size()){
			switch(doUseFreeQueue(pf, d.wq.front().fn.c_str())){
				case BAD:
					sendError(d.wq.front().idx, d.wq.front().uid);
					d.wq.pop();
					break;
				case OK:{
					uint32 idx(d.wq.front().idx),uidx(d.wq.front().uid);
					if(d.wq.front().req == 0){//istream
						StreamPtr<IStream> ps;
						if(pf->stream(ps) == OK){
							sendStream(ps, idx, uidx);
						}else{
							sendError(idx, uidx);
						}
					}else if(d.wq.front().req == 1){//ostream
						StreamPtr<OStream> ps;
						if(pf->stream(ps) == OK){
							sendStream(ps, idx, uidx);
						}else{
							sendError(idx, uidx);
						}
					}else{//iostream
						StreamPtr<IOStream> ps;
						if(pf->stream(ps) == OK){
							sendStream(ps, idx, uidx);
						}else{
							sendError(idx, uidx);
						}
					}
					d.wq.pop();
					break;
				}
				default: goto Done;
			}
		}
		d.mut->unlock();
	}
	Done:
	return NOK;
}
void StorageManager::sendError(uint32 _idx, uint32 _uid){
	Server::the().signalObject(_idx, _uid, cs::S_RAISE | cs::S_ERR);
}
}//namespace test
