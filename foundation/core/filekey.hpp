/* Declarations file filekey.hpp
	
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

#ifndef CS_FILE_KEY_HPP
#define CS_FILE_KEY_HPP

#include "system/common.hpp"
#include <string>

namespace clientserver{

class FileManager;
//! A base class for file keys
/*!
	<b>Overview:</b><br>
	In order to ensure non conflicting access to a file (no two OStreams to the same file),
	the FileManager must know which files are already open. Now it could only hold the name of
	the file and do a search on them. But this is both slow (must search for strings) and 
	memory consumming. More over, there are times when the file name can be constructed
	from binary values (intergers, timestamps etc - e.g. the message numbers within a message
	box) which are much faster to search.
	
	So file key tries to fix the problem together with FileMapper.
	Consider the FileMapper(s) as plugins for filekey containers, and the filekeys as
	proxies to those mappers.<br>
	<b>Usage:</b><br>
	- Inherit FileKey and implement the interface
	- Eventually create a FileMapper which must be registered on FileManager
*/
struct FileKey{
	virtual ~FileKey();
	//! If returns true the file key will be deleted
	virtual bool release()const;
	//! Computes the filename into _fname
	virtual void fileName(FileManager &_fm, uint32 _fileid, std::string &_fname)const = 0;
protected:
	friend class FileManager;
	//! Searches the file within the mapper
	virtual uint32 find(FileManager &_fm)const = 0;
	//! Inserts the key into the mapper
	virtual void insert(FileManager &_fm, uint32 _fileid)const = 0;
	//! Erases the key from the mapper
	virtual void erase(FileManager &_fm, uint32 _fileid)const = 0;
	//! Clones the key.
	virtual FileKey* clone()const = 0;
};


}//namespace clientserver


#endif
