// protocol/text/src/namematcher.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "protocol/text/namematcher.hpp"
#include <map>
#include <cstring>
#include "system/cassert.hpp"

using namespace std;

namespace solid{
namespace protocol{
namespace text{

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

}//namespace text
}//namespace protocol
}//namespace solid

