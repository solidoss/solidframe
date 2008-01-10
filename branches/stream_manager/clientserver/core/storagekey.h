#ifndef CS_STORAGE_KEY_H
#define CS_STORAGE_KEY_H

namespace clientserver{

class StorageManager;

struct StorageKey{
	virtual ~StorageKey();
protected:
	friend class StorageManager;
	virtual void fileName(string &_fname)const = 0;
	virtual int find(const StorageManager &_sm)const = 0;
	virtual int insert(const StorageManager &_sm, int _val)const = 0;
	virtual int erase(const StorageManager &_sm)const = 0;
	virtual StorageKey* clone()const = 0;
};


}//namespace clientserver


#endif
