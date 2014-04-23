// utility/ostream.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_OSTREAM_HPP
#define UTILITY_OSTREAM_HPP

#include "utility/stream.hpp"
#include "utility/common.hpp"

namespace solid{

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

}//namespace solid

#endif
