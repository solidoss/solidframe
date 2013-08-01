// frame/file/filemapper.hpp
//
// Copyright (c) 2010 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_FILE_FILE_MAPPER_HPP
#define FILE_MAPPER_HPP

#include "frame/file/filemanager.hpp"

namespace solid{

struct TimeSpec;

namespace frame{
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
}//namespace frame
}//namespace solid

#endif
