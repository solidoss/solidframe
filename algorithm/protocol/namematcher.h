/* Declarations file namematcher.h
	
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

#ifndef PROTOCOL_NAME_MATCHER_H
#define PROTOCOL_NAME_MATCHER_H

namespace protocol{
//!A simple and fast runtime name matcher
/*! Intended usage example:<br>
	struct Name{
		const char *name;
		Command*(*)() pf;
	} const cmd_tbl[] = {
		{"login", &create_login},
		{"delete", &create_delete},
		{"create", &create_create},
		{NULL, &create_null}
	};
	static const NameMatcher cmdm(cmd_tbl);
	
	string s;cin>>s;
	Command *pcmd = (*cmd_tbl[cmdm.match(s.c_str())].pf)();
*/

class NameMatcher{
	class Data;
public:
	template <typename Name>
	NameMatcher(const Name *_names):d(createData()){
		while(_names->name){
			push(_names->name);
			++_names;
		}
	}
	~NameMatcher();
	int match(const char *_name)const;
private:
	void push(const char *_name);
	Data& createData();
	Data &d;
};

}//namespace protocol

#endif
