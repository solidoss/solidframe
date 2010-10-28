/* Declarations file namematcher.hpp
	
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

#ifndef ALGORITHM_PROTOCOL_NAME_MATCHER_HPP
#define ALGORITHM_PROTOCOL_NAME_MATCHER_HPP

namespace protocol{
//!A simple and fast runtime case-insensitive name matcher
/*! Intended usage example:<br>
<CODE>
	struct Name{<br>
		const char *name;<br>
		Command*(*)() pf;<br>
	} const cmd_tbl[] = {<br>
		{"login", &create_login},<br>
		{"delete", &create_delete},<br>
		{"create", &create_create},<br>
		{NULL, &create_null}<br>
	};<br>
	static const NameMatcher cmdm(cmd_tbl);<br>
	<br>
	string s;cin>>s;<br>
	Command *pcmd = (*cmd_tbl[cmdm.match(s.c_str())].pf)();<br>
</CODE>
*/

class NameMatcher{
	class Data;
public:
	//! Template constructor giving the table containing the names
	/*! \see NameMatcher documentation
		There must be a last NULL name in the given table
	*/
	template <typename Name>
	NameMatcher(const Name *_names):d(createData()){
		while(_names->name){
			push(_names->name);
			++_names;
		}
	}
	//!Destructor
	~NameMatcher();
	//! Matching method
	/*!
		Returns the position of the matched string or the position of NULL
		if no match was found.
		\param _name The name to search
		\retval int the position in the table given on constructor
	*/
	int match(const char *_name)const;
private:
	void push(const char *_name);
	Data& createData();
	Data &d;
};

}//namespace protocol

#endif
