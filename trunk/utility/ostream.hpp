/* Declarations file ostream.hpp
	
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

#ifndef OSTREAMPP_HPP
#define OSTREAMPP_HPP

#include "stream.hpp"
#include "common.hpp"

//! A stream for writing
class OStream: virtual public Stream{
public:
	virtual ~OStream();
	virtual int write(const char *, uint32, uint32 _flags = 0) = 0;
	virtual int write(uint64 _offset, const char *_pbuf, uint32 _blen, uint32 _flags = 0);
	bool ook()const;
	bool oeof()const;
	bool obad()const;
	bool ofail()const;
};

//! An OStreamIterator - an offset within the stream: a pointer to an ostream
struct OStreamIterator{
	OStreamIterator(OStream *_ps = NULL, int64 _off = 0);
	void reinit(OStream *_ps = NULL, int64 _off = 0);
	int64 start();
	OStream* operator->() const{return ps;}
	OStream& operator*() {return *ps;}
	OStream		*ps;
	int64		off;
};

#ifndef NINLINES
#include "src/ostream.ipp"
#endif

#endif
