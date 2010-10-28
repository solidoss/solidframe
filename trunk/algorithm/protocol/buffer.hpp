#ifndef ALGORITHM_PROTOCOL_BUFFER_HPP
#define ALGORITHM_PROTOCOL_BUFFER_HPP

#include "system/common.hpp"

namespace protocol{

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


}//namespace protocol

#endif
