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
//===============================================================================
int Base::popEStack(Base &_rb, FncData &_rfd){
	_rb.estk.pop();
	return OK;
}

int Base::storeBinary(Base &_rb, FncData &_rfd){
	if(!cpb.ps) return OK;
	unsigned len = be.ps - cpb.ps;
	if(len > _rfd.s) len = _rfd.s;
	memcpy(cpb.ps, _rfd.p, len);
	cpb.ps += len;
	_rfd.p = (char*)_rfd.p + len;
	_rfd.s -= len;
	if(_rfd.s) return NOK;
	return OK;
}
void Base::replace(const FncData &_rfd){
	fstk.top() = _rfd;
}
int Base::storeStream(Base &_rb, FncData &_rfd){
	if(!cpb.ps) return OK;
	std::pair<IStream*, int64> &rsp(*reinterpret_cast<std::pair<IStream*, int64>*>(estk.top().buf));
	int32 toread = be.ps - cpb.ps;
	if(toread < MINSTREAMBUFLEN) return NOK;
	toread -= 2;//the buffsize
	if(toread > rsp.second) toread = rsp.second;
	int rv = rsp.first->read(cpb.ps + 2, toread);
	if(rv == toread){
		uint16 &val = *((uint16*)cpb.ps);
		val = toread;
		cpb.ps += toread + 2;
	}else{
		uint16 &val = *((uint16*)cpb.ps);
		val = 0xffff;
		cpb.ps += 2;
		return OK;
	}
	rsp.second -= toread;
	if(rsp.second) return NOK;
	return OK;
}

int Base::parseBinary(Base &_rb, FncData &_rfd){
	if(!cpb.pc) return OK;
	unsigned len = be.pc - cpb.pc;
	if(len > _rfd.s) len = _rfd.s;
	memcpy(_rfd.p, cpb.pc, len);
	cpb.pc += len;
	_rfd.p = (char*)_rfd.p + len;
	_rfd.s -= len;
	if(_rfd.s) return NOK;
	return OK;
}

int Base::parseStream(Base &_rb, FncData &_rfd){
	if(!cpb.ps) return OK;
	std::pair<OStream*, int64> &rsp(*reinterpret_cast<std::pair<OStream*, int64>*>(estk.top().buf));
	if(rsp.second < 0) return OK;
	int32 towrite = be.pc - cpb.pc;
	cassert(towrite > 2);
	towrite -= 2;
	if(towrite > rsp.second) towrite = rsp.second;
	uint16 &rsz = *((uint16*)cpb.pc);
	cpb.pc += 2;
	if(rsz == 0xffff){//error on storing side - the stream is incomplete
		rsp.second = -1;
		return OK;
	}
	int rv = rsp.first->write(cpb.pc, towrite);
	cpb.pc += towrite;
	rsp.second -= towrite;
	if(rv != towrite){
		_rfd.f = &Base::parseDummyStream;
	}
	if(rsp.second) return NOK;
	return OK;
}
int Base::parseDummyStream(Base &_rb, FncData &_rfd){
	if(!cpb.ps) return OK;
	std::pair<OStream*, int64> &rsp(*reinterpret_cast<std::pair<OStream*, int64>*>(estk.top().buf));
	if(rsp.second < 0) return OK;
	int32 towrite = be.pc - cpb.pc;
	cassert(towrite > 2);
	towrite -= 2;
	if(towrite > rsp.second) towrite = rsp.second;
	uint16 &rsz = *((uint16*)cpb.pc);
	cpb.pc += 2;
	if(rsz == 0xffff){//error on storing side - the stream is incomplete
		rsp.second = -1;
		return OK;
	}
	cpb.pc += towrite;
	rsp.second -= towrite;
	if(rsp.second) return NOK;
	rsp.second = -1;//the write was not complete
	return OK;
}
//===============================================================================
#ifdef UTHREADS
RTTIMapper::RTTIMapper():pmut(new Mutex){}
RTTIMapper::~RTTIMapper(){
	delete pmut;
}
void RTTIMapper::lock(){
	pmut->lock();
}
void RTTIMapper::unlock(){
	pmut->unlock();
}
#else
RTTIMapper::RTTIMapper():pmut(NULL){}
RTTIMapper::~RTTIMapper(){
}
void RTTIMapper::lock(){
}
void RTTIMapper::unlock(){
}
#endif

Base::FncTp RTTIMapper::map(const std::string &_s){
	Deserializer<RTTIMapper>::FncTp	pf;
	lock();
	FunctionMapTp::iterator it = m.find(_s.c_str());
	if(it != m.end()){
		pf = it->second.second;
		unlock();
		return (Base::FncTp)pf;
	}
	unlock();
	return NULL;
}

}//namespace bin
}//namespace serialization

