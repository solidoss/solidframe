#ifndef FILE_KEY_HPP
#define FILE_KEY_HPP

namespace foundation{
namespace file{

struct Key{
	virtual ~Key();
	virtual uint32 mapperId()const = 0;
	virtual Key* clone() const = 0;
	//! If returns true the file key will be deleted
	virtual bool release()const;
	virtual const char* path() const;
	virtual uint64 size()const;
};

}//namespace file
}//namepsace foundation
#endif
