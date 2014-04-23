// utility/iostream.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_IOSTREAM_HPP
#define UTILITY_IOSTREAM_HPP

#include "utility/istream.hpp"
#include "utility/ostream.hpp"

namespace solid{

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

}//namespace solid

#endif
