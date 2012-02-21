/* Declarations file ostream.hpp
	
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

#ifndef UTILITY_OSTREAM_HPP
#define UTILITY_OSTREAM_HPP

#include "utility/stream.hpp"
#include "utility/common.hpp"

//! A stream for writing
class OutputStream: virtual public Stream{
public:
	virtual ~OutputStream();
	virtual int write(const char *, uint32, uint32 _flags = 0) = 0;
	virtual int write(uint64 _offset, const char *_pbuf, uint32 _blen, uint32 _flags = 0);
	bool ook()const;
	bool oeof()const;
	bool obad()const;
	bool ofail()const;
};

//! An OutputStreamIterator - an offset within the stream: a pointer to an ostream
struct OutputStreamIterator{
	OutputStreamIterator(OutputStream *_ps = NULL, int64 _off = 0);
	void reinit(OutputStream *_ps = NULL, int64 _off = 0);
	int64 start();
	OutputStream* operator->() const{return ps;}
	OutputStream& operator*() {return *ps;}
	OutputStream		*ps;
	int64		off;
};

#ifndef NINLINES
#include "utility/ostream.ipp"
#endif

#endif
