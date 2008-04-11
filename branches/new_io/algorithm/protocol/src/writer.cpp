/* Implementation file writer.cpp
	
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

#include "algorithm/protocol/writer.hpp"
#include "utility/istream.hpp"

#include "system/debug.hpp"

namespace protocol{

Writer::Writer(Logger *_plog):plog(_plog), bbeg(new char[StartLength]), bend(bbeg + StartLength), rpos(bbeg), wpos(bbeg){
	dolog = isLogActive();
}

Writer::~Writer(){
	delete []bbeg;
}

void Writer::push(FncTp _pf, const Parameter & _rp){
	fs.push(FncPairTp(_pf, _rp));
}

void Writer::replace(FncTp _pf, const Parameter & _rp){
	fs.top().first = _pf;
	fs.top().second = _rp;
}

Parameter &Writer::push(FncTp _pf){
	fs.push(FncPairTp(_pf, Parameter()));
	return fs.top().second;
}

int Writer::run(){
	while(fs.size()){
		switch((*fs.top().first)(*this, fs.top().second)){
			case Bad:return BAD;
			case No: return NOK;//wait data
			case Ok: fs.pop();
			case Yield:return YIELD;
			case Continue: break;
			default: cassert(false);
		}
	}
	return OK;
}

int Writer::flush(){
	int towrite = wpos - rpos;
	if(towrite < FlushLength)
		return Ok;
		
	int rv = write(rpos, towrite);
	if(dolog) plog->writeFlush();
	if(rv == Ok){
		rpos = wpos = bbeg;
		return Ok;
	}
	return rv;
}

int Writer::flushAll(){
	int towrite = wpos - rpos;
	if(towrite == 0)
		return Ok;
		
	int rv = write(rpos, towrite);
	if(dolog)
		plog->writeFlush();
	if(rv == Ok){
		rpos = wpos = bbeg; 
		return Ok;
	}
	return rv;
}

void Writer::resize(uint32 _len){
	uint32 clen = bend - bbeg;			//current length
    uint32 rlen = _len + (wpos - bbeg);	//requested length
    clen <<= 1;
    if(clen > rlen && clen < MaxDoubleSizeLength){
    }else{
        clen = rlen - (rlen & 255) + 512;
    }
    //INF3("resize crtsz = %u, reqsz = %u, givensize = %u",bend - bbeg, rsz, csz);
    char *tmp = new char[clen];
    memcpy(tmp, rpos, wpos - rpos);
    delete []bbeg;
    bbeg = tmp;
    bend = bbeg + clen;
    wpos = bbeg + (wpos - rpos);
    rpos = bbeg;
}

void Writer::putChar(char _c1){
	plog->writeChar(_c1);
	putSilentChar(_c1);
}

void Writer::putChar(char _c1, char _c2){
	plog->writeChar(_c1, _c2);
	putSilentChar(_c1, _c2);
}

void Writer::putChar(char _c1, char _c2, char _c3){
	plog->writeChar(_c1, _c2, _c3);
	putSilentChar(_c1, _c2, _c3);
}

void Writer::putChar(char _c1, char _c2, char _c3, char _c4){
	plog->writeChar(_c1, _c2, _c3, _c4);
	putSilentChar(_c1, _c2, _c3, _c4);
}

void Writer::putSilentChar(char _c1){
	if(wpos != bend){
	}else resize(1);
	
	*(wpos++) = _c1;
}

void Writer::putSilentChar(char _c1, char _c2){
	if(2 < (uint32)(bend - wpos)){
	}else resize(2);
	
	*(wpos++) = _c1;
	*(wpos++) = _c2;
}

void Writer::putSilentChar(char _c1, char _c2, char _c3){
	if(3 < (uint32)(bend - wpos)){
	}else resize(3);
	
	*(wpos++) = _c1;
	*(wpos++) = _c2;
	*(wpos++) = _c3;
}

void Writer::putSilentChar(char _c1, char _c2, char _c3, char _c4){
	if(4 < (uint32)(bend - wpos)){
	}else resize(4);
	
	*(wpos++) = _c1;
	*(wpos++) = _c2;
	*(wpos++) = _c3;
	*(wpos++) = _c4;
}

void Writer::putString(const char* _s, uint32 _sz){
	plog->writeAtom(_s, _sz);
	putSilentString(_s, _sz);
}

void Writer::putSilentString(const char* _s, uint32 _sz){
	if(_sz < (uint32)(bend - wpos)){
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
		plog->writeAtom(tmp + pos, 12 - pos);
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

/*static*/ int Writer::returnValue(Writer &_rw, Parameter &_rp){
	int rv = _rp.a.i;
	_rw.fs.pop();//?!
	return rv;
}

/*static*/ int Writer::flush(Writer &_rw, Parameter &_rp){
	int rv = _rw.flush();
	if(rv == No) _rw.fs.top().first = &Writer::doneFlush;
	return rv;
}

/*static*/ int Writer::flushAll(Writer &_rw, Parameter &_rp){
	int rv = _rw.flushAll();
	if(rv == No) _rw.fs.top().first = &Writer::doneFlush;
	return rv;
}

/*static*/ int Writer::doneFlush(Writer &_rw, Parameter &_rp){
	_rw.rpos = _rw.wpos = _rw.bbeg;
	return Ok;
}

/*static*/ int Writer::putRawString(Writer &_rw, Parameter &_rp){
	if(_rp.b.u < FlushLength){
		//I'm sure that the data in buf is less then FlushLength
		//so adding something less than FlushLength will certainly fit into the buffer
		memcpy(_rw.wpos, _rp.a.p, _rp.b.u);
		_rw.wpos += _rp.b.u;
		//NOTE: no need to do the flushing now
		//int rv = _rw.flush();
		//if(rv){ _rw.fs.top().first = &Writer::doneFlush(); return rv;}
		return Ok;
	}else{
		int rv = _rw.write((char*)_rp.a.p, _rp.b.u);
		if(rv == No){//scheduled for writing
			_rw.replace(&Writer::returnValue, Parameter(Continue));
			return No;
		}
		return rv;
	}
}

/*static*/ int Writer::putStream(Writer &_rw, Parameter &_rp){
	IStreamIterator &isi = *reinterpret_cast<IStreamIterator*>(_rp.a.p);
	uint64			&sz = *static_cast<uint64*>(_rp.b.p);
	if(isi.start() < 0) return Bad;
	if(sz < FlushLength){
		cassert((_rw.bend - _rw.wpos) >= FlushLength);
		if(sz != (uint64)isi->read(_rw.wpos, sz))
			return Bad;
		_rw.wpos += sz;
		int rv = _rw.flush();
		idbg("rv = "<<rv);
		if(rv){
			_rw.fs.top().first = &Writer::doneFlush; 
			return rv;
		}
		return Ok;
	}else{//flush the buffer
		_rw.fs.top().first = &putStreamDone;
		int rv = _rw.flushAll();
		idbg("rv = "<<rv);
		if(rv) return rv;
		return Continue;
	}
}

/*static*/ int Writer::putStreamDone(Writer &_rw, Parameter &_rp){
	doneFlush(_rw, _rp);
	IStreamIterator &isi = *reinterpret_cast<IStreamIterator*>(_rp.a.p);
	uint64			&sz = *static_cast<uint64*>(_rp.b.p);
	ulong 			toread;
	const ulong		blen = _rw.bend - _rw.bbeg;
	ulong 			tmpsz = blen * 8;
	int				rv = 0;
	if(tmpsz > sz) tmpsz = sz;
	sz -= tmpsz;
	while(tmpsz){
		toread = blen;
		if(toread > tmpsz) toread = tmpsz;
		rv = isi->read(_rw.bbeg, toread);
		if(rv != toread)
			return Bad;
		tmpsz -= rv;
		if(rv < FlushLength)
			break;
		switch(_rw.write(_rw.bbeg, rv)){
			case Bad:
				return Bad;
			case Ok:
				rv = 0;
				break;
			case No:
				sz += tmpsz;
				return No;
		}
	}
	
	if(sz != 0){
		return Yield;
	}
	_rw.wpos += rv;
	return Ok;
}

/*static*/ int Writer::putAtom(Writer &_rw, Parameter &_rp){
	if(_rw.dolog) _rw.plog->writeAtom((const char*)_rp.a.p, _rp.b.u);
	if(_rp.b.u < FlushLength){
		//I'm sure that the data in buf is less then FlushLength
		//so adding something less than FlushLength will certainly fit into the buffer
		memcpy(_rw.wpos, _rp.a.p, _rp.b.u);
		_rw.wpos += _rp.b.u;
		int rv = _rw.flush();
		if(rv){ _rw.fs.top().first = &Writer::doneFlush; return rv;}
		return Ok;
	}
	//we kinda need to do it the hard way
	uint towrite = _rw.bend - _rw.wpos;
	if(towrite > _rp.b.u) towrite = _rp.b.u;
	memcpy(_rw.wpos, _rp.a.p, towrite);
	_rp.a.p = (char*)_rp.a.p + towrite;
	_rp.b.u -= towrite;
	_rw.fs.top().first = &Writer::putRawString;
	_rw.push(flushAll);
	return Continue;
}

/*static*/ int Writer::putChar(Writer &_rw, Parameter &_rp){
	_rw.putSilentChar(_rp.a.i);
	if(_rw.dolog) _rw.plog->writeChar(_rp.a.i);
	int rv = _rw.flush();
	if(rv){ _rw.fs.top().first = &Writer::doneFlush; return rv;}
	return Ok;
}

/*static*/ int Writer::manage(Writer &_rw, Parameter &_rp){
	switch(_rp.a.i){
		case ClearLogging: _rw.dolog = false; return Ok;
		case ResetLogging: _rw.dolog = _rw.isLogActive(); return Ok;
		default: return _rw.doManage(_rp.a.i);
	}
	return Ok;
}

void Writer::clear(){
	dolog = isLogActive();
}

int Writer::doManage(int _mo){
	cassert(false);
	return Ok;
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
	if(dolog) putSilent(_v);
	else put(_v);
	return *this;
}


}//namespace protocol

