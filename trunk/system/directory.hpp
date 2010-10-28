/* Declarations file directory.hpp
	
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

#ifndef SYSTEM_DIRECTORY_HPP
#define SYSTEM_DIRECTORY_HPP
//! A wrapper for filesystem directory opperations
class Directory{
public:
	//! Create a new directory
	static int create(const char *);
	//! Erase a file
	static int eraseFile(const char *);
	//! Rename a file
	static int renameFile(const char *_to, const char *_from);
};

#endif
