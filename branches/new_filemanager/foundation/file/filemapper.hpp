#ifndef FILE_MAPPER_HPP
#define FILE_MAPPER_HPP

#include "foundation/file/filemanager.hpp"

namespace foundation{
namespace file{

struct Mapper{
	virtual ~Mapper();
	virtual void id(uint32 _id) = 0;
	virtual void execute(FileManager::Stub &_rs) = 0;
	virtual int erase(File *_pf) = 0;
	virtual File* findOrCreateFile(const Key &_rk) = 0;
};

}//namespace file
}//namespace foundation

#endif