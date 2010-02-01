#ifndef FILE_KEY_HPP
#define FILE_KEY_HPP

namespace foundation{
namespace file{

struct Key{
	virtual ~Key();
	virtual uint32 mapperId()const = 0;
};

}//namespace file
}//namepsace foundation
#endif
