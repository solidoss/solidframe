// serialization/src/binary.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "serialization/binary.hpp"
#include "serialization/binarybasic.hpp"
#include "utility/ostream.hpp"
#include "utility/istream.hpp"
#include "system/cstring.hpp"
#include <cstring>

namespace solid{
namespace serialization{
namespace binary{

#ifdef HAS_SAFE_STATIC
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
		case ERR_STREAM_CHUNK_MAX_LIMIT:
			return "Stream chunk max limit";
		case ERR_STREAM_SEEK:
			return "Stream seek";
		case ERR_STREAM_READ:
			return "Stream read";
		case ERR_STREAM_WRITE:
			return "Stream write";
		case ERR_STREAM_SENDER:
			return "Stream sender";
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
		case ERR_REINIT:
			return "Reinit error";
		default:
			return "Unknown error";
	};
}
/*static*/ int Base::setStringLimit(Base& _rb, FncData &_rfd, void */*_pctx*/){
	_rb.lmts.stringlimit = static_cast<size_t>(_rfd.s);
	return OK;
}
/*static*/ int Base::setStreamLimit(Base& _rb, FncData &_rfd, void */*_pctx*/){
	_rb.lmts.streamlimit = _rfd.s;
	return OK;
}
/*static*/ int Base::setContainerLimit(Base& _rb, FncData &_rfd, void */*_pctx*/){
	_rb.lmts.containerlimit = static_cast<size_t>(_rfd.s);
	return OK;
}
void Base::replace(const FncData &_rfd){
	fstk.top() = _rfd;
}
int Base::popEStack(Base &_rb, FncData &, void */*_pctx*/){
	_rb.estk.pop();
	return OK;
}
//========================================================================
/*static*/ char* SerializerBase::storeValue(char *_pd, const uint8  _val){
	return serialization::binary::store(_pd, _val);
}
/*static*/ char* SerializerBase::storeValue(char *_pd, const uint16 _val){
	return serialization::binary::store(_pd, _val);
}
/*static*/ char* SerializerBase::storeValue(char *_pd, const uint32 _val){
	return serialization::binary::store(_pd, _val);
}
/*static*/ char* SerializerBase::storeValue(char *_pd, const uint64 _val){
	return serialization::binary::store(_pd, _val);
}
SerializerBase::~SerializerBase(){
}
void SerializerBase::clear(){
	run(NULL, 0, NULL);
}
void SerializerBase::doPushStringLimit(){
	fstk.push(FncData(&Base::setStringLimit, 0, 0, rdefaultlmts.stringlimit));
}
void SerializerBase::doPushStringLimit(size_t _v){
	fstk.push(FncData(&Base::setStringLimit, 0, 0, _v));
}
void SerializerBase::doPushStreamLimit(){
	fstk.push(FncData(&Base::setStreamLimit, 0, 0, rdefaultlmts.streamlimit));
}
void SerializerBase::doPushStreamLimit(uint64 _v){
	fstk.push(FncData(&Base::setStreamLimit, 0, 0, _v));
}
void SerializerBase::doPushContainerLimit(){
	fstk.push(FncData(&Base::setContainerLimit, 0, 0, rdefaultlmts.containerlimit));
}
void SerializerBase::doPushContainerLimit(size_t _v){
	fstk.push(FncData(&Base::setContainerLimit, 0, 0, _v));
}

int SerializerBase::run(char *_pb, unsigned _bl, void *_pctx){
	cpb = pb = _pb;
	be = cpb + _bl;
	while(fstk.size()){
		FncData &rfd = fstk.top();
		switch((*reinterpret_cast<FncT>(rfd.f))(*this, rfd, _pctx)) {
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
	cassert(fstk.size() || fstk.empty() && estk.empty());
	return cpb - pb;
}

template <>
int SerializerBase::storeBinary<0>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, ""<<_rfd.s);
	SerializerBase &rs(static_cast<SerializerBase&>(_rb));
	if(!rs.cpb) return OK;
	uint32 len = rs.be - rs.cpb;
	if(len > _rfd.s) len = static_cast<uint32>(_rfd.s);
	memcpy(rs.cpb, _rfd.p, len);
	rs.cpb += len;
	_rfd.p = (char*)_rfd.p + len;
	_rfd.s -= len;
	idbgx(Debug::ser_bin, ""<<len);
	if(_rfd.s) return NOK;
	return OK;
}

template <>
int SerializerBase::storeBinary<1>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, "");
	SerializerBase &rs(static_cast<SerializerBase&>(_rb));
	
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
int SerializerBase::storeBinary<2>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, "");
	SerializerBase &rs(static_cast<SerializerBase&>(_rb));
	
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
		_rfd.f = &SerializerBase::storeBinary<1>;
		_rfd.p = const_cast<char*>(ps + 1);
		rs.cpb += 1;
	}
	return NOK;
}

template <>
int SerializerBase::storeBinary<4>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, "");
	SerializerBase &rs(static_cast<SerializerBase&>(_rb));
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
		_rfd.f = &SerializerBase::storeBinary<1>;
		rs.cpb += 3;
		return NOK;
	}else if(len >= 2){
		*(rs.cpb + 0) = *(ps + 0);
		*(rs.cpb + 1) = *(ps + 1);
		_rfd.p = const_cast<char*>(ps + 2);
		rs.cpb += 2;
		_rfd.f = &SerializerBase::storeBinary<2>;
		return NOK;
	}else if(len >= 1){
		*(rs.cpb + 0) = *(ps + 0);
		_rfd.p = const_cast<char*>(ps + 1);
		rs.cpb += 1;
		_rfd.s = 3;
	}else{
		_rfd.s = 4;
	}
	_rfd.f = &SerializerBase::storeBinary<0>;
	return NOK;
}


template <>
int SerializerBase::storeBinary<8>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, "");
	SerializerBase &rs(static_cast<SerializerBase&>(_rb));
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
		_rfd.f = &SerializerBase::storeBinary<1>;
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
		_rfd.f = &SerializerBase::storeBinary<2>;
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
		_rfd.s = 3;
	}else if(len >= 4){
		*(rs.cpb + 0) = *(ps + 0);
		*(rs.cpb + 1) = *(ps + 1);
		*(rs.cpb + 2) = *(ps + 2);
		*(rs.cpb + 3) = *(ps + 3);
		_rfd.p = const_cast<char*>(ps + 4);
		_rfd.f = &SerializerBase::storeBinary<4>;
		rs.cpb += 4;
		return NOK;
	}else if(len >= 3){
		*(rs.cpb + 0) = *(ps + 0);
		*(rs.cpb + 1) = *(ps + 1);
		*(rs.cpb + 2) = *(ps + 2);
		_rfd.p = const_cast<char*>(ps + 3);
		rs.cpb += 3;
		_rfd.s = 5;
	}else if(len >= 2){
		*(rs.cpb + 0) = *(ps + 0);
		*(rs.cpb + 1) = *(ps + 1);
		_rfd.p = const_cast<char*>(ps + 2);
		rs.cpb += 2;
		_rfd.s = 6;
	}else if(len >= 1){
		*(rs.cpb + 0) = *(ps + 0);
		_rfd.p = const_cast<char*>(ps + 1);
		rs.cpb += 1;
		_rfd.s = 7;
	}else{
		_rfd.s = 8;
	}
	_rfd.f = &SerializerBase::storeBinary<0>;
	return NOK;
}


template <>
int SerializerBase::store<int8>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, ""<<_rfd.n);
	_rfd.s = sizeof(int8);
	_rfd.f = &SerializerBase::storeBinary<1>;
	return storeBinary<1>(_rb, _rfd, NULL);
}
template <>
int SerializerBase::store<uint8>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, ""<<_rfd.n);
	_rfd.s = sizeof(uint8);
	_rfd.f = &SerializerBase::storeBinary<1>;
	return storeBinary<1>(_rb, _rfd, NULL);
}
template <>
int SerializerBase::store<int16>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, ""<<_rfd.n);
	_rfd.s = sizeof(int16);
	_rfd.f = &SerializerBase::storeBinary<2>;
	return storeBinary<2>(_rb, _rfd, NULL);
}
template <>
int SerializerBase::store<uint16>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, ""<<_rfd.n);
	_rfd.s = sizeof(uint16);
	_rfd.f = &SerializerBase::storeBinary<2>;
	return storeBinary<2>(_rb, _rfd, NULL);
}
template <>
int SerializerBase::store<int32>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, ""<<_rfd.n);
	_rfd.s = sizeof(int32);
	_rfd.f = &SerializerBase::storeBinary<4>;
	return storeBinary<4>(_rb, _rfd, NULL);
}
template <>
int SerializerBase::store<uint32>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, ""<<_rfd.n);
	_rfd.s = sizeof(uint32);
	_rfd.f = &SerializerBase::storeBinary<4>;
	return storeBinary<4>(_rb, _rfd, NULL);
}
template <>
int SerializerBase::store<int64>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, "");
	_rfd.s = sizeof(int64);
	_rfd.f = &SerializerBase::storeBinary<8>;
	return storeBinary<8>(_rb, _rfd, NULL);
}
template <>
int SerializerBase::store<uint64>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, ""<<_rfd.n<<*((uint64*)_rfd.p));
	_rfd.s = sizeof(uint64);
	_rfd.f = &SerializerBase::storeBinary<8>;
	return storeBinary<8>(_rb, _rfd, NULL);
}
/*template <>
int SerializerBase::store<ulong>(Base &_rb, FncData &_rfd){
	idbgx(Debug::ser_bin, "");
	_rfd.s = sizeof(ulong);
	_rfd.f = &SerializerBase::storeBinary;
	return CONTINUE;
}*/
template <>
int SerializerBase::store<std::string>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	SerializerBase &rs(static_cast<SerializerBase&>(_rb));
	idbgx(Debug::ser_bin, "");
	if(!rs.cpb) return OK;
	std::string * c = reinterpret_cast<std::string*>(_rfd.p);
	if(c->size() > rs.lmts.stringlimit){
		rs.err = ERR_STRING_LIMIT;
		return BAD;
	}
	if(c->size() > CRCValue<uint64>::maximum()){
		rs.err = ERR_STRING_MAX_LIMIT;
		return BAD;
	}
	const CRCValue<uint64> crcsz((uint64)c->size());
	
	rs.estk.push(ExtData((uint64)crcsz));
	
	rs.replace(FncData(&SerializerBase::storeBinary<0>, (void*)c->data(), _rfd.n, c->size()));
	rs.fstk.push(FncData(&Base::popEStack, NULL, _rfd.n));
	rs.fstk.push(FncData(&SerializerBase::storeCross<uint64>, &rs.estk.top().u64(), _rfd.n));
	return CONTINUE;
}
int SerializerBase::storeStreamBegin(Base &_rb, FncData &_rfd, void */*_pctx*/){
	SerializerBase &rs(static_cast<SerializerBase&>(_rb));
	if(!rs.cpb) return OK;
	int32		toread = rs.be - rs.cpb;
	
	if(toread < MINSTREAMBUFLEN) return NOK;
	
	rs.streamerr = 0;
	rs.streamsz = 0;
	if(_rfd.p == NULL){
		rs.cpb = storeValue(rs.cpb, (uint16)0xffff);
		rs.pop();//returning ok will also pop storeStream
		return OK;
	}
	if(_rfd.s != -1ULL){
		if(
			static_cast<int64>(_rfd.s) != 
			reinterpret_cast<InputStream*>(_rfd.p)->seek(_rfd.s)
		){
			rs.streamerr = ERR_STREAM_SEEK;
			rs.cpb = storeValue(rs.cpb, (uint16)0xffff);
			rs.pop();//returning ok will also pop storeStream
		}
	}
	return OK;
}
int SerializerBase::storeStreamCheck(Base &_rb, FncData &_rfd, void */*_pctx*/){
	SerializerBase &rs(static_cast<SerializerBase&>(_rb));
	if(!rs.cpb) return OK;
	if(_rfd.s > rs.lmts.streamlimit){
		rs.streamerr = rs.err = ERR_STREAM_LIMIT;
		return BAD;
	}
	return OK;
}
int SerializerBase::storeStream(Base &_rb, FncData &_rfd, void */*_pctx*/){
	SerializerBase	&rs(static_cast<SerializerBase&>(_rb));
	
	idbgx(Debug::ser_bin, "");
	if(!rs.cpb) return OK;
	
	int32		toread = rs.be - rs.cpb;
	
	if(toread < MINSTREAMBUFLEN) return NOK;
	
	toread -= 2;//the buffsize
	
	if(toread > _rfd.s){
		toread = static_cast<int32>(_rfd.s);
	}
	
	if(toread > CRCValue<uint16>::maximum()){
		toread = CRCValue<uint16>::maximum();
	}
	
	if(toread == 0){
		rs.cpb = storeValue(rs.cpb, (uint16)0);
		return OK;
	}
	
	int rv = reinterpret_cast<InputStream*>(_rfd.p)->read(rs.cpb + 2, toread);
	
	idbgx(Debug::ser_bin, "toread = "<<toread<<" rv = "<<rv);
	
	if(rv > 0){
		
		if((rs.streamsz + rv) > rs.lmts.streamlimit){
			rs.streamerr = rs.err = ERR_STREAM_LIMIT;
			idbgx(Debug::ser_bin, "ERR_STREAM_LIMIT");
			return BAD;
		}
		
		toread = rv;
		
		const CRCValue<uint16> crcsz((uint16)toread);
		
		storeValue(rs.cpb, (uint16)crcsz);
		idbgx(Debug::ser_bin, "store crcsz = "<<crcsz<<" sz = "<<toread);
		
		idbgx(Debug::ser_bin, "store value "<<(uint16)crcsz);
			  
		rs.cpb += toread + 2;
		rs.streamsz += toread;
	}else if(rv == 0){
		idbgx(Debug::ser_bin, "done storing stream");
		rs.cpb = storeValue(rs.cpb, (uint16)0);
		return OK;
	}else{
		rs.streamerr = ERR_STREAM_READ;
		idbgx(Debug::ser_bin, "ERR_STREAM_READ");
		rs.cpb = storeValue(rs.cpb, (uint16)0xffff);
		return OK;
	}
	
	if(_rfd.s != -1ULL){
		_rfd.s -= toread;
		if(_rfd.s == 0){
			return CONTINUE;
		}
	}
	idbgx(Debug::ser_bin, "streamsz = "<<rs.streamsz);
	return CONTINUE;
}
/*static*/ int SerializerBase::storeUtf8(Base &_rs, FncData &_rfd, void */*_pctx*/){
	SerializerBase &rs(static_cast<SerializerBase&>(_rs));
	if((_rfd.s - 1) > rs.lmts.stringlimit){
		rs.err = ERR_UTF8_LIMIT;
		return BAD;
	}
	if((_rfd.s - 1) > CRCValue<uint32>::maximum()){
		rs.err = ERR_UTF8_MAX_LIMIT;
		return BAD;
	}
	_rfd.f = &SerializerBase::storeBinary<0>;
	return CONTINUE;
}

/*static*/ int SerializerBase::storeCrossContinue(Base &_rs, FncData &_rfd, void */*_pctx*/){
	SerializerBase	&rs(static_cast<SerializerBase&>(_rs));
	if(!rs.cpb) return OK;
	uint32		&vsz = *reinterpret_cast<uint32*>(&_rfd.p);
	uint8		*pd = reinterpret_cast<uint8*>(rs.cpb);
	while(rs.cpb != rs.be && vsz){
		*pd = (_rfd.s & 0xff);
		idbgx(Debug::ser_bin, "s = "<<_rfd.s<<" vsz = "<<vsz<<" *pd = "<<(int)*pd);
		++pd;
		++rs.cpb;
		if(vsz == 1){
			_rfd.s >>= 4;
		}else{
			_rfd.s >>= 8;
		}
		--vsz;
	}
	if(vsz == 0){
		return OK;
	}
	_rfd.p = reinterpret_cast<void*>(vsz);
	return NOK;
}

template <>
/*static*/ int SerializerBase::storeCross<uint8>(Base &_rs, FncData &_rfd, void */*_pctx*/){
	SerializerBase	&rs(static_cast<SerializerBase&>(_rs));
	
	if(!rs.cpb) return OK;
	
	const uint8		&v = *reinterpret_cast<const uint8*>(_rfd.p);
	const unsigned	len = rs.be - rs.cpb;
	const unsigned	vsz = crossSize(v);
	
	if(len >= vsz){
		rs.cpb = binary::storeCross(rs.cpb, v);
		return OK;
	}else if(len >= 1){
		uint8	*pd = reinterpret_cast<uint8*>(rs.cpb);
		*pd = ((v & 0xf) << 4) | (vsz - 1);
		_rfd.p = reinterpret_cast<void*>(vsz - 1);
		++rs.cpb;
		_rfd.s = v >> 4;
		_rfd.f = storeCrossContinue;
	}
	return NOK;
}
template <>
/*static*/ int SerializerBase::storeCross<uint16>(Base &_rs, FncData &_rfd, void */*_pctx*/){
	SerializerBase	&rs(static_cast<SerializerBase&>(_rs));
	
	if(!rs.cpb) return OK;
	
	const uint16	&v = *reinterpret_cast<const uint16*>(_rfd.p);
	const unsigned	len = rs.be - rs.cpb;
	const unsigned	vsz = crossSize(v);
	
	if(len >= vsz){
		rs.cpb = binary::storeCross(rs.cpb, v);
		return OK;
	}else if(len >= 1){
		uint8	*pd = reinterpret_cast<uint8*>(rs.cpb);
		*pd = ((v & 0xf) << 4) | (vsz - 1);
		_rfd.p = reinterpret_cast<void*>(vsz - 1);
		++rs.cpb;
		_rfd.s = v >> 4;
		_rfd.f = storeCrossContinue;
		return CONTINUE;
	}
	return NOK;
}
template <>
/*static*/ int SerializerBase::storeCross<uint32>(Base &_rs, FncData &_rfd, void */*_pctx*/){
	SerializerBase	&rs(static_cast<SerializerBase&>(_rs));
	
	if(!rs.cpb) return OK;
	
	const uint32	&v = *reinterpret_cast<const uint32*>(_rfd.p);
	const unsigned	len = rs.be - rs.cpb;
	const unsigned	vsz = crossSize(v);
	
	if(len >= vsz){
		rs.cpb = binary::storeCross(rs.cpb, v);
		return OK;
	}else if(len >= 1){
		uint8	*pd = reinterpret_cast<uint8*>(rs.cpb);
		*pd = ((v & 0xf) << 4) | (vsz - 1);
		_rfd.p = reinterpret_cast<void*>(vsz - 1);
		++rs.cpb;
		_rfd.s = v >> 4;
		_rfd.f = storeCrossContinue;
		return CONTINUE;
	}
	return NOK;
}
template <>
/*static*/ int SerializerBase::storeCross<uint64>(Base &_rs, FncData &_rfd, void */*_pctx*/){
	SerializerBase	&rs(static_cast<SerializerBase&>(_rs));
	
	if(!rs.cpb) return OK;
	
	const uint64	&v = *reinterpret_cast<const uint64*>(_rfd.p);
	const unsigned	len = rs.be - rs.cpb;
	const unsigned	vsz = crossSize(v);
	
	if(len >= vsz){
		rs.cpb = binary::storeCross(rs.cpb, v);
		return OK;
	}else if(len >= 1){
		uint8	*pd = reinterpret_cast<uint8*>(rs.cpb);
		++rs.cpb;
		*pd = ((v & 0xf) << 4) | (vsz - 1);
		_rfd.p = reinterpret_cast<void*>(vsz - 1);
		
		_rfd.s = v >> 4;
		_rfd.f = storeCrossContinue;
		return CONTINUE;
	}
	return NOK;
}
//========================================================================
//		Deserializer
//========================================================================

/*static*/ const char* DeserializerBase::loadValue(const char *_ps, uint8  &_val){
	return serialization::binary::load(_ps, _val);
}
/*static*/ const char* DeserializerBase::loadValue(const char *_ps, uint16 &_val){
	return serialization::binary::load(_ps, _val);
}
/*static*/ const char* DeserializerBase::loadValue(const char *_ps, uint32 &_val){
	return serialization::binary::load(_ps, _val);
}
/*static*/ const char* DeserializerBase::loadValue(const char *_ps, uint64 &_val){
	return serialization::binary::load(_ps, _val);
}
DeserializerBase::~DeserializerBase(){
}
void DeserializerBase::clear(){
	idbgx(Debug::ser_bin, "clear_deser");
	run(NULL, 0, NULL);
}

/*static*/ int DeserializerBase::loadTypeIdDone(Base& _rd, FncData &_rfd, void *_pctx){
	DeserializerBase	&rd(static_cast<DeserializerBase&>(_rd));
	void				*p = _rfd.p;
	const char			*n = _rfd.n;
	
	if(!rd.cpb) return OK;
	
	rd.fstk.pop();
	
	if(rd.typeMapper().prepareParsePointer(&rd, rd.tmpstr, p, n, _pctx)){
		return CONTINUE;
	}else{
		idbgx(Debug::ser_bin, "error");
		rd.err = ERR_POINTER_UNKNOWN;
		return BAD;
	}
}
/*static*/ int DeserializerBase::loadTypeId(Base& _rd, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rd));
	
	if(!rd.cpb) return OK;
	
	rd.typeMapper().prepareParsePointerId(&rd, rd.tmpstr, _rfd.n);
	_rfd.f = &loadTypeIdDone;
	return CONTINUE;
}

void DeserializerBase::doPushStringLimit(){
	fstk.push(FncData(&Base::setStringLimit, 0, 0, rdefaultlmts.stringlimit));
}
void DeserializerBase::doPushStringLimit(size_t _v){
	fstk.push(FncData(&Base::setStringLimit, 0, 0, _v));
}
void DeserializerBase::doPushStreamLimit(){
	fstk.push(FncData(&Base::setStreamLimit, 0, 0, rdefaultlmts.streamlimit));
}
void DeserializerBase::doPushStreamLimit(uint64 _v){
	fstk.push(FncData(&Base::setStreamLimit, 0, 0, _v));
}
void DeserializerBase::doPushContainerLimit(){
	fstk.push(FncData(&Base::setContainerLimit, 0, 0, rdefaultlmts.containerlimit));
}
void DeserializerBase::doPushContainerLimit(size_t _v){
	fstk.push(FncData(&Base::setContainerLimit, 0, 0, _v));
}

int DeserializerBase::run(const char *_pb, unsigned _bl, void *_pctx){
	cpb = pb = _pb;
	be = pb + _bl;
	while(fstk.size()){
		FncData &rfd = fstk.top();
		switch((*reinterpret_cast<FncT>(rfd.f))(*this, rfd, _pctx)){
			case CONTINUE: continue;
			case OK: fstk.pop(); break;
			case NOK: goto Done;
			case BAD:
				idbgx(Debug::ser_bin, "error: "<<errorString());
				resetLimits();
				return -1;
		}
	}
	resetLimits();
	Done:
	cassert(fstk.size() || fstk.empty() && estk.empty());
	return cpb - pb;
}
template <>
int DeserializerBase::loadBinary<0>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rb));
	idbgx(Debug::ser_bin, ""<<_rfd.s);
	if(!rd.cpb) return OK;
	uint32 len = rd.be - rd.cpb;
	if(len > _rfd.s) len = static_cast<uint32>(_rfd.s);
	memcpy(_rfd.p, rd.cpb, len);
	rd.cpb += len;
	_rfd.p = (char*)_rfd.p + len;
	_rfd.s -= len;
	if(len == 1){
		edbgx(Debug::ser_bin, "");
	}
	
	idbgx(Debug::ser_bin, ""<<len);
	if(_rfd.s) return NOK;
	return OK;
}

template <>
int DeserializerBase::loadBinary<1>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rb));
	idbgx(Debug::ser_bin, "");
	if(!rd.cpb) return OK;
	const unsigned	len = rd.be - rd.cpb;
	char			*ps = reinterpret_cast<char*>(_rfd.p);
	idbgx(Debug::ser_bin, ""<<len<<' '<<(void*)rd.cpb);
	if(len >= 1){
		*(ps + 0) = *(rd.cpb + 0);
		rd.cpb += 1;
		return OK;
	}	
	return NOK;
}


template <>
int DeserializerBase::loadBinary<2>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rb));
	idbgx(Debug::ser_bin, "");
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
		_rfd.f = &DeserializerBase::loadBinary<1>;
	}
	return NOK;
}

template <>
int DeserializerBase::loadBinary<3>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rb));
	idbgx(Debug::ser_bin, "");
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
		_rfd.f = &DeserializerBase::loadBinary<1>;
	}else if(len >= 1){
		*(ps + 0) = *(rd.cpb + 0);
		rd.cpb += 1;
		_rfd.p = ps + 1;
		_rfd.f = &DeserializerBase::loadBinary<2>;
	}
	return NOK;
}

template <>
int DeserializerBase::loadBinary<4>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rb));
	idbgx(Debug::ser_bin, "");
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
		edbgx(Debug::ser_bin, ""<<len<<' '<<(void*)rd.cpb);
		_rfd.f = &DeserializerBase::loadBinary<1>;
	}else if(len >= 2){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		rd.cpb += 2;
		_rfd.p = ps + 2;
		_rfd.f = &DeserializerBase::loadBinary<2>;
	}else if(len >= 1){
		*(ps + 0) = *(rd.cpb + 0);
		rd.cpb += 1;
		_rfd.p = ps + 1;
		_rfd.f = &DeserializerBase::loadBinary<3>;
	}
	return NOK;
}

template <>
int DeserializerBase::loadBinary<5>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rb));
	idbgx(Debug::ser_bin, "");
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
		_rfd.f = &DeserializerBase::loadBinary<1>;
	}else if(len >= 3){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		rd.cpb += 3;
		_rfd.p = ps + 3;
		_rfd.f = &DeserializerBase::loadBinary<2>;
	}else if(len >= 2){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		rd.cpb += 2;
		_rfd.p = ps + 2;
		_rfd.f = &DeserializerBase::loadBinary<3>;
	}else if(len >= 1){
		*(ps + 0) = *(rd.cpb + 0);
		rd.cpb += 1;
		_rfd.p = ps + 1;
		_rfd.f = &DeserializerBase::loadBinary<4>;
	}
	return NOK;
}

template <>
int DeserializerBase::loadBinary<6>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rb));
	idbgx(Debug::ser_bin, "");
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
		_rfd.f = &DeserializerBase::loadBinary<1>;
	}else if(len >= 4){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		rd.cpb += 4;
		_rfd.p = ps + 4;
		_rfd.f = &DeserializerBase::loadBinary<2>;
	}else if(len >= 3){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		rd.cpb += 3;
		_rfd.p = ps + 3;
		_rfd.f = &DeserializerBase::loadBinary<3>;
	}else if(len >= 2){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		rd.cpb += 2;
		_rfd.p = ps + 2;
		_rfd.f = &DeserializerBase::loadBinary<4>;
	}else if(len >= 1){
		*(ps + 0) = *(rd.cpb + 0);
		rd.cpb += 1;
		_rfd.p = ps + 1;
		_rfd.f = &DeserializerBase::loadBinary<5>;
	}
	return NOK;
}

template <>
int DeserializerBase::loadBinary<7>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rb));
	idbgx(Debug::ser_bin, "");
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
		_rfd.f = &DeserializerBase::loadBinary<1>;
	}else if(len >= 5){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		*(ps + 4) = *(rd.cpb + 4);
		rd.cpb += 5;
		_rfd.p = ps + 5;
		_rfd.f = &DeserializerBase::loadBinary<2>;
	}else if(len >= 4){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		rd.cpb += 4;
		_rfd.p = ps + 4;
		_rfd.f = &DeserializerBase::loadBinary<3>;
	}else if(len >= 3){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		rd.cpb += 3;
		_rfd.p = ps + 3;
		_rfd.f = &DeserializerBase::loadBinary<4>;
	}else if(len >= 2){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		rd.cpb += 2;
		_rfd.p = ps + 2;
		_rfd.f = &DeserializerBase::loadBinary<5>;
	}else if(len >= 1){
		*(ps + 0) = *(rd.cpb + 0);
		rd.cpb += 1;
		_rfd.p = ps + 1;
		_rfd.f = &DeserializerBase::loadBinary<6>;
	}
	return NOK;
}

template <>
int DeserializerBase::loadBinary<8>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rb));
	idbgx(Debug::ser_bin, "");
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
		_rfd.f = &DeserializerBase::loadBinary<1>;
	}else if(len >= 6){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		*(ps + 4) = *(rd.cpb + 4);
		*(ps + 5) = *(rd.cpb + 5);
		rd.cpb += 6;
		_rfd.p = ps + 6;
		_rfd.f = &DeserializerBase::loadBinary<2>;
	}else if(len >= 5){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		*(ps + 4) = *(rd.cpb + 4);
		rd.cpb += 5;
		_rfd.p = ps + 5;
		_rfd.f = &DeserializerBase::loadBinary<3>;
	}else if(len >= 4){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		*(ps + 3) = *(rd.cpb + 3);
		rd.cpb += 4;
		_rfd.p = ps + 4;
		_rfd.f = &DeserializerBase::loadBinary<4>;
	}else if(len >= 3){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		*(ps + 2) = *(rd.cpb + 2);
		rd.cpb += 3;
		_rfd.p = ps + 3;
		_rfd.f = &DeserializerBase::loadBinary<5>;
	}else if(len >= 2){
		*(ps + 0) = *(rd.cpb + 0);
		*(ps + 1) = *(rd.cpb + 1);
		rd.cpb += 2;
		_rfd.p = ps + 2;
		_rfd.f = &DeserializerBase::loadBinary<6>;
	}else if(len >= 1){
		*(ps + 0) = *(rd.cpb + 0);
		rd.cpb += 1;
		_rfd.p = ps + 1;
		_rfd.f = &DeserializerBase::loadBinary<7>;
	}
	return NOK;
}


template <>
int DeserializerBase::load<int8>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, "");
	_rfd.s = sizeof(int8);
	_rfd.f = &DeserializerBase::loadBinary<1>;
	return loadBinary<1>(_rb, _rfd, NULL);
}
template <>
int DeserializerBase::load<uint8>(Base &_rb, FncData &_rfd, void */*_pctx*/){	
	idbgx(Debug::ser_bin, "");
	_rfd.s = sizeof(uint8);
	_rfd.f = &DeserializerBase::loadBinary<1>;
	return loadBinary<1>(_rb, _rfd, NULL);
}
template <>
int DeserializerBase::load<int16>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, "");
	_rfd.s = sizeof(int16);
	_rfd.f = &DeserializerBase::loadBinary<2>;
	return loadBinary<2>(_rb, _rfd, NULL);
}
template <>
int DeserializerBase::load<uint16>(Base &_rb, FncData &_rfd, void */*_pctx*/){	
	idbgx(Debug::ser_bin, "");
	_rfd.s = sizeof(uint16);
	_rfd.f = &DeserializerBase::loadBinary<2>;
	return loadBinary<2>(_rb, _rfd, NULL);
}
template <>
int DeserializerBase::load<int32>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, "");
	_rfd.s = sizeof(int32);
	_rfd.f = &DeserializerBase::loadBinary<4>;
	return loadBinary<4>(_rb, _rfd, NULL);
}
template <>
int DeserializerBase::load<uint32>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, "");
	_rfd.s = sizeof(uint32);
	_rfd.f = &DeserializerBase::loadBinary<4>;
	return loadBinary<4>(_rb, _rfd, NULL);
}

template <>
int DeserializerBase::load<int64>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, "");
	_rfd.s = sizeof(int64);
	_rfd.f = &DeserializerBase::loadBinary<8>;
	return loadBinary<8>(_rb, _rfd, NULL);
}
template <>
int DeserializerBase::load<uint64>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, "");
	_rfd.s = sizeof(uint64);
	_rfd.f = &DeserializerBase::loadBinary<8>;
	return loadBinary<8>(_rb, _rfd, NULL);
}
/*template <>
int DeserializerBase::load<ulong>(Base &_rb, FncData &_rfd){
	idbgx(Debug::ser_bin, "");
	_rfd.s = sizeof(ulong);
	_rfd.f = &DeserializerBase::loadBinary;
	return CONTINUE;
}*/
template <>
int DeserializerBase::load<std::string>(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, "load generic non pointer string");
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rb));
	if(!rd.cpb) return OK;
	rd.estk.push(ExtData((uint64)0));
	rd.replace(FncData(&DeserializerBase::loadBinaryString, _rfd.p, _rfd.n));
	rd.fstk.push(FncData(&DeserializerBase::loadBinaryStringCheck, NULL, _rfd.n));
	rd.fstk.push(FncData(&DeserializerBase::loadCross<uint64>, &rd.estk.top().u64()));
	return CONTINUE;
}
int DeserializerBase::loadBinaryStringCheck(Base &_rb, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rb));
	if(!rd.cpb) return OK;
	const uint64 len = rd.estk.top().u64();
	
	if(len != static_cast<uint64>(-1)){
		const CRCValue<uint64> crcsz(CRCValue<uint64>::check_and_create(len));
		if(crcsz.ok()){
			rd.estk.top().u64() = crcsz.value();
		}else{
			rd.err = ERR_STRING_MAX_LIMIT;
			return BAD;
		}
	}
	uint64 ul = rd.estk.top().u64();
	
	if(ul >= rd.lmts.stringlimit){
		idbgx(Debug::ser_bin, "error");
		rd.err = ERR_STRING_LIMIT;
		return BAD;
	}
	return OK;
}
int DeserializerBase::loadBinaryString(Base &_rb, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rb));
	idbgx(Debug::ser_bin, "");
	if(!rd.cpb){
		rd.estk.pop();
		return OK;
	}
	
	size_t			len = rd.be - rd.cpb;
	uint64			ul = rd.estk.top().u64();
	
	if(ul < 0){
		edbgx(Debug::ser_bin, "ul = "<<ul);
		return OK;
	}
	if(len > ul){
		len = static_cast<size_t>(ul);
	}
	
	std::string		*ps = reinterpret_cast<std::string*>(_rfd.p);
	
	ps->append(rd.cpb, len);
	rd.cpb += len;
	ul -= len;
	if(ul){
		rd.estk.top().u64() = ul;
		return NOK;
	}
	idbgx(Debug::ser_bin, ""<<*ps);
	rd.estk.pop();
	return OK;
}
int DeserializerBase::loadStreamCheck(Base &_rb, FncData &_rfd, void */*_pctx*/){
	DeserializerBase	&rd(static_cast<DeserializerBase&>(_rb));
	
	if(!rd.cpb) return OK;
	
	if(_rfd.s > static_cast<int64>(rd.lmts.streamlimit)){
		idbgx(Debug::ser_bin, "error");
		rd.err = ERR_STREAM_LIMIT;
		return BAD;
	}
	return OK;
}
int DeserializerBase::loadStreamBegin(Base &_rb, FncData &_rfd, void */*_pctx*/){
	DeserializerBase	&rd(static_cast<DeserializerBase&>(_rb));
	
	if(!rd.cpb) return OK;
	
	rd.streamerr = 0;
	rd.streamsz = 0;
	
	if(_rfd.p == NULL){
		rd.pop();
		rd.fstk.top().f = &DeserializerBase::loadDummyStream;
		rd.fstk.top().s = 0;
		return CONTINUE;
	}
	
	if(_rfd.s != -1ULL){
		if(
			static_cast<int64>(_rfd.s) != 
			reinterpret_cast<OutputStream*>(_rfd.p)->seek(_rfd.s)
		){
			rd.streamerr = ERR_STREAM_SEEK;
			rd.pop();
			rd.fstk.top().f = &DeserializerBase::loadDummyStream;
			rd.fstk.top().s = 0;
			return CONTINUE;
		}
	}
	return OK;
}

int DeserializerBase::loadStream(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, "");
	
	DeserializerBase	&rd(static_cast<DeserializerBase&>(_rb));
	
	if(!rd.cpb) return OK;
	
	int32			towrite = rd.be - rd.cpb;
	
	if(towrite < 2){
		return NOK;
	}
	towrite -= 2;
	
	
	if(towrite > _rfd.s){
		towrite = static_cast<int32>(_rfd.s);
	}
	
	uint16	sz(0);
	rd.cpb = loadValue(rd.cpb, sz);
	idbgx(Debug::ser_bin, "sz = "<<sz);
	
	if(sz == 0xffff){//error on storing side - the stream is incomplete
		idbgx(Debug::ser_bin, "error on storing side");
		rd.streamerr = ERR_STREAM_SENDER;
		return OK;
	}else{
		CRCValue<uint16> crcsz(CRCValue<uint16>::check_and_create(sz));
		if(crcsz.ok()){
			sz = crcsz.value();
		}else{
			rd.streamerr = rd.err = ERR_STREAM_CHUNK_MAX_LIMIT;
			idbgx(Debug::ser_bin, "crcval = "<<crcsz.value()<<" towrite = "<<towrite);
			return BAD;
		}
	}
	if(towrite > sz) towrite = sz;
	idbgx(Debug::ser_bin, "towrite = "<<towrite);
	if(towrite == 0){
		return OK;
	}
	
	if((rd.streamsz + towrite) > rd.lmts.streamlimit){
		idbgx(Debug::ser_bin, "ERR_STREAM_LIMIT");
		rd.streamerr = rd.err = ERR_STREAM_LIMIT;
		return BAD;
	}
	
	int rv = reinterpret_cast<OutputStream*>(_rfd.p)->write(rd.cpb, towrite);
	
	rd.cpb += sz;
	
	if(_rfd.s != -1ULL){
		_rfd.s -= towrite;
		idbgx(Debug::ser_bin, "_rfd.s = "<<_rfd.s);
		if(_rfd.s == 0){
			_rfd.f = &loadDummyStream;
			_rfd.s = rd.streamsz + sz;
		}
	}
	
	if(rv != towrite){
		rd.streamerr = ERR_STREAM_WRITE;
		_rfd.f = &loadDummyStream;
		_rfd.s = rd.streamsz + sz;
	}else{
		rd.streamsz += rv;
	}
	idbgx(Debug::ser_bin, "streamsz = "<<rd.streamsz);
	return CONTINUE;
}

int DeserializerBase::loadDummyStream(Base &_rb, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, "");
	
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rb));
	
	if(!rd.cpb) return OK;
	
	int32 towrite = rd.be - rd.cpb;
	if(towrite < 2){
		return NOK;
	}
	towrite -= 2;
	if(towrite > _rfd.s){
		towrite = static_cast<int32>(_rfd.s);
	}
	uint16	sz(0);
	rd.cpb = loadValue(rd.cpb, sz);
	idbgx(Debug::ser_bin, "sz = "<<sz);
	if(sz == 0xffff){//error on storing side - the stream is incomplete
		rd.streamerr = ERR_STREAM_SENDER;
		return OK;
	}else if(sz == 0){
		return OK;
	}else{
		CRCValue<uint16> crcsz(CRCValue<uint16>::check_and_create(sz));
		if(crcsz.ok()){
			sz = crcsz.value();
		}else{
			rd.streamerr = rd.err = ERR_STREAM_CHUNK_MAX_LIMIT;
			idbgx(Debug::ser_bin, "crcval = "<<crcsz.value()<<" towrite = "<<towrite);
			return BAD;
		}
	}
	rd.cpb += sz;
	_rfd.s += sz;
	if(_rfd.s > rd.lmts.streamlimit){
		idbgx(Debug::ser_bin, "ERR_STREAM_LIMIT");
		rd.streamerr = rd.err = ERR_STREAM_LIMIT;
		return BAD;
	}
	return CONTINUE;
}
int DeserializerBase::loadUtf8(Base &_rb, FncData &_rfd, void */*_pctx*/){
	DeserializerBase	&rd(static_cast<DeserializerBase&>(_rb));
	idbgx(Debug::ser_bin, "");
	std::string			*ps = reinterpret_cast<std::string*>(_rfd.p);
	unsigned			len = rd.be - rd.cpb;
	size_t				slen = cstring::nlen(rd.cpb, len);
	size_t				totlen = ps->size() + slen;
	idbgx(Debug::ser_bin, "len = "<<len);
	if(totlen > rd.lmts.stringlimit){
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

/*
 * _rfd.s contains the size of the value data
 * estk.top contains the uint64 value
 * _rfd.p contains the current index
 */
/*static*/ int DeserializerBase::loadCrossContinue(Base& _rd, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rd));
	if(!rd.cpb)	return OK;
	
	uint32			&rcrtidx = *reinterpret_cast<uint32*>(&_rfd.p);
	
	idbgx(Debug::ser_bin, ""<<rcrtidx);
	
	while(rd.cpb != rd.be && _rfd.s){
		const uint8		*ps = reinterpret_cast<const uint8*>(rd.cpb);
		uint64			tmp = *ps;
		idbgx(Debug::ser_bin, " v = "<<rd.estk.top().u64()<<" sz = "<<_rfd.s<<" ridx = "<<rcrtidx<<" *ps = "<<tmp);
		rd.estk.top().u64() |= (tmp << rcrtidx);
		rcrtidx += 8;
		--_rfd.s;
		++rd.cpb;
	}
	if(!_rfd.s){
		return OK;
	}
	return NOK;
}
template <>
/*static*/ int DeserializerBase::loadCross<uint8>(Base& _rd, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rd));
	idbgx(Debug::ser_bin, "");
	if(!rd.cpb) return OK;
	const unsigned	len = rd.be - rd.cpb;
	if(!len) return NOK;
	const unsigned	vsz = crossSize(rd.cpb);
	uint8			&v = *reinterpret_cast<uint8*>(_rfd.p);
	
	if(vsz < len){
		rd.cpb = binary::loadCross(rd.cpb, v);
		return OK;
	}
	const uint8		*ps = reinterpret_cast<const uint8*>(rd.cpb);
	++rd.cpb;
	rd.estk.push(ExtData((uint64)0));
	rd.estk.top().u64() = (*ps >> 4);
	_rfd.f = loadCrossContinue;
	_rfd.s = vsz;
	rd.fstk.push(_rfd);
	_rfd.f = loadCrossDone<uint8>;
	_rfd.p = reinterpret_cast<void*>(4);
	return CONTINUE;
}

template <>
/*static*/ int DeserializerBase::loadCross<uint16>(Base& _rd, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rd));
	idbgx(Debug::ser_bin, "");
	if(!rd.cpb) return OK;
	const unsigned	len = rd.be - rd.cpb;
	if(!len) return NOK;
	const unsigned	vsz = crossSize(rd.cpb);
	uint16			&v = *reinterpret_cast<uint16*>(_rfd.p);
	
	if(vsz < len){
		rd.cpb = binary::loadCross(rd.cpb, v);
		return OK;
	}
	const uint8		*ps = reinterpret_cast<const uint8*>(rd.cpb);
	++rd.cpb;
	rd.estk.push(ExtData((uint64)0));
	rd.estk.top().u64() = (*ps >> 4);
	_rfd.f = loadCrossContinue;
	_rfd.s = vsz;
	rd.fstk.push(_rfd);
	_rfd.f = loadCrossDone<uint16>;
	_rfd.p = reinterpret_cast<void*>(4);
	return CONTINUE;
}

template <>
/*static*/ int DeserializerBase::loadCross<uint32>(Base& _rd, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rd));
	idbgx(Debug::ser_bin, "");
	if(!rd.cpb) return OK;
	const unsigned	len = rd.be - rd.cpb;
	if(!len) return NOK;
	const unsigned	vsz = crossSize(rd.cpb);
	uint32			&v = *reinterpret_cast<uint32*>(_rfd.p);
	
	if(vsz < len){
		rd.cpb = binary::loadCross(rd.cpb, v);
		return OK;
	}
	const uint8		*ps = reinterpret_cast<const uint8*>(rd.cpb);
	++rd.cpb;
	rd.estk.push(ExtData((uint64)0));
	rd.estk.top().u64() = (*ps >> 4);
	_rfd.f = loadCrossContinue;
	_rfd.s = vsz;
	rd.fstk.push(_rfd);
	_rfd.f = loadCrossDone<uint32>;
	rd.fstk.top().p = reinterpret_cast<void*>(4);
	return CONTINUE;
}

template <>
/*static*/ int DeserializerBase::loadCross<uint64>(Base& _rd, FncData &_rfd, void */*_pctx*/){
	DeserializerBase &rd(static_cast<DeserializerBase&>(_rd));
	idbgx(Debug::ser_bin, "");
	if(!rd.cpb) return OK;
	const unsigned	len = rd.be - rd.cpb;
	if(!len) return NOK;
	const unsigned	vsz = crossSize(rd.cpb);
	uint64			&v = *reinterpret_cast<uint64*>(_rfd.p);
	
	if(vsz < len){
		rd.cpb = binary::loadCross(rd.cpb, v);
		idbgx(Debug::ser_bin, ""<<(rd.be - rd.cpb)<<" vsz = "<<vsz<<" len = "<<len);
		return OK;
	}
	const uint8		*ps = reinterpret_cast<const uint8*>(rd.cpb);
	++rd.cpb;
	rd.estk.push(ExtData((uint64)0));
	rd.estk.top().u64() = (*ps >> 4);
	_rfd.f = loadCrossContinue;
	_rfd.s = vsz;
	rd.fstk.push(_rfd);
	_rfd.f = loadCrossDone<uint64>;
	rd.fstk.top().p = reinterpret_cast<void*>(4);
	return CONTINUE;
}
//========================================================================
//========================================================================
}//namespace binary
}//namespace serialization
}//namespace solid

