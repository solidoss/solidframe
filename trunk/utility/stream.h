/* Declarations file stream.h
	
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

#ifndef UTILITY_STREAM_H
#define UTILITY_STREAM_H

#include "common.h"
#include <cstdlib>

struct StreamFlags{
	enum{
		IBad  = 1,
		OBad  = 2,
		IFail = 4,
		OFail = 8,
		IEof  = 16,
		OEof  = 32,
	};
	StreamFlags(uint32 _flags = 0):flags(_flags){}
	uint32 flags;
};

//! The base class for all streams
class Stream{
public:
	virtual ~Stream(){}
	virtual int64 seek(int64, SeekRef _ref = SeekBeg) = 0;
	virtual int release();
	virtual int64 size()const;
	bool ok()const{return flags.flags == 0;}
	bool eof()const{return flags.flags & (StreamFlags::IEof | StreamFlags::OEof);}
	bool bad()const{return flags.flags & (StreamFlags::IBad | StreamFlags::OBad);}
	bool fail()const{return bad() | flags.flags & (StreamFlags::IFail | StreamFlags::OFail);}
protected:
	StreamFlags	flags;
};

#endif

