#include "gammawriter.hpp"
#include "gammafilters.hpp"
#include "gammaconnection.hpp"


namespace concept{
namespace gamma{

Writer::Writer(
	uint _sid,
	protocol::Logger *_plog
):protocol::Writer(_plog), sid(_sid){
}

Writer::~Writer(){
	vdbg(""<<(void*)this);
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
	if(_rp.b.u32 > 128 || isLiteralString(ps, _rp.b.u32)){
		//right here i know for sure I can write safely (buflen - FlushLength) chars
		rw<<'{'<<_rp.b.u32;
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
	return Connection::the().socketSend(sid, _pb, _bl);
}

}//namespace gamma
}//namespace concept

