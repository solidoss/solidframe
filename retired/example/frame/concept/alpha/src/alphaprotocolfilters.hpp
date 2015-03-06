// alphaprotocolfilters.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef ALPHA_PROTOCOL_FILTERS_HPP
#define ALPHA_PROTOCOL_FILTERS_HPP

namespace concept{
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

struct DigitFilter{
	static bool check(int _c){
		return isdigit(_c);
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
}//namespace concept


#endif
