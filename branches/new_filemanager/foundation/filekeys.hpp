/* Declarations file filekeys.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CS_FILE_KEYS_HPP
#define CS_FILE_KEYS_HPP

#include "foundation/filekey.hpp"

namespace foundation{
//! A filekey based on the name/path of the file
struct NameFileKey: public FileKey{
	static void registerMapper(FileManager &, const char *_prefix = NULL);
	NameFileKey(const char *_fname = NULL);
	NameFileKey(const std::string &_fname);
	std::string 	name;
private:
	/*virtual*/ void fileName(FileManager &_fm, uint32 _fileid, std::string &_fname)const;
	/*virtual*/ uint32 find(FileManager &_fm)const;
	/*virtual*/ void insert(FileManager &_fm, uint32 _fileid)const;
	/*virtual*/ void erase(FileManager &_fm, uint32 _fileid)const;
	/*virtual*/ FileKey* clone()const;
};
//! A filekey based on the name/path of the file
/*!
	For speed/memory allocation purposes use this instead of the above
*/
struct FastNameFileKey: public FileKey{
	FastNameFileKey(const char *_name):name(_name){}
	const char *name;
private:
	/*virtual*/ void fileName(FileManager &_fm, uint32 _fileid, std::string &_fname)const;
	/*virtual*/ uint32 find(FileManager &_fm)const;
	/*virtual*/ void insert(FileManager &_fm, uint32 _fileid)const;
	/*virtual*/ void erase(FileManager &_fm, uint32 _fileid)const;
	/*virtual*/ FileKey* clone()const;
};

//! A key for temporary files
struct TempFileKey: public FileKey{
	static void registerMapper(FileManager &, const char *_prefix = NULL);
	TempFileKey(){}
private:
	/*virtual*/ bool release()const;
	/*virtual*/ void fileName(FileManager &_fm, uint32 _fileid, std::string &_fname)const;
	/*virtual*/ uint32 find(FileManager &_fm)const;
	/*virtual*/ void insert(FileManager &_fm, uint32 _fileid)const;
	/*virtual*/ void erase(FileManager &_fm, uint32 _fileid)const;
	/*virtual*/ FileKey* clone()const;
};


//! A key for temporary in memory files
struct MemoryFileKey: public FileKey{
	static void registerMapper(FileManager &, const char *_prefix = NULL);
	MemoryFileKey(const uint64& _size):size(_size){}
	uint64	size;
private:
	/*virtual*/ bool release()const;
	/*virtual*/ void fileName(FileManager &_fm, uint32 _fileid, std::string &_fname)const;
	/*virtual*/ uint32 find(FileManager &_fm)const;
	/*virtual*/ void insert(FileManager &_fm, uint32 _fileid)const;
	/*virtual*/ void erase(FileManager &_fm, uint32 _fileid)const;
	/*virtual*/ FileKey* clone()const;
};


}//namespace foundation


#endif
