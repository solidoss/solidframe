/* Declarations file istream.hpp
	
	Copyright 2007, 2008 Valentin Palade 
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

#ifndef UTILITY_ISTREAM_HPP
#define UTILITY_ISTREAM_HPP

#include "utility/stream.hpp"
#include "utility/common.hpp"

//! A stream for reading 
class InputStream: virtual public Stream{
public:
	virtual ~InputStream();
	virtual int read(char *, uint32, uint32 _flags = 0) = 0;
	virtual int read(uint64 _offset, char*, uint32, uint32 _flags = 0);
	bool readAll(char *, uint32, uint32 _flags = 0);
	bool iok()const;
	bool ieof()const;
	bool ibad()const;
	bool ifail()const;
};

//! An InputStreamIterator - an offset within the stream: a pointer to an istream
struct InputStreamIterator{
	InputStreamIterator(InputStream *_ps = NULL, int64 _off = 0);
	void reinit(InputStream *_ps = NULL, int64 _off = 0);
	int64 start();
	InputStream* operator->() const{return ps;}
	InputStream& operator*() {return *ps;}
	InputStream		*ps;
	int64		off;
};

#ifndef NINLINES
#include "utility/istream.ipp"
#endif

#endif
