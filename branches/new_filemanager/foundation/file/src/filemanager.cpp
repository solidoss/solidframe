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

namespace fdt = foundation;

namespace foundation{
namespace file{

//------------------------------------------------------------------

struct Manager::Data{
	enum {Running = 1, Stopping, Stopped = -1};
	
	typedef MutualObjectContainer<Mutex>	MutexContainerTp;
	typedef Stack<uint>						FreeStackTp;
	struct FileData{
		FileData(File *_pfile = NULL):pfile(_pfile), toutidx(-1), uid(0), tout(TimeSpec::max){}
		FileData(const FileExtData &_rfext):pfile(_rfext.pfile), toutidx(-1), uid(0), tout(TimeSpec::max){}
		File		*pfile;
		int32		toutidx;
		uint32		uid;
		TimeSpec	tout;
	};
	typedef std::deque<FileData>		FileVectorTp;
	typedef std::deque<int32>			TimeoutVectorTp;
	typedef std::vector<Mapper*>		MapperVectorTp;
	typedef Queue<Mapper*>				MapperQueueTp;
	
	Data(Controller *_pc):pc(_pc), sz(0), freeidx(0), mut(NULL){}
	~Data();
	
	ulong createFilePosition();
	void collectFilePosition(ulong _pos);
	
	void eraseToutPos(ulong _pos);
	
//data:
	Controller				*pc;//pointer to controller
	uint32					sz;
	Mutex					*mut;
	MutexContainerTp		mutpool;
	FileVectorTp			fv;//file vector
	MapperVectorTp			mv;//mapper vector
	MapperQueueTp			mexecq;
	FreeStackTp				fs;//free stack
	TimeoutVectorTp			toutv;
	TimeSpec				tout;
};

//------------------------------------------------------------------
//------------------------------------------------------------------
//------------------------------------------------------------------
//------------------------------------------------------------------

Manager::Manager(Controller *_pc):d(*(new Data(_pc))){}

Manager::~Manager(){
	idbgx(Dbg::fdt_file, "");
	delete &d;
}
//------------------------------------------------------------------

int Manager::execute(ulong _evs, TimeSpec &_rtout){
	d.mut->unlock();
	if(signaled()){
		ulong sm = grabSignalMask(0);
		idbgx(Dbg::fdt_file, "signalmask "<<sm);
		if(sm & fdt::S_KILL){
			state(Data::Stopping);
			idbgx(Dbg::fdt_file, "kill "<<d.sz);
			if(!d.sz){//no file
				state(-1);
				d.mut->unlock();
				//Manager::the().removeFileManager();
				idbgx(Dbg::fdt_file, "");
				d.pc->removeFileManager();
				return BAD;
			}
		}
	}
	
	
}
//------------------------------------------------------------------
//overload from object
void Manager::mutex(Mutex *_pmut){
	d.mut = _pmut;
}
//===================================================================
// stream method - the basis for all stream methods
template <typename StreamP>
int Manager::doGetStream(
	StreamP &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_requid,
	const FileKey &_rk,
	uint32 _flags
){
	Mutex::Locker	lock1(*d.mut);
	
	Mapper			&rmp(d.doGetMapper(_rk));
	Stub			mstub(*this);
	File			*pf = rmp.findOrCreateFile(mstub, _rk);
	
	if(!pf) return BAD;
	//file found
	if(pf->isRegistered() && pf->isOpen()){
		Mutex::Locker	lock2(d.mutpool.object(fid));
		_rfuid.first = pf->id();
		_rfuid.second = d.fv[pf->id()].uid; 
		return pf->stream(Stub(*this), _sptr, _requid, _flags);
	}
	//delay stream creation until successfull file open
	return pf->stream(Stub(*this), _sptr, _requid, _flags | FileManager::ForcePending);
}
//===================================================================
// Most general stream methods
int Manager::stream(
	StreamPointer<IStream> &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_requid,
	const FileKey &_rk,
	uint32 _flags
){
	if(state() != Data::Running) return BAD;
	return doGetStream(_sptr, _rfuid, _requid, _rk, _flags);
}
int Manager::stream(
	StreamPointer<OStream> &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_requid,
	const FileKey &_rk,
	uint32 _flags
){	
	if(state() != Data::Running) return BAD;
	return doGetStream(_sptr, _rfuid, _requid, _rk, _flags);
}

int FileManager::stream(
	StreamPointer<IOStream> &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_requid,
	const FileKey &_rk,
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
	FileUidTp fuid;
	return stream(_sptr, fuid, _rrequid, _fn, _flags);
}

int Manager::stream(
	StreamPointer<OStream> &_sptr,
	const RequestUid &_rrequid,
	const char *_fn,
	uint32 _flags
){
	FileUidTp fuid;
	return stream(_sptr, fuid, _rrequid, _fn, _flags);
}

int Manager::stream(
	StreamPointer<IOStream> &_sptr,
	const RequestUid &_rrequid,
	const char *_fn,
	uint32 _flags
){
	FileUidTp fuid;
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

int Manager::stream(
	StreamPointer<OStream> &_sptr,
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

int Manager::stream(
	StreamPointer<IOStream> &_sptr,
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
int Manager::streamSpecific(
	StreamPointer<IStream> &_sptr,
	FileUidTp &_rfuid,
	const char *_fn,
	uint32 _flags
){
	return stream(_sptr, _rfuid, *requestuidptr, _fn, _flags);
}
int Manager::streamSpecific(
	StreamPointer<OStream> &_sptr,
	FileUidTp &_rfuid,
	const char *_fn,
	uint32 _flags
){
	return stream(_sptr, _rfuid, *requestuidptr, _fn, _flags);
}
int Manager::streamSpecific(
	StreamPointer<IOStream> &_sptr,
	FileUidTp &_rfuid,
	const char *_fn,
	uint32 _flags
){
	return stream(_sptr, _rfuid, *requestuidptr, _fn, _flags);
}
//---------------------------------------------------------------
int Manager::stream(StreamPointer<IStream> &_sptr, const char *_fn, uint32 _flags){
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

int Manager::stream(StreamPointer<OStream> &_sptr, const char *_fn, uint32 _flags){
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

int Manager::stream(StreamPointer<IOStream> &_sptr, const char *_fn, uint32 _flags){
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
int Manager::stream(StreamPointer<IStream> &_sptr, const FileKey &_rk, uint32 _flags){
	FileUidTp	fuid;
	RequestUid	requid;
	return stream(_sptr, fuid, requid, _rk, _flags | NoWait);
}

int Manager::stream(StreamPointer<OStream> &_sptr, const FileKey &_rk, uint32 _flags){
	FileUidTp	fuid;
	RequestUid	requid;
	return stream(_sptr, fuid, requid, _rk, _flags | NoWait);
}

int Manager::stream(StreamPointer<IOStream> &_sptr, const FileKey &_rk, uint32 _flags){
	FileUidTp	fuid;
	RequestUid	requid;
	return stream(_sptr, fuid, requid, _rk, _flags | NoWait);
}
//=======================================================================
int Manager::stream(
	StreamPointer<IStream> &_sptr,
	const FileUidTp &_rfuid,
	const RequestUid &_requid,
	uint32 _flags
){
	Mutex::Locker	lock1(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].uid == _rfuid.second){
		Mutex::Locker	lock2(d.mutpool.object(_rfuid.first));
		if(d.fv[_rfuid.first].pfile)
			return d.fv[_rfuid.first].pfile->stream(*this, _rfuid.first, _sptr, _requid, _flags);
	}
	return BAD;
}

int Manager::stream(
	StreamPointer<OStream> &_sptr,
	const FileUidTp &_rfuid,
	const RequestUid &_requid,
	uint32 _flags
){
	Mutex::Locker	lock1(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].uid == _rfuid.second){
		Mutex::Locker	lock2(d.mutpool.object(_rfuid.first));
		if(d.fv[_rfuid.first].pfile)
			return d.fv[_rfuid.first].pfile->stream(*this, _rfuid.first, _sptr, _requid, _flags);
	}
	return BAD;
}

int Manager::stream(
	StreamPointer<IOStream> &_sptr,
	const FileUidTp &_rfuid,
	const RequestUid &_requid,
	uint32 _flags
){
	Mutex::Locker	lock1(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].uid == _rfuid.second){
		Mutex::Locker	lock2(d.mutpool.object(_rfuid.first));
		if(d.fv[_rfuid.first].pfile)
			return d.fv[_rfuid.first].pfile->stream(*this, _rfuid.first, _sptr, _requid, _flags);
	}
	return BAD;
}
//=======================================================================
// The internal implementation of streams streams
//=======================================================================

class FileIStream: public IStream{
public:
	FileIStream(Manager &_rm, IndexTp _fileid):rm(_rm), fileid(_fileid), off(0){}
	~FileIStream();
	int read(char *, uint32, uint32 _flags = 0);
	int release();
	int64 seek(int64, SeekRef);
	int64 size()const;
private:
	Manager			&rm;
	const IndexTp	fileid;
	int64			off;
};
//------------------------------------------------------------------------------
class FileOStream: public OStream{
public:
	FileOStream(Manager &_rm, IndexTp _fileid):rm(_rm), fileid(_fileid), off(0){}
	~FileOStream();
	int write(const char *, uint32, uint32 _flags = 0);
	int release();
	int64 seek(int64, SeekRef);
	int64 size()const;
private:
	Manager			&rm;
	const IndexTp	fileid;
	int64			off;
};
//------------------------------------------------------------------------------
class FileIOStream: public IOStream{
public:
	FileIOStream(Manager &_rm, IndexTp _fileid):rm(_rm), fileid(_fileid), off(0){}
	~FileIOStream();
	int read(char *, uint32, uint32 _flags = 0);
	int write(const char *, uint32, uint32 _flags = 0);
	int release();
	int64 seek(int64, SeekRef);
	int64 size()const;
private:
	Manager			&rm;
	const IndexTp	fileid;
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
	return s.fileSize(fileid);
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
IStream* Manager::Stub::createIStream(IndexTp _fileid){
	return new FileIStream(rm, _fileid);
}
OStream* Manager::Stub::createOStream(IndexTp _fileid){
	return new FileOStream(rm, _fileid);
}
IOStream* Manager::Stub::createIOStream(IndexTp _fileid){
	return new FileIOStream(rm, _fileid);
}
void Manager::Stub::pushMapper(Mapper *_mp){
	rm.d.mexecq.push(_mp);
	if(static_cast<fdt::Object*>(&rm)->signal(fdt::S_RAISE)){
		fdt::Manager::the().raiseObject(*rm);
	}
}
//=======================================================================
//=======================================================================

}//namespace file
}//namespace foundation


