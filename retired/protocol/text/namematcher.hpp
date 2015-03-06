// protocol/text/namematcher.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_PROTOCOL_TEXT_NAME_MATCHER_HPP
#define SOLID_PROTOCOL_TEXT_NAME_MATCHER_HPP


namespace solid{
namespace protocol{
namespace text{
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


}//namespace text
}//namespace protocol
}//namespace solid


#endif
