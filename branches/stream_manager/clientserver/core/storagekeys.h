#ifndef CS_STORAGE_KEYS_H
#define CS_STORAGE_KEYS_H

#include <string>

namespace clientserver{

struct NameStorageKey: public StorageKey{
	static void registerMapper(StorageManager &);
	NameStorageKey(const char *_fname = NULL);
	NameStorageKey(const std::string &_fname);
	std::string 	name;
private:
	/*virtual*/ void fileName(string &_fname)const;
	/*virtual*/ int find(const StorageManager &_sm)const;
	/*virtual*/ int insert(const StorageManager &_sm, int _val)const;
	/*virtual*/ int erase(const StorageManager &_sm)const;
	/*virtual*/ StorageKey* clone()const;
};

struct FastNameStorageKey: public StorageKey{
	FastNameStorageKey(const char *_name):name(_name){}
	const char *name;
private:
	/*virtual*/ void fileName(string &_fname)const;
	/*virtual*/ int find(const StorageManager &_sm)const;
	/*virtual*/ int insert(const StorageManager &_sm, int _val)const;
	/*virtual*/ int erase(const StorageManager &_sm)const;
	/*virtual*/ StorageKey* clone()const;
};


}//namespace clientserver


#endif
