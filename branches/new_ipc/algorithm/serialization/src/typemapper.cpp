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

#include <deque>
#include <vector>
#include <map>

#include "system/cassert.hpp"
#include "system/mutex.hpp"
#include "algorithm/serialization/typemapper.hpp"
#include "algorithm/serialization/idtypemap.hpp"

namespace serialization{
//================================================================
struct TypeMapper::Data{
	typedef std::vector<BaseTypeMap*> TypeMapVectorT;
	Data();
	~Data();
	ulong 			sercnt;
	TypeMapVectorT	tmvec;
#ifndef N_SERIALIZATION_MUTEX
	Mutex			m;
#endif
/*#else
#ifdef U_SERIALIZATION_RWMUTEX
	RWMutex			m;
#else
#endif*/

};
//================================================================
TypeMapper::Data::Data():sercnt(0){}
TypeMapper::Data::~Data(){
	for(TypeMapVectorT::const_iterator it(tmvec.begin()); it != tmvec.end(); ++it){
		delete *it;
	}
}
TypeMapper::TypeMapper():d(*(new Data)){
}

TypeMapper::~TypeMapper(){
	delete &d;
}

/*static*/ unsigned TypeMapper::newMapId(){
	//TODO: staticproblem
	static unsigned d(-1);
	return ++d;
}

/*static*/ unsigned TypeMapper::newSerializerId(){
	//TODO: staticproblem
	static unsigned d(-1);
	return ++d;
}

void TypeMapper::doRegisterMap(unsigned _id, BaseTypeMap *_pbm){
	cassert(_id == d.tmvec.size());
	d.tmvec.push_back(_pbm);
}

void TypeMapper::doMap(FncT _pf, unsigned _pos, const char *_name){
	for(Data::TypeMapVectorT::const_iterator it(d.tmvec.begin()); it != d.tmvec.end(); ++it){
		(*it)->insert(_pf, _pos, _name, d.sercnt);
	}
}

void TypeMapper::serializerCount(unsigned _cnt){
	d.sercnt = _cnt + 1;
}

BaseTypeMap &TypeMapper::getMap(unsigned _id){
	return *d.tmvec[_id];
}

void TypeMapper::lock(){
#ifndef N_SERIALIZATION_MUTEX
	d.m.lock();
#endif
}
void TypeMapper::unlock(){
#ifndef N_SERIALIZATION_MUTEX
	d.m.unlock();
#endif
}
//================================================================

BaseTypeMap::~BaseTypeMap(){
}

//================================================================

struct IdTypeMap::Data{
	typedef std::deque<IdTypeMap::FncT>	FncVectorT;
	typedef std::map<const char*, uint32>	NameMapT;
	NameMapT	nmap;
	FncVectorT	fncvec;
};

IdTypeMap::IdTypeMap():d(*(new Data)){
}

IdTypeMap::~IdTypeMap(){
	delete &d;
}

BaseTypeMap::FncT IdTypeMap::parseTypeIdDone(const std::string &_rstr, ulong _serid){
	const uint32 *const pu = reinterpret_cast<const uint32*>(_rstr.data());
	cassert(*pu < d.fncvec.size());
	cassert(d.fncvec[*pu]);
	return d.fncvec[*pu];
}

/*virtual*/ void IdTypeMap::insert(FncT _pf, unsigned _pos, const char *_name, unsigned _maxcnt){
	uint32 sz = d.fncvec.size();
	std::pair<Data::NameMapT::iterator, bool> p(d.nmap.insert(Data::NameMapT::value_type(_name, sz)));
	if(p.second){//key inserted
		d.fncvec.resize(sz + _maxcnt);
		for(uint32 i = sz; i < d.fncvec.size(); ++i){
			d.fncvec[i] = NULL;
		}
		d.fncvec[sz + _pos] = _pf;
	}else{//key already mapped
		d.fncvec[p.first->second + _pos] = _pf;
	}
}

uint32 &IdTypeMap::getFunction(FncT &_rpf, const char *_name, std::string &_rstr, ulong _serid){
	Data::NameMapT::const_iterator it(d.nmap.find(_name));
	cassert(it != d.nmap.end());
	uint32 *pu = reinterpret_cast<uint32*>(const_cast<char*>(_rstr.data()));
	*pu = it->second + _serid;
	_rpf = d.fncvec[*pu];
	cassert(_rpf != NULL);
	return *pu;
}

//================================================================

}//namespace serialization
