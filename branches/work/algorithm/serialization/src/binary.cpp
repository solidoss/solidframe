/* Implementation file binary.cpp
	
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

#include "algorithm/serialization/binary.hpp"
#include "utility/ostream.hpp"
#include "utility/istream.hpp"
#include "system/cstring.hpp"
#include "cstring"

namespace serialization{
namespace binary{

inline char *storeValue(char *_pd, const uint16 _val){
	uint8 *pd = reinterpret_cast<uint8*>(_pd);
	*(pd) 		= ((_val >> 8) & 0xff);
	*(pd + 1)	= (_val & 0xff);
	return _pd + 2;
}
inline const char* parseValue(const char *_ps, uint16 &_val){
	const uint8 *ps = reinterpret_cast<const uint8*>(_ps);
	_val = (uint8)(*ps);
	_val <<= 8;
	_val += (uint8)(*(ps + 1));
	return _ps + 2;
}
#ifdef HAVE_SAFE_STATIC
/*static*/ Limits const& Limits::the(){
	static const Limits l;
	return l;
}
#else
Limits const& the_limits(){
	static const Limits l;
	return l;
}

void once_limits(){
	the_limits();
}

/*static*/ Limits const& Limits::the(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_limits, once);
	return the_limits();
}
#endif
//========================================================================
/*static*/ const char *Base::errorString(const uint16 _err){
	switch(_err){
		case ERR_NOERROR:
			return "No error";
		case ERR_ARRAY_LIMIT:
			return "Array limit";
		case ERR_ARRAY_MAX_LIMIT:
			return "Array max limit";
		case ERR_CONTAINER_LIMIT:
			return "Container limit";
		case ERR_CONTAINER_MAX_LIMIT:
			return "Container max limit";
		case ERR_STREAM_LIMIT:
			return "Stream limit";
		case ERR_STREAM_MAX_LIMIT:
			return "Stream max limit";
		case ERR_STREAM_CHUNK_MAX_LIMIT:
			return "Stream chunk max limit";
		case ERR_STRING_LIMIT:
			return "String limit";
		case ERR_STRING_MAX_LIMIT:
			return "String max limit";
		case ERR_UTF8_LIMIT:
			return "Utf8 limit";
		case ERR_UTF8_MAX_LIMIT:
			return "Utf8 max limit";
		case ERR_POINTER_UNKNOWN:
			return "Unknown pointer type id";
		default:
			return "Unknown error";
	};
}
/*static*/ int Base::setStringLimit(Base& _rb, FncData &_rfd){
	_rb.limits.stringlimit = static_cast<uint32>(_rfd.s);
	return OK;
}
/*static*/ int Base::setStreamLimit(Base& _rb, FncData &_rfd){
	_rb.limits.streamlimit = _rfd.s;
	return OK;
}
/*static*/ int Base::setContainerLimit(Base& _rb, FncData &_rfd){
	_rb.limits.containerlimit = static_cast<uint32>(_rfd.s);
	return OK;
}
void Base::replace(const FncData &_rfd){
	fstk.top() = _rfd;
}
int Base::popEStack(Base &_rb, FncData &){
	_rb.estk.pop();
	return OK;
}
//========================================================================
Serializer::~Serializer(){
}
void Serializer::clear(){
	run(NULL, 0);
}
Serializer& Serializer::pushStringLimit(){
	fstk.push(FncData(&Base::setStringLimit, 0, 0, rdefaultlimits.stringlimit));
	return *this;
}
Serializer& Serializer::pushStringLimit(uint32 _v){
	fstk.push(FncData(&Base::setStringLimit, 0, 0, _v));
	return *this;
}
Serializer& Serializer::pushStreamLimit(){
	fstk.push(FncData(&Base::setStreamLimit, 0, 0, rdefaultlimits.streamlimit));
	return *this;
}
Serializer& Serializer::pushStreamLimit(uint64 _v){
	fstk.push(FncData(&Base::setStreamLimit, 0, 0, _v));
	return *this;
}
Serializer& Serializer::pushContainerLimit(){
	fstk.push(FncData(&Base::setContainerLimit, 0, 0, rdefaultlimits.containerlimit));
	return *this;
}
Serializer& Serializer::pushContainerLimit(uint32 _v){
	fstk.push(FncData(&Base::setContainerLimit, 0, 0, _v));
	return *this;
}

int Serializer::run(char *_pb, unsigned _bl){
	cpb = pb = _pb;
	be = cpb + _bl;
	while(fstk.size()){
		FncData &rfd = fstk.top();
		switch((*reinterpret_cast<FncT>(rfd.f))(*this, rfd)) {
			case CONTINUE: continue;
			case OK: fstk.pop(); break;
			case NOK: goto Done;
			case BAD: 
				resetLimits();
				return -1;
		}
	}
	resetLimits();
	Done:
	return cpb - pb;
}
template <>
int Serializer::storeBinary<0>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, "");
	Serializer &rs(static_cast<Serializer&>(_rb));
	if(!rs.cpb) return OK;
	uint32 len = rs.be - rs.cpb;
	if(len > _rfd.s) len = static_cast<uint32>(_rfd.s);
	memcpy(rs.cpb, _rfd.p, len);
	rs.cpb += len;
	_rfd.p = (char*)_rfd.p + len;
	_rfd.s -= len;
	if(_rfd.s) return NOK;
	return OK;
}

template <>
int Serializer::storeBinary<1>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, "");
	Serializer &rs(static_cast<Serializer&>(_rb));
	if(!rs.cpb) return OK;
	const unsigned len = rs.be - rs.cpb;
	if(len){
		*rs.cpb = *reinterpret_cast<const char*>(_rfd.p);
		++rs.cpb;
		return OK;
	}
	return NOK;
}

template <>
int Serializer::storeBinary<2>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, "");
	Serializer &rs(static_cast<Serializer&>(_rb));
	if(!rs.cpb) return OK;
	const unsigned	len = rs.be - rs.cpb;
	const char		*ps = reinterpret_cast<const char*>(_rfd.p);
	if(len >= 2){
		*(rs.cpb + 0) = *(ps + 0);
		*(rs.cpb + 1) = *(ps + 1);
		rs.cpb += 2;
		return OK;
	}else if(len >= 1){
		*(rs.cpb + 0) = *(ps + 0);
		_rfd.f = &Serializer::storeBinary<1>;
		_rfd.p = const_cast<char*>(ps + 1);
		rs.cpb += 1;
	}
	return NOK;
}

template <>
int Serializer::storeBinary<4>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, "");
	Serializer &rs(static_cast<Serializer&>(_rb));
	if(!rs.cpb) return OK;
	const unsigned	len = rs.be - rs.cpb;
	const char		*ps = reinterpret_cast<const char*>(_rfd.p);
	if(len >= 4){
		*(rs.cpb + 0) = *(ps + 0);
		*(rs.cpb + 1) = *(ps + 1);
		*(rs.cpb + 2) = *(ps + 2);
		*(rs.cpb + 3) = *(ps + 3);
		rs.cpb += 4;
		return OK;
	}else if(len >= 3){
		*(rs.cpb + 0) = *(ps + 0);
		*(rs.cpb + 1) = *(ps + 1);
		*(rs.cpb + 2) = *(ps + 2);
		_rfd.p = const_cast<char*>(ps + 3);
		_rfd.f = &Serializer::storeBinary<1>;
		rs.cpb += 3;
		return NOK;
	}else if(len >= 2){
		*(rs.cpb + 0) = *(ps + 0);
		*(rs.cpb + 1) = *(ps + 1);
		_rfd.p = const_cast<char*>(ps + 2);
		rs.cpb += 2;
		_rfd.f = &Serializer::storeBinary<2>;
		return NOK;
	}else if(len >= 1){
		*(rs.cpb + 0) = *(ps + 0);
		_rfd.p = const_cast<char*>(ps + 1);
		rs.cpb += 1;
	}
	_rfd.f = &Serializer::storeBinary<0>;
	return NOK;
}


template <>
int Serializer::storeBinary<8>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, "");
	Serializer &rs(static_cast<Serializer&>(_rb));
	if(!rs.cpb) return OK;
	const unsigned	len = rs.be - rs.cpb;
	const char		*ps = reinterpret_cast<const char*>(_rfd.p);
	if(len >= 8){
		*(rs.cpb + 0) = *(ps + 0);
		*(rs.cpb + 1) = *(ps + 1);
		*(rs.cpb + 2) = *(ps + 2);
		*(rs.cpb + 3) = *(ps + 3);
		*(rs.cpb + 4) = *(ps + 4);
		*(rs.cpb + 5) = *(ps + 5);
		*(rs.cpb + 6) = *(ps + 6);
		*(rs.cpb + 7) = *(ps + 7);
		rs.cpb += 8;
		return OK;
	}else if(len >= 7){
		*(rs.cpb + 0) = *(ps + 0);
		*(rs.cpb + 1) = *(ps + 1);
		*(rs.cpb + 2) = *(ps + 2);
		*(rs.cpb + 3) = *(ps + 3);
		*(rs.cpb + 4) = *(ps + 4);
		*(rs.cpb + 5) = *(ps + 5);
		*(rs.cpb + 6) = *(ps + 6);
		_rfd.p = const_cast<char*>(ps + 7);
		_rfd.f = &Serializer::storeBinary<1>;
		rs.cpb += 7;
		return NOK;
	}else if(len >= 6){
		*(rs.cpb + 0) = *(ps + 0);
		*(rs.cpb + 1) = *(ps + 1);
		*(rs.cpb + 2) = *(ps + 2);
		*(rs.cpb + 3) = *(ps + 3);
		*(rs.cpb + 4) = *(ps + 4);
		*(rs.cpb + 5) = *(ps + 5);
		_rfd.p = const_cast<char*>(ps + 6);
		_rfd.f = &Serializer::storeBinary<2>;
		rs.cpb += 6;
		return NOK;
	}else if(len >= 5){
		*(rs.cpb + 0) = *(ps + 0);
		*(rs.cpb + 1) = *(ps + 1);
		*(rs.cpb + 2) = *(ps + 2);
		*(rs.cpb + 3) = *(ps + 3);
		*(rs.cpb + 4) = *(ps + 4);
		_rfd.p = const_cast<char*>(ps + 5);
		rs.cpb += 5;
	}else if(len >= 4){
		*(rs.cpb + 0) = *(ps + 0);
		*(rs.cpb + 1) = *(ps + 1);
		*(rs.cpb + 2) = *(ps + 2);
		*(rs.cpb + 3) = *(ps + 3);
		_rfd.p = const_cast<char*>(ps + 4);
		_rfd.f = &Serializer::storeBinary<4>;
		rs.cpb += 4;
		return NOK;
	}else if(len >= 3){
		*(rs.cpb + 0) = *(ps + 0);
		*(rs.cpb + 1) = *(ps + 1);
		*(rs.cpb + 2) = *(ps + 2);
		_rfd.p = const_cast<char*>(ps + 3);
		rs.cpb += 3;
	}else if(len >= 2){
		*(rs.cpb + 0) = *(ps + 0);
		*(rs.cpb + 1) = *(ps + 1);
		_rfd.p = const_cast<char*>(ps + 2);
		rs.cpb += 2;
	}else if(len >= 1){
		*(rs.cpb + 0) = *(ps + 0);
		_rfd.p = const_cast<char*>(ps + 1);
		rs.cpb += 1;
	}
	_rfd.f = &Serializer::storeBinary<0>;
	return NOK;
}


template <>
int Serializer::store<int8>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, ""<<_rfd.n);
	_rfd.s = sizeof(int8);
	_rfd.f = &Serializer::storeBinary<1>;
	return storeBinary<1>(_rb, _rfd);
}
template <>
int Serializer::store<uint8>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, ""<<_rfd.n);
	_rfd.s = sizeof(uint8);
	_rfd.f = &Serializer::storeBinary<1>;
	return storeBinary<1>(_rb, _rfd);
}
template <>
int Serializer::store<int16>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, ""<<_rfd.n);
	_rfd.s = sizeof(int16);
	_rfd.f = &Serializer::storeBinary<2>;
	return storeBinary<2>(_rb, _rfd);
}
template <>
int Serializer::store<uint16>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, ""<<_rfd.n);
	_rfd.s = sizeof(uint16);
	_rfd.f = &Serializer::storeBinary<2>;
	return storeBinary<2>(_rb, _rfd);
}
template <>
int Serializer::store<int32>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, ""<<_rfd.n);
	_rfd.s = sizeof(int32);
	_rfd.f = &Serializer::storeBinary<4>;
	return storeBinary<4>(_rb, _rfd);
}
template <>
int Serializer::store<uint32>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, ""<<_rfd.n);
	_rfd.s = sizeof(uint32);
	_rfd.f = &Serializer::storeBinary<4>;
	return storeBinary<4>(_rb, _rfd);
}
template <>
int Serializer::store<int64>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, "");
	_rfd.s = sizeof(int64);
	_rfd.f = &Serializer::storeBinary<8>;
	return storeBinary<8>(_rb, _rfd);
}
template <>
int Serializer::store<uint64>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, ""<<_rfd.n<<*((uint64*)_rfd.p));
	_rfd.s = sizeof(uint64);
	_rfd.f = &Serializer::storeBinary<8>;
	return storeBinary<8>(_rb, _rfd);
}
/*template <>
int Serializer::store<ulong>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, "");
	_rfd.s = sizeof(ulong);
	_rfd.f = &Serializer::storeBinary;
	return CONTINUE;
}*/
template <>
int Serializer::store<std::string>(Base &_rb, FncData &_rfd){
	Serializer &rs(static_cast<Serializer&>(_rb));
	idbgx(Dbg::ser_bin, "");
	if(!rs.cpb) return OK;
	std::string * c = reinterpret_cast<std::string*>(_rfd.p);
	if(rs.limits.stringlimit && c->size() > rs.limits.stringlimit){
		rs.err = ERR_STRING_LIMIT;
		return BAD;
	}
	if(c->size() > CRCValue<uint32>::maximum()){
		rs.err = ERR_STRING_MAX_LIMIT;
		return BAD;
	}
	const CRCValue<uint32> crcsz((uint32)c->size());
	
	rs.estk.push(ExtData((uint32)crcsz));
	
	rs.replace(FncData(&Serializer::storeBinary<0>, (void*)c->data(), _rfd.n, c->size()));
	rs.fstk.push(FncData(&Base::popEStack, NULL, _rfd.n));
	rs.fstk.push(FncData(&Serializer::store<uint32>, &rs.estk.top().u32(), _rfd.n));
	return CONTINUE;
}

int Serializer::storeStream(Base &_rb, FncData &_rfd){
	Serializer &rs(static_cast<Serializer&>(_rb));
	idbgx(Dbg::ser_bin, "");
	if(!rs.cpb) return OK;
	InputStreamData &rsp(*reinterpret_cast<InputStreamData*>(rs.estk.top().buf));
	int32 toread = rs.be - rs.cpb;
	if(toread < MINSTREAMBUFLEN) return NOK;
	toread -= 2;//the buffsize
	if(toread > rsp.sz){
		toread = static_cast<int32>(rsp.sz);
	}
	if(toread > CRCValue<uint16>::maximum()){
		toread = CRCValue<uint16>::maximum();
	}
	idbgx(Dbg::ser_bin, "toread = "<<toread<<" MINSTREAMBUFLEN = "<<MINSTREAMBUFLEN);
	int rv = rsp.pis->read(rsp.off, rs.cpb + 2, toread);
	if(rv == toread){
		const CRCValue<uint16> crcsz((uint16)toread);
		
		storeValue(rs.cpb, (uint16)crcsz);
		
		idbgx(Dbg::ser_bin, "store value "<<(uint16)crcsz);
			  
		rs.cpb += toread + 2;
		rsp.off += toread;
	}else{
		rs.cpb = storeValue(rs.cpb, (uint16)0xffff);
		return OK;
	}
	rsp.sz -= toread;
	idbgx(Dbg::ser_bin, "rsp.sz = "<<rsp.sz);
	if(rsp.sz) return NOK;
	return OK;
}
/*static*/ int Serializer::storeUtf8(Base &_rs, FncData &_rfd){
	Serializer &rs(static_cast<Serializer&>(_rs));
	if(rs.limits.stringlimit && (_rfd.s - 1) > rs.limits.stringlimit){
		rs.err = ERR_UTF8_LIMIT;
		return BAD;
	}
	if((_rfd.s - 1) > CRCValue<uint32>::maximum()){
		rs.err = ERR_UTF8_MAX_LIMIT;
		return BAD;
	}
	_rfd.f = &Serializer::storeBinary<0>;
	return CONTINUE;
}
Serializer& Serializer::pushBinary(void *_p, size_t _sz, const char *_name){
	fstk.push(FncData(&Serializer::storeBinary<0>, _p, _name, _sz));
	return *this;
}
Serializer& Serializer::pushUtf8(const std::string& _str, const char *_name){
	fstk.push(FncData(&Serializer::storeUtf8, const_cast<char*>(_str.c_str()), _name, _str.length() + 1));
	return *this;
}
//========================================================================
Deserializer::~Deserializer(){
}
void Deserializer::clear(){
	idbgx(Dbg::ser_bin, "clear_deser");
	run(NULL, 0);
}

/*static*/ int Deserializer::parseTypeIdDone(Base& _rd, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rd));
	void		*p = _rfd.p;
	const char	*n = _rfd.n;
	rd.fstk.pop();
	if(rd.typeMapper().prepareParsePointer(&rd, rd.tmpstr, p, n)){
		return CONTINUE;
	}else{
		idbgx(Dbg::ser_bin, "error");
		rd.err = ERR_POINTER_UNKNOWN;
		return BAD;
	}
}
/*static*/ int Deserializer::parseTypeId(Base& _rd, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rd));
	rd.typeMapper().prepareParsePointerId(&rd, rd.tmpstr, _rfd.n);
	_rfd.f = &parseTypeIdDone;
	return CONTINUE;
}

Deserializer& Deserializer::pushStringLimit(){
	fstk.push(FncData(&Base::setStringLimit, 0, 0, rdefaultlimits.stringlimit));
	return *this;
}
Deserializer& Deserializer::pushStringLimit(uint32 _v){
	fstk.push(FncData(&Base::setStringLimit, 0, 0, _v));
	return *this;
}
Deserializer& Deserializer::pushStreamLimit(){
	fstk.push(FncData(&Base::setStreamLimit, 0, 0, rdefaultlimits.streamlimit));
	return *this;
}
Deserializer& Deserializer::pushStreamLimit(uint64 _v){
	fstk.push(FncData(&Base::setStreamLimit, 0, 0, _v));
	return *this;
}
Deserializer& Deserializer::pushContainerLimit(){
	fstk.push(FncData(&Base::setContainerLimit, 0, 0, rdefaultlimits.containerlimit));
	return *this;
}
Deserializer& Deserializer::pushContainerLimit(uint32 _v){
	fstk.push(FncData(&Base::setContainerLimit, 0, 0, _v));
	return *this;
}

int Deserializer::run(const char *_pb, unsigned _bl){
	cpb = pb = _pb;
	be = pb + _bl;
	while(fstk.size()){
		FncData &rfd = fstk.top();
		switch((*reinterpret_cast<FncT>(rfd.f))(*this, rfd)){
			case CONTINUE: continue;
			case OK: fstk.pop(); break;
			case NOK: goto Done;
			case BAD:
				idbgx(Dbg::ser_bin, "error: "<<errorString());
				resetLimits();
				return -1;
		}
	}
	resetLimits();
	Done:
	return cpb - pb;
}
template <>
int Deserializer::parseBinary<0>(Base &_rb, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	idbgx(Dbg::ser_bin, "");
	if(!rd.cpb) return OK;
	uint32 len = rd.be - rd.cpb;
	if(len > _rfd.s) len = static_cast<uint32>(_rfd.s);
	memcpy(_rfd.p, rd.cpb, len);
	rd.cpb += len;
	_rfd.p = (char*)_rfd.p + len;
	_rfd.s -= len;
	if(_rfd.s) return NOK;
	return OK;
}

template <>
int Deserializer::parseBinary<1>(Base &_rb, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	idbgx(Dbg::ser_bin, "");
	if(!rd.cpb) return OK;
	const unsigned	len = rd.be - rd.cpb;
	char			*ps = reinterpret_cast<char*>(_rfd.p);
	if(len >= 1){
		*(ps + 0) = *(rd.cpb + 0);
		rd.cpb += 1;
		return OK;
	}	
	return NOK;
}


template <>
int Deserializer::parseBinary<2>(Base &_rb, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	idbgx(Dbg::ser_bin, "");
	if(!rd.cpb) return OK;
	const unsigned	len = rd.be - rd.cpb;
	char			*ps = reinterpret_cast<char*>(_rfd.p);
	if(len >= 2){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		rd.cpb += 2;
		return OK;
	}else if(len >= 1){
		*(ps + 0) = *(rd.cpb + 0);
		rd.cpb += 1;
		_rfd.p = ps + 1;
		_rfd.f = &Deserializer::parseBinary<1>;
	}
	return NOK;
}

template <>
int Deserializer::parseBinary<3>(Base &_rb, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	idbgx(Dbg::ser_bin, "");
	if(!rd.cpb) return OK;
	const unsigned	len = rd.be - rd.cpb;
	char			*ps = reinterpret_cast<char*>(_rfd.p);
	if(len >= 3){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		rd.cpb += 3;
		return OK;
	}else if(len >= 2){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		rd.cpb += 2;
		_rfd.p = ps + 2;
		_rfd.f = &Deserializer::parseBinary<1>;
	}else if(len >= 1){
		*(ps + 0) = *(rd.cpb + 0);
		rd.cpb += 1;
		_rfd.p = ps + 1;
		_rfd.f = &Deserializer::parseBinary<2>;
	}
	return NOK;
}

template <>
int Deserializer::parseBinary<4>(Base &_rb, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	idbgx(Dbg::ser_bin, "");
	if(!rd.cpb) return OK;
	const unsigned	len = rd.be - rd.cpb;
	char			*ps = reinterpret_cast<char*>(_rfd.p);
	if(len >= 4){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		rd.cpb += 4;
		return OK;
	}else if(len >= 3){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		rd.cpb += 3;
		_rfd.p = ps + 3;
		_rfd.f = &Deserializer::parseBinary<1>;
	}else if(len >= 2){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		rd.cpb += 2;
		_rfd.p = ps + 2;
		_rfd.f = &Deserializer::parseBinary<2>;
	}else if(len >= 1){
		*(ps + 0) = *(rd.cpb + 0);
		rd.cpb += 1;
		_rfd.p = ps + 1;
		_rfd.f = &Deserializer::parseBinary<3>;
	}
	return NOK;
}

template <>
int Deserializer::parseBinary<5>(Base &_rb, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	idbgx(Dbg::ser_bin, "");
	if(!rd.cpb) return OK;
	const unsigned	len = rd.be - rd.cpb;
	char			*ps = reinterpret_cast<char*>(_rfd.p);
	if(len >= 5){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		*(ps + 4) = *(rd.cpb + 4);
		rd.cpb += 5;
		return OK;
	}else if(len >= 4){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		rd.cpb += 4;
		_rfd.p = ps + 4;
		_rfd.f = &Deserializer::parseBinary<1>;
	}else if(len >= 3){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		rd.cpb += 3;
		_rfd.p = ps + 3;
		_rfd.f = &Deserializer::parseBinary<2>;
	}else if(len >= 2){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		rd.cpb += 2;
		_rfd.p = ps + 2;
		_rfd.f = &Deserializer::parseBinary<3>;
	}else if(len >= 1){
		*(ps + 0) = *(rd.cpb + 0);
		rd.cpb += 1;
		_rfd.p = ps + 1;
		_rfd.f = &Deserializer::parseBinary<4>;
	}
	return NOK;
}

template <>
int Deserializer::parseBinary<6>(Base &_rb, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	idbgx(Dbg::ser_bin, "");
	if(!rd.cpb) return OK;
	const unsigned	len = rd.be - rd.cpb;
	char			*ps = reinterpret_cast<char*>(_rfd.p);
	if(len >= 6){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		*(ps + 4) = *(rd.cpb + 4);
		*(ps + 5) = *(rd.cpb + 5);
		rd.cpb += 6;
		return OK;
	}else if(len >= 5){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		*(ps + 4) = *(rd.cpb + 4);
		rd.cpb += 5;
		_rfd.p = ps + 5;
		_rfd.f = &Deserializer::parseBinary<1>;
	}else if(len >= 4){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		rd.cpb += 4;
		_rfd.p = ps + 4;
		_rfd.f = &Deserializer::parseBinary<2>;
	}else if(len >= 3){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		rd.cpb += 3;
		_rfd.p = ps + 3;
		_rfd.f = &Deserializer::parseBinary<3>;
	}else if(len >= 2){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		rd.cpb += 2;
		_rfd.p = ps + 2;
		_rfd.f = &Deserializer::parseBinary<4>;
	}else if(len >= 1){
		*(ps + 0) = *(rd.cpb + 0);
		rd.cpb += 1;
		_rfd.p = ps + 1;
		_rfd.f = &Deserializer::parseBinary<5>;
	}
	return NOK;
}

template <>
int Deserializer::parseBinary<7>(Base &_rb, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	idbgx(Dbg::ser_bin, "");
	if(!rd.cpb) return OK;
	const unsigned	len = rd.be - rd.cpb;
	char			*ps = reinterpret_cast<char*>(_rfd.p);
	if(len >= 7){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		*(ps + 4) = *(rd.cpb + 4);
		*(ps + 5) = *(rd.cpb + 5);
		*(ps + 6) = *(rd.cpb + 6);
		rd.cpb += 7;
		return OK;
	}else if(len >= 6){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		*(ps + 4) = *(rd.cpb + 4);
		*(ps + 5) = *(rd.cpb + 5);
		rd.cpb += 6;
		_rfd.p = ps + 6;
		_rfd.f = &Deserializer::parseBinary<1>;
	}else if(len >= 5){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		*(ps + 4) = *(rd.cpb + 4);
		rd.cpb += 5;
		_rfd.p = ps + 5;
		_rfd.f = &Deserializer::parseBinary<2>;
	}else if(len >= 4){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		rd.cpb += 4;
		_rfd.p = ps + 4;
		_rfd.f = &Deserializer::parseBinary<3>;
	}else if(len >= 3){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		rd.cpb += 3;
		_rfd.p = ps + 3;
		_rfd.f = &Deserializer::parseBinary<4>;
	}else if(len >= 2){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		rd.cpb += 2;
		_rfd.p = ps + 2;
		_rfd.f = &Deserializer::parseBinary<5>;
	}else if(len >= 1){
		*(ps + 0) = *(rd.cpb + 0);
		rd.cpb += 1;
		_rfd.p = ps + 1;
		_rfd.f = &Deserializer::parseBinary<6>;
	}
	return NOK;
}

template <>
int Deserializer::parseBinary<8>(Base &_rb, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	idbgx(Dbg::ser_bin, "");
	if(!rd.cpb) return OK;
	const unsigned	len = rd.be - rd.cpb;
	char			*ps = reinterpret_cast<char*>(_rfd.p);
	if(len >= 8){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		*(ps + 4) = *(rd.cpb + 4);
		*(ps + 5) = *(rd.cpb + 5);
		*(ps + 6) = *(rd.cpb + 6);
		*(ps + 7) = *(rd.cpb + 7);
		rd.cpb += 8;
		return OK;
	}else if(len >= 7){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		*(ps + 4) = *(rd.cpb + 4);
		*(ps + 5) = *(rd.cpb + 5);
		*(ps + 6) = *(rd.cpb + 6);
		rd.cpb += 7;
		_rfd.p = ps + 7;
		_rfd.f = &Deserializer::parseBinary<1>;
	}else if(len >= 6){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		*(ps + 4) = *(rd.cpb + 4);
		*(ps + 5) = *(rd.cpb + 5);
		rd.cpb += 6;
		_rfd.p = ps + 6;
		_rfd.f = &Deserializer::parseBinary<2>;
	}else if(len >= 5){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		*(ps + 4) = *(rd.cpb + 4);
		rd.cpb += 5;
		_rfd.p = ps + 5;
		_rfd.f = &Deserializer::parseBinary<3>;
	}else if(len >= 4){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		rd.cpb += 4;
		_rfd.p = ps + 4;
		_rfd.f = &Deserializer::parseBinary<4>;
	}else if(len >= 3){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		rd.cpb += 3;
		_rfd.p = ps + 3;
		_rfd.f = &Deserializer::parseBinary<5>;
	}else if(len >= 2){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		rd.cpb += 2;
		_rfd.p = ps + 2;
		_rfd.f = &Deserializer::parseBinary<6>;
	}else if(len >= 1){
		*(ps + 0) = *(rd.cpb + 0);
		rd.cpb += 1;
		_rfd.p = ps + 1;
		_rfd.f = &Deserializer::parseBinary<7>;
	}
	return NOK;
}


template <>
int Deserializer::parse<int8>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, "");
	_rfd.s = sizeof(int8);
	_rfd.f = &Deserializer::parseBinary<1>;
	return parseBinary<1>(_rb, _rfd);
}
template <>
int Deserializer::parse<uint8>(Base &_rb, FncData &_rfd){	
	idbgx(Dbg::ser_bin, "");
	_rfd.s = sizeof(uint8);
	_rfd.f = &Deserializer::parseBinary<1>;
	return parseBinary<1>(_rb, _rfd);
}
template <>
int Deserializer::parse<int16>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, "");
	_rfd.s = sizeof(int16);
	_rfd.f = &Deserializer::parseBinary<2>;
	return parseBinary<2>(_rb, _rfd);
}
template <>
int Deserializer::parse<uint16>(Base &_rb, FncData &_rfd){	
	idbgx(Dbg::ser_bin, "");
	_rfd.s = sizeof(uint16);
	_rfd.f = &Deserializer::parseBinary<2>;
	return parseBinary<2>(_rb, _rfd);
}
template <>
int Deserializer::parse<int32>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, "");
	_rfd.s = sizeof(int32);
	_rfd.f = &Deserializer::parseBinary<4>;
	return parseBinary<4>(_rb, _rfd);
}
template <>
int Deserializer::parse<uint32>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, "");
	_rfd.s = sizeof(uint32);
	_rfd.f = &Deserializer::parseBinary<4>;
	return parseBinary<4>(_rb, _rfd);
}

template <>
int Deserializer::parse<int64>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, "");
	_rfd.s = sizeof(int64);
	_rfd.f = &Deserializer::parseBinary<8>;
	return parseBinary<8>(_rb, _rfd);
}
template <>
int Deserializer::parse<uint64>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, "");
	_rfd.s = sizeof(uint64);
	_rfd.f = &Deserializer::parseBinary<8>;
	return parseBinary<8>(_rb, _rfd);
}
/*template <>
int Deserializer::parse<ulong>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, "");
	_rfd.s = sizeof(ulong);
	_rfd.f = &Deserializer::parseBinary;
	return CONTINUE;
}*/
template <>
int Deserializer::parse<std::string>(Base &_rb, FncData &_rfd){
	idbgx(Dbg::ser_bin, "parse generic non pointer string");
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	if(!rd.cpb) return OK;
	rd.estk.push(ExtData((int32)0));
	rd.replace(FncData(&Deserializer::parseBinaryString, _rfd.p, _rfd.n));
	rd.fstk.push(FncData(&Deserializer::parseBinaryStringCheck, NULL, _rfd.n));
	rd.fstk.push(FncData(&Deserializer::parse<int32>, &rd.estk.top().i32()));
	return CONTINUE;
}
int Deserializer::parseBinaryStringCheck(Base &_rb, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	if(!rd.cpb) return OK;
	const int32 len = rd.estk.top().i32();
	if(len != -1){
		const CRCValue<uint32> crcsz(CRCValue<uint32>::check_and_create((uint32)len));
		if(crcsz.ok()){
			rd.estk.top().u32() = crcsz.value();
		}else{
			rd.err = ERR_STRING_MAX_LIMIT;
			return BAD;
		}
	}
	int32 ul = rd.estk.top().i32();
	
	if(ul > 0 && rd.limits.stringlimit && ul >= static_cast<int32>(rd.limits.stringlimit)){
		idbgx(Dbg::ser_bin, "error");
		rd.err = ERR_STRING_LIMIT;
		return BAD;
	}
	return OK;
}
int Deserializer::parseBinaryString(Base &_rb, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	idbgx(Dbg::ser_bin, "");
	if(!rd.cpb){
		rd.estk.pop();
		return OK;
	}
	
	uint32 len = rd.be - rd.cpb;
	int32 ul = rd.estk.top().i32();
	
	if(ul < 0){
		return OK;
	}
	if(static_cast<int32>(len) > ul) len = static_cast<uint32>(ul);
	std::string *ps = reinterpret_cast<std::string*>(_rfd.p);
	ps->append(rd.cpb, len);
	rd.cpb += len;
	ul -= len;
	if(ul){
		rd.estk.top().i32() = ul;
		return NOK;
	}
	rd.estk.pop();
	return OK;
}

int Deserializer::parseStreamCheck(Base &_rb, FncData &_rfd){
	Deserializer	&rd(static_cast<Deserializer&>(_rb));
	
	if(!rd.cpb) return OK;
	
	OutputStreamData		&rsp(*reinterpret_cast<OutputStreamData*>(rd.estk.top().buf));
	if(rsp.sz == -1LL){
		rd.fstk.pop();//skip parse stream
		return OK;
	}
	{
		const CRCValue<uint64> crcsz(CRCValue<uint64>::check_and_create((uint64)rsp.sz));
		if(crcsz.ok()){
			rsp.sz = crcsz.value();
		}else{
			idbgx(Dbg::ser_bin, "error");
			rd.err = ERR_STREAM_MAX_LIMIT;
			return BAD;
		}
	}
	if(rd.limits.streamlimit && rsp.sz > static_cast<int64>(rd.limits.streamlimit)){
		idbgx(Dbg::ser_bin, "error");
		rd.err = ERR_STREAM_LIMIT;
		return BAD;
	}
	return OK;
}

int Deserializer::parseStream(Base &_rb, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	idbgx(Dbg::ser_bin, "");
	if(!rd.cpb) return OK;
	OutputStreamData &rsp(*reinterpret_cast<OutputStreamData*>(rd.estk.top().buf));
	idbgx(Dbg::ser_bin, "rsp.sz = "<<rsp.sz);
	if(rsp.sz < 0) return OK;
	
	int32 towrite = rd.be - rd.cpb;
	if(towrite < 2){
		cassert(towrite == 0);
		return NOK;
	}
	towrite -= 2;
	if(towrite > rsp.sz) towrite = static_cast<int32>(rsp.sz);
	uint16 sz(0);
	rd.cpb = parseValue(rd.cpb, sz);
	idbgx(Dbg::ser_bin, "sz = "<<sz);
	if(sz == 0xffff){//error on storing side - the stream is incomplete
		idbgx(Dbg::ser_bin, "error on storing side");
		rsp.sz = -1;
		return OK;
	}else{
		CRCValue<uint16> crcsz(CRCValue<uint16>::check_and_create(sz));
		if(crcsz.ok() && crcsz.value() == towrite){
			sz = crcsz.value();
		}else{
			rd.err = ERR_STREAM_CHUNK_MAX_LIMIT;
			idbgx(Dbg::ser_bin, "crcval = "<<crcsz.value()<<" towrite = "<<towrite);
			cassert(0);
			return BAD;
		}
	}
	idbgx(Dbg::ser_bin, "towrite = "<<towrite);
	cassert(sz <= rsp.sz);
	int rv = rsp.pos->write(rsp.off, rd.cpb, towrite);
	rd.cpb += towrite;
	rsp.sz -= towrite;
	rsp.off += towrite;
	idbgx(Dbg::ser_bin, "rsp.sz = "<<rsp.sz);
	if(rv != towrite){
		_rfd.f = &parseDummyStream;
	}
	if(rsp.sz) return NOK;
	return OK;
}
int Deserializer::parseDummyStream(Base &_rb, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	idbgx(Dbg::ser_bin, "");
	if(!rd.cpb) return OK;
	OutputStreamData &rsp(*reinterpret_cast<OutputStreamData*>(rd.estk.top().buf));
	if(rsp.sz < 0) return OK;
	int32 towrite = rd.be - rd.cpb;
	cassert(towrite > 2);
	towrite -= 2;
	if(towrite > rsp.sz) towrite = static_cast<int32>(rsp.sz);
	const uint16 rsz = *((uint16*)rd.cpb);
	rd.cpb += 2;
	if(rsz == 0xffff){//error on storing side - the stream is incomplete
		rsp.sz = -1;
		return OK;
	}
	rd.cpb += towrite;
	rsp.sz -= towrite;
	if(rsp.sz) return NOK;
	rsp.sz = -1;//the write was not complete
	return OK;
}
int Deserializer::parseUtf8(Base &_rb, FncData &_rfd){
	Deserializer	&rd(static_cast<Deserializer&>(_rb));
	idbgx(Dbg::ser_bin, "");
	std::string		*ps = reinterpret_cast<std::string*>(_rfd.p);
	unsigned		len = rd.be - rd.cpb;
	size_t			slen = cstring::nlen(rd.cpb, len);
	size_t			totlen = ps->size() + slen;
	if(rd.limits.stringlimit && totlen > rd.limits.stringlimit){
		rd.err = ERR_UTF8_LIMIT;
		return BAD;
	}
	if(totlen > CRCValue<uint32>::maximum()){
		rd.err = ERR_UTF8_MAX_LIMIT;
		return BAD;
	}
	ps->append(rd.cpb, slen);
	rd.cpb += slen;
	if(slen == len){
		return NOK;
	}
	++rd.cpb;
	return OK;
}
Deserializer& Deserializer::pushBinary(void *_p, size_t _sz, const char *_name){
	fstk.push(FncData(&Deserializer::parseBinary<0>, _p, _name, _sz));
	return *this;
}
Deserializer& Deserializer::pushUtf8(std::string& _str, const char *_name){
	_str.clear();
	fstk.push(FncData(&Deserializer::parseUtf8, &_str, _name, 0));
	return *this;
}
//========================================================================
//========================================================================
}//namespace binary
}//namespace serialization


