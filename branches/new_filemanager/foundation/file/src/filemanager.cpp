
/* Implementation file filemanager.cpp
	
	Copyright 2010 Valentin Palade 
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

#include <deque>
#include <vector>

#include "system/debug.hpp"
#include "system/timespec.hpp"
#include "system/filedevice.hpp"
#include "system/mutex.hpp"
#include "system/thread.hpp"
#include "system/timespec.hpp"

#include "utility/mutualobjectcontainer.hpp"
#include "utility/iostream.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"


#include "foundation/manager.hpp"

#include "foundation/file/filemanager.hpp"
#include "foundation/file/filekeys.hpp"
#include "foundation/file/filebase.hpp"
#include "foundation/file/filemapper.hpp"

namespace fdt = foundation;

namespace foundation{
namespace file{

//------------------------------------------------------------------

struct Manager::Data{
	enum {Running = 1, Stopping, Stopped = -1};
	
	typedef MutualObjectContainer<Mutex>		MutexContainerT;
	typedef Stack<IndexT	>					FreeStackT;
	struct FileData{
		FileData(
			File *_pfile = NULL
		):pfile(_pfile), /*toutidx(-1), */uid(0), tout(TimeSpec::max),inexecq(false), events(0){}
		void clear(){
			pfile = NULL;
			++uid;
			tout = TimeSpec::max;
			inexecq = false;
		}
		File		*pfile;
		//int32		toutidx;
		uint32		uid;
		TimeSpec	tout;
		bool		inexecq;
		uint16		events;
	};
	template <class Str>
	struct SendStreamData{
		SendStreamData(){}
		SendStreamData(
			const StreamPointer<Str> &_rs,
			const FileUidT	&_rfuid,
			const RequestUid _rrequid
		):s(_rs), fuid(_rfuid), requid(_rrequid){}
		StreamPointer<Str>	s;
		FileUidT			fuid;
		RequestUid			requid;
	};
	
	typedef SendStreamData<OStream>		SendOStreamDataT;
	typedef SendStreamData<IStream>		SendIStreamDataT;
	typedef SendStreamData<IOStream>	SendIOStreamDataT;
	typedef std::pair<int, RequestUid>	SendErrorDataT;
	
	typedef std::deque<FileData>		FileVectorT;
	typedef std::deque<int32>			TimeoutVectorT;
	typedef std::vector<Mapper*>		MapperVectorT;
	typedef Queue<uint32>				Index32QueueT;
	typedef Queue<IndexT>				IndexQueueT;
	typedef Queue<File*>				FileQueueT;
	typedef Queue<SendOStreamDataT>		SendOStreamQueueT;
	typedef Queue<SendIOStreamDataT>	SendIOStreamQueueT;
	typedef Queue<SendIStreamDataT>		SendIStreamQueueT;
	typedef Queue<SendErrorDataT>		SendErrorQueueT;
	
	
	Data(Controller *_pc):pc(_pc), sz(0), mut(NULL), mutpool(0, 3, 5){}
	~Data(){}
	void pushFileInTemp(File *_pf);
//data:
	Controller				*pc;//pointer to controller
	uint32					sz;
	Mutex					*mut;
	MutexContainerT			mutpool;
	FileVectorT				fv;//file vector
	MapperVectorT			mv;//mapper vector
	Index32QueueT			meq;//mapper execution queue
	FileQueueT				feq;//file execution q
	IndexQueueT				tmpfeq;//temp file execution q
	IndexQueueT				delfq;//file delete  q
	FreeStackT				fs;//free stack
	TimeSpec				tout;
	SendOStreamQueueT		sndosq;
	SendIOStreamQueueT		sndiosq;
	SendIStreamQueueT		sndisq;
	SendErrorQueueT			snderrq;
};
//------------------------------------------------------------------
void Manager::Data::pushFileInTemp(File *_pf){
	IndexT idx;
	if(_pf->isRegistered()){
		idx = _pf->id();
	}else{
		if(fs.size()){
			idx = fs.top();fs.pop();
			Mutex::Locker lock(mutpool.safeObject(idx));
			fv[idx].pfile = _pf;
		}else{
			idx = fv.size();
			Mutex::Locker lock(mutpool.safeObject(idx));
			fv.push_back(FileData(_pf));
		}
		_pf->id(idx);
		++sz;
	}
	FileData &rfd(fv[idx]);
	if(!rfd.inexecq){
		rfd.inexecq = true;
		tmpfeq.push(idx);
	}
}
//------------------------------------------------------------------
Manager::Controller::~Controller(){
}
//------------------------------------------------------------------
//------------------------------------------------------------------

Manager::Manager(Controller *_pc):d(*(new Data(_pc))){
	_pc->init(InitStub(*this));
	state(Data::Running);
}

Manager::~Manager(){
	idbgx(Dbg::file, "");
	for(Data::MapperVectorT::const_iterator it(d.mv.begin()); it != d.mv.end(); ++it){
		delete *it;
	}
	delete d.pc;
	delete &d;
}
//------------------------------------------------------------------

int Manager::execute(ulong _evs, TimeSpec &_rtout){
	d.mut->lock();
	if(signaled()){
		ulong sm = grabSignalMask(0);
		idbgx(Dbg::file, "signalmask "<<sm);
		if(sm & fdt::S_KILL){
			state(Data::Stopping);
			vdbgx(Dbg::file, "kill "<<d.sz);
			if(!d.sz){//no file
				state(-1);
				d.mut->unlock();
				vdbgx(Dbg::file, "");
				d.pc->removeFileManager();
				return BAD;
			}
			doPrepareStop();
		}
	}
	Stub s(*this);
	
	//copy the queued files into a temp queue - to be used outside lock
	while(d.feq.size()){
		d.pushFileInTemp(d.feq.front());
		d.feq.pop();
	}
	
	doDeleteFiles();
	
	d.mut->unlock();//done with the locking
	
	doScanTimeout(_rtout);
	
	doExecuteMappers();
	
	uint32 tmpfeqsz(d.tmpfeq.size());
	
	while(tmpfeqsz--){
		IndexT idx(d.tmpfeq.front());
		
		d.tmpfeq.pop();
		
		doExecuteFile(idx, _rtout);
	}
	doSendStreams();
	if(d.meq.size() || d.delfq.size() || d.tmpfeq.size()) return OK;
	
	if(!d.sz && state() == Data::Stopping){
		state(-1);
		d.pc->removeFileManager();
		return BAD;
	}
	
	if(d.tout > _rtout){
		_rtout = d.tout;
	}
	return NOK;
}
//------------------------------------------------------------------
void Manager::doSendStreams(){
	while(d.sndisq.size()){
		d.pc->sendStream(
			d.sndisq.front().s,
			d.sndisq.front().fuid,
			d.sndisq.front().requid
		);
		d.sndisq.pop();
	}
	while(d.sndiosq.size()){
		d.pc->sendStream(
			d.sndiosq.front().s,
			d.sndiosq.front().fuid,
			d.sndiosq.front().requid
		);
		d.sndiosq.pop();
	}
	while(d.sndosq.size()){
		d.pc->sendStream(
			d.sndosq.front().s,
			d.sndosq.front().fuid,
			d.sndosq.front().requid
		);
		d.sndosq.pop();
	}
	while(d.snderrq.size()){
		d.pc->sendError(
			d.snderrq.front().first,
			d.snderrq.front().second
		);
		d.snderrq.pop();
	}
}
//------------------------------------------------------------------
void Manager::doPrepareStop(){
	//call stop for all mappers:
	for(Data::MapperVectorT::const_iterator it(d.mv.begin()); it != d.mv.end(); ++it){
		(*it)->stop();
	}
	//empty the mapper q
	while(d.meq.size()) d.meq.pop();
	
	//move the incomming files to temp, before iterating through all the files
	while(d.feq.size()){
		d.pushFileInTemp(d.feq.front());
		d.feq.pop();
	}
	//signall all existing files to stop;
	for(Data::FileVectorT::iterator it(d.fv.begin()); it != d.fv.end(); ++it){
		Data::FileData &rfd(*it);
		if(!rfd.pfile) continue;
		rfd.events |= File::MustDie;
		if(!rfd.inexecq){
			rfd.inexecq = true;
			d.tmpfeq.push(rfd.pfile->id());
		}
	}
}
//------------------------------------------------------------------
void Manager::doExecuteMappers(){
	uint32	tmpqsz(d.meq.size());
	Stub 	s(*this);
	cassert(!(tmpqsz && state() != Data::Running));
	//execute the enqued mappers
	while(tmpqsz--){
		const ulong 	v(d.meq.front());
		Mapper			*pm(d.mv[v]);
		
		d.meq.pop();
		pm->execute(s);
	}
}
//------------------------------------------------------------------
void Manager::doExecuteFile(const IndexT &_idx, const TimeSpec &_rtout){
	Mutex			&rm(d.mutpool.object(_idx));
	Data::FileData	&rfd(d.fv[_idx]);
	TimeSpec 		ts(_rtout);
	Stub 			s(*this);
	uint16			evs(rfd.events);
	
	rfd.tout = TimeSpec::max;
	rfd.inexecq = false;
	rfd.events = 0;
	
	switch(rfd.pfile->execute(s, evs, ts, rm)){
		case File::Bad:
			d.delfq.push(_idx);
			/*NOTE:
				The situation is spooky:
				- the file is closed but it is not unregistered from the mapper
				- we cannot close the file in the doDeleteFiles method, because
				closing may mean flushing an will take a lot of time.
				- so there will be a method tryRevive in file, which 
				will revive the file if there are incomming requests.
			*/
			break;
		case File::Ok:
			d.tmpfeq.push(_idx);
			rfd.inexecq = true;
			break;//reschedule
		case File::No:
			if(ts != _rtout){
				rfd.tout = ts;
				if(d.tout > rfd.tout){
					d.tout = rfd.tout;
				}
			}else{
				rfd.tout = TimeSpec::max;
			}
			break;
	}
}
//------------------------------------------------------------------
void Manager::doDeleteFiles(){
	Stub	s(*this);
	
	while(d.delfq.size()){
		const IndexT	idx(d.delfq.front());
		Data::FileData	&rfd(d.fv[d.delfq.front()]);
		File			*pfile;
		
		d.delfq.pop();
		
		{
			
			//Mutex::Locker lock(d.mutpool.object(idx));
			
			if(rfd.pfile->tryRevive()){
				if(!rfd.inexecq){
					rfd.inexecq = true;
					d.tmpfeq.push(idx);
				}
				continue;
			}
			d.fs.push(rfd.pfile->id());
			--d.sz;
			pfile = rfd.pfile;
			rfd.clear();
		}
		
		uint32 mid(pfile->key().mapperId());
		//mapper creates - mapper destroys files
		if(s.mapper(mid).erase(pfile)){
			cassert(state() == Data::Running);
			d.meq.push(mid);
		}
	}
}
//------------------------------------------------------------------
void Manager::doScanTimeout(const TimeSpec &_rtout){
	if(_rtout < d.tout) return;
	d.tout = TimeSpec::max;
	for(Data::FileVectorT::iterator it(d.fv.begin()); it != d.fv.end(); ++it){
		Data::FileData &rfd(*it);
		if(rfd.pfile){
			if(rfd.tout <= _rtout){
				if(!rfd.inexecq){
					rfd.inexecq = true;
					d.tmpfeq.push(rfd.pfile->id());
				}
				rfd.events |= File::Timeout;
			}else if(d.tout > rfd.tout){
				d.tout = rfd.tout;
			}
		}
	}
}
//------------------------------------------------------------------
//overload from object
void Manager::mutex(Mutex *_pmut){
	d.mut = _pmut;
}
//------------------------------------------------------------------
void Manager::releaseIStream(IndexT _fileid){
	bool b = false;
	{
		Mutex::Locker lock(d.mutpool.object(_fileid));
		b = d.fv[_fileid].pfile->decreaseInCount();
	}
	if(b){
		Mutex::Locker	lock(*d.mut);
		//we must signal the filemanager
		d.feq.push(d.fv[_fileid].pfile);
		vdbgx(Dbg::file, "sq.push "<<_fileid);
		if(static_cast<fdt::Object*>(this)->signal((int)fdt::S_RAISE)){
			fdt::Manager::the().raiseObject(*this);
		}
	}
}
//------------------------------------------------------------------
void Manager::releaseOStream(IndexT _fileid){
	bool b = false;
	{
		Mutex::Locker lock(d.mutpool.object(_fileid));
		b = d.fv[_fileid].pfile->decreaseOutCount();
	}
	if(b){
		Mutex::Locker	lock(*d.mut);
		//we must signal the filemanager
		d.feq.push(d.fv[_fileid].pfile);
		vdbgx(Dbg::file, "sq.push "<<_fileid);
		if(static_cast<fdt::Object*>(this)->signal((int)fdt::S_RAISE)){
			fdt::Manager::the().raiseObject(*this);
		}
	}
}
//------------------------------------------------------------------
void Manager::releaseIOStream(IndexT _fileid){
	releaseOStream(_fileid);
}
//------------------------------------------------------------------
int Manager::fileWrite(
	IndexT _fileid,
	const char *_pb,
	uint32 _bl,
	const int64 &_off,
	uint32 _flags
){
	Mutex::Locker lock(d.mutpool.object(_fileid));
	return d.fv[_fileid].pfile->write(_pb, _bl, _off, _flags);
}
//------------------------------------------------------------------
int Manager::fileRead(
	IndexT _fileid,
	char *_pb,
	uint32 _bl,
	const int64 &_off,
	uint32 _flags
){
	Mutex::Locker lock(d.mutpool.object(_fileid));
	return d.fv[_fileid].pfile->read(_pb, _bl, _off, _flags);
}
//------------------------------------------------------------------
int64 Manager::fileSize(IndexT _fileid){
	Mutex::Locker lock(d.mutpool.object(_fileid));
	return d.fv[_fileid].pfile->size();
}
//===================================================================
// stream method - the basis for all stream methods
template <typename StreamP>
int Manager::doGetStream(
	StreamP &_sptr,
	FileUidT &_rfuid,
	const RequestUid &_requid,
	const Key &_rk,
	uint32 _flags
){
	Mutex::Locker	lock1(*d.mut);
	
	if(state() != Data::Running) return BAD;
	
	Stub			s(*this);
	ulong			mid(_rk.mapperId());
	Mapper			&rm(*d.mv[mid]);
	File			*pf = rm.findOrCreateFile(_rk);
	int				rv(BAD);
	
	if(!pf) return BAD;
	
	if(pf->isRegistered()){//
		const IndexT	fid = pf->id();
		Mutex::Locker	lock2(d.mutpool.object(fid));
		_rfuid.first = fid;
		_rfuid.second = d.fv[fid].uid; 
		rv = pf->stream(s, _sptr, _requid, _flags);
	}else{
		//delay stream creation until successfull file open
		rv = pf->stream(s, _sptr, _requid, _flags | Manager::ForcePending);
	}
	
	switch(rv){
		case File::MustSignal:
			d.feq.push(pf);
			if(static_cast<fdt::Object*>(this)->signal((int)fdt::S_RAISE)){
				fdt::Manager::the().raiseObject(*this);
			}
		case File::MustWait:
			return NOK;
		default: return rv;
	}
}
//===================================================================
// Most general stream methods
int Manager::stream(
	StreamPointer<IStream> &_sptr,
	FileUidT &_rfuid,
	const RequestUid &_requid,
	const Key &_rk,
	uint32 _flags
){
	if(state() != Data::Running) return BAD;
	return doGetStream(_sptr, _rfuid, _requid, _rk, _flags);
}
int Manager::stream(
	StreamPointer<OStream> &_sptr,
	FileUidT &_rfuid,
	const RequestUid &_requid,
	const Key &_rk,
	uint32 _flags
){	
	if(state() != Data::Running) return BAD;
	return doGetStream(_sptr, _rfuid, _requid, _rk, _flags);
}

int Manager::stream(
	StreamPointer<IOStream> &_sptr,
	FileUidT &_rfuid,
	const RequestUid &_requid,
	const Key &_rk,
	uint32 _flags
){	
	if(state() != Data::Running) return BAD;
	return doGetStream(_sptr, _rfuid, _requid, _rk, _flags);
}


//====================== stream method proxies =========================
// proxies for the above methods
int Manager::stream(
	StreamPointer<IStream> &_sptr,
	const RequestUid &_rrequid,
	const char *_fn,
	uint32 _flags
){
	FileUidT fuid;
	return stream(_sptr, fuid, _rrequid, _fn, _flags);
}

int Manager::stream(
	StreamPointer<OStream> &_sptr,
	const RequestUid &_rrequid,
	const char *_fn,
	uint32 _flags
){
	FileUidT fuid;
	return stream(_sptr, fuid, _rrequid, _fn, _flags);
}

int Manager::stream(
	StreamPointer<IOStream> &_sptr,
	const RequestUid &_rrequid,
	const char *_fn,
	uint32 _flags
){
	FileUidT fuid;
	return stream(_sptr, fuid, _rrequid, _fn, _flags);
}
//---------------------------------------------------------------
int Manager::streamSpecific(
	StreamPointer<IStream> &_sptr,
	const char *_fn,
	uint32 _flags
){
	return stream(_sptr, *requestuidptr, _fn, _flags);
}
int Manager::streamSpecific(
	StreamPointer<OStream> &_sptr,
	const char *_fn,
	uint32 _flags
){
	return stream(_sptr, *requestuidptr, _fn, _flags);
}
int Manager::streamSpecific(
	StreamPointer<IOStream> &_sptr,
	const char *_fn,
	uint32 _flags
){
	return stream(_sptr, *requestuidptr, _fn, _flags);
}
//---------------------------------------------------------------
int Manager::stream(
	StreamPointer<IStream> &_sptr,
	FileUidT &_rfuid,
	const RequestUid &_rrequid,
	const char *_fn,
	uint32 _flags
){
	if(_fn){
		FastNameKey k(_fn);
		return stream(_sptr, _rfuid, _rrequid, k, _flags);
	}
	return BAD;
}

int Manager::stream(
	StreamPointer<OStream> &_sptr,
	FileUidT &_rfuid,
	const RequestUid &_rrequid,
	const char *_fn,
	uint32 _flags
){
	if(_fn){
		FastNameKey k(_fn);
		return stream(_sptr, _rfuid, _rrequid, k, _flags);
	}
	return BAD;
}

int Manager::stream(
	StreamPointer<IOStream> &_sptr,
	FileUidT &_rfuid,
	const RequestUid &_rrequid,
	const char *_fn,
	uint32 _flags
){
	if(_fn){
		FastNameKey k(_fn);
		return stream(_sptr, _rfuid, _rrequid, k, _flags);
	}
	return BAD;
}
//---------------------------------------------------------------
int Manager::streamSpecific(
	StreamPointer<IStream> &_sptr,
	FileUidT &_rfuid,
	const char *_fn,
	uint32 _flags
){
	return stream(_sptr, _rfuid, *requestuidptr, _fn, _flags);
}
int Manager::streamSpecific(
	StreamPointer<OStream> &_sptr,
	FileUidT &_rfuid,
	const char *_fn,
	uint32 _flags
){
	return stream(_sptr, _rfuid, *requestuidptr, _fn, _flags);
}
int Manager::streamSpecific(
	StreamPointer<IOStream> &_sptr,
	FileUidT &_rfuid,
	const char *_fn,
	uint32 _flags
){
	return stream(_sptr, _rfuid, *requestuidptr, _fn, _flags);
}
//---------------------------------------------------------------
int Manager::stream(StreamPointer<IStream> &_sptr, const char *_fn, uint32 _flags){
	FileUidT	fuid;
	RequestUid	requid;
	if(_fn){
		FastNameKey k(_fn);
		return stream(_sptr, fuid, requid, k, _flags | NoWait);
	}
	return BAD;
}

int Manager::stream(StreamPointer<OStream> &_sptr, const char *_fn, uint32 _flags){
	FileUidT	fuid;
	RequestUid	requid;
	if(_fn){
		FastNameKey k(_fn);
		return stream(_sptr, fuid, requid, k, _flags | NoWait);
	}
	return BAD;
}

int Manager::stream(StreamPointer<IOStream> &_sptr, const char *_fn, uint32 _flags){
	FileUidT	fuid;
	RequestUid	requid;
	if(_fn){
		FastNameKey k(_fn);
		return stream(_sptr, fuid, requid, k, _flags | NoWait);
	}
	return BAD;
}
//-------------------------------------------------------------------------------------
int Manager::stream(StreamPointer<IStream> &_sptr, const Key &_rk, uint32 _flags){
	FileUidT	fuid;
	RequestUid	requid;
	return stream(_sptr, fuid, requid, _rk, _flags | NoWait);
}

int Manager::stream(StreamPointer<OStream> &_sptr, const Key &_rk, uint32 _flags){
	FileUidT	fuid;
	RequestUid	requid;
	return stream(_sptr, fuid, requid, _rk, _flags | NoWait);
}

int Manager::stream(StreamPointer<IOStream> &_sptr, const Key &_rk, uint32 _flags){
	FileUidT	fuid;
	RequestUid	requid;
	return stream(_sptr, fuid, requid, _rk, _flags | NoWait);
}
//=======================================================================
int Manager::stream(
	StreamPointer<IStream> &_sptr,
	const FileUidT &_rfuid,
	const RequestUid &_requid,
	uint32 _flags
){
	Mutex::Locker	lock1(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].uid == _rfuid.second){
		Mutex::Locker	lock2(d.mutpool.object(_rfuid.first));
		Data::FileData	&rfd(d.fv[_rfuid.first]);
		if(rfd.pfile){
			Stub	s(*this);
			return rfd.pfile->stream(s, _sptr, _requid, _flags);
		}
	}
	return BAD;
}

int Manager::stream(
	StreamPointer<OStream> &_sptr,
	const FileUidT &_rfuid,
	const RequestUid &_requid,
	uint32 _flags
){
	Mutex::Locker	lock1(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].uid == _rfuid.second){
		Mutex::Locker	lock2(d.mutpool.object(_rfuid.first));
		Data::FileData	&rfd(d.fv[_rfuid.first]);
		if(rfd.pfile){
			Stub	s(*this);
			return rfd.pfile->stream(s, _sptr, _requid, _flags);
		}
	}
	return BAD;
}

int Manager::stream(
	StreamPointer<IOStream> &_sptr,
	const FileUidT &_rfuid,
	const RequestUid &_requid,
	uint32 _flags
){
	Mutex::Locker	lock1(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].uid == _rfuid.second){
		Mutex::Locker	lock2(d.mutpool.object(_rfuid.first));
		Data::FileData	&rfd(d.fv[_rfuid.first]);
		if(d.fv[_rfuid.first].pfile){
			Stub	s(*this);
			return rfd.pfile->stream(s, _sptr, _requid, _flags);
		}
	}
	return BAD;
}
//=======================================================================
// The internal implementation of streams streams
//=======================================================================

class FileIStream: public IStream{
public:
	FileIStream(Manager &_rm, IndexT _fileid):rm(_rm), fileid(_fileid), off(0){}
	~FileIStream();
	int read(char *, uint32, uint32 _flags = 0);
	int release();
	int64 seek(int64, SeekRef);
	int64 size()const;
private:
	Manager			&rm;
	const IndexT	fileid;
	int64			off;
};
//------------------------------------------------------------------------------
class FileOStream: public OStream{
public:
	FileOStream(Manager &_rm, IndexT _fileid):rm(_rm), fileid(_fileid), off(0){}
	~FileOStream();
	int write(const char *, uint32, uint32 _flags = 0);
	int release();
	int64 seek(int64, SeekRef);
	int64 size()const;
private:
	Manager			&rm;
	const IndexT	fileid;
	int64			off;
};
//------------------------------------------------------------------------------
class FileIOStream: public IOStream{
public:
	FileIOStream(Manager &_rm, IndexT _fileid):rm(_rm), fileid(_fileid), off(0){}
	~FileIOStream();
	int read(char *, uint32, uint32 _flags = 0);
	int write(const char *, uint32, uint32 _flags = 0);
	int release();
	int64 seek(int64, SeekRef);
	int64 size()const;
private:
	Manager			&rm;
	const IndexT	fileid;
	int64			off;
};


//--------------------------------------------------------------------------
// FileIStream
FileIStream::~FileIStream(){
	rm.releaseIStream(fileid);
}

int FileIStream::read(char * _pb, uint32 _bl, uint32 _flags){
	int rv = rm.fileRead(fileid, _pb, _bl, off, _flags);
	if(rv > 0) off += rv;
	return rv;
}

int FileIStream::release(){	
	return BAD;
}

int64 FileIStream::seek(int64 _off, SeekRef _ref){
	cassert(_ref == SeekBeg);
	off = _off;
	return off;
}
int64 FileIStream::size()const{
	return rm.fileSize(fileid);
}

//--------------------------------------------------------------------------
// FileOStream
FileOStream::~FileOStream(){
	rm.releaseOStream(fileid);
}
int  FileOStream::write(const char *_pb, uint32 _bl, uint32 _flags){
	int rv = rm.fileWrite(fileid, _pb, _bl, off, _flags);
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
	return rm.fileSize(fileid);
}

//--------------------------------------------------------------------------
// FileIOStream
FileIOStream::~FileIOStream(){
	rm.releaseIOStream(fileid);
}

int FileIOStream::read(char * _pb, uint32 _bl, uint32 _flags){
	int rv = rm.fileRead(fileid, _pb, _bl, off, _flags);
	if(rv > 0) off += rv;
	return rv;
}

int  FileIOStream::write(const char *_pb, uint32 _bl, uint32 _flags){
	int rv = rm.fileWrite(fileid, _pb, _bl, off, _flags);
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
	return rm.fileSize(fileid);
}

//=======================================================================
// The Manager stub:
//=======================================================================
IStream* Manager::Stub::createIStream(IndexT _fileid){
	return new FileIStream(rm, _fileid);
}
OStream* Manager::Stub::createOStream(IndexT _fileid){
	return new FileOStream(rm, _fileid);
}
IOStream* Manager::Stub::createIOStream(IndexT _fileid){
	return new FileIOStream(rm, _fileid);
}
void Manager::Stub::pushFileTempExecQueue(const IndexT &_idx, uint16 _evs){
	Data::FileData &rfd(rm.d.fv[_idx]);
	rfd.events |= _evs;
	if(!rfd.inexecq){
		rm.d.tmpfeq.push(_idx);
		rfd.inexecq = true;
	}
}
Mapper &Manager::Stub::mapper(ulong _id){
	cassert(_id < rm.d.mv.size());
	return *rm.d.mv[_id];
}
uint32	Manager::Stub::fileUid(IndexT _uid)const{
	return rm.d.fv[_uid].uid;
}
Manager::Controller& Manager::Stub::controller(){
	return *rm.d.pc;
}
void Manager::Stub::push(
	const StreamPointer<IStream> &_rsp,
	const FileUidT &_rfuid,
	const RequestUid &_rrequid
	
){
	rm.d.sndisq.push(Manager::Data::SendIStreamDataT(_rsp, _rfuid, _rrequid));
}
void Manager::Stub::push(
	const StreamPointer<IOStream> &_rsp,
	const FileUidT &_rfuid,
	const RequestUid &_rrequid
){
	rm.d.sndiosq.push(Manager::Data::SendIOStreamDataT(_rsp, _rfuid, _rrequid));
}
void Manager::Stub::push(
	const StreamPointer<OStream> &_rsp,
	const FileUidT &_rfuid,
	const RequestUid &_rrequid
){
	rm.d.sndosq.push(Manager::Data::SendOStreamDataT(_rsp, _rfuid, _rrequid));
}
void Manager::Stub::push(
	int _err,
	const RequestUid& _rrequid
){
	rm.d.snderrq.push(Manager::Data::SendErrorDataT(_err, _rrequid));
}
//=======================================================================
uint32 Manager::InitStub::registerMapper(Mapper *_pm)const{
	rm.d.mv.push_back(_pm);
	_pm->id(rm.d.mv.size() - 1);
	return rm.d.mv.size() - 1;
}
//=======================================================================

}//namespace file
}//namespace foundation


