/* Implementation file alphawriter.cpp
	
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

#include "alphawriter.hpp"
#include "alphaprotocolfilters.hpp"
#include "clientserver/tcp/channel.hpp"


namespace test{
namespace alpha{

Writer::Writer(clientserver::tcp::Channel &_rch):rch(_rch){
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

/*static*/ int Writer::putAString(protocol::Writer &_rw, protocol::Parameter &_rp){
	Writer &rw = static_cast<Writer&>(_rw);
	const char *ps = (const char*)_rp.a.p;
	if(_rp.b.u > 128 || isLiteralString(ps, _rp.b.u)){
		//right here i know for sure I can write safely (buflen - FlushLength) chars
		rw<<'{'<<(uint32)_rp.b.u;
		rw.putChar('}','\r','\n');
		rw.fs.top().first = &Writer::putAtom;
	}else{//send quoted
		rw<<'\"';
		protocol::Parameter ppp(_rp);
		
		rw.replace(&Writer::putChar, protocol::Parameter('\"'));
		rw.push(&Writer::putAtom, ppp);
		rw.push(&Writer::flush);//only try to do a flush
	}
	return Continue;
}
//NOTE: to be used by the above method
/*static*/ /*int Writer::putQString(protocol::Writer &_rw, protocol::Parameter &_rp){
	return Bad;
}*/

/*static*/ int Writer::putStatus(protocol::Writer &_rw, protocol::Parameter &_rp){
	Writer &rw = static_cast<Writer&>(_rw);
	protocol::Parameter rp = _rp;
	rw.replace(&Writer::clear);
	rw.push(&Writer::flushAll);
	rw.push(&Writer::putCrlf);
	if(rp.a.p){
		rw.push(&Writer::putAtom, rp);
	}else{//send the msg
		rw.push(&Writer::putAtom, protocol::Parameter((void*)rw.msgs.data(), rw.msgs.size()));	
	}
	//rw.push(&Writer::putChar, protocol::Parameter(' '));
	if(rw.tags.size()){
		rw.push(&Writer::putAtom, protocol::Parameter((void*)rw.tags.data(), rw.tags.size()));
	}else{
		rw.push(&Writer::putChar, protocol::Parameter('*'));
	}
	return Continue;
}

/*static*/ int Writer::clear(protocol::Writer &_rw, protocol::Parameter &_rp){
	Writer &rw = static_cast<Writer&>(_rw);
	rw.clear();
	return Ok;
}

/*static*/ int Writer::putCrlf(protocol::Writer &_rw, protocol::Parameter &_rp){
	_rw.replace(&Writer::putChar, protocol::Parameter('\n'));
	_rw.push(&Writer::putChar, protocol::Parameter('\r'));
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
	return rch.send(_pb, _bl);
}

}//namespace alpha
}//namespace test

