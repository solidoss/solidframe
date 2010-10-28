/* Declarations file filekeys.hpp
	
	Copyright 2010 Valentin Palade 
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

#ifndef FOUNDATION_FILE_FILE_KEYS_HPP
#define FOUNDATION_FILE_FILE_KEYS_HPP

#include "foundation/file/filekey.hpp"

namespace foundation{
namespace file{

//! A key for requesting disk files from a NameMapper
/*!
	The file is identified through its absolute path.
	The path will be deeply copy.
	Use FastNameString to optimize the copying of the
	path, if for example the path is given as literal.
*/
struct NameKey: Key{
	NameKey(const char *_fname = NULL);
	NameKey(const std::string &_fname);
	~NameKey();
	/*virtual*/ uint32 mapperId()const;
	/*virtual*/ Key* clone() const;
	/*virtual*/ const char* path() const;
	std::string 	name;
};
//! A key like NameKey, for paths as literals
struct FastNameKey: Key{
	FastNameKey(const char *_fname = NULL);
	~FastNameKey();
	/*virtual*/ uint32 mapperId()const;
	/*virtual*/ Key* clone() const;
	/*virtual*/ const char* path() const;
private:
	const char	*name;
};

//! A key for requesting temporary files of certain capacity
struct TempKey: Key{
	TempKey(uint64 _cp = -1):cp(_cp){}
	~TempKey();
	const uint64	cp;
	/*virtual*/ uint32 mapperId()const;
	/*virtual*/ Key* clone() const;
	/*virtual*/ bool release()const;
	/*virtual*/ uint64 capacity()const;
};

//! A key for requesting memory files of certain capacity
struct MemoryKey: Key{
	MemoryKey(uint64 _cp = -1):cp(_cp){}
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
