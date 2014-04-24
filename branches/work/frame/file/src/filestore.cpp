// frame/file/src/filestore.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "frame/file/filestore.hpp"

#ifdef HAS_CPP11

#define HASH_NS std
#include <functional>
#include <unordered_set>

#else

#include "boost/functional/hash.hpp"
#include "boost/unordered_set.hpp"
#define HASH_NS boost

#endif

#include "system/atomic.hpp"
#include "system/directory.hpp"
#include "system/debug.hpp"

#include "utility/binaryseeker.hpp"
#include "utility/stack.hpp"
#include "utility/sharedmutex.hpp"

#include "filetemp.hpp"

#include <algorithm>
#include <cstdio>


namespace solid{
namespace frame{
namespace file{

uint32 dbgid(){
	static uint32 id = Debug::the().registerModule("frame_file");
	return id;
}

//---------------------------------------------------------------------------
//		Utf8Controller::Data
//---------------------------------------------------------------------------
typedef ATOMIC_NS::atomic<size_t>	AtomicSizeT;
typedef std::pair<size_t, size_t>	SizePairT;
typedef std::vector<SizePairT>		SizePairVectorT;
typedef std::vector<size_t>			SizeVectorT;
typedef Stack<size_t>				SizeStackT;


typedef std::deque<Utf8PathStub>	PathDequeT;

struct TempWaitStub{
	TempWaitStub(
		UidT const &_uid = frame::invalid_uid(),
		File *_pfile = NULL,
		uint64 _size = 0,
		size_t _value = 0
	):objuid(_uid), pfile(_pfile), size(_size), value(_value){}
	
	void clear(){
		pfile = NULL;
	}
	bool empty()const{
		return pfile == NULL;
	}
	
	UidT		objuid;
	File		*pfile;
	uint64		size;
	size_t		value;
};

typedef std::deque<TempWaitStub>	TempWaitDequeT;
typedef std::vector<TempWaitStub>	TempWaitVectorT;

HASH_NS::hash<std::string>			stringhasher;

struct PathEqual{
	bool operator()(const Utf8PathStub* _req1, const Utf8PathStub* _req2)const{
		return _req1->storeidx == _req2->storeidx && _req1->path == _req2->path;
	}
};

struct PathHash{
	size_t operator()(const Utf8PathStub* _req1)const{
		return _req1->storeidx ^ stringhasher(_req1->path);
	}
};

struct IndexEqual{
	bool operator()(const Utf8PathStub* _req1, const Utf8PathStub* _req2)const{
		return _req1->idx == _req2->idx;
	}
};

struct IndexHash{
	size_t operator()(const Utf8PathStub* _req1)const{
		return _req1->idx;
	}
};


typedef HASH_NS::unordered_set<const Utf8PathStub*, PathHash, PathEqual>	PathSetT;
typedef HASH_NS::unordered_set<const Utf8PathStub*, IndexHash, IndexEqual>	IndexSetT;
typedef Stack<Utf8PathStub*>												PathStubStackT;

struct SizePairCompare {
	bool operator() (SizePairT const& _a, SizePairT const& _b)const{
		return (_a.first < _b.first);
	}
	bool operator() (SizePairT const& _a, size_t _b)const{
		if(_a.first < _b) return -1;
		if(_b < _a.first) return 1;
		return 0;
	}
} szcmp;


BinarySeeker<SizePairCompare>		sizeseeker;

struct Utf8ConfigurationImpl{
	struct Storage{
		Storage(){}
		Storage(Utf8Configuration::Storage const &_rstrg):globalprefix(_rstrg.globalprefix), localprefix(_rstrg.localprefix){
		}
		std::string		globalprefix;
		std::string		localprefix;
		size_t			globalsize;
	};
	
	Utf8ConfigurationImpl(){}
	Utf8ConfigurationImpl(Utf8Configuration const &_cfg){
		storagevec.reserve(_cfg.storagevec.size());
		for(
			Utf8Configuration::StorageVectorT::const_iterator it = _cfg.storagevec.begin();
			it != _cfg.storagevec.end();
			++it
		){
			storagevec.push_back(Storage(*it));
		}
	}
	
	typedef std::vector<Storage>		StorageVectorT;
	
	StorageVectorT		storagevec;
};

struct TempConfigurationImpl{
	struct Storage{
		Storage(
		):	level(0), capacity(0), minsize(0), maxsize(0), waitcount(0),
		waitsizefirst(0), waitidxfirst(-1), usedsize(0), currentid(0),
		enqued(false), removemode(RemoveAfterCreateE){}
		Storage(
			TempConfiguration::Storage const &_cfg
		):	path(_cfg.path), level(_cfg.level), capacity(_cfg.capacity),
			minsize(_cfg.minsize), maxsize(_cfg.maxsize), waitcount(0),
			waitsizefirst(0), waitidxfirst(-1), usedsize(0), currentid(0), enqued(false),
			removemode(_cfg.removemode)
		{
			if(maxsize > capacity || maxsize == 0){
				maxsize = capacity;
			}
		}
		bool canUse(const uint64 _sz, const size_t _flags)const{
			return _flags & level && _sz <= maxsize && _sz >= minsize;
		}
		bool shouldUse(const uint64 _sz)const{
			return waitcount == 0 && ((capacity - usedsize) >= _sz);
		}
		bool canDeliver()const{
			return waitcount && ((capacity - usedsize) >= waitsizefirst) ;
		}
		std::string 	path;
		uint32			level;
		uint64			capacity;
		uint64			minsize;
		uint64			maxsize;
		size_t			waitcount;
		uint64			waitsizefirst;
		size_t			waitidxfirst;
		uint64			usedsize;
		size_t			currentid;
		SizeStackT		idcache;
		bool			enqued;
		TempRemoveMode	removemode;
	};
	
	TempConfigurationImpl(){}
	TempConfigurationImpl(TempConfiguration const &_cfg){
		storagevec.reserve(_cfg.storagevec.size());
		for(
			TempConfiguration::StorageVectorT::const_iterator it = _cfg.storagevec.begin();
			it != _cfg.storagevec.end();
			++it
		){
			storagevec.push_back(Storage(*it));
		}
	}
	typedef std::vector<Storage>		StorageVectorT;
	
	StorageVectorT		storagevec;
};

struct Utf8Controller::Data{
	
	Utf8ConfigurationImpl	filecfg;//NOTE: it is accessed without lock in openFile
	TempConfigurationImpl	tempcfg;
	size_t					minglobalprefixsz;
	size_t					maxglobalprefixsz;
	SizePairVectorT			hashvec;
	std::string				tmp;
	PathDequeT				pathdq;
	PathSetT				pathset;
	IndexSetT				indexset;
	PathStubStackT			pathcache;
	
	SizeVectorT				tempidxvec;
	TempWaitDequeT			tempwaitdq;
	TempWaitVectorT			tempwaitvec[2];
	TempWaitVectorT			*pfilltempwaitvec;
	TempWaitVectorT			*pconstempwaitvec;
	
	Data(
		const Utf8Configuration &_rfilecfg,
		const TempConfiguration &_rtempcfg
	):filecfg(_rfilecfg), tempcfg(_rtempcfg){
		pfilltempwaitvec = &tempwaitvec[0];
		pconstempwaitvec = &tempwaitvec[1];
	}
	
	void prepareFile();
	void prepareTemp();
	
	size_t findFileStorage(std::string const&_path);
};

//---------------------------------------------------------------------------
void Utf8Controller::Data::prepareFile(){
	minglobalprefixsz = -1;
	maxglobalprefixsz = 0;
	for(
		Utf8ConfigurationImpl::StorageVectorT::const_iterator it = filecfg.storagevec.begin();
		it != filecfg.storagevec.end();
		++it
	){
		if(it->globalprefix.size() < minglobalprefixsz){
			minglobalprefixsz = it->globalprefix.size();
		}
		if(it->globalprefix.size() > maxglobalprefixsz){
			maxglobalprefixsz = it->globalprefix.size();
		}
	}
	
	for(
		Utf8ConfigurationImpl::StorageVectorT::iterator it = filecfg.storagevec.begin();
		it != filecfg.storagevec.end();
		++it
	){
		const size_t idx = it - filecfg.storagevec.begin();
		it->globalsize = it->globalprefix.size();
		it->globalprefix.resize(maxglobalprefixsz, '\0');
		hashvec.push_back(SizePairT(stringhasher(it->globalprefix), idx));
	}
	std::sort(hashvec.begin(), hashvec.end(), szcmp);
}

void Utf8Controller::Data::prepareTemp(){
	
}

size_t Utf8Controller::Data::findFileStorage(std::string const&_path){
	tmp.assign(_path, 0, _path.size() < maxglobalprefixsz ? _path.size() : maxglobalprefixsz);
	tmp.resize(maxglobalprefixsz, '\0');
	HASH_NS::hash<std::string>		sh;
	size_t							h = sh(tmp);
	BinarySeekerResultT				r = sizeseeker.first(hashvec.begin(), hashvec.end(), h);
	if(r.first){
		while(hashvec[r.second].first == h){
			const size_t							strgidx = hashvec[r.second].second;
			Utf8ConfigurationImpl::Storage const	&rstrg = filecfg.storagevec[strgidx];
			if(_path.compare(0, rstrg.globalsize, rstrg.globalprefix) == 0){
				return strgidx;
			}
			++r.second;
		}
	}
	return -1;
}

//---------------------------------------------------------------------------
//		Utf8Controller
//---------------------------------------------------------------------------

Utf8Controller::Utf8Controller(
	const Utf8Configuration &_rfilecfg,
	const TempConfiguration &_rtempcfg
):d(*(new Data(_rfilecfg, _rtempcfg))){
	
	d.prepareFile();
	d.prepareTemp();
}

Utf8Controller::~Utf8Controller(){
	delete &d;
}

bool Utf8Controller::prepareIndex(
	shared::StoreBase::Accessor &/*_rsbacc*/, Utf8OpenCommandBase &_rcmd,
	size_t &_ridx, size_t &_rflags, ERROR_NS::error_code &_rerr
){
	//find _rcmd.inpath file and set _rcmd.outpath
	//if found set _ridx and return true
	//else return false
	const size_t							storeidx = d.findFileStorage(_rcmd.inpath);
	
	_rcmd.outpath.storeidx = storeidx;
	
	if(storeidx == static_cast<size_t>(-1)){
		_rerr.assign(1, _rerr.category());
		return true;
	}
	
	Utf8ConfigurationImpl::Storage const	&rstrg = d.filecfg.storagevec[storeidx];
	
	_rcmd.outpath.path.assign(_rcmd.inpath.c_str() + rstrg.globalsize);
	PathSetT::const_iterator it = d.pathset.find(&_rcmd.outpath);
	if(it != d.pathset.end()){
		_ridx = (*it)->idx;
		return true;
	}
	return false;
}

bool Utf8Controller::preparePointer(
	shared::StoreBase::Accessor &/*_rsbacc*/, Utf8OpenCommandBase &_rcmd,
	FilePointerT &_rptr, size_t &_rflags, ERROR_NS::error_code &_rerr
){
	//just do map[_rcmd.outpath] = _rptr.uid().first
	Utf8PathStub	*ppath;
	if(d.pathcache.size()){
		ppath = d.pathcache.top();
		*ppath = _rcmd.outpath;
		d.pathcache.pop();
	}else{
		d.pathdq.push_back(_rcmd.outpath);
		ppath = &d.pathdq.back();
	}
	ppath->idx = _rptr.id().first;
	d.pathset.insert(ppath);
	d.indexset.insert(ppath);
	return true;//we don't store _runiptr for later use
}

void Utf8Controller::openFile(Utf8OpenCommandBase &_rcmd, FilePointerT &_rptr, ERROR_NS::error_code &_rerr){
	Utf8ConfigurationImpl::Storage const	&rstrg = d.filecfg.storagevec[_rcmd.outpath.storeidx];
	std::string								path;
	
	path.reserve(rstrg.localprefix.size() + _rcmd.outpath.path.size());
	path.assign(rstrg.localprefix);
	path.append(_rcmd.outpath.path);
	if(!_rptr->open(path.c_str(), _rcmd.openflags)){
		_rerr = last_system_error();
	}
	
}

bool Utf8Controller::prepareIndex(
	shared::StoreBase::Accessor &_rsbacc, CreateTempCommandBase &_rcmd,
	size_t &_ridx, size_t &_rflags, ERROR_NS::error_code &_rerr
){
	//nothing to do
	return false;//no stored index
}

bool Utf8Controller::preparePointer(
	shared::StoreBase::Accessor &_rsbacc, CreateTempCommandBase &_rcmd,
	FilePointerT &_rptr, size_t &_rflags, ERROR_NS::error_code &_rerr
){
	//We're under Store's mutex lock
	UidT	uid = _rptr.id();
	File	*pf = _rptr.release();

	d.pfilltempwaitvec->push_back(TempWaitStub(uid, pf, _rcmd.size, _rcmd.openflags));
	if(d.pfilltempwaitvec->size() == 1){
		//notify the shared store object
		_rsbacc.notify(frame::S_SIG | frame::S_RAISE);
	}
	
	return false;//will always store _rptr
}

void Utf8Controller::executeOnSignal(shared::StoreBase::Accessor &_rsbacc, ulong _sm){
	//We're under Store's mutex lock
	solid::exchange(d.pfilltempwaitvec, d.pconstempwaitvec);
	d.pfilltempwaitvec->clear();
}

bool Utf8Controller::executeBeforeErase(shared::StoreBase::Accessor &_rsbacc){
	//We're NOT under Store's mutex lock
	if(d.pconstempwaitvec->size()){
		for(
			TempWaitVectorT::const_iterator waitit = d.pconstempwaitvec->begin();
			waitit != d.pconstempwaitvec->end();
			++waitit
		){
			const size_t	tempwaitdqsize = d.tempwaitdq.size();
			bool			canuse = false;
			for(
				TempConfigurationImpl::StorageVectorT::iterator it(d.tempcfg.storagevec.begin());
				it != d.tempcfg.storagevec.end();
				++it
			){
				if(it->canUse(waitit->size, waitit->value)){
					const size_t 					strgidx = it - d.tempcfg.storagevec.begin();
					TempConfigurationImpl::Storage	&rstrg(*it);
					
					if(it->shouldUse(waitit->size)){
						d.tempwaitdq.resize(tempwaitdqsize);
						doPrepareOpenTemp(*waitit->pfile, waitit->size, strgidx);
						//we schedule for erase the waitit pointer
						_rsbacc.consumeEraseVector().push_back(waitit->objuid);
					}else{
						d.tempwaitdq.push_back(*waitit);
						// we dont need the openflags any more - we know which 
						// storages apply
						d.tempwaitdq.back().value = strgidx;
						++rstrg.waitcount;
						if(rstrg.waitcount == 1){
							rstrg.waitsizefirst = waitit->size;
						}
					}
					canuse = true;
				}
			}
			if(!canuse){
				//a temp file with uninitialized tempbase means an error
				_rsbacc.consumeEraseVector().push_back(waitit->objuid);
			}
		}
	}
	if(d.tempidxvec.size()){
		for(SizeVectorT::const_iterator it = d.tempidxvec.begin(); it != d.tempidxvec.begin(); ++it){
			doDeliverTemp(_rsbacc, *it);
		}
		d.tempidxvec.clear();
	}
	return false;
}

bool Utf8Controller::clear(shared::StoreBase::Accessor &_rsbacc, File &_rf, const size_t _idx){
	//We're under Store's mutex lock
	//We're under File's mutex lock
	if(!_rf.isTemp()){
		_rf.clear();
		Utf8PathStub		path;
		path.idx = _idx;
		IndexSetT::iterator it = d.indexset.find(&path);
		if(it != d.indexset.end()){
			Utf8PathStub *ps = const_cast<Utf8PathStub *>(*it);
			d.pathset.erase(ps);
			d.indexset.erase(it);
			d.pathcache.push(ps);
		}
	}else{
		TempBase						&temp = *_rf.temp();
		const size_t					strgidx = temp.tempstorageid;
		TempConfigurationImpl::Storage	&rstrg(d.tempcfg.storagevec[strgidx]);
		
		doCloseTemp(temp);
		
		_rf.clear();
		
		if(rstrg.canDeliver() && !rstrg.enqued){
			d.tempidxvec.push_back(strgidx);
			rstrg.enqued = true;
		}
	}
	return !d.tempidxvec.empty();
}

void Utf8Controller::doPrepareOpenTemp(File &_rf, uint64 _sz, const size_t _storeid){
	
	TempConfigurationImpl::Storage	&rstrg(d.tempcfg.storagevec[_storeid]);
	size_t							fileid;
	
	if(rstrg.idcache.size()){
		fileid = rstrg.idcache.top();
		rstrg.idcache.pop();
	}else{
		fileid = rstrg.currentid;
		++rstrg.currentid;
	}
	rstrg.usedsize += _sz;
	
	//only creates the file backend - does not open it:
	if((rstrg.level & MemoryLevelFlag) && rstrg.path.empty()){
		_rf.ptmp = new TempMemory(_storeid, fileid, _sz);
	}else{
		_rf.ptmp = new TempFile(_storeid, fileid, _sz);
	}
	
}

void Utf8Controller::openTemp(CreateTempCommandBase &_rcmd, FilePointerT &_rptr, ERROR_NS::error_code &_rerr){
	if(_rptr->isTemp()){
		TempConfigurationImpl::Storage	&rstrg(d.tempcfg.storagevec[_rptr->temp()->tempstorageid]);
		_rptr->temp()->open(rstrg.path.c_str(), _rcmd.openflags, rstrg.removemode == RemoveAfterCreateE, _rerr);
	}else{
		_rerr.assign(1, _rerr.category());
	}
}

void Utf8Controller::doCloseTemp(TempBase &_rtemp){
	//erase the temp file for on-disk temps
	TempConfigurationImpl::Storage	&rstrg(d.tempcfg.storagevec[_rtemp.tempstorageid]);
	rstrg.usedsize -= _rtemp.tempsize;
	rstrg.idcache.push(_rtemp.tempid);
	_rtemp.close(rstrg.path.c_str(), rstrg.removemode == RemoveAfterCloseE);
}

void Utf8Controller::doDeliverTemp(shared::StoreBase::Accessor &_rsbacc, const size_t _storeid){
	
	TempConfigurationImpl::Storage	&rstrg(d.tempcfg.storagevec[_storeid]);
	TempWaitDequeT::iterator 		it = d.tempwaitdq.begin();
	
	rstrg.enqued = false;
	while(it != d.tempwaitdq.end()){
		//first we find the first item waiting on storage
		TempWaitDequeT::iterator 		waitit = it;
		for(; it != d.tempwaitdq.end(); ++it){
			if(it->objuid.first != waitit->objuid.first){
				waitit = it;
			}
			if(it->value == _storeid){
				break;
			}
		}
		
		cassert(it != d.tempwaitdq.end());
		if(rstrg.waitsizefirst == 0){
			rstrg.waitsizefirst = it->size;
		}
		if(!rstrg.canDeliver()){
			break;
		}
		cassert(rstrg.waitsizefirst == it->size);
		--rstrg.waitcount;
		rstrg.waitsizefirst = 0;
		doPrepareOpenTemp(*it->pfile, it->size, _storeid);
		//we schedule for erase the waitit pointer
		_rsbacc.consumeEraseVector().push_back(it->objuid);
		//delete the whole range [waitit, itend]
		const size_t	objidx = it->objuid.first;
		
		if(waitit == d.tempwaitdq.begin()){
			while(waitit != d.tempwaitdq.end() && (waitit->objuid.first == objidx || waitit->pfile == NULL)){
				waitit = d.tempwaitdq.erase(waitit);
			}
		}else{
			while(waitit != d.tempwaitdq.end() && (waitit->objuid.first == objidx || waitit->pfile == NULL)){
				waitit->pfile = NULL;
				++waitit;
			}
		}
		it = waitit;
		if(!rstrg.waitcount){
			break;
		}
	}
}

//--------------------------------------------------------------------------
//		TempBase
//--------------------------------------------------------------------------

/*virtual*/ TempBase::~TempBase(){
}
/*virtual*/ void TempBase::flush(){
}
//--------------------------------------------------------------------------
//		TempFile
//--------------------------------------------------------------------------
TempFile::TempFile(
	size_t _storageid,
	size_t _id,
	uint64 _size
):TempBase(_storageid, _id, _size){}

/*virtual*/ TempFile::~TempFile(){
	fd.close();
}

namespace{

bool prepare_temp_file_path(std::string &_rpath, const char *_prefix, size_t _id){
	_rpath.assign(_prefix);
	if(_rpath.empty()) return false;
	
	if(*_rpath.rbegin() != '/'){
		_rpath += '/';
	}
	
	char	fldrbuf[128];
	char	filebuf[128];
	
	if(sizeof(_id) == sizeof(uint64)){
		size_t fldrid = _id >> 32;
		size_t fileid = _id & 0xffffffffUL;
		std::sprintf(fldrbuf, "%8.8X", fldrid);
		std::sprintf(filebuf, "/%8.8x.tmp", fileid);
	}else{
		const size_t fldrid = _id >> 16;
		const size_t fileid = _id & 0xffffUL;
		std::sprintf(fldrbuf, "%4.4X", fldrid);
		std::sprintf(filebuf, "/%4.4x.tmp", fileid);
	}
	_rpath.append(fldrbuf);
	Directory::create(_rpath.c_str());
	_rpath.append(filebuf);
	return true;
}

}//namespace

/*virtual*/ bool TempFile::open(const char *_path, const size_t _openflags, bool _remove, ERROR_NS::error_code &_rerr){
	
	std::string path;
	
	prepare_temp_file_path(path, _path, tempid);
	
	bool rv = fd.open(path.c_str(), FileDevice::CreateE | FileDevice::TruncateE | FileDevice::ReadWriteE);
	if(!rv){
		_rerr = last_system_error();
	}else{
		if(_remove){
			Directory::eraseFile(path.c_str());
		}
	}
	return rv;
}

/*virtual*/ void TempFile::close(const char *_path, bool _remove){
	fd.close();
	if(_remove){
		std::string path;
		prepare_temp_file_path(path, _path, tempid);
		Directory::eraseFile(path.c_str());
	}
}
/*virtual*/ int TempFile::read(char *_pb, uint32 _bl, int64 _off){
	const int64 endoff = _off + _bl;
	if(endoff > tempsize){
		if((endoff - tempsize) <= _bl){
			_bl = static_cast<uint32>(endoff - tempsize);
		}else{
			return -1;
		}
	}
	return fd.read(_pb, _bl, _off);
}
/*virtual*/ int TempFile::write(const char *_pb, uint32 _bl, int64 _off){
	const int64 endoff = _off + _bl;
	if(endoff > tempsize){
		if((endoff - tempsize) <= _bl){
			_bl = static_cast<uint32>(endoff - tempsize);
		}else{
			errno = ENOSPC;
			return -1;
		}
	}
	return fd.write(_pb, _bl, _off);
}
/*virtual*/ int64 TempFile::size()const{
	return fd.size();
}

/*virtual*/ bool TempFile::truncate(int64 _len){
	return fd.truncate(_len);
}
/*virtual*/ void TempFile::flush(){
	fd.flush();
}

//--------------------------------------------------------------------------
//		TempMemory
//--------------------------------------------------------------------------
TempMemory::TempMemory(
	size_t _storageid,
	size_t _id,
	uint64 _size
):TempBase(_storageid, _id, _size), mf(_size)
{
	shared_mutex_safe(this);
}

/*virtual*/ TempMemory::~TempMemory(){
}

/*virtual*/ bool TempMemory::open(const char *_path, const size_t _openflags, bool /*_remove*/, ERROR_NS::error_code &_rerr){
	Locker<Mutex> lock(shared_mutex(this));
	mf.truncate(0);
	return true;
}
/*virtual*/ void TempMemory::close(const char */*_path*/, bool /*_remove*/){
}
/*virtual*/ int TempMemory::read(char *_pb, uint32 _bl, int64 _off){
	Locker<Mutex> lock(shared_mutex(this));
	return mf.read(_pb, _bl, _off);
}
/*virtual*/ int TempMemory::write(const char *_pb, uint32 _bl, int64 _off){
	Locker<Mutex> lock(shared_mutex(this));
	return mf.write(_pb, _bl, _off);
}
/*virtual*/ int64 TempMemory::size()const{
	Locker<Mutex> lock(shared_mutex(this));
	return mf.size();
}

/*virtual*/ bool TempMemory::truncate(int64 _len){
	Locker<Mutex> lock(shared_mutex(this));
	return mf.truncate(_len) == 0;
}

}//namespace file
}//namespace frame
}//namespace solid
