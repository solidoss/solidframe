/* Implementation file namematcher.cpp
	
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

#include "algorithm/protocol/namematcher.hpp"
#include <map>
#include <cstring>
//#include <tr1/unordered_map>
#include "system/cassert.hpp"
using namespace std;

namespace protocol{

struct StrCmp{
	bool operator()(const char *const &_s1, const char *const &_s2)const{
		return strcasecmp(_s1, _s2) < 0;
	}
};
struct NameMatcher::Data{
	Data(){}
	typedef map<const char*, int, StrCmp> StrMapT;
	//typedef tr1::unordered_map<const char*, int/*, StrCmp*/> StrMapT;
	StrMapT 	m;
};

NameMatcher::Data& NameMatcher::createData(){
	return *new Data;
}
NameMatcher::~NameMatcher(){
	delete &d;
}

int NameMatcher::match(const char *_name)const{
	Data::StrMapT::const_iterator it(d.m.find(_name));
	if(it != d.m.end()) return it->second;
	return d.m.size();
}

void NameMatcher::push(const char *_name){
	pair<Data::StrMapT::iterator, bool> r(d.m.insert(Data::StrMapT::value_type(_name, d.m.size())));
	cassert(r.second);
}


}//namespace protocol

