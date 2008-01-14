#ifndef CS_STORAGE_KEYS_H
#define CS_STORAGE_KEYS_H

#include <string>

namespace clientserver{

struct NameStorageKey: public StorageKey{
	static void registerMapper(StorageManager &, const char *_prefix = NULL);
	NameStorageKey(const char *_fname = NULL);
	NameStorageKey(const std::string &_fname);
	std::string 	name;
private:
	/*virtual*/ void fileName(StorageManager &_sm, int _fileid, string &_fname)const;
	/*virtual*/ int find(StorageManager &_sm)const;
	/*virtual*/ void insert(const StorageManager &_sm, int _fileid)const;
	/*virtual*/ void erase(const StorageManager &_sm, int _fileid)const;
	/*virtual*/ StorageKey* clone()const;
};

struct FastNameStorageKey: public StorageKey{
	FastNameStorageKey(const char *_name):name(_name){}
	const char *name;
private:
	/*virtual*/ void fileName(StorageManager &_sm, int _fileid, string &_fname)const;
	/*virtual*/ int find(StorageManager &_sm)const;
	/*virtual*/ void insert(StorageManager &_sm, int _fileid)const;
	/*virtual*/ void erase(StorageManager &_sm, int _fileid)const;
	/*virtual*/ StorageKey* clone()const;
};

struct TempStorageKey: public StorageKey{
	static void registerMapper(StorageManager &, const char *_prefix = NULL);
	TempStorageKey(){}
private:
	/*virtual*/ bool release()const;
	/*virtual*/ void fileName(StorageManager &_sm, int _fileid, string &_fname)const;
	/*virtual*/ int find(StorageManager &_sm)const;
	/*virtual*/ void insert(StorageManager &_sm, int _fileid)const;
	/*virtual*/ void erase(StorageManager &_sm, int _fileid)const;
	/*virtual*/ StorageKey* clone()const;
};


}//namespace clientserver


#endif
