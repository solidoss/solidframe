// alphawriter.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "alphawriter.hpp"
#include "alphaprotocolfilters.hpp"
#include "alphaconnection.hpp"

using namespace solid;

namespace concept{
namespace alpha{

Writer::Writer(
	protocol::text::Logger *_plog
):protocol::text::Writer(_plog){
}

Writer::~Writer(){
}

void Writer::clear(){
	tags.clear();
	msgs.clear();
	cassert(wpos == rpos);
}

bool isLiteralString(const char *_pb, unsigned _bl){
	while(_bl--){
		//if it's not quoted means its literal
		if(!QuotedFilter::check(*_pb)) return true;
		++_pb;
	}
	return false;
}

/*static*/ int Writer::putAString(protocol::text::Writer &_rw, protocol::text::Parameter &_rp){
	Writer &rw = static_cast<Writer&>(_rw);
	const char *ps = (const char*)_rp.a.p;
	if(_rp.b.u32 > 128 || isLiteralString(ps, _rp.b.u32)){
		//right here i know for sure I can write safely (buflen - FlushLength) chars
		rw<<'{'<<_rp.b.u32;
		rw.putChar('}','\r','\n');
		rw.fs.top().first = &Writer::putAtom;
	}else{//send quoted
		rw<<'\"';
		protocol::text::Parameter ppp(_rp);
		
		rw.replace(&Writer::putChar, protocol::text::Parameter('\"'));
		rw.push(&Writer::putAtom, ppp);
		rw.push(&Writer::flush);//only try to do a flush
	}
	return Continue;
}
//NOTE: to be used by the above method
/*static*/ /*int Writer::putQString(protocol::text::Writer &_rw, protocol::text::Parameter &_rp){
	return Bad;
}*/

/*static*/ int Writer::putStatus(protocol::text::Writer &_rw, protocol::text::Parameter &_rp){
	Writer &rw = static_cast<Writer&>(_rw);
	protocol::text::Parameter rp = _rp;
	rw.replace(&Writer::clear);
	rw.push(&Writer::flushAll);
	rw.push(&Writer::putCrlf);
	if(rp.a.p){
		rw.push(&Writer::putAtom, rp);
	}else{//send the msg
		rw.push(&Writer::putAtom, protocol::text::Parameter((void*)rw.msgs.data(), rw.msgs.size()));
	}
	//rw.push(&Writer::putChar, protocol::text::Parameter(' '));
	if(rw.tags.size()){
		rw.push(&Writer::putAtom, protocol::text::Parameter((void*)rw.tags.data(), rw.tags.size()));
	}else{
		rw.push(&Writer::putChar, protocol::text::Parameter('*'));
	}
	return Continue;
}

/*static*/ int Writer::clear(protocol::text::Writer &_rw, protocol::text::Parameter &_rp){
	Writer &rw = static_cast<Writer&>(_rw);
	rw.clear();
	return Success;
}

/*static*/ int Writer::putCrlf(protocol::text::Writer &_rw, protocol::text::Parameter &_rp){
	_rw.replace(&Writer::putChar, protocol::text::Parameter('\n'));
	_rw.push(&Writer::putChar, protocol::text::Parameter('\r'));
	return Continue;
}

int Writer::write(char *_pb, uint32 _bl){
// 	switch(rch.send(_pb, _bl)){
// 		case BAD: return Bad;
// 		case OK: return Ok;
// 		case NOK: return No;
// 	}
// 	cassert(false);
// 	return Bad;
	return Connection::the().socketSend(_pb, _bl);
}

}//namespace alpha
}//namespace concept

