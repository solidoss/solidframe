/* Implementation file reader.cpp
	
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

#include "algorithm/protocol/reader.hpp"
#include "utility/ostream.hpp"
#include "system/debug.hpp"
#include <cerrno>

namespace protocol{

static DummyKey dk;

DummyKey* DummyKey::pointer(){
	return &dk;
}

void DummyKey::initReader(const Reader &_){
	//does absolutely nothing
}

Reader::Reader(Logger *_plog):plog(_plog), bbeg(NULL), bend(NULL), rpos(NULL), wpos(NULL), state(RunState){
	bbeg = new char[2*1024];
	bend = bbeg + 2*1024;
	dolog = isLogActive();
}

Reader::~Reader(){
	delete []bbeg;
}

Parameter& Reader::push(FncT _pf){
	fs.push(FncPairT(_pf, Parameter()));
	return fs.top().second;
}

void Reader::push(FncT _pf, const Parameter & _rp){
	fs.push(FncPairT(_pf, _rp));
}

void Reader::pop(){
	fs.pop();
}

void Reader::replace(FncT _pf, const Parameter & _rp){
	fs.top().first = _pf;
	fs.top().second = _rp;
}

bool Reader::isRecovering()const{
	return state == RecoverState;
}

bool Reader::isError()const{
	return state != RunState;
}

void Reader::resetState(){
	state = RunState;
}
	
int Reader::peek(int &_c){
	if(rpos != wpos) { _c = *(rpos); return Ok;}
	return No;
}

int Reader::get(int &_c){
	if(rpos != wpos) {
        _c = *(rpos++);
        if(dolog) plog->inChar(_c);
        return Ok;
    }
    return No;
}

void Reader::drop(){
	if(dolog) plog->inChar(*rpos);
	++rpos;
}

int Reader::fetchLiteral(String &_rds, uint32 &_rdsz){
	ulong mlen = wpos - rpos;
	if(mlen > _rdsz) mlen = _rdsz;
	_rds.append(rpos, mlen);
	_rdsz -= mlen;
	rpos += mlen;
	if(_rdsz == 0){
		if(dolog) plog->inLiteral(_rds.data(), _rds.size());
		return Ok;
	}
	return No;
}

int Reader::run(){
	while(fs.size()){
		const int rval((*fs.top().first)(*this, fs.top().second)); 
		switch(rval){
			case Bad:return BAD;
			case No: return NOK;//wait data
			case Error:
				cassert(state != RecoverState);
				state = RecoverState;
				while(fs.size())fs.pop();
				prepareErrorRecovery();
				break;
			case Ok: fs.pop();break;
			case Yield:return YIELD;
			case Continue: break;
			default: fs.pop(); return rval;
		}
	}
	return OK;
}

/*static*/ int Reader::checkChar(Reader &_rr, Parameter &_rp){
	int c;
	if(_rr.peek(c)){
		_rr.push(&Reader::refill);
		return Continue;
	}
	if(c == _rp.a.i){
		_rr.drop();
		return Ok;
	}else{
		_rr.charError(c, _rp.a.i);
		return Error;
	}
}

/*static*/ int Reader::fetchLiteralString(Reader &_rr, Parameter &_rp){
	if(_rr.fetchLiteral(*reinterpret_cast<String*>(_rp.a.p), _rp.b.u32)){
		_rr.push(&Reader::refill);
		return Continue;
	}
	return Ok;
}

/*static*/ int Reader::fetchLiteralDummy(Reader &_rr, Parameter &_rp){
	uint32 sz = _rr.wpos - _rr.rpos;
	if(sz > _rp.a.u32) sz = _rp.a.u32;
	_rr.rpos += sz;
	_rp.a.u32 -= sz;
	if(_rp.a.u32){
		_rr.push(&Reader::refill);
		return Continue;
	}
	return Ok;
}

/*static*/ int Reader::fetchLiteralStream(Reader &_rr, Parameter &_rp){
	OStreamIterator &osi(*static_cast<OStreamIterator*>(_rp.a.p));
	if(osi.start() < 0){
		cassert(false);
	}
	uint64		&sz = *static_cast<uint64*>(_rp.b.p);
	ulong		minlen = (ulong)(_rr.wpos - _rr.rpos);
	if(sz < minlen) minlen = sz;
	//write what we allready have in buffer into the stream
	if(minlen) osi->write(_rr.rpos, minlen);
	idbgx(Dbg::protocol, "stream size = "<<sz<<" minlen = "<<minlen);
	sz -= minlen;
	if(sz){
		_rr.rpos = _rr.wpos = _rr.bbeg;
		ulong		toread = _rr.bend - _rr.bbeg;
		if(sz < toread) toread = sz;
		int rv = Continue;
		switch(_rr.read(_rr.bbeg, toread)){
			case Bad: return Bad;
			case Ok: break;
			case No:
				rv = No;
				break;
		}
		_rr.replace(&Reader::fetchLiteralStreamContinue, _rp);
		return rv;
	}else{//done
		_rr.rpos += minlen;
		return Ok;
	}
}

/*static*/ int Reader::fetchLiteralStreamContinue(Reader &_rr, Parameter &_rp){
	OStreamIterator &osi(*static_cast<OStreamIterator*>(_rp.a.p));
	uint64		&sz = *static_cast<uint64*>(_rp.b.p);
	const ulong	bufsz = _rr.bend - _rr.bbeg;
	ulong		toread;
	ulong		tmpsz = bufsz * 8;
	if(sz < tmpsz) tmpsz = sz;
	sz -= tmpsz;
	while(tmpsz){
		const ulong rdsz = _rr.readSize();
		osi->write(_rr.bbeg, rdsz);
		tmpsz -= rdsz; 
		if(!tmpsz) break;
		toread = bufsz;
		if(tmpsz < toread) toread = tmpsz;
		switch(_rr.read(_rr.bbeg, toread)){
			case Bad:
				return Bad;
			case Ok:
				break;
			case No:
				sz += tmpsz;
				return No;
		}
	}
	if(sz){
		toread = bufsz;
		if(sz < toread) toread = sz;
		switch(_rr.read(_rr.bbeg, toread)){
			case Bad:
				return Bad;
			case Ok:
				break;
			case No:
				return No;
		}
		return Yield;
	}
	idbgx(Dbg::protocol, "fetch stream done "<<sz);
	return Ok;//Done
}

/*static*/ int Reader::refillDone(Reader &_rr, Parameter &_rp){
	_rr.wpos = _rr.bbeg + _rr.readSize();
	_rr.rpos = _rr.bbeg;
	//writedbg(_rr.rpos, _rr.readSize());
	return Ok;
}

/*static*/ int Reader::refill(Reader &_rr, Parameter &_rp){
	int rv = _rr.read(_rr.bbeg, _rr.bend - _rr.bbeg);
	if(rv == Bad){
		_rr.state = IOErrorState;
		return Bad;
	}else if(rv == Ok){
		_rr.wpos = _rr.bbeg + _rr.readSize();
		_rr.rpos = _rr.bbeg;
		//writedbg(_rr.rpos, _rr.readSize());
		return Ok;
	}
	_rr.replace(&Reader::refillDone);
	return No;
}

template <>
/*static*/ int Reader::returnValue<true>(Reader &_rr, Parameter &_rp){
	int rv = _rp.a.i;
	_rr.fs.pop();//?!
	return rv;
}

template <>
/*static*/ int Reader::returnValue<false>(Reader &_rr, Parameter &_rp){
	int rv = _rp.a.i;
	return rv;
}


/*static*/ int Reader::pop(Reader &_rr, Parameter &_rp){
	_rr.fs.pop();//this
	while(_rp.a.i--){
		_rr.fs.pop();
	}
	return Continue;
}

/*static*/ int Reader::manage(Reader &_rr, Parameter &_rp){
	switch(_rp.a.i){
		case ClearLogging: _rr.dolog = false; return Ok;
		case ResetLogging: _rr.dolog = _rr.isLogActive(); return Ok;
		case ClearTmpString: _rr.tmp.clear(); return Ok;
		default: return _rr.doManage(_rp.a.i);
	}
	return Ok;
}

struct DigitFilter{
	static int check(int _c){
		return isdigit(_c);
	}
};

/*static*/ int Reader::fetchUInt32(Reader &_rr, Parameter &_rp){
	int rv = _rr.fetch<DigitFilter>(_rr.tmp, 12);
	switch(rv){
		case Ok:break;
		case No:
			_rr.push(&Reader::refill);
			return Continue;
		case Error:
			_rr.basicError(StringTooLong);
			return Error;
	}
	*reinterpret_cast<uint32 *>(_rp.a.p) = strtoul(_rr.tmp.c_str(), NULL, 10);
	
	if(_rr.tmp.empty() || errno==ERANGE/* || *reinterpret_cast<uint32 *>(_rp.a.p) == 0*/){
		_rr.keyError(_rr.tmp, NotANumber);
		_rr.tmp.clear();
		return Error;
	}
	_rr.tmp.clear();
	return Ok;
}

/*static*/ int Reader::fetchUInt64(Reader &_rr, Parameter &_rp){
	int rv = _rr.fetch<DigitFilter>(_rr.tmp, 32);
	switch(rv){
		case Ok:break;
		case No:
			_rr.push(&Reader::refill);
			return Continue;
		case Error:
			_rr.basicError(StringTooLong);
			return Error;
	}
	//*reinterpret_cast<uint32 *>(_rp.a.p) = strtoul(_rr.tmp.c_str(), NULL, 10);
	*reinterpret_cast<uint64 *>(_rp.a.p) = strtoull(_rr.tmp.c_str(), NULL, 10);
	
	if(_rr.tmp.empty() || errno==ERANGE/* || *reinterpret_cast<uint32 *>(_rp.a.p) == 0*/){
		_rr.keyError(_rr.tmp, NotANumber);
		_rr.tmp.clear();
		return Error;
	}
	_rr.tmp.clear();
	return Ok;
}

/*static*/ int Reader::dropChar(Reader &_rr, Parameter &_rp){
	_rr.drop();
	return Ok;
}

/*static*/ int Reader::saveCurrentChar(Reader &_rr, Parameter &_rp){
	int c;
	if(_rr.peek(c)){
		_rr.push(&Reader::refill);
		return Continue;
	}
	if(_rp.b.i == c){
		*reinterpret_cast<int*>(_rp.a.p) = c;
		_rr.drop();
	}
	return Ok;
}
/*static*/ int Reader::fetchChar(Reader &_rr, Parameter &_rp){
	int c;
	if(_rr.peek(c)){
		_rr.push(&Reader::refill);
		return Continue;
	}
	*reinterpret_cast<char*>(_rp.a.p) = c;
	_rr.drop();
	return Ok;
}
//default we close
void Reader::prepareErrorRecovery(){
	push(&Reader::returnValue<true>, Parameter(Bad));
}

int Reader::doManage(int _mo){
	cassert(false);
	return Ok;
}

}//namespace protocol
