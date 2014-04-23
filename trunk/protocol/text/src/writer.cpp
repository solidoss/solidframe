// protocol/text/src/writer.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <cstring>
#include "protocol/text/writer.hpp"
#include <istream>

#include "system/debug.hpp"

using namespace std;

namespace solid{
namespace protocol{
namespace text{

Writer::Writer(Logger *_plog):plog(_plog), rpos(NULL), wpos(NULL){
	dolog = isLogActive();
}

Writer::~Writer(){
}

void Writer::push(FncT _pf, const Parameter & _rp){
	fs.push(FncPairT(_pf, _rp));
}

void Writer::replace(FncT _pf, const Parameter & _rp){
	fs.top().first = _pf;
	fs.top().second = _rp;
}

Parameter &Writer::push(FncT _pf){
	fs.push(FncPairT(_pf, Parameter()));
	return fs.top().second;
}

int Writer::run(){
	while(fs.size()){
		const int rval((*fs.top().first)(*this, fs.top().second));
		switch(rval){
			case Failure:return Failure;
			case Wait: return Wait;//wait data
			case Success: fs.pop();break;
			case Yield:return Yield;
			case Continue: break;
			default: fs.pop(); return rval;
		}
	}
	return Success;
}

int Writer::flush(){
	int towrite = wpos - rpos;
	if(towrite < FlushLength)
		return Success;
		
	int rv = write(rpos, towrite);
	if(dolog) plog->outFlush();
	if(rv == Success){
		rpos = wpos = bh->pbeg;
		return Success;
	}
	return rv;
}

int Writer::flushAll(){
	int towrite = wpos - rpos;
	if(towrite == 0)
		return Success;
		
	int rv = write(rpos, towrite);
	if(dolog)
		plog->outFlush();
	if(rv == Success){
		rpos = wpos = bh->pbeg; 
		return Success;
	}
	return rv;
}
void Writer::resize(uint32 _len){
	if(_len < bh->capacity()) return;
	const uint32 rlen(rpos - bh->pbeg);
	if(!bh->resize(_len, rpos, wpos)){
		HeapBuffer hb(_len);
		memcpy(hb.pbeg, rpos, wpos - rpos);
		bh = hb;
	}
	wpos = bh->pbeg + (wpos -rpos);
	rpos = bh->pbeg + rlen;
}
// void Writer::resize(uint32 _len){
// 	uint32 clen = bh->capacity();			//current length
//     uint32 rlen = _len + (wpos - pbeg);	//requested length
//     clen <<= 1;
//     if(clen > rlen && clen < MaxDoubleSizeLength){
//     }else{
//         clen = rlen - (rlen & 255) + 512;
//     }
//     //INF3("resize crtsz = %u, reqsz = %u, givensize = %u",pend - pbeg, rsz, csz);
//     char *tmp = new char[clen];
//     memcpy(tmp, rpos, wpos - rpos);
//     delete []pbeg;
//     pbeg = tmp;
//     pend = pbeg + clen;
//     wpos = pbeg + (wpos - rpos);
//     rpos = pbeg;
// }

void Writer::putChar(char _c1){
	if(dolog) plog->outChar(_c1);
	putSilentChar(_c1);
}

void Writer::putChar(char _c1, char _c2){
	if(dolog) plog->outChar(_c1, _c2);
	putSilentChar(_c1, _c2);
}

void Writer::putChar(char _c1, char _c2, char _c3){
	if(dolog) plog->outChar(_c1, _c2, _c3);
	putSilentChar(_c1, _c2, _c3);
}

void Writer::putChar(char _c1, char _c2, char _c3, char _c4){
	if(dolog) plog->outChar(_c1, _c2, _c3, _c4);
	putSilentChar(_c1, _c2, _c3, _c4);
}

void Writer::putSilentChar(char _c1){
	cassert(wpos <= bh->pend);
	if(wpos != bh->pend){
	}else resize(1);
	
	*(wpos++) = _c1;
}

void Writer::putSilentChar(char _c1, char _c2){
	cassert(wpos <= bh->pend);
	if(2 < (uint32)(bh->pend - wpos)){
	}else resize(2);
	
	*(wpos++) = _c1;
	*(wpos++) = _c2;
}

void Writer::putSilentChar(char _c1, char _c2, char _c3){
	cassert(wpos <= bh->pend);
	if(3 < (uint32)(bh->pend - wpos)){
	}else resize(3);
	
	*(wpos++) = _c1;
	*(wpos++) = _c2;
	*(wpos++) = _c3;
}

void Writer::putSilentChar(char _c1, char _c2, char _c3, char _c4){
	cassert(wpos <= bh->pend);
	if(4 < (uint32)(bh->pend - wpos)){
	}else resize(4);
	
	*(wpos++) = _c1;
	*(wpos++) = _c2;
	*(wpos++) = _c3;
	*(wpos++) = _c4;
}

void Writer::putString(const char* _s, uint32 _sz){
	if(dolog) plog->outAtom(_s, _sz);
	putSilentString(_s, _sz);
}

void Writer::putSilentString(const char* _s, uint32 _sz){
	cassert(wpos <= bh->pend);
	if(_sz < (uint32)(bh->pend - wpos)){
	}else resize(_sz);
	memcpy(wpos, _s, _sz);
	wpos += _sz;
}

void Writer::put(uint32 _v){
	if(!_v){
		putChar('0');
	}else{
		char tmp[12];
		uint pos = 11;
		while(_v){
			*(tmp + pos) = '0' + _v % 10;
			_v /= 10;
			--pos;
		}
		++pos;
		putSilentString(tmp + pos, 12 - pos);
		if(dolog) plog->outAtom(tmp + pos, 12 - pos);
	}
}

void Writer::putSilent(uint32 _v){
	if(!_v){
		putSilentChar('0');
	}else{
		char tmp[12];
		uint pos = 11;
		while(_v){
			*(tmp + pos) = '0' + _v % 10;
			_v /= 10;
			--pos;
		}
		++pos;
		putSilentString(tmp + pos, 12 - pos);
	}
}


void Writer::put(uint64 _v){
	if(!_v){
		putChar('0');
	}else{
		char tmp[32];
		uint pos = 31;
		while(_v){
			*(tmp + pos) = '0' + _v % 10;
			_v /= 10;
			--pos;
		}
		++pos;
		putSilentString(tmp + pos, 32 - pos);
		if(dolog) plog->outAtom(tmp + pos, 32 - pos);
	}
}

void Writer::putSilent(uint64 _v){
	if(!_v){
		putSilentChar('0');
	}else{
		char tmp[32];
		uint pos = 31;
		while(_v){
			*(tmp + pos) = '0' + _v % 10;
			_v /= 10;
			--pos;
		}
		++pos;
		putSilentString(tmp + pos, 32 - pos);
	}
}

template <>
/*static*/ int Writer::returnValue<true>(Writer &_rw, Parameter &_rp){
	int rv = _rp.a.i;
	_rw.fs.pop();//?!
	return rv;
}

template <>
/*static*/ int Writer::returnValue<false>(Writer &_rw, Parameter &_rp){
	int rv = _rp.a.i;
	return rv;
}

/*static*/ int Writer::flush(Writer &_rw, Parameter &_rp){
	int rv = _rw.flush();
	if(rv == Wait) _rw.fs.top().first = &Writer::doneFlush;
	return rv;
}

/*static*/ int Writer::flushAll(Writer &_rw, Parameter &_rp){
	int rv = _rw.flushAll();
	if(rv == Wait) _rw.fs.top().first = &Writer::doneFlush;
	return rv;
}

/*static*/ int Writer::doneFlush(Writer &_rw, Parameter &_rp){
	_rw.rpos = _rw.wpos = _rw.bh->pbeg;
	return Success;
}

/*static*/ int Writer::putRawString(Writer &_rw, Parameter &_rp){
	if(_rp.b.u32 < FlushLength){
		//I'm sure that the data in buf is less then FlushLength
		//so adding something less than FlushLength will certainly fit into the buffer
		memcpy(_rw.wpos, _rp.a.p, _rp.b.u32);
		_rw.wpos += _rp.b.u32;
		//NOTE: no need to do the flushing now
		//int rv = _rw.flush();
		//if(rv){ _rw.fs.top().first = &Writer::doneFlush(); return rv;}
		return Success;
	}else{
		int rv = _rw.write((char*)_rp.a.p, _rp.b.u32);
		if(rv == Wait){//scheduled for writing
			_rw.replace(&Writer::returnValue<true>, Parameter(Continue));
			return Wait;
		}
		return rv;
	}
}

bool read_stream_all(istream &_ris, char *_pb, size_t _sz){
	bool rv = true;
	while(_sz){
		_ris.read(_pb, _sz);
		if(_ris.eof()){
			rv = false;
			break;
		}
		
		size_t rsz = _ris.gcount();
		
		if(rsz == 0){
			rv = false;
			break;
		}
		_pb += rsz;
		_sz -= rsz;
	}
	return rv;
}

/*static*/ int Writer::putStream(Writer &_rw, Parameter &_rp){
	istream 	&ris = *reinterpret_cast<istream*>(_rp.a.p);
	uint64		&sz = *static_cast<uint64*>(_rp.b.p);
	//if(ris.start() < 0) return Failure;
	if(sz < FlushLength){
		cassert((_rw.bh->pend - _rw.wpos) >= FlushLength);
		if(!read_stream_all(ris, _rw.wpos, sz)){
			return Failure;
		}
		_rw.wpos += sz;
		int rv = _rw.flush();
		if(rv){
			_rw.fs.top().first = &Writer::doneFlush; 
			return rv;
		}
		return Success;
	}else{//flush the buffer
		_rw.fs.top().first = &putStreamDone;
		int rv = _rw.flushAll();
		if(rv) return rv;
		return Continue;
	}
}

/*static*/ int Writer::putStreamDone(Writer &_rw, Parameter &_rp){
	doneFlush(_rw, _rp);
	istream 		&ris = *reinterpret_cast<istream*>(_rp.a.p);
	uint64			&sz = *static_cast<uint64*>(_rp.b.p);
	ulong 			toread;
	const ulong		blen = _rw.bh->pend - _rw.bh->pbeg;
	ulong 			tmpsz = blen * 16;
	int				rv = 0;
	
	
	if(tmpsz > sz) tmpsz = sz;
	sz -= tmpsz;
	
	while(tmpsz){
		toread = blen;
		if(toread > tmpsz) toread = tmpsz;
		
		if(!read_stream_all(ris, _rw.bh->pbeg, toread)){
			return Failure;
		}
		rv = toread;
		tmpsz -= rv;
		if(rv < FlushLength)
			break;
		switch(_rw.write(_rw.bh->pbeg, rv)){
			case Failure:
				return Failure;
			case Success:
				rv = 0;
				break;
			case Wait:
				sz += tmpsz;
				return Wait;
		}
	}
	
	if(sz != 0){
		return Yield;
	}
	_rw.wpos += rv;
	return Success;
}

/*static*/ int Writer::putAtom(Writer &_rw, Parameter &_rp){
	if(_rw.dolog) _rw.plog->outAtom((const char*)_rp.a.p, _rp.b.u32);
	if(_rp.b.u32 < FlushLength){
		//I'm sure that the data in buf is less then FlushLength
		//so adding something less than FlushLength will certainly fit into the buffer
		memcpy(_rw.wpos, _rp.a.p, _rp.b.u32);
		_rw.wpos += _rp.b.u32;
		int rv = _rw.flush();
		if(rv){ _rw.fs.top().first = &Writer::doneFlush; return rv;}
		return Success;
	}
	//we kinda need to do it the hard way
	uint towrite = _rw.bh->pend - _rw.wpos;
	if(towrite > _rp.b.u32) towrite = _rp.b.u32;
	memcpy(_rw.wpos, _rp.a.p, towrite);
	_rp.a.p = (char*)_rp.a.p + towrite;
	_rp.b.u32 -= towrite;
	_rw.fs.top().first = &Writer::putRawString;
	_rw.push(flushAll);
	return Continue;
}

/*static*/ int Writer::putChar(Writer &_rw, Parameter &_rp){
	_rw.putSilentChar(_rp.a.i);
	if(_rw.dolog) _rw.plog->outChar(_rp.a.i);
	int rv = _rw.flush();
	if(rv){ _rw.fs.top().first = &Writer::doneFlush; return rv;}
	return Success;
}

/*static*/ int Writer::putUInt32(Writer &_rw, Parameter &_rp){
	_rw.put(_rp.a.u32);
	int rv = _rw.flush();
	if(rv){ _rw.fs.top().first = &Writer::doneFlush; return rv;}
	return Success;
}

/*static*/ int Writer::putUInt64(Writer &_rw, Parameter &_rp){
	_rw.put(_rp.a.u64);
	int rv = _rw.flush();
	if(rv){ _rw.fs.top().first = &Writer::doneFlush; return rv;}
	return Success;
}

/*static*/ int Writer::manage(Writer &_rw, Parameter &_rp){
	switch(_rp.a.i){
		case ClearLogging: _rw.dolog = false; return Success;
		case ResetLogging: _rw.dolog = _rw.isLogActive(); return Success;
		default: return _rw.doManage(_rp.a.i);
	}
	return Success;
}

void Writer::clear(){
	dolog = isLogActive();
}

int Writer::doManage(int _mo){
	cassert(false);
	return Success;
}

Writer& Writer::operator << (char _c){
	if(dolog) putChar(_c);
	else putSilentChar(_c); 
	return *this;
}

Writer& Writer::operator << (const char* _s){
	if(dolog) putString(_s, strlen(_s));
	else putSilentString(_s, strlen(_s));
	return *this;
}

Writer& Writer::operator << (const String &_str){
	if(dolog) putString(_str.data(), _str.size());
	else putSilentString(_str.data(), _str.size());
	return *this;
}

Writer& Writer::operator << (uint32 _v){
	put(_v);
	return *this;
}

Writer& Writer::operator << (uint64 _v){
	put(_v);
	return *this;
}

void Writer::doPrepareBuffer(char *_newbeg, const char *_newend){
	const uint32 sz(wpos - rpos);
	
	if(sz){
		memcpy(_newbeg, rpos, wpos - rpos);
	}
	rpos = _newbeg;
	wpos = rpos + sz;
}

}//namespace text
}//namespace protocol
}//namespace solid

