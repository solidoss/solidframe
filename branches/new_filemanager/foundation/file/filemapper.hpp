#ifndef FILE_MAPPER_HPP
#define FILE_MAPPER_HPP

#include "foundation/file/filemanager.hpp"

struct TimeSpec;

namespace foundation{
namespace file{

struct Mapper{
	virtual ~Mapper();
	virtual void id(uint32 _id) = 0;
	virtual void execute(Manager::Stub &_rs) = 0;
	virtual void stop() = 0;
	virtual int erase(File *_pf) = 0;
	virtual File* findOrCreateFile(const Key &_rk) = 0;
	virtual int open(File &_rf) = 0;
	virtual bool setTimeout(TimeSpec &_rts) = 0;
};

}//namespace file
}//namespace foundation

#endif