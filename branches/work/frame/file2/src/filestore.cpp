#include "frame/file2/filestore.hpp"

#ifdef HAS_CPP11

#define HASH_NS std
#include <functional>

#else

#include "boost/functional/hash.hpp"
#define HASH_NS boost

#endif

#include "utility/binaryseeker.hpp"
#include "utility/stack.hpp"

#include <algorithm>
#include <unordered_set>


namespace solid{
namespace frame{
namespace file{

//---------------------------------------------------------------------------
//		Utf8Controller::Data
//---------------------------------------------------------------------------

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
		size_t _storageid = -1
	):objuid(_uid), pfile(pfile), size(_size), storageid(_storageid){}
	
	void clear(){
		pfile = NULL;
	}
	bool empty()const{
		return pfile == NULL;
	}
	
	UidT		objuid;
	File		*pfile;
	uint64		size;
	size_t		storageid;
};

typedef std::deque<TempWaitStub>	TempWaitDequeT;

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


typedef std::unordered_set<const Utf8PathStub*, PathHash, PathEqual>	PathSetT;
typedef std::unordered_set<const Utf8PathStub*, IndexHash, IndexEqual>	IndexSetT;
typedef Stack<Utf8PathStub*>											PathStubStackT;

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
		Storage():level(0), capacity(0), minsize(0), maxsize(0), waitcount(0), waitsizefirst(0), waitidxfirst(-1), usedsize(0), currentid(0){}
		Storage(
			TempConfiguration::Storage const &_cfg
		):	path(_cfg.path), level(_cfg.level), capacity(_cfg.capacity),
			minsize(_cfg.minsize), maxsize(_cfg.maxsize), waitcount(0), waitsizefirst(0), waitidxfirst(-1), usedsize(0), currentid(0)
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
	
	Data(
		const Utf8Configuration &_rfilecfg,
		const TempConfiguration &_rtempcfg
	):filecfg(_rfilecfg), tempcfg(_rtempcfg){}
	
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

bool Utf8Controller::prepareFileIndex(
	Utf8OpenCommandBase &_rcmd, size_t &_ridx, size_t &_rflags, ERROR_NS::error_code &_rerr
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
	if(it == d.pathset.end()){
		_ridx = (*it)->idx;
		return true;
	}
	return false;
}

void Utf8Controller::prepareFilePointer(
	Utf8OpenCommandBase &_rcmd, FilePointerT &_rptr, size_t &_rflags, ERROR_NS::error_code &_rerr
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
}

void Utf8Controller::openFile(Utf8OpenCommandBase &_rcmd, FilePointerT &_rptr, ERROR_NS::error_code &_rerr){
	//use _rcmd.outpath to acctually open the file
	//set _rerr accordingly
	//d.filecfg is accessed without lock!!
	Utf8ConfigurationImpl::Storage const	&rstrg = d.filecfg.storagevec[_rcmd.outpath.storeidx];
	std::string								path;
	
	path.reserve(rstrg.localprefix.size() + _rcmd.outpath.path.size());
	path.assign(rstrg.localprefix);
	path.append(_rcmd.outpath.path);
	if(!_rptr->open(path.c_str(), _rcmd.openflags)){
		_rerr = specific_error_back();
	}
	
}

void Utf8Controller::prepareTempIndex(
	CreateTempCommandBase &_rcmd, size_t &_rflags, ERROR_NS::error_code &_rerr
){
	/*
	if the requested size is greater than what we can offer set _rerr and return
	if we can deliver right now - store in cmd the information about the storage where we have available
		space and return
	IDEA:
		Keep for every storage an value waitcount
		if a requested temp could not be delivered right now, increment the waitcount for every storage that applies
			(based on _createflags and on the requested size - the requested size shoud be < what the storage can offer)
			push the FilePointer in a waitqueue
		
		for a new request, although there are waiting requests, we see if we can deliver from a storage with waitcount == 0
			and enough free space
			
		when a request from waitqueue is delivered/popped, decrement the waitcount for all the storages that apply
	*/
	size_t		strgidx = -1;
	for(
		TempConfigurationImpl::StorageVectorT::const_iterator it(d.tempcfg.storagevec.begin());
		it != d.tempcfg.storagevec.end();
		++it
	){
		if(it->canUse(_rcmd.size, _rcmd.openflags)){
			if(it->shouldUse(_rcmd.size)){
				strgidx = it - d.tempcfg.storagevec.begin();
				d.tempidxvec.clear();
				break;
			}else{
				d.tempidxvec.push_back(it - d.tempcfg.storagevec.begin());
			}
		}
	}
	if(strgidx != static_cast<size_t>(-1)){
		//found a storage with free space
		TempConfigurationImpl::Storage	&rstrg(d.tempcfg.storagevec[strgidx]);
		rstrg.usedsize += _rcmd.size;
		size_t							fileid;
		if(rstrg.idcache.size()){
			fileid = rstrg.idcache.top();
			rstrg.idcache.pop();
		}else{
			fileid = rstrg.currentid;
			++rstrg.currentid;
		}
		_rcmd.fileid = fileid;
		_rcmd.storageid = strgidx;
	}else if(d.tempidxvec.size()){
		
	}else{
		//no storage can fulfill the request
		_rerr.assign(1, _rerr.category());
	}
}

bool Utf8Controller::prepareTempPointer(
	CreateTempCommandBase &_rcmd, FilePointerT &_rptr, size_t &_rflags, ERROR_NS::error_code &_rerr
){
	//if prepareTempIndex was able to find a free storage, we initiate *(_rptr) accordingly and return true
	//otherwise we store/queue the pointer (eventually, in a stripped down version) and return false
	if(_rcmd.storageid != static_cast<size_t>(-1)){
		//found a valid storage
		prepareOpenTemp(*_rptr, _rcmd.size, _rcmd.fileid, _rcmd.storageid);
		return true;
	}else{
		cassert(d.tempidxvec.size());
		
		UidT	uid = _rptr.id();
		File	*pf = _rptr.release();
		
		//must wait for a storage to free up
		for(SizeVectorT::const_iterator it = d.tempidxvec.begin(); it != d.tempidxvec.end(); ++it){
			TempConfigurationImpl::Storage	&rstrg(d.tempcfg.storagevec[*it]);
			++rstrg.waitcount;
			if(rstrg.waitcount == 1){
				rstrg.waitsizefirst = _rcmd.size;
				rstrg.waitidxfirst = uid.first;
			}
			d.tempwaitdq.push_back(TempWaitStub(uid, pf, _rcmd.size, *it));
		}
	}
	return false;
}

void Utf8Controller::prepareOpenTemp(File &_rf, uint64 _sz, const size_t _fileid, const size_t _storeid){
	TempConfigurationImpl::Storage	&rstrg(d.tempcfg.storagevec[_storeid]);
	//only creates the file backend - does not open it
}

void Utf8Controller::clear(File &_rf, const size_t _idx, shared::UidVectorT &_erasevec){
	if(!_rf.isTemp()){
		_rf.clear();
		Utf8PathStub	path;
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
		TempConfigurationImpl::Storage	&rstrg(d.tempcfg.storagevec[temp.tempstorageid]);
		rstrg.usedsize -= temp.tempsize;
		rstrg.idcache.push(temp.tempid);
		
		doCloseTemp(temp);
		_rf.clear();
		
		if(rstrg.canDeliver()){
			doDeliverTemp(temp.tempstorageid, _erasevec);
		}
	}
}

void Utf8Controller::doCloseTemp(TempBase &_rtemp){
	//erase the temp file for on-disk temps
	TempConfigurationImpl::Storage	&rstrg(d.tempcfg.storagevec[_rtemp.tempstorageid]);
}

void Utf8Controller::doDeliverTemp(const size_t _storeid, shared::UidVectorT &_erasevec){
	TempConfigurationImpl::Storage	&rstrg(d.tempcfg.storagevec[_storeid]);
	TempWaitDequeT::iterator 		it = d.tempwaitdq.begin();
	size_t							fileid;
	
	if(rstrg.idcache.size()){
		fileid = rstrg.idcache.top();
		rstrg.idcache.pop();
	}else{
		fileid = rstrg.currentid;
		++rstrg.currentid;
	}
	
	if(rstrg.waitsizefirst != 0){
		for(; it != d.tempwaitdq.end(); ++it){
			if(it->objuid.first == rstrg.waitidxfirst){
				break;
			}
		}
	}else{
		//TODO: find the first wait for _storeid and go back while it->objuid.first == foundwaitidx
	}
	
	prepareOpenTemp(*it->pfile, it->size, fileid, _storeid);
	_erasevec.push_back(it->objuid);//this way the waiting callback will be called
	
	//clear the items in waitdq
	size_t						waitidx = rstrg.waitidxfirst;
	
	for(; it != d.tempwaitdq.end(); ++it){
		if(it->objuid.first == waitidx){
			TempConfigurationImpl::Storage	&rstrg2(d.tempcfg.storagevec[it->storageid]);
			--rstrg2.waitcount;
			if(rstrg2.waitidxfirst == waitidx){
				rstrg2.waitidxfirst = -1;
				rstrg2.waitsizefirst = 0;
			}
			if(it == d.tempwaitdq.begin()){
				it = d.tempwaitdq.erase(it);
			}else{
				it->clear();
			}
		}else if(it->empty() && it == d.tempwaitdq.begin()){
			it = d.tempwaitdq.erase(it);
		}else{
			break;
		}
	}
	//TODO:
	/*
	 * Need to loop until we deliver everything we can from rstrg
	 * Need to do something about rstrg2
	 * 		- need to reset the waitidxfirst to -1 and let it be computed on future close on rstrg2
	 */
	
}


}//namespace file
}//namespace frame
}//namespace solid
