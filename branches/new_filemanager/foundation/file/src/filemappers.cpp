#include "foundation/file/filemappers.hpp"
#include "foundation/file/filekeys.hpp"
#include "foundation/file/filebase.hpp"

#include "utility/queue.hpp"
#include "utility/memoryfile.hpp"
#include "utility/string.hpp"

#include "system/timespec.hpp"
#include "system/filedevice.hpp"
#include "system/directory.hpp"

#include <map>
#include <cstdio>

namespace foundation{
namespace file{

//==================================================================
//	file::NameFile
//==================================================================
class NameFile: public File{
public:
	NameFile(const Key& _rk):File(_rk){}
private:
	/*virtual*/ int read(
		char *_pb,
		uint32 _bl,
		const int64 &_off,
		uint32 _flags
	);
	/*virtual*/ int write(
		const char *_pb,
		uint32 _bl,
		const int64 &_off,
		uint32 _flags
	);
	/*virtual*/ int64 size()const;
	
	/*virtual*/ int open(const char *_path);
	/*virtual*/ void close(const char *_path);
	/*virtual*/ int64 capacity()const;
private:
	FileDevice	fd;
};

//==================================================================
//	file::MemoryFile
//==================================================================

class MemoryFile: public File{
public:
	MemoryFile(const Key& _rk, uint64 _maxsz = -1L):File(_rk), mf(_maxsz){}
private:
	/*virtual*/ int read(
		char *_pb,
		uint32 _bl,
		const int64 &_off,
		uint32 _flags
	);
	/*virtual*/ int write(
		const char *_pb,
		uint32 _bl,
		const int64 &_off,
		uint32 _flags
	);
	/*virtual*/ int64 size()const;
	
	/*virtual*/ int open(const char *_path);
	/*virtual*/ void close(const char *_path);
	/*virtual*/ int64 capacity()const;
private:
	::MemoryFile	mf;
};

//==================================================================
//	file::TempFile
//==================================================================

class TempFile: public File{
public:
	TempFile(const Key& _rk, uint64 _maxsz = -1L):File(_rk), maxsz(_maxsz){}
private:
	/*virtual*/ int read(
		char *_pb,
		uint32 _bl,
		const int64 &_off,
		uint32 _flags
	);
	/*virtual*/ int write(
		const char *_pb,
		uint32 _bl,
		const int64 &_off,
		uint32 _flags
	);
	/*virtual*/ int64 size()const;
	
	/*virtual*/ int open(const char *_path);
	/*virtual*/ void close(const char *_path);
	/*virtual*/ int64 capacity()const;
private:
	FileDevice	fd;
	uint64		maxsz;
};

//==================================================================
//	file::Mapper
//==================================================================
/*virtual*/ Mapper::~Mapper(){
}
//==================================================================
//	file::NameMapper
//==================================================================
struct LessStrCmp{
	inline bool operator()(const char* const& _s1, const char* const& _s2)const{
		return cstring::cmp(_s1, _s2) < 0;
	}
};
struct NameMapper::Data{
	Data(uint32 _cnt, uint32 _wait):maxcnt(_cnt), crtcnt(0), wait(_wait), state(0){}
	typedef Queue<IndexT>								IndexTQueueTp;
	typedef std::map<const char*, File*, LessStrCmp>	NameMapT;
	uint32			maxcnt;
	uint32			crtcnt;
	uint32			wait;
	int				state;
	IndexTQueueTp	waitq;
	NameMapT		nm;
};

NameMapper::NameMapper(uint32 _cnt, uint32 _wait):d(*(new Data(_cnt, _wait))){
}
/*virtual*/ NameMapper::~NameMapper(){
	delete &d;
}
namespace{
	uint32	name_mapper_id(-1);
}
/*virtual*/ void NameMapper::id(uint32 _id){
	cassert(name_mapper_id == (uint32)-1);
	name_mapper_id = _id;
}
/*virtual*/ void NameMapper::execute(Manager::Stub &_rs){
	cassert(!d.state);
	uint64 tmpcnt = d.crtcnt;
	while(d.waitq.size() && tmpcnt <= d.maxcnt){
		++tmpcnt;
		_rs.pushFileTempExecQueue(d.waitq.front(), File::RetryOpen);
		d.waitq.pop();
	}
}
/*virtual*/ void NameMapper::stop(){
	cassert(!d.state);
	d.state = -1;
	while(d.waitq.size()){
		d.waitq.pop();
	}
}
/*virtual*/ bool NameMapper::erase(File *_pf){
	--d.crtcnt;
	d.nm.erase(_pf->key().path());
	delete _pf;
	return d.waitq.size() != 0;
}
/*virtual*/ File* NameMapper::findOrCreateFile(const Key &_rk){
	cassert(!d.state);
	Data::NameMapT::const_iterator it(d.nm.find(_rk.path()));
	if(it != d.nm.end()){
		return it->second;
	}
	File *pf = new NameFile(_rk);
	d.nm[pf->key().path()] = pf;
	return pf;
}
/*virtual*/ int NameMapper::open(File &_rf){
	cassert(!d.state);
	if(d.crtcnt <= d.maxcnt){
		++d.crtcnt;
		return _rf.open(_rf.key().path());
	}
	d.waitq.push(_rf.id());
	return File::No;
}
/*virtual*/ void NameMapper::close(File &_rf){
	_rf.close(_rf.key().path());
}
/*virtual*/ bool NameMapper::setTimeout(TimeSpec &_rts){
	if(d.wait){
		_rts += d.wait;
		return true;
	}
	return false;
}

//==================================================================
//	file::TempMapper
//==================================================================
struct TempMapper::Data{
	Data(uint64 _cp, const char *_pfx):cp(_cp), wait(0), state(0), sz(0), pfx(_pfx){}
	
	void createFileName(std::string &_fname, uint64 _fileid);
	void initFolders();
	
	typedef std::pair<IndexT, uint64>	IndexPairT;
	typedef Queue<IndexPairT>			IndexQueueT;
	
	uint64			cp;
	uint32			wait;
	int				state;
	uint64			sz;
	std::string		pfx;
	IndexQueueT		waitq;
};

void TempMapper::Data::createFileName(std::string &_fname, uint64 _fileid){
	char name[32];
	unsigned int fldid = _fileid & 0xff;
	unsigned int filid = _fileid >> 8;
	sprintf(name, "%2.2X/%6.6x", fldid, filid);
	_fname = pfx;
	_fname.append(name);
}
void TempMapper::Data::initFolders(){
	std::string path(pfx);
	char name[32];
	for(unsigned i(0); i <= 0xff; ++i){
		path.resize(pfx.size());
		sprintf(name, "%2.2X", i);
		path.append(name);
		Directory::create(path.c_str());
	}
}
TempMapper::TempMapper(uint64 _cp, const char *_pfx):d(*(new Data(_cp, ""))){
	if(_pfx && _pfx[0]){
		d.pfx = _pfx;
		if(d.pfx[d.pfx.size() - 1] != '/'){
			d.pfx += '/';
		}
	}
	Directory::create(d.pfx.c_str());
	d.initFolders();
}
/*virtual*/ TempMapper::~TempMapper(){
	delete &d;
}
namespace{
	uint32	temp_mapper_id(-1);
}
/*virtual*/ void TempMapper::id(uint32 _id){
	cassert(temp_mapper_id == (uint32)-1);
	temp_mapper_id = _id;
}
/*virtual*/ void TempMapper::execute(Manager::Stub &_rs){
	cassert(!d.state);
	uint64 tmpsz = d.sz;
	while(d.waitq.size() && d.waitq.front().second <= (d.cp - tmpsz)){
		tmpsz += d.waitq.front().second;
		_rs.pushFileTempExecQueue(d.waitq.front().first, File::RetryOpen);
		d.waitq.pop();
	}
}
/*virtual*/ void TempMapper::stop(){
	d.state = -1;
	while(d.waitq.size()){
		d.waitq.pop();
	}
}
/*virtual*/ bool TempMapper::erase(File *_pf){
	uint64 fcp = _pf->capacity();
	delete _pf;
	d.sz -= fcp;
	if(d.waitq.size() && d.waitq.front().second <= (d.cp - d.sz)){
		return true;
	}
	return false;
}
/*virtual*/ File* TempMapper::findOrCreateFile(const Key &_rk){
	return new TempFile(_rk, _rk.capacity());
}
/*virtual*/ int TempMapper::open(File &_rf){
	cassert(!d.state);
	uint64 fcp = _rf.capacity();
	if(fcp > d.cp)
		return File::Bad;
	if(fcp <= (d.cp - d.sz)){
		d.sz += fcp;
		std::string fpath;
		d.createFileName(fpath, _rf.id());
		return _rf.open(fpath.c_str());
	}
	d.waitq.push(Data::IndexPairT(_rf.id(), fcp));
	return File::No;
}
/*virtual*/ void TempMapper::close(File &_rf){
	std::string fpath;
	d.createFileName(fpath, _rf.id());
	_rf.close(fpath.c_str());
}
/*virtual*/ bool TempMapper::setTimeout(TimeSpec &_rts){
	if(d.wait){
		_rts += d.wait;
		return true;
	}
	return false;
}

//==================================================================
//	file::MemoryMapper
//==================================================================
struct MemoryMapper::Data{
	Data(uint64 _cp, uint32 _wait = 0):cp(_cp), wait(_wait),state(0),sz(0){}
	typedef std::pair<IndexT, uint64>	IndexPairT;
	typedef Queue<IndexPairT>			IndexQueueT;
	uint64			cp;
	uint32			wait;
	int				state;
	uint64			sz;
	IndexQueueT		waitq;
};

MemoryMapper::MemoryMapper(uint64 _cp):d(*(new Data(_cp))){
}
/*virtual*/ MemoryMapper::~MemoryMapper(){
	cassert(!d.sz);
	delete &d;
}
namespace{
	uint32	memory_mapper_id(-1);
}
/*virtual*/ void MemoryMapper::id(uint32 _id){
	cassert(memory_mapper_id == (uint32)-1);
	memory_mapper_id = _id;
}
/*virtual*/ void MemoryMapper::execute(Manager::Stub &_rs){
	cassert(!d.state);
	uint64 tmpsz = d.sz;
	while(d.waitq.size() && d.waitq.front().second <= (d.cp - tmpsz)){
		tmpsz += d.waitq.front().second;
		_rs.pushFileTempExecQueue(d.waitq.front().first, File::RetryOpen);
		d.waitq.pop();
	}
}
/*virtual*/ void MemoryMapper::stop(){
	d.state = -1;
	while(d.waitq.size()){
		d.waitq.pop();
	}
}
/*virtual*/ bool MemoryMapper::erase(File *_pf){
	uint64 fcp = _pf->capacity();
	delete _pf;
	d.sz -= fcp;
	if(d.waitq.size() && d.waitq.front().second <= (d.cp - d.sz)){
		return true;
	}
	return false;
}
/*virtual*/ File* MemoryMapper::findOrCreateFile(const Key &_rk){
	cassert(!d.state);
	
	return new MemoryFile(_rk, _rk.capacity());
}
/*virtual*/ int MemoryMapper::open(File &_rf){
	cassert(!d.state);
	uint64 fcp = _rf.capacity();
	if(fcp > d.cp)
		return File::Bad;
	if(fcp <= (d.cp - d.sz)){
		d.sz += fcp;
		return _rf.open();
	}
	d.waitq.push(Data::IndexPairT(_rf.id(), fcp));
	return File::No;
}
/*virtual*/ void MemoryMapper::close(File &_rf){
	_rf.close();
}
/*virtual*/ bool MemoryMapper::setTimeout(TimeSpec &_rts){
	if(d.wait){
		_rts += d.wait;
		return true;
	}
	return false;
}


//==================================================================
//	file::NameFile
//==================================================================

/*virtual*/ int NameFile::read(
	char *_pb,
	uint32 _bl,
	const int64 &_off,
	uint32 _flags
){
	return fd.read(_pb, _bl, _off);
}

/*virtual*/ int NameFile::write(
	const char *_pb,
	uint32 _bl,
	const int64 &_off,
	uint32 _flags
){
	return fd.write(_pb, _bl, _off);
}

/*virtual*/ int64 NameFile::size()const{
	return fd.size();
}

/*virtual*/ int64 NameFile::capacity()const{
	return -1;
}

/*virtual*/ int NameFile::open(const char *_path){
	//this is a little bit more complicated
	//because we want to open the file in the most
	//accurate mode
	int mode = 0;
	//just to be sure:
		
	if(openmoderequest & Manager::Create){
		mode |= FileDevice::CR;
	}
	
	if((openmoderequest & (Manager::OpenR | Manager::OpenW)) == (Manager::OpenR | Manager::OpenW))
		openmoderequest |= Manager::OpenRW;
	
	if(openmoderequest & Manager::Truncate)
		mode |= FileDevice::TR;
		
	if(openmoderequest & Manager::Append)
		mode |= FileDevice::AP;
	
	if(openmoderequest & Manager::OpenRW){
		mode |= FileDevice::RW;
		int rv = fd.open(_path, mode);
		if(!rv){
			openmode = (Manager::OpenR | Manager::OpenW | Manager::OpenRW);
			return Ok;
		}
		//now try to fall back to another requested open mode
		if(openmoderequest & Manager::OpenW){
			mode &= ~FileDevice::FileDevice::RW;
			mode |= FileDevice::RO;
			if(!rv){
				openmode = Manager::OpenR;
				return Ok;
			}
		}
		if(openmoderequest & Manager::OpenW){
			mode &= ~FileDevice::FileDevice::RW;
			mode |= FileDevice::WO;
			if(!rv){
				openmode = Manager::OpenW;
				return Ok;
			}
		}
		openmode = 0;
		return Bad;
	}else if(openmoderequest & Manager::OpenR){
		mode |= FileDevice::RO;
		int rv = fd.open(_path, mode);
		if(rv){
			openmode = 0;
			return Bad;
		}
		openmode = Manager::OpenR;
		return Ok;
	}else if(openmoderequest & Manager::OpenW){
		mode |= FileDevice::WO;
		int rv = fd.open(_path, mode);
		if(rv){
			openmode = 0;
			return Bad;
		}
		openmode = Manager::OpenW;
		return Ok;
	}
	
	openmode = 0;
	return BAD;
}

/*virtual*/ void NameFile::close(const char *_path){
	fd.close();
	openmode = 0;
}


//==================================================================
//	file::MemoryFile
//==================================================================

/*virtual*/ int MemoryFile::read(
	char *_pb,
	uint32 _bl,
	const int64 &_off,
	uint32 _flags
){
	return mf.read(_pb, _bl, _off);
}
/*virtual*/ int MemoryFile::write(
	const char *_pb,
	uint32 _bl,
	const int64 &_off,
	uint32 _flags
){
	return mf.write(_pb, _bl, _off);
}
/*virtual*/ int64 MemoryFile::size()const{
	return mf.size();
}

/*virtual*/ int MemoryFile::open(const char *_path){
	openmode = Manager::OpenR | Manager::OpenW | Manager::OpenRW;
	return OK;
}
/*virtual*/ void MemoryFile::close(const char *_path){
	mf.truncate();
}
/*virtual*/ int64 MemoryFile::capacity()const{
	return mf.capacity();
}


//==================================================================
//	file::TempFile
//==================================================================

/*virtual*/ int TempFile::read(
	char *_pb,
	uint32 _bl,
	const int64 &_off,
	uint32 _flags
){
	return fd.read(_pb, _bl, _off);
}
/*virtual*/ int TempFile::write(
	const char *_pb,
	uint32 _bl,
	const int64 &_off,
	uint32 _flags
){
	return fd.write(_pb, _bl, _off);
}
/*virtual*/ int64 TempFile::size()const{
	return fd.size();
}

/*virtual*/ int TempFile::open(const char *_path){
	//only open in RW mode
	int rv = fd.open(_path, FileDevice::RW | FileDevice::CR | FileDevice::TR);
	if(!rv){
		this->openmode = 0;
		return Bad;
	}
	openmode = Manager::OpenR | Manager::OpenW | Manager::OpenRW;
	return OK;
}
/*virtual*/ void TempFile::close(const char *_path){
	fd.close();
	openmode = 0;
	Directory::eraseFile(_path);
}
/*virtual*/ int64 TempFile::capacity()const{
	return maxsz;
}
//==================================================================
//	file::Key
//==================================================================
/*virtual*/ Key::~Key(){
}
/*virtual*/ bool Key::release()const{
	return true;
}
/*virtual*/ const char* Key::path() const{
	return NULL;
}
/*virtual*/ uint64 Key::capacity()const{
	return -1L;
}
//==================================================================
//	file::NameKey
//==================================================================
NameKey::NameKey(const char *_fname):name(_fname ? _fname : ""){
}
NameKey::NameKey(const std::string &_fname):name(_fname){
}
NameKey::~NameKey(){}

/*virtual*/ uint32 NameKey::mapperId()const{
	return name_mapper_id;
}
/*virtual*/ Key* NameKey::clone() const{
	return new NameKey(name);
}
/*virtual*/ const char* NameKey::path() const{
	return name.c_str();
}

//==================================================================
//	file::FastNameKey
//==================================================================

FastNameKey::FastNameKey(const char *_fname):name(_fname){
}
FastNameKey::~FastNameKey(){
}
/*virtual*/ uint32 FastNameKey::mapperId()const{
	return name_mapper_id;
}
/*virtual*/ Key* FastNameKey::clone() const{
	return new NameKey(name);
}
/*virtual*/ const char* FastNameKey::path() const{
	return name;
}
//==================================================================
//	file::TempKey
//==================================================================
namespace{
	TempKey		tmpkey;
}
TempKey::~TempKey(){
}
/*virtual*/ uint32 TempKey::mapperId()const{
	return temp_mapper_id;
}
/*virtual*/ Key* TempKey::clone() const{
	return &tmpkey;
}
/*virtual*/ bool TempKey::release()const{
	return false;
}
/*virtual*/ uint64 TempKey::capacity()const{
	return cp;
}
//==================================================================
//	file::MemoryKey
//==================================================================
namespace{
	MemoryKey memkey;
}
MemoryKey::~MemoryKey(){
}
/*virtual*/ uint32 MemoryKey::mapperId()const{
	return memory_mapper_id;
}
/*virtual*/ Key* MemoryKey::clone() const{
	return &memkey;
}
/*virtual*/ bool MemoryKey::release()const{
	return false;
}
/*virtual*/ uint64 MemoryKey::capacity()const{
	return cp;
}

}//namespace file
}//namespace foundation
