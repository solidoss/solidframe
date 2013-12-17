// system/directory.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_DIRECTORY_HPP
#define SYSTEM_DIRECTORY_HPP

namespace solid{

//! A wrapper for filesystem directory opperations
class Directory{
public:
	//! Create a new directory
	static bool create(const char *);
	//! Erase a file
	static bool eraseFile(const char *);
	//! Rename a file
	static bool renameFile(const char *_to, const char *_from);
};

}//namespace solid

#endif
