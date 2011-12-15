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

#include <deque>
#include <vector>
#include <map>

#include "system/cassert.hpp"
#include "system/mutex.hpp"
#include "algorithm/serialization_new/idtypemapper.hpp"

namespace serialization_new{
//================================================================
struct TypeMapperBase::Data{
	Data(){}
	~Data();
};
//================================================================
TypeMapperBase::Data::~Data(){
	
}
TypeMapperBase::TypeMapperBase():d(*(new Data())){
}

TypeMapperBase::~TypeMapperBase(){
	delete &d;
}

/*virtual*/ void TypeMapperBase::prepareStorePointer(
	void *_pser, void *_p,
	uint32 _rid, const char *_name
){
}
/*virtual*/ void TypeMapperBase::prepareStorePointer(
	void *_pser, void *_p,
	const char *_pid, const char *_name
){
}

TypeMapperBase::FncT TypeMapperBase::function(const uint32 _id, uint32* &_rpid){
	return NULL;
}
TypeMapperBase::FncT TypeMapperBase::function(const char *_pid, uint32* &_rpid){
	return NULL;
}
TypeMapperBase::FncT TypeMapperBase::function(const uint32 _id){
	return NULL;
}

/*virtual*/ void TypeMapperBase::prepareParsePointer(
	void *_pdes, std::string &_rs,
	void *_p, const char *_name
){
	
}
/*virtual*/ void TypeMapperBase::prepareParsePointerId(
	void *_pdes, std::string &_rs,
	const char *_name
){
	
}
uint32 TypeMapperBase::insertFunction(FncT _f, uint32 _pos, const char *_name){
	
}

}//namespace serialization
