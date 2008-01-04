/* Implementation file reader.cpp
	
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

#include "algorithm/protocol/reader.h"
#include "utility/ostream.h"
#include <cerrno>

namespace protocol{

static DummyKey dk;

DummyKey* DummyKey::pointer(){
	return &dk;
}
void DummyKey::initReader(const Reader &_){
	//does absolutely nothing
}

Reader::Reader(Logger *_plog, char *_pb, unsigned _bs):plog(_plog), bbeg(_pb), bend(_pb + _bs), rpos(NULL), wpos(NULL), state(RunState){
	if(!bbeg){
		bbeg = new char[2*1024];
		bend = bbeg + 2*1024;
	}
	dolog = isLogActive();
}
Reader::~Reader(){
}

Parameter& Reader::push(FncTp _pf){
	fs.push(FncPairTp(_pf, Parameter()));
	return fs.top().second;
}

void Reader::push(FncTp _pf, const Parameter & _rp){
	fs.push(FncPairTp(_pf, _rp));
}
void Reader::replace(FncTp _pf, const Parameter & _rp){
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
        if(dolog) plog->readChar(_c);
        return Ok;
    }
    return No;
}
void Reader::drop(){
	if(dolog) plog->readChar(*rpos);
	++rpos;
}

int Reader::fetchLiteral(String &_rds, ulong &_rdsz){
	ulong mlen = wpos - rpos;
	if(mlen > _rdsz) mlen = _rdsz;
	_rds.append(rpos, mlen);
	_rdsz -= mlen;
	rpos += mlen;
	if(_rdsz == 0){
		if(dolog) plog->readLiteral(_rds.data(), _rds.size());
		return Ok;
	}
	return No;
}

int Reader::run(){
	while(fs.size()){
		switch((*fs.top().first)(*this, fs.top().second)){
			case Bad:return BAD;
			case No: return NOK;//wait data
			case Error:
				assert(state != RecoverState);
				state = RecoverState;
				while(fs.size())fs.pop();
				prepareErrorRecovery();
				break;
			case Ok: fs.pop();
			case Continue: break;
			default: assert(false);
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
	if(_rr.fetchLiteral(*reinterpret_cast<String*>(_rp.a.p), _rp.b.u)){
		_rr.push(&Reader::refill);
		return Continue;
	}
	return Ok;
}

/*static*/ int Reader::fetchLiteralDummy(Reader &_rr, Parameter &_rp){
	uint32 sz = _rr.wpos - _rr.rpos;
	if(sz > _rp.a.u) sz = _rp.a.u;
	_rr.rpos += sz;
	_rp.a.u -= sz;
	if(_rp.a.u){
		_rr.push(&Reader::refill);
		return Continue;
	}
	return Ok;
}


/*static*/ int Reader::fetchLiteralStream(Reader &_rr, Parameter &_rp){
	OStreamIterator *osi(static_cast<OStreamIterator*>(_rp.a.p));
	if(osi->start() < 0){
		assert(false);
	}
	uint64 sz = *static_cast<uint64*>(_rp.b.p);
	ulong minlen = (ulong)(_rr.wpos - _rr.rpos);
	if((long long)minlen > sz) minlen = sz;
	if(minlen) (*osi)->write(_rr.rpos, minlen);
	sz -= minlen;
	if(sz){
		_rr.rpos = _rr.wpos = _rr.bbeg;
		int rv = _rr.read(*osi, sz, _rr.bbeg, _rr.bend - _rr.bbeg);
		if(rv < Bad){
			_rr.state = IOError;
			return Bad;
		}else if(rv == Ok){
			_rr.wpos += _rr.readSize();
			return Ok;
		}
		_rr.replace(&Reader::refillDone);
		return No;
	}else{//done
		_rr.rpos += minlen;
		return Ok;
	}
}

/*static*/ int Reader::refillDone(Reader &_rr, Parameter &_rp){
	_rr.wpos = _rr.bbeg + _rr.readSize();
	_rr.rpos = _rr.bbeg;
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
		return Ok;
	}
	_rr.replace(&Reader::refillDone);
	return No;
}

/*static*/ int Reader::returnValue(Reader &_rr, Parameter &_rp){
	int rv = _rp.a.i;
	_rr.fs.pop();//?!
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

/*static*/ int Reader::fetchUint32(Reader &_rr, Parameter &_rp){
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

//default we close
void Reader::prepareErrorRecovery(){
	push(&Reader::returnValue, Parameter(Bad));
}
int Reader::doManage(int _mo){
	assert(false);
	return Ok;
}

}//namespace protocol
