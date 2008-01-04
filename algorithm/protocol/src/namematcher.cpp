/* Implementation file namematcher.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "algorithm/protocol/namematcher.h"
#include <map>
//#include <tr1/unordered_map>
#include <cassert>
using namespace std;

namespace protocol{

struct StrCmp{
	bool operator()(const char *const &_s1, const char *const &_s2)const{
		return strcasecmp(_s1, _s2) < 0;
	}
};
struct NameMatcher::Data{
	Data(){}
	typedef map<const char*, int, StrCmp> StrMapTp;
	//typedef tr1::unordered_map<const char*, int/*, StrCmp*/> StrMapTp;
	StrMapTp 	m;
};

NameMatcher::Data& NameMatcher::createData(){
	return *new Data;
}
NameMatcher::~NameMatcher(){
	delete &d;
}

int NameMatcher::match(const char *_name)const{
	Data::StrMapTp::const_iterator it(d.m.find(_name));
	if(it != d.m.end()) return it->second;
	return d.m.size();
}

void NameMatcher::push(const char *_name){
	pair<Data::StrMapTp::iterator, bool> r(d.m.insert(Data::StrMapTp::value_type(_name, d.m.size())));
	assert(r.second);
}


}//namespace protocol

