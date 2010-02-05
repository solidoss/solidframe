#ifndef FILE_KEYS_HPP
#define FILE_KEYS_HPP

#include "foundation/file/filekey.hpp"

namespace foundation{
namespace file{

struct NameKey: Key{
	NameKey(const char *_fname = NULL);
	NameKey(const std::string &_fname);
	~NameKey();
	/*virtual*/ uint32 mapperId()const;
	/*virtual*/ Key* clone() const;
	/*virtual*/ const char* path() const;
private:
	std::string 	name;
};

struct FastNameKey: Key{
	FastNameKey(const char *_fname = NULL);
	~FastNameKey();
	/*virtual*/ uint32 mapperId()const;
	/*virtual*/ Key* clone() const;
	/*virtual*/ const char* path() const;
private:
	const char	*name;
};

struct TempKey: Key{
	TempKey(uint64 _cp = -1L):cp(_cp){}
	~TempKey();
	const uint64	cp;
	/*virtual*/ uint32 mapperId()const;
	/*virtual*/ Key* clone() const;
	/*virtual*/ bool release()const;
	/*virtual*/ uint64 capacity()const;
};

struct MemoryKey: Key{
	MemoryKey(uint64 _cp = -1L):cp(_cp){}
	~MemoryKey();
	const uint64 cp;
	/*virtual*/ uint32 mapperId()const;
	/*virtual*/ Key* clone() const;
	/*virtual*/ bool release()const;
	/*virtual*/ uint64 capacity()const;
};


}//namespace file
}//namespace foundation

#endif
