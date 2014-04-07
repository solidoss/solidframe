// protocol/text/src/reader.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "protocol/text/reader.hpp"
#include <ostream>
#include "system/debug.hpp"
#include <cstring>
#include <cerrno>

using namespace std;

namespace solid{
namespace protocol{
namespace text{

static DummyKey dk;

DummyKey* DummyKey::pointer(){
	return &dk;
}

void DummyKey::initReader(const Reader &_){
	//does absolutely nothing
}

Reader::Reader(Logger *_plog):plog(_plog), rpos(NULL), wpos(NULL), state(RunState){
	dolog = isLogActive();
}

Reader::~Reader(){
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
	if(rpos != wpos) { _c = *(rpos); return Success;}
	return Wait;
}

int Reader::get(int &_c){
	if(rpos != wpos) {
        _c = *(rpos++);
        if(dolog) plog->inChar(_c);
        return Success;
    }
    return Wait;
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
		return Success;
	}
	return Wait;
}

int Reader::run(){
	while(fs.size()){
		const int rval((*fs.top().first)(*this, fs.top().second)); 
		switch(rval){
			case Failure:return Failure;
			case Wait: return Wait;//wait data
			case Error:
				cassert(state != RecoverState);
				state = RecoverState;
				while(fs.size())fs.pop();
				prepareErrorRecovery();
				break;
			case Success: fs.pop();break;
			case Yield:return Yield;
			case Continue: break;
			default: fs.pop(); return rval;
		}
	}
	return Success;
}

/*static*/ int Reader::checkChar(Reader &_rr, Parameter &_rp){
	int c;
	if(_rr.peek(c)){
		_rr.push(&Reader::refill);
		return Continue;
	}
	if(c == _rp.a.i){
		_rr.drop();
		return Success;
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
	return Success;
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
	return Success;
}

/*static*/ int Reader::fetchLiteralStream(Reader &_rr, Parameter &_rp){
	ostream			&ros(*reinterpret_cast<ostream*>(_rp.a.p));
	
	uint64			&sz = *static_cast<uint64*>(_rp.b.p);
	ulong			minlen = (ulong)(_rr.wpos - _rr.rpos);
	
	if(sz < minlen) minlen = sz;
	
	//write what we allready have in buffer into the stream
	if(minlen){
		ros.write(_rr.rpos, minlen);
	}
	
	idbgx(Debug::proto_txt, "stream size = "<<sz<<" minlen = "<<minlen);
	sz -= minlen;
	
	if(sz){
		_rr.rpos = _rr.wpos = _rr.bh->pbeg;
		
		ulong	toread(_rr.bh->pend - _rr.bh->pbeg);
		
		if(sz < toread) toread = sz;
		
		int		rv(Continue);
		
		switch(_rr.read(_rr.bh->pbeg, toread)){
			case Failure: return Failure;
			case Success: break;
			case Wait:
				rv = Wait;
				break;
		}
		_rr.replace(&Reader::fetchLiteralStreamContinue, _rp);
		return rv;
	}else{//done
		_rr.rpos += minlen;
		return Success;
	}
}

/*static*/ int Reader::fetchLiteralStreamContinue(Reader &_rr, Parameter &_rp){
	ostream			&ros(*reinterpret_cast<ostream*>(_rp.a.p));
	uint64			&sz(*static_cast<uint64*>(_rp.b.p));
	const ulong		bufsz(_rr.bh->pend - _rr.bh->pbeg);
	ulong			toread;
	ulong			tmpsz(bufsz * 8);
	
	if(sz < tmpsz) tmpsz = sz;
	sz -= tmpsz;
	while(tmpsz){
		const ulong rdsz(_rr.readSize());
		
		ros.write(_rr.bh->pbeg, rdsz);
		tmpsz -= rdsz; 
		
		if(!tmpsz) break;
		toread = bufsz;
		if(tmpsz < toread) toread = tmpsz;
		switch(_rr.read(_rr.bh->pbeg, toread)){
			case Failure:
				return Failure;
			case Success:
				break;
			case Wait:
				sz += tmpsz;
				return Wait;
		}
	}
	if(sz){
		toread = bufsz;
		if(sz < toread) toread = sz;
		switch(_rr.read(_rr.bh->pbeg, toread)){
			case Failure:
				return Failure;
			case Success:
				break;
			case Wait:
				return Wait;
		}
		return Yield;
	}
	idbgx(Debug::proto_txt, "fetch stream done "<<sz);
	return Success;//Done
}

/*static*/ int Reader::refillDone(Reader &_rr, Parameter &_rp){
	_rr.wpos = _rr.bh->pbeg + _rr.readSize();
	_rr.rpos = _rr.bh->pbeg;
	//writedbg(_rr.rpos, _rr.readSize());
	return Success;
}

/*static*/ int Reader::refill(Reader &_rr, Parameter &_rp){
	int rv = _rr.read(_rr.bh->pbeg, _rr.bh->pend - _rr.bh->pbeg);
	if(rv == Failure){
		_rr.state = IOErrorState;
		return Failure;
	}else if(rv == Success){
		_rr.wpos = _rr.bh->pbeg + _rr.readSize();
		_rr.rpos = _rr.bh->pbeg;
		//writedbg(_rr.rpos, _rr.readSize());
		return Success;
	}
	_rr.replace(&Reader::refillDone);
	return Wait;
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
		case ClearLogging: _rr.dolog = false; return Success;
		case ResetLogging: _rr.dolog = _rr.isLogActive(); return Success;
		case ClearTmpString: _rr.tmp.clear(); return Success;
		default: return _rr.doManage(_rp.a.i);
	}
	return Success;
}

struct DigitFilter{
	static int check(int _c){
		return isdigit(_c);
	}
};

/*static*/ int Reader::fetchUInt32(Reader &_rr, Parameter &_rp){
	int rv = _rr.fetch<DigitFilter>(_rr.tmp, 12);
	switch(rv){
		case Success:break;
		case Wait:
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
	return Success;
}

/*static*/ int Reader::fetchUInt64(Reader &_rr, Parameter &_rp){
	int rv = _rr.fetch<DigitFilter>(_rr.tmp, 32);
	switch(rv){
		case Success:break;
		case Wait:
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
	return Success;
}

/*static*/ int Reader::dropChar(Reader &_rr, Parameter &_rp){
	int c;
	if(_rr.peek(c)){
		_rr.push(&Reader::refill);
		return Continue;
	}
	_rr.drop();
	return Success;
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
	return Success;
}
/*static*/ int Reader::fetchChar(Reader &_rr, Parameter &_rp){
	int c;
	if(_rr.peek(c)){
		_rr.push(&Reader::refill);
		return Continue;
	}
	*reinterpret_cast<char*>(_rp.a.p) = c;
	_rr.drop();
	return Success;
}
//default we close
void Reader::prepareErrorRecovery(){
	push(&Reader::returnValue<true>, Parameter(Failure));
}

int Reader::doManage(int _mo){
	cassert(false);
	return Success;
}

void Reader::doPrepareBuffer(char *_newbeg, const char *_newend){
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
