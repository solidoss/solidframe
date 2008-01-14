#include "clientserver/storagekeys.h"
#include "clientserver/storagemapper.h"
#include "clientserver/storagemanager.h"
#include <map>

using namespace std;

namespace clientserver{
//---------------------------------------------------------------
struct LessStrCmp{
	inline bool operator()(const char* const& _s1, const char* const& _s2)const{
		return strcmp(_s1, _s2) < 0;
	}
};

class NameStorageMapper: public StorageMapper{
public:
	NameStorageMapper(const char *_prefix){
		if(_prefix && _prefix[0]){
			pfx = _prefix;
			if(pfx[pfx.size() - 1] != '\\'){
				pfx += '\\';
			}
		}
	}
	int find(const char *_fname)const;
	void insert(const char *_fname, int _val);
	void erase(const char *_fname);
	const string & prefix()const {return pfx;}
private:
	typedef std::map<const char*, uint32, LessStrCmp> NameMapTp;
	NameMapTp nm;
	string pfx;
};

class TempStorageMapper: public StorageMapper{
public:
	TempStorageMapper(const char *_prefix){
		if(_prefix && _prefix[0]){
			pfx = _prefix;
			if(pfx[pfx.size() - 1] != '\\'){
				pfx += '\\';
			}
		}
	}
	void createFileName(string &_fname, unsigned _fileid);
private:
	void initFolders();
	string pfx;
};
//---------------------------------------------------------------
bool StorageKey::release()const{
	return true;
}
//---------------------------------------------------------------
/*static*/ void NameStorageKey::registerMapper(StorageManager &_sm, const char *_prefix){
	_sm.mapper(new NameStorageMapper(_prefix));
}
NameStorageKey::NameStorageKey(const char *_fname):name(_fname){}

NameStorageKey::NameStorageKey(const std::string &_fname):name(_fname){}

void NameStorageKey::fileName(StorageManager &_sm, int _fileid, string &_fname)const{
	const NameStorageMapper *pm(_sm.mapper<NameStorageMapper>());
	assert(pm);
	_fname = pm->prefix();
	_fname.append(name);
}
int NameStorageKey::find(StorageManager &_sm)const{
	const NameStorageMapper *pm(_sm.mapper<NameStorageMapper>());
	assert(pm);
	return pm->find(name.c_str());
	
}

void NameStorageKey::insert(StorageManager &_sm, int _fileid)const{
	NameStorageMapper *pm(_sm.mapper<NameStorageMapper>());
	assert(pm);
	pm->insert(name.c_str(), _val);
}

void NameStorageKey::erase(StorageManager &_sm, int _fileid)const{
	NameStorageMapper *pm(_sm.mapper<NameStorageMapper>());
	assert(pm);
	pm->erase(name.c_str());
}

StorageKey* NameStorageKey::clone()const{
	return new NameStorageKey(*this);
}
//---------------------------------------------------------------
void FastNameStorageKey::fileName(const StorageManager &, int _fileid, string &)const{
	assert(false);
}

int FastNameStorageKey::find(StorageManager &_sm)const{
	const NameStorageMapper *pm(_sm.mapper<NameStorageMapper>());
	assert(pm);
	return pm->find(name);
}

void FastNameStorageKey::insert(StorageManager &_sm, int _fileid)const{
	assert(false);
	return -1;
}

void FastNameStorageKey::erase(StorageManager &_sm, int _fileid)const{
	assert(false);
	return -1;
}

StorageKey* FastNameStorageKey::clone()const{
	return new NameStorageKey(name);
}
//---------------------------------------------------------------
int NameStorageMapper::find(const char *_fname)const{
	NameMapTp::const_iterator it(nm.find(_fname));
	if(it != nm.end()) return it->second;
	return -1;
}
void NameStorageMapper::insert(const char *_fname, int _fileid){
	pair<NameMapTp::iterator,bool> rv(nm.insert(NameMapTp::value_type(_fname, _fileid)));
	assert(rv.second);
}
void NameStorageMapper::erase(const char *_fname){
	nm.erase(_fname);
}

//---------------------------------------------------------------
/*static */void TempStorageKey::registerMapper(StorageManager &, const char *_prefix){
	_sm.mapper(new TempStorageMapper(_prefix));
}
void TempStorageKey::fileName(StorageManager &_sm, int _fileid, string &_fname)const{
	TempStorageMapper *pm(_sm.mapper<TempStorageMapper>());
	pm->createFileName(_fname, _fileid);
}
int TempStorageKey::find(StorageManager &)const{
	return -1;
}
void TempStorageKey::insert(StorageManager &_sm, int _fileid)const{
/*	TempStorageMapper *pm(_sm.mapper<TempStorageMapper>());
	assert(pm);*/
}
void TempStorageKey::erase(StorageManager &_sm, int _fileid)const{
	TempStorageMapper *pm(_sm.mapper<TempStorageMapper>());
	assert(pm);
	
}

static TempStorageKey tempk;

bool TempStorageKey::release()const{
	return false;
}

StorageKey* TempStorageKey::clone()const{
	return &tempk;
}
//---------------------------------------------------------------
TempStorageMapper::TempStorageMapper(const char *_prefix){
	if(_prefix && _prefix[0]){
		pfx = _prefix;
		if(pfx.size() && pfx[pfx.size() - 1] != '\\'){
			pfx += '\\';
		}
	}
	initFolders();
}
void TempStorageMapper::createFileName(string &_fname, unsigned _fileid){
	char name[32];
	unsigned int fldid = _fileid & 0xff;
	unsigned int filid = _fileid >> 8;
	sptrintf(name, "%X/%x", fldid, filid);
	_fname = pfx;
	_fname.append(name);
}
void TempStorageMapper::initFolders(){
	string path(pfx);
	char name[32];
	for(unsigned i(0); i <= 0xff; ++i){
		path.resize(pfx.size());
		sptrintf(name, "%X", fldid, filid);
		path.append(name);
		//TODO: createFolder(path);
	}
}

}//namespace clientserver

