/* Declarations file filemappers.hpp
	
	Copyright 2010 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FOUNDATION_FILE_FILE_MAPPERS_HPP
#define FOUNDATION_FILE_FILE_MAPPERS_HPP

#include "foundation/file/filemanager.hpp"
#include "foundation/file/filemapper.hpp"

namespace foundation{
namespace file{

//! A mapper for disk files
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
	/*virtual*/ bool getTimeout(TimeSpec &_rts);
private:
	struct Data;
	Data &d;
};

//! A mapper for temporary files
struct TempMapper: Mapper{
	//! Constructor
	/*!
		You must use a TempKey to request a file from
		TempMapper. You will need to specify the maximum size of
		the requested temp file.
		\param _cp The total capacity for all open temp files
		\param _pfx Path to temp directory
	*/
	TempMapper(uint64 _cp, const char *_pfx);
	/*virtual*/ ~TempMapper();
	/*virtual*/ void id(uint32 _id);
	/*virtual*/ void execute(Manager::Stub &_rs);
	/*virtual*/ void stop();
	/*virtual*/ bool erase(File *_pf);
	/*virtual*/ File* findOrCreateFile(const Key &_rk);
	/*virtual*/ int open(File &_rf);
	/*virtual*/ void close(File &_rf);
	/*virtual*/ bool getTimeout(TimeSpec &_rts);
private:
	struct Data;
	Data &d;
};

//! A mapper for in Memory files
struct MemoryMapper: Mapper{
	//! Constructor
	/*!
		You must use a MemoryKey to request a file from
		MemoryMapper. You will need to specify the maximum size of
		the requested memory file.
		\param _cp The total capacity for all open memory files
	*/
	MemoryMapper(uint64 _cp);
	/*virtual*/ ~MemoryMapper();
	/*virtual*/ void id(uint32 _id);
	/*virtual*/ void execute(Manager::Stub &_rs);
	/*virtual*/ void stop();
	/*virtual*/ bool erase(File *_pf);
	/*virtual*/ File* findOrCreateFile(const Key &_rk);
	/*virtual*/ int open(File &_rf);
	/*virtual*/ void close(File &_rf);
	/*virtual*/ bool getTimeout(TimeSpec &_rts);
private:
	struct Data;
	Data &d;
};

}//namespace file
}//namespace foundation


#endif
