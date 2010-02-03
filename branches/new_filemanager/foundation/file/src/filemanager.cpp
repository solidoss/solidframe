
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
	
	typedef MutualObjectContainer<Mutex>	MutexContainerT;
	typedef Stack<ulong>					FreeStackT;
	struct FileData{
		FileData(
			File *_pfile = NULL
		):pfile(_pfile), toutidx(-1), uid(0), tout(TimeSpec::max),inexecq(false), events(0){}
		void clear(){
			pfile = NULL;
			++uid;
			tout = TimeSpec::max;
			inexecq = false;
		}
		File		*pfile;
		int32		toutidx;
		uint32		uid;
		TimeSpec	tout;
		bool		inexecq;
		uint16		events;
	};
	
	typedef std::deque<FileData>		FileVectorT;
	typedef std::deque<int32>			TimeoutVectorT;
	typedef std::vector<Mapper*>		MapperVectorT;
	typedef Queue<uint32>				MapperQueueT;
	typedef Queue<File*>				FileQueueT;
	
	
	Data(Controller *_pc):pc(_pc), sz(0), mut(NULL){}
	~Data();
	
//data:
	Controller				*pc;//pointer to controller
	uint32					sz;
	Mutex					*mut;
	MutexContainerT		mutpool;
	FileVectorT			fv;//file vector
	MapperVectorT			mv;//mapper vector
	MapperQueueT			meq;//mapper execution queue
	FileQueueT				feq;//file execution q
	FileQueueT				tmpfeq;//temp file execution q
	FileQueueT				delfq;//file delete  q
	FileQueueT				regfq;//file register  q
	FreeStackT				fs;//free stack
	TimeSpec				tout;
};

//------------------------------------------------------------------
//------------------------------------------------------------------
//------------------------------------------------------------------
//------------------------------------------------------------------

Manager::Manager(Controller *_pc):d(*(new Data(_pc))){
	_pc->init(InitStub(*this));
}

Manager::~Manager(){
	idbgx(Dbg::file, "");
	d.pc->removeFileManager();
	delete &d;
}
//------------------------------------------------------------------

int Manager::execute(ulong _evs, TimeSpec &_rtout){
	d.mut->unlock();
	if(signaled()){
		ulong sm = grabSignalMask(0);
		idbgx(Dbg::file, "signalmask "<<sm);
		if(sm & fdt::S_KILL){
			state(Data::Stopping);
			idbgx(Dbg::file, "kill "<<d.sz);
			if(!d.sz){//no file
				state(-1);
				d.mut->unlock();
				idbgx(Dbg::file, "");
				return BAD;
			}
		}
	}
	Stub s(*this);
	
	//copy the queued files into a temp queue - to be used outside lock
	while(d.feq.size()){
		s.pushFileTempExecQueue(d.feq.front());
		d.feq.pop();
	}
	
	doDeleteFiles();
	doRegisterFiles();
	
	d.mut->unlock();//done with the locking
	
	doScanTimeout(_rtout);
	
	doExecuteMappers();
	
	uint32 tmpfeqsz(d.tmpfeq.size());
	while(tmpfeqsz--){
		File	&rf(*d.tmpfeq.front());
		
		d.tmpfeq.pop();
		
		if(rf.isRegistered()){
			doExecuteRegistered(rf, _rtout);
		}else{//not registered
			doExecuteUnregistered(rf, _rtout);
		}
	}
	
	if(d.meq.size() || d.delfq.size() || d.tmpfeq.size() || d.regfq.size()) return OK;
	
	if(!d.sz && state() == Data::Stopping){
		state(-1);
		return BAD;
	}
	
	if(d.tout > _rtout){
		_rtout = d.tout;
	}
	return NOK;
}
//------------------------------------------------------------------
void Manager::doExecuteMappers(){
	uint32	tmpqsz(d.meq.size());
	Stub 	s(*this);
	while(tmpqsz--){
		const ulong 	v(d.meq.front());
		Mapper			*pm(d.mv[v]);
		
		d.meq.pop();
		pm->execute(s);
	}
}
//------------------------------------------------------------------
void Manager::doExecuteRegistered(File &_rf, const TimeSpec &_rtout){
	Mutex			&rm(d.mutpool.object(_rf.id()));
	Data::FileData	&rfd(d.fv[_rf.id()]);
	TimeSpec 		ts(_rtout);
	Stub 			s(*this);
	uint16			evs(rfd.events);
	
	rfd.tout = TimeSpec::max;
	rfd.inexecq = false;
	rfd.events = 0;
	
	switch(_rf.execute(s, evs, ts, rm)){
		case File::Bad:
			d.delfq.push(&_rf);
			break;
		case File::Ok:
			d.tmpfeq.push(&_rf);
			rfd.inexecq = true;
			break;//reschedule
		case File::No:
			if(state() == Data::Running){
				if(ts != _rtout){
					rfd.tout = ts;
					//d.insertTimeWait(rfd);
				}else{
					rfd.tout = TimeSpec::max;
				}
			}else{
				d.tmpfeq.push(&_rf);
				rfd.events = TIMEOUT;
			}
			break;
	}
}
//------------------------------------------------------------------
void Manager::doExecuteUnregistered(File &_rf, const TimeSpec &_rtout){
	//all we need is to call mapper::open
	Stub 		s(*this);
	Mapper		&mid(s.mapper(_rf.key().mapperId()));
	
	switch(mid.open(_rf)){
		case File::Ok://file is opened, register it
			d.regfq.push(&_rf);
			break;
		default://file not open, queued
			break;
	}
}
//------------------------------------------------------------------
void Manager::doDeleteFiles(){
	Stub	s(*this);
	
	while(d.delfq.size()){
		File *pf(d.delfq.front());
		d.delfq.pop();
		if(pf->isRegistered()){
			d.fs.push(pf->id());
			--d.sz;
			d.fv[pf->id()].clear();
		}
		uint32 mid(pf->key().mapperId());
		if(s.mapper(mid).erase(pf)){
			d.meq.push(mid);
		}
	}
}
//------------------------------------------------------------------
void Manager::doRegisterFiles(){
	while(d.regfq.size()){
		File *pf(d.regfq.front());
		d.regfq.pop();
		cassert(!pf->isRegistered());
		++d.sz;
		if(d.fs.size()){
			uint32 id(d.fs.top());
			d.fs.pop();
			Data::FileData	&rfd(d.fv[id]);
			rfd.pfile = pf;
			pf->id(id);
		}else{
			pf->id(d.fv.size());
			d.fv.push_back(Data::FileData(pf));
		}
		d.tmpfeq.push(pf);
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
					d.tmpfeq.push(rfd.pfile);
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
		idbgx(Dbg::file, "sq.push "<<_fileid);
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
		idbgx(Dbg::file, "sq.push "<<_fileid);
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
	
	Stub			mstub(*this);
	ulong			mid(_rk.mapperId());
	Mapper			&rm(*d.mv[mid]);
	File			*pf = rm.findOrCreateFile(_rk);
	int				rv(BAD);
	
	if(!pf) return BAD;
	
	if(!pf->isOpened())
		_flags |= Manager::ForcePending;
	
	if(pf->isRegistered()){//
		const IndexT	fid = pf->id();
		Mutex::Locker	lock2(d.mutpool.object(fid));
		_rfuid.first = fid;
		_rfuid.second = d.fv[fid].uid; 
		rv = pf->stream(mstub, _sptr, _requid, _flags);
	}else{
		//delay stream creation until successfull file open
		rv = pf->stream(mstub, _sptr, _requid, _flags);
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
	}else{
		TempKey k;
		return stream(_sptr, _rfuid, _rrequid, k, _flags | Create);
	}
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
	}else{
		TempKey k;
		return stream(_sptr, _rfuid, _rrequid, k, _flags | Create);
	}
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
	}else{
		TempKey k;
		return stream(_sptr, _rfuid, _rrequid, k, _flags | Create);
	}
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
	}else{
		TempKey k;
		return stream(_sptr, fuid, requid, k, _flags | NoWait | Create);
	}
}

int Manager::stream(StreamPointer<OStream> &_sptr, const char *_fn, uint32 _flags){
	FileUidT	fuid;
	RequestUid	requid;
	if(_fn){
		FastNameKey k(_fn);
		return stream(_sptr, fuid, requid, k, _flags | NoWait);
	}else{
		TempKey k;
		return stream(_sptr, fuid, requid, k, _flags | NoWait | Create);
	}
}

int Manager::stream(StreamPointer<IOStream> &_sptr, const char *_fn, uint32 _flags){
	FileUidT	fuid;
	RequestUid	requid;
	if(_fn){
		FastNameKey k(_fn);
		return stream(_sptr, fuid, requid, k, _flags | NoWait);
	}else{
		TempKey k;
		return stream(_sptr, fuid, requid, k, _flags | NoWait | Create);
	}
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
		if(d.fv[_rfuid.first].pfile){
			Stub	s(*this);
			return d.fv[_rfuid.first].pfile->stream(s, _sptr, _requid, _flags);
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
		if(d.fv[_rfuid.first].pfile){
			Stub	s(*this);
			return d.fv[_rfuid.first].pfile->stream(s, _sptr, _requid, _flags);
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
		if(d.fv[_rfuid.first].pfile){
			Stub	s(*this);
			return d.fv[_rfuid.first].pfile->stream(s, _sptr, _requid, _flags);
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
void Manager::Stub::pushFileTempExecQueue(File *_pf){
	if(_pf->isRegistered()){
		Data::FileData &rfd(rm.d.fv[_pf->id()]);
		if(!rfd.inexecq){
			rm.d.tmpfeq.push(_pf);
			rfd.inexecq = true;
		}
	}else{
		rm.d.tmpfeq.push(_pf);
	}
}
Mapper &Manager::Stub::mapper(ulong _id){
	cassert(_id < rm.d.mv.size());
	return *rm.d.mv[_id];
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


