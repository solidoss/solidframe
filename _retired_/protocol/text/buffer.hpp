// protocol/text/buffer.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_PROTOCOL_TEXT_BUFFER_HPP
#define SOLID_PROTOCOL_TEXT_BUFFER_HPP

#include "system/common.hpp"

namespace solid{
namespace protocol{
namespace text{

struct Buffer{
	mutable char	*pbeg;
	const char		*pend;
	
	Buffer(char *_pb = NULL, uint32 _bl = 0):pbeg(_pb), pend(_pb + _bl){
	}
	virtual ~Buffer();
	virtual bool resize(uint32 _sz, const char* _rpos, const char* _wpos);
	uint32 capacity()const{
		return pend - pbeg;
	}
	char *release()const{
        char *tmp(pbeg);
		pbeg = NULL;
		return tmp;
	}

protected:

	Buffer(char *_pb,const char *_pe):pbeg(_pb), pend(_pe){}
};

struct HeapBuffer: Buffer{
	HeapBuffer(uint32 _sz = 1024);
	HeapBuffer(char *_pb = NULL, uint32 _bl = 0):Buffer(_pb, _bl){
	}
	~HeapBuffer();
	HeapBuffer(const HeapBuffer &_rb):Buffer(_rb.release(), _rb.pend){
	}
	/*virtual*/ bool resize(uint32 _sz, const char* _rpos, const char* _wpos);
};

struct SpecificBuffer: Buffer{
	SpecificBuffer(uint32 _sz = 1024);
	~SpecificBuffer();
	SpecificBuffer(const SpecificBuffer &_rb):Buffer(_rb.release(), _rb.pend){
	}
	/*virtual*/ bool resize(uint32 _sz, const char* _rpos, const char* _wpos);
};

}//namespace text
}//namespace protocol
}//namespace solid

#endif
