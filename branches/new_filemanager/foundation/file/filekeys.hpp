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
	TempKey();
	~TempKey();
	/*virtual*/ uint32 mapperId()const;
	/*virtual*/ Key* clone() const;
	/*virtual*/ bool release()const;
};

struct MemoryKey: Key{
	MemoryKey();
	~MemoryKey();
	/*virtual*/ uint32 mapperId()const;
	/*virtual*/ Key* clone() const;
	/*virtual*/ bool release()const;
};


}//namespace file
}//namespace foundation

#endif
