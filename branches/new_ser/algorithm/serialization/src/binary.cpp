/* Implementation file binary.cpp
	
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

#include "serialization/binary.hpp"
#include "utility/ostream.hpp"
#include "utility/istream.hpp"
#ifdef UTHREADS
#include "system/synchronization.hpp"
#endif

namespace serialization{
namespace bin{
//========================================================================
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
int Serializer::run(char *_pb, unsigned _bl){
	cpb = pb = _pb;
	be = cpb + _bl;
	while(fstk.size()){
		FncData &rfd = fstk.top();
		switch((*reinterpret_cast<FncTp>(rfd.f))(*this, rfd)) {
			case CONTINUE: continue;
			case OK: fstk.pop(); break;
			case NOK: goto Done;
			case BAD: return -1;
		}
	}
	Done:
	return cpb - pb;
}
int Serializer::storeBinary(Base &_rb, FncData &_rfd){
	Serializer &rs(static_cast<Serializer&>(_rb));
	if(!rs.cpb) return OK;
	unsigned len = rs.be - rs.cpb;
	if(len > _rfd.s) len = _rfd.s;
	memcpy(rs.cpb, _rfd.p, len);
	rs.cpb += len;
	_rfd.p = (char*)_rfd.p + len;
	_rfd.s -= len;
	if(_rfd.s) return NOK;
	return OK;
}
template <>
int Serializer::store<int16>(Base &_rb, FncData &_rfd){
	_rfd.s = sizeof(int16);
	return storeBinary(_rb, _rfd);
}
template <>
int Serializer::store<uint16>(Base &_rb, FncData &_rfd){
	_rfd.s = sizeof(uint16);
	return storeBinary(_rb, _rfd);
}
template <>
int Serializer::store<int32>(Base &_rb, FncData &_rfd){
	_rfd.s = sizeof(int32);
	return storeBinary(_rb, _rfd);
}
template <>
int Serializer::store<uint32>(Base &_rb, FncData &_rfd){
	_rfd.s = sizeof(uint32);
	idbg(""<<*(uint32*)_rfd.p);
	return storeBinary(_rb, _rfd);
}
template <>
int Serializer::store<int64>(Base &_rb, FncData &_rfd){
	_rfd.s = sizeof(int64);
	return storeBinary(_rb, _rfd);
}
template <>
int Serializer::store<uint64>(Base &_rb, FncData &_rfd){
	_rfd.s = sizeof(uint64);
	return storeBinary(_rb, _rfd);
}
template <>
int Serializer::store<std::string>(Base &_rb, FncData &_rfd){
	Serializer &rs(static_cast<Serializer&>(_rb));
	if(!rs.cpb) return OK;
	std::string * c = reinterpret_cast<std::string*>(_rfd.p);
	rs.estk.push(ExtData(c->size()));
	rs.replace(FncData(&Serializer::storeBinary, (void*)c->data(), _rfd.n, c->size()));
	rs.fstk.push(FncData(&Base::popEStack, NULL));
	rs.fstk.push(FncData(&Serializer::store<uint32>, &rs.estk.top().u32()));
	return CONTINUE;
}
//========================================================================
Deserializer::~Deserializer(){
}
void Deserializer::clear(){
	run(NULL, 0);
}
int Deserializer::run(const char *_pb, unsigned _bl){
	cpb = pb = _pb;
	be = pb + _bl;
	while(fstk.size()){
		FncData &rfd = fstk.top();
		switch((*reinterpret_cast<FncTp>(rfd.f))(*this, rfd)){
			case CONTINUE: continue;
			case OK: fstk.pop(); break;
			case NOK: goto Done;
			case BAD: return -1;
		}
	}
	Done:
	return cpb - pb;
}
int Deserializer::parseBinary(Base &_rb, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	if(!rd.cpb) return OK;
	unsigned len = rd.be - rd.cpb;
	if(len > _rfd.s) len = _rfd.s;
	memcpy(_rfd.p, rd.cpb, len);
	rd.cpb += len;
	_rfd.p = (char*)_rfd.p + len;
	_rfd.s -= len;
	if(_rfd.s) return NOK;
	return OK;
}
template <>
int Deserializer::parse<int16>(Base &_rb, FncData &_rfd){
	_rfd.s = sizeof(int16);
	return parseBinary(_rb, _rfd);
}
template <>
int Deserializer::parse<uint16>(Base &_rb, FncData &_rfd){
	_rfd.s = sizeof(uint16);
	return parseBinary(_rb, _rfd);
}
template <>
int Deserializer::parse<int32>(Base &_rb, FncData &_rfd){
	_rfd.s = sizeof(int32);
	return parseBinary(_rb, _rfd);
}
template <>
int Deserializer::parse<uint32>(Base &_rb, FncData &_rfd){
	_rfd.s = sizeof(uint32);
	return parseBinary(_rb, _rfd);
}

template <>
int Deserializer::parse<int64>(Base &_rb, FncData &_rfd){
	_rfd.s = sizeof(int64);
	return parseBinary(_rb, _rfd);
}
template <>
int Deserializer::parse<uint64>(Base &_rb, FncData &_rfd){
	_rfd.s = sizeof(uint64);
	return parseBinary(_rb, _rfd);
}
template <>
int Deserializer::parse<std::string>(Base &_rb, FncData &_rfd){
	idbg("parse generic non pointer string");
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	if(!rd.cpb) return OK;
	rd.estk.push(ExtData(0));
	rd.replace(FncData(&Deserializer::parseBinaryString, _rfd.p, _rfd.n));
	rd.fstk.push(FncData(&Deserializer::parse<uint32>, &rd.estk.top().u32()));
	return CONTINUE;
}
int Deserializer::parseBinaryString(Base &_rb, FncData &_rfd){
	Deserializer &rd(static_cast<Deserializer&>(_rb));
	if(!rd.cpb){
		rd.estk.pop();
		return OK;
	}
	unsigned len = rd.be - rd.cpb;
	uint32 ul = rd.estk.top().u32();
	if(len > ul) len = ul;
	std::string *ps = reinterpret_cast<std::string*>(_rfd.p);
	ps->append(rd.cpb, len);
	rd.cpb += len;
	ul -= len;
	if(ul){rd.estk.top().u32() = ul; return NOK;}
	rd.estk.pop();
	return OK;
}
//========================================================================
//========================================================================
}//namespace bin
}//namespace serialization

