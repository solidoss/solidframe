/* Declarations file filemapper.hpp
	
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

#ifndef FILE_MAPPER_HPP
#define FILE_MAPPER_HPP

#include "foundation/file/filemanager.hpp"

struct TimeSpec;

namespace foundation{
namespace file{

//! An interface class for all mappers
/*!
	A mapper can:<br>
	> keep a map with all open files<br>
	> limit the number of open files<br>
	> limit the size of open files (for temp/memory files with 
	limited capacity)<br>
*/
struct Mapper{
	virtual ~Mapper();
	//! Setter with mapper id
	virtual void id(uint32 _id) = 0;
	//! Called by manager
	/*!
		Uses Manager::Stub::pushFileTempExecQueue to
		add files ready for open.
	*/
	virtual void execute(Manager::Stub &_rs) = 0;
	//! Clears the queue with files waiting for open
	virtual void stop() = 0;
	//! Erases a file object
	virtual bool erase(File *_pf) = 0;
	//! Creates a file or returns a pointer to an already created file with the same key
	virtual File* findOrCreateFile(const Key &_rk) = 0;
	//! Opens a file
	/*!
		When return No, the file is queued for later open.
	*/
	virtual int open(File &_rf) = 0;
	//! Closes a file
	virtual void close(File &_rf) = 0;
	//! Gets the timeout for a file
	/*!
		After the last stream for a file gets deleted,
		if there are no pending files for open,
		the file can remain open for certain
		amount of time.
	*/
	virtual bool getTimeout(TimeSpec &_rts) = 0;
};

}//namespace file
}//namespace foundation

#endif
