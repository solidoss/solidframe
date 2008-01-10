#include "clientserver/storagekeys.h"
#include "clientserver/storagemapper.h"
#include "clientserver/storagemanager.h"
#include <map>

namespace clientserver{
//---------------------------------------------------------------
struct LessStrCmp{
	inline bool operator()(const char* const& _s1, const char* const& _s2)const{
		return strcmp(_s1, _s2) < 0;
	}
};

class NameStorageMapper: public StorageMapper{
public:
	int find(const char *_fname)const;
	void insert(const char *_fname, int _val);
	void erase(const char *_fname);
private:
	typedef std::map<const char*, uint32, LessStrCmp> NameMapTp;
	NameMapTp nm;
};

//---------------------------------------------------------------
/*static*/ void NameStorageKey::registerMapper(StorageManager &_sm){
	_sm.mapper(new NameStorageMapper);
}
NameStorageKey::NameStorageKey(const char *_fname):name(_fname){}

NameStorageKey::NameStorageKey(const std::string &_fname):name(_fname){}

void NameStorageKey::fileName(string &_fname)const{
	_fname = name;
}
int NameStorageKey::find(const StorageManager &_sm)const{
	const NameStorageMapper *pm(_sm.mapper<NameStorageMapper>());
	assert(pm);
	return pm->find(name.c_str());
	
}

void NameStorageKey::insert(const StorageManager &_sm, int _val)const{
	NameStorageMapper *pm(_sm.mapper<NameStorageMapper>());
	assert(pm);
	pm->insert(name.c_str(), _val);
}

void NameStorageKey::erase(const StorageManager &_sm)const{
	NameStorageMapper *pm(_sm.mapper<NameStorageMapper>());
	assert(pm);
	pm->erase(name.c_str());
}

StorageKey* NameStorageKey::clone()const{
	return new NameStorageKey(*this);
}
//---------------------------------------------------------------
void FastNameStorageKey::fileName(string &_fname)const{
}

int FastNameStorageKey::find(const StorageManager &_sm)const{
	const NameStorageMapper *pm(_sm.mapper<NameStorageMapper>());
	assert(pm);
	return pm->find(name);
}

void FastNameStorageKey::insert(const StorageManager &_sm, int _val)const{
	assert(false);
	return -1;
}

void FastNameStorageKey::erase(const StorageManager &_sm)const{
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
void NameStorageMapper::insert(const char *_fname, int _val){
	pair<NameMapTp::iterator,bool> rv(nm.insert(NameMapTp::value_type(_fname, _val)));
	assert(rv.second);
}
void NameStorageMapper::erase(const char *_fname){
	nm.erase(_fname);
}

}//namespace clientserver

