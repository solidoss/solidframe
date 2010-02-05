#ifndef FILE_MAPPERS_HPP
#define FILE_MAPPERS_HPP

#include "foundation/file/filemanager.hpp"
#include "foundation/file/filemapper.hpp"

namespace foundation{
namespace file{

struct NameMapper: Mapper{
	NameMapper(uint32 _cnt = -1, uint32 _wait = 0);
	/*virtual*/ ~NameMapper();
	/*virtual*/ void id(uint32 _id);
	/*virtual*/ void execute(Manager::Stub &_rs);
	/*virtual*/ void stop();
	/*virtual*/ bool erase(File *_pf);
	/*virtual*/ File* findOrCreateFile(const Key &_rk);
	/*virtual*/ int open(File &_rf);
	/*virtual*/ void close(File &_rf);
	/*virtual*/ bool setTimeout(TimeSpec &_rts);
private:
	struct Data;
	Data &d;
};


struct TempMapper: Mapper{
	TempMapper(uint64 _cp, const char *_pfx);
	/*virtual*/ ~TempMapper();
	/*virtual*/ void id(uint32 _id);
	/*virtual*/ void execute(Manager::Stub &_rs);
	/*virtual*/ void stop();
	/*virtual*/ bool erase(File *_pf);
	/*virtual*/ File* findOrCreateFile(const Key &_rk);
	/*virtual*/ int open(File &_rf);
	/*virtual*/ void close(File &_rf);
	/*virtual*/ bool setTimeout(TimeSpec &_rts);
private:
	struct Data;
	Data &d;
};

struct MemoryMapper: Mapper{
	MemoryMapper(uint64 _cp);
	/*virtual*/ ~MemoryMapper();
	/*virtual*/ void id(uint32 _id);
	/*virtual*/ void execute(Manager::Stub &_rs);
	/*virtual*/ void stop();
	/*virtual*/ bool erase(File *_pf);
	/*virtual*/ File* findOrCreateFile(const Key &_rk);
	/*virtual*/ int open(File &_rf);
	/*virtual*/ void close(File &_rf);
	/*virtual*/ bool setTimeout(TimeSpec &_rts);
private:
	struct Data;
	Data &d;
};

}//namespace file
}//namespace foundation


#endif
