/* Declarations file stream.h
	
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

#ifndef UTILITY_STREAM_H
#define UTILITY_STREAM_H

#include "common.h"
#include <cstdlib>

class Stream{
public:
	virtual ~Stream(){}
	virtual int64 seek(int64, SeekRef _ref = SeekBeg) = 0;
	virtual int release();
	virtual int64 size()const;
	virtual bool isOk()const;
	//virtual int64 wseek(int64) = 0;
};

#endif

