/* Declarations file ostream.h
	
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

#ifndef OSTREAMPP_H
#define OSTREAMPP_H

#include "stream.h"
#include "common.h"

//! A stream for writing
class OStream: virtual public Stream{
public:
	virtual ~OStream(){}
	virtual int write(const char *, uint32, uint32 _flags = 0) = 0;
	bool ook()const{return flags.flags == 0;}
	bool oeof()const{return flags.flags & StreamFlags::OEof;}
	bool obad()const{return flags.flags & StreamFlags::OBad;}
	bool ofail()const{return obad() | flags.flags & StreamFlags::OFail;}
};

//! An OStreamIterator - an offset within the stream: a pointer to an ostream
struct OStreamIterator{
	OStreamIterator(OStream *_ps = NULL, int64 _off = 0):ps(_ps),off(_off){}
	void reinit(OStream *_ps = NULL, int64 _off = 0){
		ps = _ps;
		off = _off;
	}
	int64 start(){
		return ps->seek(off);
	}
	OStream* operator->() const{return ps;}
	OStream& operator*() {return *ps;}
	OStream		*ps;
	int64		off;
};

#endif
