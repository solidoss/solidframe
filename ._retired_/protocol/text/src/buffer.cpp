// protocol/text/src/buffer.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "protocol/text/buffer.hpp"
#include "system/cassert.hpp"
#include "system/specific.hpp"
#include <cstring>

namespace solid{
namespace protocol{
namespace text{

/*virtual*/ Buffer::~Buffer(){
}
/*virtual*/ bool Buffer::resize(uint32 _sz, const char* _rpos, const char* _wpos){
	return false;
}

HeapBuffer::HeapBuffer(uint32 _sz):Buffer(new char[_sz], _sz){
	const uint32 sz(_sz - (_sz & 255) + 512);
	pbeg = new char[sz];
	pend = pbeg + sz;
}
HeapBuffer::~HeapBuffer(){
	delete []pbeg;
}
/*virtual*/ bool HeapBuffer::resize(uint32 _sz, const char* _rpos, const char* _wpos){
	//TODO: implement properly for better speed
	return false;
}

SpecificBuffer::SpecificBuffer(uint32 _sz){
	const uint32 id(Specific::sizeToIndex(_sz));
	pbeg = Specific::popBuffer(id);
	pend = pbeg + Specific::indexToCapacity(id);
}
SpecificBuffer::~SpecificBuffer(){
	if(pbeg){
		const uint32 id(Specific::sizeToIndex(pend - pbeg));
		Specific::pushBuffer(pbeg, id);
	}
}
/*virtual*/ bool SpecificBuffer::resize(uint32 _sz, const char* _rpos, const char* _wpos){
	if(_sz <= capacity()){
		memmove(pbeg, _rpos, _wpos - _rpos);
		return true;
	}
	const uint32 id(Specific::sizeToIndex(_sz));
	char *ptmp(Specific::popBuffer(id));
	memcpy(ptmp, _rpos, _wpos - _rpos);
	
	const uint32 oldid(Specific::sizeToIndex(pend - pbeg));
	Specific::pushBuffer(pbeg, oldid);
	
	pbeg = ptmp;
	pend = pbeg + Specific::indexToCapacity(id);
	return true;
}

}//namespace text
}//namespace protocol
}//namespace solid
