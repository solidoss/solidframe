/* Declarations file alphaprotocolfilters.h
	
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

#ifndef ALPHA_PROTOCOL_FILTERS_H
#define ALPHA_PROTOCOL_FILTERS_H

namespace test{
namespace alpha{

template <int C>
struct CharFilter{
	static bool check(int _c){
		return _c == C;
	}
};


template <typename T>
struct NotFilter{
	static bool check(int _c){
		return !T::check(_c);
	}
};

struct AtomFilter{
	static bool check(int _c){
		return isalpha(_c) || isdigit(_c);
	}
};

struct QuotedFilter{
    //TODO: change to use bitset
    static bool check(int _c){
    	if(!_c) 		return false;
        if(_c == '\r')	return false;
        if(_c == '\n')	return false;
        if(_c == '\"')	return false;
        if(_c == '\\')	return false;
        if(((unsigned char)_c) > 127) return false;
        return true;
    }
};

struct TagFilter{
	static bool check(int _c){
		return isalnum(_c);
	}
};

}//namespace alpha
}//namespace test


#endif
