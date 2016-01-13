#include "gammareader.hpp"
#include "gammawriter.hpp"
#include "gammafilters.hpp"
#include "gammaconnection.hpp"

#include <cerrno>

using namespace solid;
using namespace solid::protocol::text;

namespace concept{
namespace gamma{

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
	uint _sid,
	Writer &_rw,
	protocol::text::Logger *_plog
):protocol::text::Reader(_plog), sid(_sid), rw(_rw){
}
Reader::~Reader(){
}
void Reader::clear(){
	resetState();
	cassert(fs.empty());
	tmp.clear();
}
/*static*/ int Reader::fetchAString(protocol::text::Reader &_rr, protocol::text::Parameter &_rp){
	Reader &rr = static_cast<Reader&>(_rr);
	int c;
    int rv = rr.peek(c);
    if(rv == Wait){
		_rr.push(&Reader::refill);
		return Continue;
	}
	
	cassert(rv == Success);
	if(c == '{'){
		rr.drop();
		rr.fs.top().first = &Reader::copyTmpString;
		protocol::text::Parameter &rp = rr.push(&Reader::checkLiteral);
		rp.b.i = 0;
		rr.push(&Reader::checkChar, protocol::text::Parameter('\n'));
		rr.push(&Reader::checkChar, protocol::text::Parameter('\r'));
		rr.push(&Reader::checkChar, protocol::text::Parameter('}'));
		rr.push(&Reader::saveCurrentChar, protocol::text::Parameter(&rp.b.i, (int)'+'));
		rr.push(&Reader::fetchUInt32, protocol::text::Parameter(&rp.a.u32));
	}else if(c == '\"'){
		rr.drop();
		rr.fs.top().first = &Reader::copyTmpString;
		rr.push(&Reader::checkChar, protocol::text::Parameter('\"'));
		rr.push(&Reader::fetchQString, protocol::text::Parameter(&rr.tmp));
		
	}else{//we have an atom
		rr.fs.top().first = &Reader::fetchFilteredString<AtomFilter>;
		_rp.b.u32 = 1024;//the string max size
	}
	return Continue;
}
/*static*/ int Reader::fetchQString(protocol::text::Reader &_rr, protocol::text::Parameter &_rp){
	Reader &rr = static_cast<Reader&>(_rr);
	while(true){
		int rv = rr.fetch<QuotedFilter>(rr.tmp, 1024);
		if(rv == Wait){
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
	return Success;
}
/*static*/ int Reader::copyTmpString(protocol::text::Reader &_rr, protocol::text::Parameter &_rp){
	Reader &rr = static_cast<Reader&>(_rr);
	*static_cast<String*>(_rp.a.p) = rr.tmp;
	rr.tmp.clear();
	return Success;
}

/*static*/ int Reader::flushWriter(protocol::text::Reader &_rr, protocol::text::Parameter &_rp){
	Reader &rr = static_cast<Reader&>(_rr);
	switch(rr.rw.flushAll()){
		case Failure:
			rr.state = IOErrorState;
			return Failure;
		case Success: return Success;
		case Wait:
			rr.replace(&Reader::returnValue<true>, protocol::text::Parameter(Continue));
			return Wait;
	}
	cassert(false);
	return Failure;
}

/*static*/ int Reader::checkLiteral(protocol::text::Reader &_rr, protocol::text::Parameter &_rp){
	//_rp.a contains the size of the literal
	//_rp.b contains '+' or 0
	Reader &rr = static_cast<Reader&>(_rr);
	//TODO: check for too big literals
	if(_rp.b.i){
		cassert(_rp.b.i == '+');
		rr.replace(&Reader::fetchLiteralString, protocol::text::Parameter(&rr.tmp, _rp.a.u32));
	}else{
		rr.rw<<"+ Expecting "<<(uint32)_rp.a.u32<<" Chars\r\n";
		rr.replace(&Reader::fetchLiteralString, protocol::text::Parameter(&rr.tmp, _rp.a.u32));
		rr.push(&Reader::flushWriter);
	}
	return Continue;
}

/*virtual*/ int Reader::read(char *_pb, uint32 _bl){
	return Connection::the().socketRecv(sid, _pb, _bl);
}
/*virtual*/ int Reader::readSize()const{
	return Connection::the().socketRecvSize(sid);
}
/*virtual*/ void Reader::prepareErrorRecovery(){
	push(&protocol::text::Reader::manage, protocol::text::Parameter(ResetLogging));
	push(&flushWriter);
	push(&errorRecovery, protocol::text::Parameter(64 * 1024));
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
		case QuotedString:
			rw.message() += "Error parsing quoted string@";
			break;
		default:
			rw.message() += "Unknown basic parser error@";
	}
}

/*static*/ int Reader::errorPrepare(protocol::text::Reader &_rr, protocol::text::Parameter &_rp){
	Reader &rr = static_cast<Reader&>(_rr);
	rr.tmp.clear();
	return Success;
}
/*static*/ int Reader::errorRecovery(protocol::text::Reader &_rr, protocol::text::Parameter &_rp){
	Reader &rr = static_cast<Reader&>(_rr);
	int rv = rr.locate<CharFilter<'\n'> >(rr.tmp, _rp.a.u32, 16);
	if(rv == Wait){
		_rr.push(&Reader::refill);
		return Continue;
	}else if(rv == Failure){
		rr.state = FatalErrorState;//give up
		return Failure;
	}
	rr.drop();
	uint32 litlen;
	rv = rr.extractLiteralLength(litlen);
	
	if(rv == Success){
		//rw.putTag()<<es;
		//rw.put('\r','\n');
		return Success;
	}
	if(rv == Failure){
		rr.state = FatalErrorState;//give up
		return Failure;
	}
	//rc == Wait cant send the response yet
	
	_rp.a.u32 = 64 * 1024;
	rr.tmp.clear();
	rr.push(&Reader::fetchLiteralDummy, protocol::text::Parameter(litlen));
	return Continue;
}

int Reader::extractLiteralLength(uint32 &_litlen){
	const char*str = tmp.c_str();
	unsigned sz = tmp.size();
    if(*(str + sz - 2) != '}') return Success;//no literal
    const char* p = str;
    const char* pend = str + sz;
    for(;p != pend && *p != '{'; ++p);
    if(*p == '{'){
        ++p;
        _litlen = strtoul(p, NULL, 10);
        if(errno == ERANGE){
            return Failure;
        }
    }else{
        return Failure;
    }
    if(*(str + sz - 3) == '+') return Wait;//literal plus
    return Success;//no literal plus
}

}//namespace gamma
}//namespace concept

