/* Declarations file iostream.hpp
	
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

#ifndef UTILITY_IOSTREAM_HPP
#define UTILITY_IOSTREAM_HPP

#include "utility/istream.hpp"
#include "utility/ostream.hpp"

//! A stream for both input and output
class InputOutputStream: public InputStream, public OutputStream{
public:
	virtual ~InputOutputStream();
};
//! An InputOutputStreamIterator - an offset within the stream: a pointer to an iostream
struct InputOutputStreamIterator{
	InputOutputStreamIterator(InputOutputStream *_ps = NULL, int64 _off = 0);
	void reinit(InputOutputStream *_ps = NULL, int64 _off = 0);
	int64 start();
	InputOutputStream* operator->() const{return ps;}
	InputOutputStream& operator*() {return *ps;}
	InputOutputStream	*ps;
	int64		off;
};

#ifndef NINLINES
#include "utility/iostream.ipp"
#endif

#endif
