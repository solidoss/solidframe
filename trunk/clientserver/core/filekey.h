/* Declarations file filekey.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CS_FILE_KEY_H
#define CS_FILE_KEY_H

#include "system/common.h"
#include <string>

namespace clientserver{

class FileManager;

struct FileKey{
	virtual ~FileKey();
	virtual bool release()const;
	virtual void fileName(FileManager &_fm, uint32 _fileid, std::string &_fname)const = 0;
protected:
	friend class FileManager;
	virtual uint32 find(FileManager &_fm)const = 0;
	virtual void insert(FileManager &_fm, uint32 _fileid)const = 0;
	virtual void erase(FileManager &_fm, uint32 _fileid)const = 0;
	virtual FileKey* clone()const = 0;
};


}//namespace clientserver


#endif
