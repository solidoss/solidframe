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


typedef std::deque<Utf8PathStub>	PathDequeT;


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

struct Utf8Controller::Data{
	
	Utf8ConfigurationImpl	filecfg;//NOTE: it is accessed without lock in openFile
	TempConfiguration		tempcfg;
	size_t					minglobalprefixsz;
	size_t					maxglobalprefixsz;
	SizePairVectorT			hashvec;
	std::string				tmp;
	PathDequeT				pathdq;
	PathSetT				pathset;
	IndexSetT				indexset;
	PathStubStackT			pathcache;
	
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
}

bool Utf8Controller::prepareTempPointer(
	CreateTempCommandBase &_rcmd, FilePointerT &_rptr, size_t &_rflags, ERROR_NS::error_code &_rerr
){
	//if prepareTempIndex was able to find a free storage, we initiate *(_rptr) accordingly and return true
	//otherwise we store/queue the pointer (eventually, in a stripped down version) and return false
	return false;
}

void Utf8Controller::clear(File &_rf, const size_t _idx){
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
}

}//namespace file
}//namespace frame
}//namespace solid
