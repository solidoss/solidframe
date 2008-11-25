/* Implementation file filemanager.cpp
	
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

#include <vector>
#include <queue>
#include <map>
#include <iostream>
#include <string>
#include <cstring>

#include "system/debug.hpp"
#include "system/timespec.hpp"
#include "system/filedevice.hpp"
#include "system/mutex.hpp"
#include "system/timespec.hpp"

#include "utility/mutualobjectcontainer.hpp"
#include "utility/iostream.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "core/filemanager.hpp"
#include "core/server.hpp"
#include "core/common.hpp"
#include "core/filekeys.hpp"
#include "core/filemapper.hpp"
#include "core/requestuid.hpp"

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

//typedef FileManager::RequestUid FMRequestUidTp;

//------------------------------------------------------------------------------
class File: public FileDevice{
	enum {
		NotOpened = 0,
		Opened,
		Timeout = 1,
	};
public:
	File(FileKey &_rk, uint32 _flags = 0):rk(_rk), ousecnt(0), iusecnt(0), msectout(0), flags(_flags & 0xffff), state(NotOpened){}
	~File();
	FileKey &key(){return rk;}
	int open(FileManager &_rsm, uint32 _flags);
// 	int read(char *, uint32, int64);
// 	int write(const char *, uint32, int64);
	int stream(FileManager& _rsm, uint _fileid, StreamPtr<IStream> &_sptr, const RequestUid &_roi, uint32 _flags);
	int stream(FileManager& _rsm, uint _fileid, StreamPtr<OStream> &_sptr, const RequestUid &_roi, uint32 _flags);
	int stream(FileManager& _rsm, uint _fileid, StreamPtr<IOStream> &_sptr, const RequestUid &_roi, uint32 _flags);
	//int64 size()const;
	bool decreaseOutCount();
	bool decreaseInCount();
	int executeSignal(FileManager &_rsm, const FileUidTp &_fuid, uint32 _flags, uint32 &_rtout);
	int executeOpen(FileManager &_rsm, const FileUidTp &_fuid, uint32 _flags, uint32 &_rtout, Mutex &_rm);
	void clear(FileManager &_rsm, const FileUidTp &_fuid);
	bool inUse()const{
		return ousecnt || iusecnt;
	}
private:
	bool signalStreams(FileManager &_rsm, const FileUidTp &_fuid);
	struct WaitData{
		WaitData(const RequestUid &_rrequid, uint32 _flags):requid(_rrequid), flags(_flags){}
		RequestUid	requid;
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
	~FileIStream();
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
	~FileOStream();
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
	~FileIOStream();
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
	typedef MutualObjectContainer<Mutex>	MutexContainer;
	typedef Queue<uint>						SignalQueueTp;
	typedef Stack<uint>						FreeStackTp;
	struct FileExtData{
		FileExtData(File *_pfile = NULL, uint32 _pos = 0):pfile(_pfile), pos(_pos){}
		File	*pfile;
		uint32	pos;
	};
	struct FileData{
		FileData(File *_pfile = NULL):pfile(_pfile), toutidx(-1), uid(0), tout(0xffffffff){}
		FileData(const FileExtData &_rfext):pfile(_rfext.pfile), toutidx(-1), uid(0), tout(0xffffffff){}
		File		*pfile;
		int32		toutidx;
		uint32		uid;
		TimeSpec	tout;
	};
	typedef std::deque<FileData>		FileVectorTp;
	typedef std::deque<FileExtData>		FileExtVectorTp;
	typedef std::deque<int32>			TimeoutVectorTp;
	typedef std::vector<FileMapper*>	FileMapperVectorTp;
	Data(uint32 _maxsz):maxsz(_maxsz),sz(0), freeidx(0), mut(NULL){}
	~Data();
	int  fileWrite(uint _fileid, const char *_pb, uint32 _bl, const int64 &_off, uint32 _flags);
	int  fileRead(uint _fileid, char *_pb, uint32 _bl, const int64 &_off, uint32 _flags);
	int  fileSize(uint _fileid);
	unsigned createFilePosition();
	void collectFilePosition(unsigned _pos);
	unsigned createFilePositionExt();
	void collectFilePositionExt(unsigned _pos);
	void eraseToutPos(unsigned _pos);
	
	const uint32			maxsz;
	uint32					sz;
	uint32					freeidx;
	Mutex					*mut;
	MutexContainer			mutpool;
	FileVectorTp			fv;//file vector
	FileExtVectorTp			fextv;
	SignalQueueTp			sq;//stream queue
	SignalQueueTp			oq;//open queue
	FileMapperVectorTp		mv;
	FreeStackTp				fs;//free stack
	TimeoutVectorTp			toutv;
	TimeSpec				tout;
};

FileManager::Data::~Data(){
	for(FileMapperVectorTp::const_iterator it(mv.begin()); it != mv.end(); ++it){
		delete *it;
	}
}
//	FileManager definitions ==================================================
FileManager::FileManager(uint32 _maxfcnt):d(*(new Data(_maxfcnt))){
	state(Data::Running);
	//TODO: add some empty filedatas to fv and to fs
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
	//before anything, we move from fextv to fv:
	if(d.fextv.size()){
		for(Data::FileExtVectorTp::const_iterator it(d.fextv.begin()); it != d.fextv.end(); ++it){
			cassert(d.fv.size() == it->pos);
			d.fv.push_back(Data::FileData(*it));
		}
		d.fextv.clear();
		d.freeidx = 0;
		//TODO: add some more empty filedatas to fv and to fs
	}
	if(signaled()){
		ulong sm = grabSignalMask(0);
		idbgx(Dbg::filemanager, "signalmask "<<sm);
		if(sm & cs::S_KILL){
			state(Data::Stopping);
			idbgx(Dbg::filemanager, "kill "<<d.sz);
			if(!d.sz){//no file
				state(-1);
				d.mut->unlock();
				Server::the().removeFileManager();
				idbgx(Dbg::filemanager, "~FileManager");
				return BAD;
			}
		}
	}
	//First we deal with the signaled files
	uint32	fileids[256];
	uint	qsz = d.sq.size();
	uint	oqsz = d.oq.size();
	uint	ofront = 0;
	uint	dsz = d.sz;
	if(state() != Data::Running) oqsz = 0;
	if(oqsz){
		ofront = d.oq.front();
	}
	if(!qsz) d.mut->unlock();
	while(qsz){
		uint sz = 0;
		while(sz < 256 && qsz){
			fileids[sz] = d.sq.front();
			d.sq.pop();
			--qsz;
			++sz;
		}
		d.mut->unlock();
		const uint32	*pfid = fileids;
		while(sz){
			Data::FileData	&rf(d.fv[*pfid]);
			if(!rf.pfile){
				++pfid;
				--sz;
				continue;
			}
			FileUidTp 		fuid(*pfid, rf.uid);
			Mutex			&m(d.mutpool.object(*pfid));
			uint32 			msectout = 0;
			m.lock();
			switch(rf.pfile->executeSignal(*this, fuid, 0, msectout)){
				case BAD:{//delete the file and collect the position
					idbgx(Dbg::filemanager, "delete file");
					File *pf(rf.pfile);
					m.unlock();//avoid crosslocking deadlock
					{
						Mutex::Locker lock0(*d.mut);
						{
							Mutex::Locker lock1(m);
							if(rf.pfile->inUse()) break;//false alarm
							if(state() == Data::Running){
								rf.pfile->clear(*this, fuid);//send errors only if we're not stopping
							}
						}
						pf->key().erase(*this, *pfid);
						rf.pfile = NULL;
						d.collectFilePosition(*pfid);
						dsz = d.sz;
					}
					rf.tout.set(0xffffffff);
					if(rf.toutidx >= 0){
						d.eraseToutPos(rf.toutidx);
						rf.toutidx = -1;
					}
					delete pf;
					}break;
				case OK://the file waits for nothing - we can safely delete it
					if(state() != Data::Running){//the state is only set within this thread!!!
						//rf.pfile->clear(*this, fuid);//not sending errors when stopping
						idbgx(Dbg::filemanager, "");
						File *pf(rf.pfile);
						m.unlock();//avoid crosslocking deadlock
						{
							Mutex::Locker lock0(*d.mut);
							{
								Mutex::Locker lock1(m);
								if(rf.pfile->inUse()) break;//false alarm
							}
							pf->key().erase(*this, *pfid);
							rf.pfile = NULL;
							d.collectFilePosition(*pfid);
							dsz = d.sz;
						}
						rf.tout.set(0xffffffff);
						if(rf.toutidx >= 0){
							d.eraseToutPos(rf.toutidx);
							rf.toutidx = -1;
						}
						delete pf;
					}else{
						idbgx(Dbg::filemanager, "");
						m.unlock();
						rf.tout = _rtout;
						rf.tout += msectout;
						if(rf.tout < d.tout){
							d.tout = rf.tout;
						}
						if(rf.toutidx < 0){
							d.toutv.push_back(*pfid);
							rf.toutidx = d.toutv.size() - 1;
						}
						
					}
					break;
				case NOK://the file is in use - wait indefinetly
					idbgx(Dbg::filemanager, "file in use - wait indefinetly");
					m.unlock();
					rf.tout.set(0xffffffff);
					if(rf.toutidx >= 0){
						d.eraseToutPos(rf.toutidx);
						rf.toutidx = -1;
					}
					break;
			}
			++pfid;
			--sz;
		}
		if(qsz) d.mut->lock();
	}
	//then we deal with opening files
	//d.mut is unlocked
	while(oqsz && dsz < d.maxsz){//very rare - there are files waiting to be open
		Data::FileData	&rf(d.fv[ofront]);
		uint32 			msectout = 0;
		//files are only deleted from within this thread. so its safe without locking
		if(rf.pfile){
			Mutex			&m(d.mutpool.object(ofront));
			FileUidTp 		fuid(ofront, d.fv[ofront].uid);
			//also the files from oq are only open from this thread
			//so, no locking here too.
			switch(rf.pfile->executeOpen(*this, fuid, 0, msectout, m)){
				case BAD:{//delete the file and collect the position
					File *pf(rf.pfile);
					m.unlock();//avoid crosslocking deadlock
					{
						Mutex::Locker lock0(*d.mut);
						{
							Mutex::Locker lock1(m);
							if(rf.pfile->inUse()) break;//false alarm
							if(state() == Data::Running){
								rf.pfile->clear(*this, fuid);//send errors only if we're not stopping
							}
						}
						pf->key().erase(*this, ofront);
						rf.pfile = NULL;
						d.collectFilePosition(ofront);
						dsz = d.sz;
						d.oq.pop();
						oqsz = d.oq.size();
						if(oqsz) ofront = d.oq.front();
					}
					rf.tout.set(0xffffffff);
					if(rf.toutidx >= 0){
						d.eraseToutPos(rf.toutidx);
						rf.toutidx = -1;
					}
					delete pf;
					}break;
				case OK://the file was successfully opened
					{
						Mutex::Locker lock0(*d.mut);
						++d.sz;
						dsz = d.sz;
						d.oq.pop();
						oqsz = d.oq.size();
						if(oqsz) ofront = d.oq.front();
					}
					rf.tout.set(0xffffffff);
					if(rf.toutidx >= 0){
						d.eraseToutPos(rf.toutidx);
						rf.toutidx = -1;
					}
					break;
				case NOK:	//we'll we must still wait
					oqsz = 0;
					break;
			}
		}
	}
	if(state() != Data::Running){
		//simulate a timeout
		_evs |= TIMEOUT;
		_rtout.set(0xffffffff);
	}
	if((_evs & TIMEOUT) && _rtout <= d.tout){
		//scan all files for timeout
		//as we change the fv[].tout only in this thread - no need for locking while scanning
		TimeSpec tout(0xffffffff);
		for(Data::TimeoutVectorTp::const_iterator it(d.toutv.begin()); it != d.toutv.end();){
			Data::FileData &rf(d.fv[*it]);
			if(_rtout >= rf.tout){
				{
					Mutex &rm(d.mutpool.object(*it));
					rm.lock();
					//if(rf.pfile){//pfile is set to null only in this thread - the file will not be in toutv
					if(!rf.pfile->inUse()){
						FileUidTp 		fuid(ofront, d.fv[*it].uid);
						File *pf(rf.pfile);
						rm.unlock();//avoid crosslocking deadlock
						{
							Mutex::Locker lock0(*d.mut);
							{
								Mutex::Locker lock1(rm);
								if(rf.pfile->inUse()) break;//false alarm
								rf.pfile->clear(*this, fuid);//send errors only if we're not stopping
							}
							pf->key().erase(*this, *it);
							rf.pfile = NULL;
							d.collectFilePosition(*it);
							dsz = d.sz;
							d.oq.pop();
							oqsz = d.oq.size();
						}
						rf.tout.set(0xffffffff);
						rf.toutidx = -1;
						delete pf;
					}
				//}
				}
				//erase the iterator from toutv
				d.toutv[it - d.toutv.begin()] = d.toutv.back();
				d.toutv.pop_back();
				if(d.toutv.empty()) break;
			}else{
				if(rf.tout < tout) tout = rf.tout;
				++it;
			}
		}
		d.tout = tout;
	}
	if(state() != Data::Running && !d.sz){
		idbgx(Dbg::filemanager, "kill "<<d.sz);
		cassert(!d.sz);
		Server::the().removeFileManager();
		state(-1);//TODO: is there a pb that mut is not locked?!
		idbgx(Dbg::filemanager, "~FileManager");
		return BAD;
	}
	if(d.toutv.size()) _rtout = d.tout;
	return NOK;
}

int64 FileManager::fileSize(const char *_fn)const{
	//TODO:
	return -1;
}

int64 FileManager::fileSize(const FileKey &_rk)const{
	return -1;
}
	
int FileManager::setFileTimeout(const FileUidTp &_rfuid, const TimeSpec &_rtout){
	//TODO:
	return BAD;
}

//overload from object
void FileManager::mutex(Mutex *_pmut){
	d.mut = _pmut;
}
//---------------------------------------------------------------
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
//---------------------------------------------------------------
int FileManager::streamSpecific(
	StreamPtr<IStream> &_sptr,
	const char *_fn,
	uint32 _flags
){
	return stream(_sptr, *requestuidptr, _fn, _flags);
}
int FileManager::streamSpecific(
	StreamPtr<OStream> &_sptr,
	const char *_fn,
	uint32 _flags
){
	return stream(_sptr, *requestuidptr, _fn, _flags);
}
int FileManager::streamSpecific(
	StreamPtr<IOStream> &_sptr,
	const char *_fn,
	uint32 _flags
){
	return stream(_sptr, *requestuidptr, _fn, _flags);
}
//---------------------------------------------------------------
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
		return stream(_sptr, _rfuid, _rrequid, k, _flags | Create);
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
		return stream(_sptr, _rfuid, _rrequid, k, _flags | Create);
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
		return stream(_sptr, _rfuid, _rrequid, k, _flags | Create);
	}
}
//---------------------------------------------------------------
int FileManager::streamSpecific(
	StreamPtr<IStream> &_sptr,
	FileUidTp &_rfuid,
	const char *_fn,
	uint32 _flags
){
	return stream(_sptr, _rfuid, *requestuidptr, _fn, _flags);
}
int FileManager::streamSpecific(
	StreamPtr<OStream> &_sptr,
	FileUidTp &_rfuid,
	const char *_fn,
	uint32 _flags
){
	return stream(_sptr, _rfuid, *requestuidptr, _fn, _flags);
}
int FileManager::streamSpecific(
	StreamPtr<IOStream> &_sptr,
	FileUidTp &_rfuid,
	const char *_fn,
	uint32 _flags
){
	return stream(_sptr, _rfuid, *requestuidptr, _fn, _flags);
}
//---------------------------------------------------------------
int FileManager::stream(StreamPtr<IStream> &_sptr, const char *_fn, uint32 _flags){
	FileUidTp	fuid;
	RequestUid	requid;
	if(_fn){
		FastNameFileKey k(_fn);
		return stream(_sptr, fuid, requid, k, _flags | NoWait);
	}else{
		TempFileKey k;
		return stream(_sptr, fuid, requid, k, _flags | NoWait | Create);
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
		return stream(_sptr, fuid, requid, k, _flags | NoWait | Create);
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
		return stream(_sptr, fuid, requid, k, _flags | NoWait | Create);
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
template <typename StreamP>
int FileManager::doGetStream(
	StreamP &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_requid,
	const FileKey &_rk,
	uint32 _flags
){
	Mutex::Locker	lock(*d.mut);
	if(state() != Data::Running) return BAD;
	uint32 fid = _rk.find(*this);
	if(fid != (uint32)-1){
		Mutex::Locker	lock2(d.mutpool.object(fid));
		File *pf = NULL;
		if(fid < d.fv.size() && d.fv[fid].pfile){//we have a file
			pf = d.fv[fid].pfile;
		}else{
			//search the file within the ext vector
			for(Data::FileExtVectorTp::const_iterator it(d.fextv.begin()); it != d.fextv.end(); ++it){
				if(it->pos == fid){
					pf = it->pfile;
					break;
				}
			}
		}
		cassert(pf);
		switch(pf->stream(*this, fid, _sptr, _requid, _flags)){
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
	}
	
	//the file is not open nor is it waiting for oppening
	//first create the file object:
	uint32 pos = d.createFilePositionExt();
	File *pf = new File(*_rk.clone(), _flags);
	//now try to open the file:
	if(d.sz < d.maxsz){
		switch(pf->open(*this, pos)){
			case BAD:
				//unable to open file - not now not ever
				delete pf;
				d.collectFilePositionExt(pos);
				return BAD;
			case OK:{
				//register into a mapper
				++d.sz;
				pf->key().insert(*this, pos);
				_rfuid.first = pos;
				Mutex::Locker	lock2(d.mutpool.safeObject(pos));
				if(pos < d.fv.size()){
					_rfuid.second = d.fv[pos].uid;
					d.fv[pos].pfile = pf;
					pf->stream(*this, pos, _sptr, _requid, _flags);//MUST NOT FAIL
					cassert(_sptr.ptr());
					return OK;
				}else if(!(_flags & NoWait)){
					//no stream is given while not in fv
					_rfuid.second = 0;
					d.fextv.push_back(Data::FileExtData(pf, pos));
					pf->stream(*this, pos, _sptr, _requid, _flags | ForcePending);
					d.sq.push(pos);
					idbgx(Dbg::filemanager, "sq.push "<<pos);
					if(static_cast<cs::Object*>(this)->signal((int)cs::S_RAISE)){
						Server::the().raiseObject(*this);
					}
					return NOK;
				}else{
					delete pf;
					pf->key().erase(*this, pos);
					d.collectFilePositionExt(pos);
					return BAD;
				}
			}
			case NOK:break;
		}
	}
	//we cant open file for now - no more fds etc
	if(_flags & NoWait){
		delete pf;
		d.collectFilePositionExt(pos);
		return BAD;
	}
	//register into a mapper
	pf->key().insert(*this, pos);
	_rfuid.first = pos;
	Mutex::Locker	lock2(d.mutpool.safeObject(pos));
	if(pos < d.fv.size()){
		_rfuid.second = d.fv[pos].uid;
		d.fv[pos].pfile = pf;
		pf->stream(*this, pos, _sptr, _requid, _flags);//WILL FAIL: NOK
		cassert(!_sptr.ptr());
	}else{
		//no stream is given while not in fv
		_rfuid.second = 0;
		d.fextv.push_back(Data::FileExtData(pf, pos));
		pf->stream(*this, pos, _sptr, _requid, _flags | ForcePending);
	}
	d.oq.push(pos);
	if(static_cast<cs::Object*>(this)->signal((int)cs::S_RAISE)){
		Server::the().raiseObject(*this);
	}
	return NOK;
}
int FileManager::stream(
	StreamPtr<IStream> &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_requid,
	const FileKey &_rk,
	uint32 _flags
){
	return doGetStream(_sptr, _rfuid, _requid, _rk, _flags);
}
int FileManager::stream(
	StreamPtr<OStream> &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_requid,
	const FileKey &_rk,
	uint32 _flags
){
	return doGetStream(_sptr, _rfuid, _requid, _rk, _flags);
}

/*
	NOTE:
	In order to be able to scan d.fv within the manager's execute without locking,
	we must ensure that the vector is not resized outside execute.
*/
int FileManager::stream(
	StreamPtr<IOStream> &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_requid,
	const FileKey &_rk,
	uint32 _flags
){	
	return doGetStream(_sptr, _rfuid, _requid, _rk, _flags);
}

//---------------------------------------------------------------
int FileManager::streamSpecific(
	StreamPtr<IStream> &_sptr,
	FileUidTp &_rfuid,
	const FileKey &_rk,
	uint32 _flags
){
	return stream(_sptr, _rfuid, *requestuidptr, _rk, _flags);
}
int FileManager::streamSpecific(
	StreamPtr<OStream> &_sptr,
	FileUidTp &_rfuid,
	const FileKey &_rk,
	uint32 _flags
){
	return stream(_sptr, _rfuid, *requestuidptr, _rk, _flags);
}
int FileManager::streamSpecific(
	StreamPtr<IOStream> &_sptr,
	FileUidTp &_rfuid,
	const FileKey &_rk,
	uint32 _flags
){
	return stream(_sptr, _rfuid, *requestuidptr, _rk, _flags);
}
//---------------------------------------------------------------
int FileManager::stream(
	StreamPtr<IStream> &_sptr,
	const FileUidTp &_rfuid,
	const RequestUid &_requid,
	uint32 _flags
){
	Mutex::Locker	lock(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].uid == _rfuid.second){
		Mutex::Locker	lock2(d.mutpool.object(_rfuid.first));
		if(d.fv[_rfuid.first].pfile)
			return d.fv[_rfuid.first].pfile->stream(*this, _rfuid.first, _sptr, _requid, _flags);
	}
	return BAD;
}

int FileManager::stream(
	StreamPtr<OStream> &_sptr,
	const FileUidTp &_rfuid,
	const RequestUid &_requid,
	uint32 _flags
){
	Mutex::Locker	lock(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].uid == _rfuid.second){
		Mutex::Locker	lock2(d.mutpool.object(_rfuid.first));
		if(d.fv[_rfuid.first].pfile)
			return d.fv[_rfuid.first].pfile->stream(*this, _rfuid.first, _sptr, _requid, _flags);
	}
	return BAD;
}

int FileManager::stream(
	StreamPtr<IOStream> &_sptr,
	const FileUidTp &_rfuid,
	const RequestUid &_requid,
	uint32 _flags
){
	Mutex::Locker	lock(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].uid == _rfuid.second){
		Mutex::Locker	lock2(d.mutpool.object(_rfuid.first));
		if(d.fv[_rfuid.first].pfile)
			return d.fv[_rfuid.first].pfile->stream(*this, _rfuid.first, _sptr, _requid, _flags);
	}
	return BAD;
}
//---------------------------------------------------------------
//using specific request
int FileManager::streamSpecific(
	StreamPtr<IStream> &_sptr,
	const FileUidTp &_rfuid,
	uint32 _flags
){
	return stream(_sptr, _rfuid, *requestuidptr, _flags);
}
int FileManager::streamSpecific(
	StreamPtr<OStream> &_sptr,
	const FileUidTp &_rfuid,
	uint32 _flags
){
	return stream(_sptr, _rfuid, *requestuidptr, _flags);
}
int FileManager::streamSpecific(
	StreamPtr<IOStream> &_sptr,
	const FileUidTp &_rfuid,
	uint32 _flags
){
	return stream(_sptr, _rfuid, *requestuidptr, _flags);
}
//---------------------------------------------------------------

void FileManager::releaseIStream(uint _fileid){
	bool b = false;
	{
		Mutex::Locker lock(d.mutpool.object(_fileid));
		//if(d.fv[_fileid].pfile)// the file cannot be deleted because of the usecount!!
		b = d.fv[_fileid].pfile->decreaseInCount();
	}
	if(b){
		Mutex::Locker	lock(*d.mut);
		idbgx(Dbg::filemanager, "signal filemanager "<<_fileid);
		//we must signal the filemanager
		d.sq.push(_fileid);
		idbgx(Dbg::filemanager, "sq.push "<<_fileid);
		if(static_cast<cs::Object*>(this)->signal((int)cs::S_RAISE)){
			Server::the().raiseObject(*this);
		}
	}
}
void FileManager::releaseOStream(uint _fileid){
	bool b = false;
	{
		Mutex::Locker lock(d.mutpool.object(_fileid));
		//if(d.fv[_fileid].pfile)// the file cannot be deleted because of the usecount!!
		b = d.fv[_fileid].pfile->decreaseOutCount();
	}
	if(b){
		Mutex::Locker	lock(*d.mut);
		//we must signal the filemanager
		d.sq.push(_fileid);
		idbgx(Dbg::filemanager, "sq.push "<<_fileid);
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
		cassert(_id < d.mv.size() && d.mv[_id]);
		return d.mv[_id];
	}else{
		if(_id >= d.mv.size()){
			unsigned oldsize = d.mv.size();
			d.mv.resize(_id + 1);
			for(unsigned i = oldsize; i < _id; ++i){
				d.mv[i] = NULL;
			}
			d.mv[_id] = _pm;
			return _pm;
		}else{
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
	cassert(false);
	return BAD;
}
//--------------------------------------------------------------------------
// FileManager::Data
int FileManager::Data::fileWrite(uint _fileid, const char *_pb, uint32 _bl, const int64 &_off, uint32 _flags){
	Mutex::Locker lock(mutpool.object(_fileid));
	//if(d.fv[_fileid].pfile)// the file cannot be deleted because of the usecount!!
	return fv[_fileid].pfile->write(_pb, _bl, _off);
}
int FileManager::Data::fileRead(uint _fileid, char *_pb, uint32 _bl, const int64 &_off, uint32 _flags){
	Mutex::Locker lock(mutpool.object(_fileid));
	//if(d.fv[_fileid].pfile)// the file cannot be deleted because of the usecount!!
	return fv[_fileid].pfile->read(_pb, _bl, _off);
}
int FileManager::Data::fileSize(uint _fileid){
	Mutex::Locker lock(mutpool.object(_fileid));
	//if(d.fv[_fileid].pfile)// the file cannot be deleted because of the usecount!!
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
	--sz;
	++fv[_pos].uid;
}
//used outside execute
unsigned FileManager::Data::createFilePositionExt(){
	if(fs.size()){
		uint t = fs.top();
		fs.pop();
		return t;
	}else{
		++freeidx;
		return fv.size() + freeidx - 1;
	}
}
//used outside execute
void FileManager::Data::collectFilePositionExt(unsigned _pos){
	if(_pos < fv.size()){
		fs.push(_pos);
		--sz;
		++fv[_pos].uid;
	}else{
		--freeidx;
		cassert((_pos - fv.size()) == freeidx);
	}
}

void FileManager::Data::eraseToutPos(unsigned _pos){
	cassert(_pos < toutv.size());
	toutv[_pos] = toutv.back();
	fv[toutv.back()].toutidx = _pos;
	toutv.pop_back();
}
//--------------------------------------------------------------------------
// FileIStream
FileIStream::~FileIStream(){
	Server::the().fileManager().releaseIStream(fileid);
}

int FileIStream::read(char * _pb, uint32 _bl, uint32 _flags){
	int rv = rd.fileRead(fileid, _pb, _bl, off, _flags);
	if(rv > 0) off += rv;
	return rv;
}

int FileIStream::release(){	
	return -1;
}

int64 FileIStream::seek(int64 _off, SeekRef _ref){
	cassert(_ref == SeekBeg);
	off = _off;
	return off;
}
int64 FileIStream::size()const{
	return rd.fileSize(fileid);
}

//--------------------------------------------------------------------------
// FileOStream
FileOStream::~FileOStream(){
	Server::the().fileManager().releaseOStream(fileid);
}
int  FileOStream::write(const char *_pb, uint32 _bl, uint32 _flags){
	int rv = rd.fileWrite(fileid, _pb, _bl, off, _flags);
	if(rv > 0) off += rv;
	return rv;
}

int FileOStream::release(){
	return -1;
}

int64 FileOStream::seek(int64 _off, SeekRef _ref){
	cassert(_ref == SeekBeg);
	off = _off;
	return off;
}

int64 FileOStream::size()const{
	return rd.fileSize(fileid);
}

//--------------------------------------------------------------------------
// FileIOStream
FileIOStream::~FileIOStream(){
	Server::the().fileManager().releaseIOStream(fileid);
}

int FileIOStream::read(char * _pb, uint32 _bl, uint32 _flags){
	int rv = rd.fileRead(fileid, _pb, _bl, off, _flags);
	if(rv > 0) off += rv;
	return rv;
}

int  FileIOStream::write(const char *_pb, uint32 _bl, uint32 _flags){
	int rv = rd.fileWrite(fileid, _pb, _bl, off, _flags);
	if(rv > 0) off += rv;
	return rv;
}

int FileIOStream::release(){
	return -1;
}

int64 FileIOStream::seek(int64 _off, SeekRef _ref){
	cassert(_ref == SeekBeg);
	off = _off;
	return off;
}

int64 FileIOStream::size()const{
	return rd.fileSize(fileid);
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
	idbgx(Dbg::filemanager, "open file "<<_fileid);
	string fn;
	rk.fileName(_rsm, _fileid, fn);
	if(FileDevice::open(fn.c_str(), FileDevice::RW) < 0){
		if(canRetryOpen()) return NOK;//will return true if errno is ENFILE or ENOMEM
		if(flags & FileManager::Create){
			if(FileDevice::create(fn.c_str(), FileDevice::RW)){
				if(canRetryOpen()) return NOK;//will return true if errno is ENFILE or ENOMEM
				idbgx(Dbg::filemanager, "");
				return BAD;
			}else{
				state = Opened;
				return OK;
			}
		}else{
			return BAD;
		}
	}
	state = Opened;
	return OK;
}

int File::stream(FileManager& _rsm, uint _fileid, StreamPtr<IStream> &_sptr, const RequestUid &_roi, uint32 _flags){
	{
		if(_flags & FileManager::Forced){
			if(!state || _flags & FileManager::ForcePending) return BAD;
		}else if(!state || ousecnt || owq.size() || _flags & FileManager::ForcePending){//no writers and no writers waiting
			iwq.push(WaitData(_roi, _flags));
			return NOK;
		}
		++iusecnt;
	}
	_sptr = new FileIStream(_rsm.d, _fileid);
	return OK;
}

int File::stream(FileManager& _rsm, uint _fileid, StreamPtr<OStream> &_sptr, const RequestUid &_roi, uint32 _flags){
	{
		if(_flags & FileManager::Forced){
			//TODO: maybe you dont need owq.size() too
			if(!state || ousecnt || owq.size() || _flags & FileManager::ForcePending) return BAD;
		}else if(!state || ousecnt || iusecnt || owq.size() || _flags & FileManager::ForcePending){
			owq.push(WaitData(_roi, _flags));
			return NOK;
		}
		++ousecnt;
	}
	_sptr = new FileOStream(_rsm.d, _fileid);
	return OK;
}

int File::stream(FileManager& _rsm, uint _fileid, StreamPtr<IOStream> &_sptr, const RequestUid &_roi, uint32 _flags){
	{
		if(_flags & FileManager::Forced){
			//TODO: maybe you dont need owq.size() too
			if(!state || ousecnt || owq.size() || _flags & FileManager::ForcePending) return BAD;
		}else if(!state || ousecnt || iusecnt || owq.size() || _flags & FileManager::ForcePending){
			owq.push(WaitData(_roi, _flags | FileManager::IOStreamRequest));
			return NOK;
		}
		++ousecnt;
	}
	_sptr = new FileIOStream(_rsm.d, _fileid);
	return OK;
}

bool File::decreaseOutCount(){
	--ousecnt;
	if(!ousecnt){
		//return true if there are waiting readers or writers or noone waiting
		return iwq.size() || owq.size() || (owq.empty() && iwq.empty());
	}
	//we must signal the filemanager to take the next writer
	return true;
}

bool File::decreaseInCount(){
	--iusecnt;
	if(!iusecnt){
		return owq.size() || (owq.empty() && iwq.empty());//return true if there are waiting writers or noone waiting
	}
	return false;
}

int File::executeSignal(FileManager &_rsm, const FileUidTp &_fuid, uint32 _flags, uint32 &_rtout){
	int		inuse;
	uint32	ttout;
	if(_flags & Timeout){
		cassert(!ousecnt && !iusecnt && iwq.empty() && owq.empty());
		return BAD;
	}
	cassert(state);//file is opened
	inuse = signalStreams(_rsm, _fuid);
	ttout = msectout;

	if(inuse){
		return NOK;//wait indefinetly
	}else{
		if(msectout){
			_rtout = ttout;
			return OK;//wait
		}else return BAD;
	}
}

int File::executeOpen(FileManager &_rsm, const FileUidTp &_fuid, uint32 _flags, uint32 &_rtout, Mutex &_rm){
	//_rm.unlock();
	cassert(!state);
	int		inuse;
	uint32	ttout;
	switch(this->open(_rsm, _fuid.first)){
		case BAD:return BAD;
		case OK:
			_rtout = 0;//infinite
			_rm.lock();
			state = Opened;
			inuse = signalStreams(_rsm, _fuid);
			ttout = msectout;
			_rm.unlock();
			cassert(inuse);//TODO: see if it's ok!!!
			return OK;//wait indefinetly
		case NOK:
			_rtout += _rsm.fileOpenTimeout();
			return NOK;
	}
	cassert(false);
	return BAD;
}

void File::clear(FileManager &_rsm, const FileUidTp &_fuid){
	while(this->iwq.size()){//signal all istreams
		_rsm.sendError(-1, iwq.front().requid);
		iwq.pop();
	}
	while(this->owq.size()){//signal all istreams
		_rsm.sendError(-1, owq.front().requid);
		owq.pop();
	}
}

bool File::signalStreams(FileManager &_rsm, const FileUidTp &_fuid){
	if(ousecnt) return true;//can do nothing
	if(this->owq.size()){//we have writers waiting
		if(iusecnt) return true;
		if(owq.front().flags & FileManager::IOStreamRequest){
			StreamPtr<IOStream> sptr(new FileIOStream(_rsm.d, _fuid.first));
			_rsm.sendStream(sptr, _fuid, owq.front().requid);
		}else{
			StreamPtr<OStream> sptr(new FileOStream(_rsm.d, _fuid.first));
			_rsm.sendStream(sptr, _fuid, owq.front().requid);
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

