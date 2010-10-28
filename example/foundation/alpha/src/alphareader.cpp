/* Implementation file alphareader.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "alphareader.hpp"
#include "alphawriter.hpp"
#include "alphaprotocolfilters.hpp"
#include "alphaconnection.hpp"
#include <cerrno>

namespace concept{
namespace alpha{

static const char *char2name[128] = {
    "\\0x0", "\\0x1", "\\0x2", "\\0x3", "\\0x4", "\\0x5", "\\0x6", "\\0x7", "\\0x8", "\\0x9",
    "LF", "\\0xb", "\\0xc", "CR", "\\0xe", "\\0xf", "\\0x10", "TAB", "\\0x12", "\\0x13",
    "\\0x14", "\\0x15", "\\0x16", "\\0x17", "\\0x18", "\\0x19", "\\0x1a", "\\0x1b", "\\0x1c", "\\0x1d",
    "\\0x1e", "\\0x1f", "SPACE", "!", "DQUOTE", "#", "$", "%", "&", "'",
    "(", ")", "*", "+", ",", "-", ".", "/", "0", "1",
    "2", "3", "4", "5", "6", "7", "8", "9", ":", ";",
    "<", "=", ">", "?", "@", "A", "B", "C", "D", "E",
    "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y",
    "Z", "[", "\"", "]", "^", "_", "`", "a", "b", "c",
    "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w",
    "x", "y", "z", "{", "|", "}", "~", "\\0x7f"
};


Reader::Reader(
	Writer &_rw,
	protocol::Logger *_plog
):protocol::Reader(_plog), rw(_rw){
}
Reader::~Reader(){
}
void Reader::clear(){
	resetState();
	cassert(fs.empty());
	tmp.clear();
}
/*static*/ int Reader::fetchAString(protocol::Reader &_rr, protocol::Parameter &_rp){
	Reader &rr = static_cast<Reader&>(_rr);
	int c;
    int rv = rr.peek(c);
    if(rv == No){
		_rr.push(&Reader::refill);
		return Continue;
	}
	
	cassert(rv == Ok);
	if(c == '{'){
		rr.drop();
		rr.fs.top().first = &Reader::copyTmpString;
		protocol::Parameter &rp = rr.push(&Reader::checkLiteral);
		rp.b.i = 0;
		rr.push(&Reader::checkChar, protocol::Parameter('\n'));
		rr.push(&Reader::checkChar, protocol::Parameter('\r'));
		rr.push(&Reader::checkChar, protocol::Parameter('}'));
		rr.push(&Reader::saveCurrentChar, protocol::Parameter(&rp.b.i, (int)'+'));
		rr.push(&Reader::fetchUInt32, protocol::Parameter(&rp.a.u32));
	}else if(c == '\"'){
		rr.drop();
		rr.fs.top().first = &Reader::copyTmpString;
		rr.push(&Reader::checkChar, protocol::Parameter('\"'));
		rr.push(&Reader::fetchQString, protocol::Parameter(&rr.tmp));
		
	}else{//we have an atom
		rr.fs.top().first = &Reader::fetchFilteredString<AtomFilter>;
		_rp.b.u32 = 1024;//the string max size
	}
	return Continue;
}
/*static*/ int Reader::fetchQString(protocol::Reader &_rr, protocol::Parameter &_rp){
	Reader &rr = static_cast<Reader&>(_rr);
	while(true){
		int rv = rr.fetch<QuotedFilter>(rr.tmp, 1024);
		if(rv == No){
			rr.push(&Reader::refill);
			return Continue;
		}else if(rv == Error && rr.tmp.size() > 1024){
			rr.basicError(StringTooLong);
			return Error;
		}
		int c;
		rr.peek(c);//non blocking
		if(c == '\"'){
			if(_rp.a.i == '\\'){
				rr.tmp += c;
				_rp.a.i = '\0';
				rr.drop();
				continue;
			}
			break;
		}else if(c == '\\'){
			if(_rp.a.i == '\\'){
				rr.tmp += '\\';
				_rp.a.i = '\0';
			}else{
				_rp.a.i = '\\';
			}
			rr.drop();
			continue;
		}
		_rp.a.i = '\\';
		break;
	}
	if(_rp.a.i == '\\'){
		rr.basicError(QuotedString);
		return Error;
	}
	return Ok;
}
/*static*/ int Reader::copyTmpString(protocol::Reader &_rr, protocol::Parameter &_rp){
	Reader &rr = static_cast<Reader&>(_rr);
	*static_cast<String*>(_rp.a.p) = rr.tmp;
	rr.tmp.clear();
	return Ok;
}

/*static*/ int Reader::flushWriter(protocol::Reader &_rr, protocol::Parameter &_rp){
	Reader &rr = static_cast<Reader&>(_rr);
	switch(rr.rw.flushAll()){
		case Writer::Bad:
			rr.state = IOErrorState;
			return Bad;
		case Writer::Ok: return Ok;
		case Writer::No:
			rr.replace(&Reader::returnValue<true>, protocol::Parameter(Continue));
			return No;
	}
	cassert(false);
	return Bad;
}

/*static*/ int Reader::checkLiteral(protocol::Reader &_rr, protocol::Parameter &_rp){
	//_rp.a contains the size of the literal
	//_rp.b contains '+' or 0
	Reader &rr = static_cast<Reader&>(_rr);
	//TODO: check for too big literals
	if(_rp.b.i){
		cassert(_rp.b.i == '+');
		rr.replace(&Reader::fetchLiteralString, protocol::Parameter(&rr.tmp, _rp.a.u32));
	}else{
		rr.rw<<"+ Expecting "<<_rp.a.u32<<" Chars\r\n";
		rr.replace(&Reader::fetchLiteralString, protocol::Parameter(&rr.tmp, _rp.a.u32));
		rr.push(&Reader::flushWriter);
	}
	return Continue;
}

/*virtual*/ int Reader::read(char *_pb, uint32 _bl){
// 	switch(rch.recv(_pb, _bl)){
//     	case BAD:	return Bad;
//     	case OK:	return Ok;
//     	case NOK:	return No;
//     }
//     return Bad;
	return Connection::the().socketRecv(_pb, _bl);
}
/*virtual*/ int Reader::readSize()const{
	return Connection::the().socketRecvSize();
}
/*virtual*/ void Reader::prepareErrorRecovery(){
	push(&protocol::Reader::manage, protocol::Parameter(ResetLogging));
	push(&flushWriter);
	push(&errorRecovery, protocol::Parameter(64 * 1024));
	push(&errorPrepare);
}
/*virtual*/ void Reader::charError(char _popc, char _expc){
	rw.message().clear();
	rw.message() += " BAD Expected ";
	rw.message() += char2name[((uint8)_expc) & 127];
	rw.message() += " but found ";
	rw.message() += char2name[((uint8)_popc) & 127];
	rw.message() += "@";
}
/*virtual*/ void Reader::keyError(const String &_pops, int _id){
	rw.message() = " BAD ";
	switch(_id){
		case Unexpected:
			rw.message() += "Unexpected ";
			rw.message() += _pops;
			rw.message() += " tocken found@";
			break;
		case NotANumber:
			rw.message() += "Exected a number but found ";
			rw.message() += _pops;
			rw.message() += ' ';
			rw.message() += '@';
			break;
		default:
			rw.message() += "Unknown key error ";
			rw.message() += _pops;
			rw.message() += ' ';
			rw.message() += '@';
	}
}
/*virtual*/ void Reader::basicError(int _id){
	rw.message() = " BAD ";
	switch(_id){
		case StringTooLong:
			rw.message() += "Tocken too long@";
			break;
		case EmptyAtom:
			rw.message() += "Empty atom@";
		case QuotedString:
			rw.message() += "Error parsing quoted string@";
			break;
		default:
			rw.message() += "Unknown basic parser error@";
	}
}

/*static*/ int Reader::errorPrepare(protocol::Reader &_rr, protocol::Parameter &_rp){
	Reader &rr = static_cast<Reader&>(_rr);
	rr.tmp.clear();
	return Ok;
}
/*static*/ int Reader::errorRecovery(protocol::Reader &_rr, protocol::Parameter &_rp){
	Reader &rr = static_cast<Reader&>(_rr);
	int rv = rr.locate<CharFilter<'\n'> >(rr.tmp, _rp.a.u32, 16);
	if(rv == No){
		_rr.push(&Reader::refill);
		return Continue;
	}else if(rv == Bad){
		rr.state = FatalErrorState;//give up
		return Bad;
	}
	rr.drop();
	uint32 litlen;
	rv = rr.extractLiteralLength(litlen);
	
	if(rv == Ok){
		//rw.putTag()<<es;
		//rw.put('\r','\n');
		return Ok;
	}
	if(rv == Bad){
		rr.state = FatalErrorState;//give up
		return Bad;
	}
	//rc == No cant send the response yet
	
	_rp.a.u32 = 64 * 1024;
	rr.tmp.clear();
	rr.push(&Reader::fetchLiteralDummy, protocol::Parameter(litlen));
	return Continue;
}

int Reader::extractLiteralLength(uint32 &_litlen){
	const char*str = tmp.c_str();
	unsigned sz = tmp.size();
    if(*(str + sz - 2) != '}') return Ok;//no literal
    const char* p = str;
    const char* pend = str + sz;
    for(;p != pend && *p != '{'; ++p);
    if(*p == '{'){
        ++p;
        _litlen = strtoul(p, NULL, 10);
        if(errno == ERANGE){
            return Bad;
        }
    }else{
        return Bad;
    }
    if(*(str + sz - 3) == '+') return No;//literal plus
    return Ok;//no literal plus
}

}//namespace alpha
}//namespace concept

