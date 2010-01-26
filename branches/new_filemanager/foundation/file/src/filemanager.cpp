namespace foundation{
namespace file{

//------------------------------------------------------------------

struct Manager::Data{
	template <typename StreamP>
	int doGetStream(
		StreamP &_sptr,
		FileUidTp &_rfuid,
		const RequestUid &_rrequid,
		const FileKey &_rk,
		uint32 _flags
	);
};

//------------------------------------------------------------------
//------------------------------------------------------------------
//------------------------------------------------------------------
//------------------------------------------------------------------

int Manager::execute(ulong _evs, TimeSpec &_rtout){
	
}


//===================================================================
// Data stream method - the basis for all stream methods
template <typename StreamP>
int Manager::Data::doGetStream(
	StreamP &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_requid,
	const FileKey &_rk,
	uint32 _flags
){
	Mutex::Locker	lock1(*mut);
	
	Mapper &rp(doGetMapper(_rk));
	
	uint32 fid = rp.find(_rk);
	idbgx(Dbg::fdt_file, ""<<fid);
	
	if(fid != (uint32)-1){//file found
		Mutex::Locker	lock2(mutpool.object(fid));
		_rfuid.first = fid;
		_rfuid.second = fv[fid].uid; 
		return doGetFile(fid).stream(Stub(*this), _sptr, _requid, _flags | extflags);
	}
	
	
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
	return d.doGetStream(_sptr, _rfuid, _requid, _rk, _flags);
}
int Manager::stream(
	StreamPointer<OStream> &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_requid,
	const FileKey &_rk,
	uint32 _flags
){	
	if(state() != Data::Running) return BAD;
	return d.doGetStream(_sptr, _rfuid, _requid, _rk, _flags);
}

int FileManager::stream(
	StreamPointer<IOStream> &_sptr,
	FileUidTp &_rfuid,
	const RequestUid &_requid,
	const FileKey &_rk,
	uint32 _flags
){	
	if(state() != Data::Running) return BAD;
	return d.doGetStream(_sptr, _rfuid, _requid, _rk, _flags);
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
	Mutex::Locker	lock(*d.mut);
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
	Mutex::Locker	lock(*d.mut);
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
	Mutex::Locker	lock(*d.mut);
	if(_rfuid.first < d.fv.size() && d.fv[_rfuid.first].uid == _rfuid.second){
		Mutex::Locker	lock2(d.mutpool.object(_rfuid.first));
		if(d.fv[_rfuid.first].pfile)
			return d.fv[_rfuid.first].pfile->stream(*this, _rfuid.first, _sptr, _requid, _flags);
	}
	return BAD;
}
//=======================================================================

}//namespace file
}//namespace foundation


