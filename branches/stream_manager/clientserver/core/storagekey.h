#ifndef CS_STORAGE_KEY_H
#define CS_STORAGE_KEY_H

namespace clientserver{

class StorageManager;

struct StorageKey{
	virtual ~StorageKey();
protected:
	friend class StorageManager;
	//if returns true the object will be deleted - default will return true
	virtual bool release()const;
	virtual void fileName(StorageManager &_sm, int _fileid, string &_fname)const = 0;
	virtual int find(StorageManager &_sm)const = 0;
	virtual void insert(StorageManager &_sm, int _fileid)const = 0;
	virtual void erase(StorageManager &_sm, int _fileid)const = 0;
	virtual StorageKey* clone()const = 0;
};


}//namespace clientserver


#endif
